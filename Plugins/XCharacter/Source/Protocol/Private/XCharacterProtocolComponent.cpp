// Fill out your copyright notice in the Description page of Project Settings.

#include "XCharacterProtocolComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Animation/AnimInstance.h"
#include "JsonObjectConverter.h"
#include "DataSynthesizerLibrary.h"


DEFINE_LOG_CATEGORY_STATIC(LogXCharacterProtocolComponent, Verbose, All);

void UXCharacterProtocolComponent::OnWebsocketConnect_CallBack()
{
	UE_LOG(LogXCharacterProtocolComponent, Warning, TEXT("Successfully Connected with the Server:"));
	WebSocketHandler->SendText(Message);
}

void UXCharacterProtocolComponent::OnWebsocketOnMessageSent_CallBack(const FString& MessageSent)
{
	UE_LOG(LogXCharacterProtocolComponent, Warning, TEXT("Connect Sent. %s"), *MessageSent);
}

void UXCharacterProtocolComponent::OnWebsocketTextMessage_CallBack(const FString& TextMessage)
{	

	double DeltaTime = WorldRef->GetTimeSeconds() - LastTimeSecond;
	UE_LOG(LogXCharacterProtocolComponent, Warning, TEXT("Time is: %.2f"), DeltaTime);
	LastTimeSecond = WorldRef->GetTimeSeconds();

	UE_LOG(LogTemp, Warning, TEXT("Buffer Size is %d"), DataNums);

	
	FResponse Config;
	UDataSynthesizerLibrary::Decode(TextMessage, Config);


	if (Config.err_no != 0) {
		OnResponseErrorDelegate.Broadcast(Config.err_no, Config.err_msg);
		return;
	}

	if (Config.is_end) {
		bResponse = false;
		OnResponseEnd.Broadcast();
		return;
	}

	FString AudioBase64 = Config.respond_content.tts_channel.audio;

	if (!AudioBase64.IsEmpty()) {
		TArray<uint8> AudioData;
		UDataSynthesizerLibrary::DecodePCM(AudioBase64, AudioData);
		AudioBuffer.Enqueue(AudioData);
	}

	FString GestureBase64 = Config.respond_content.anim_channel.gesture_channel.gesture_data.bone_bytes;
	BoneNames = Config.respond_content.anim_channel.gesture_channel.gesture_data.bone_names;

	if (!GestureBase64.IsEmpty()) {
		TArray<float> GestureData;
		UDataSynthesizerLibrary::DecodeGesture(GestureBase64, GestureData);
		int32 Length = 7 * BoneNames.Num();

		for (int i = 0; i < GestureData.Num(); i += Length) {
			TArray<float> TempData;
			TempData.SetNum(Length);
			float* Pointer = GestureData.GetData() + i;
			int32 ArrayLength = Length * sizeof(float);
			FMemory::Memcpy(TempData.GetData(), Pointer, ArrayLength);
			GestureBuffer.Enqueue(TempData);
		}
	}

	FString FaceBase64 = Config.respond_content.anim_channel.face_channel.face_bytes;

	if (!FaceBase64.IsEmpty()) {
		TArray<float> BSData;
		
		int32 Rows = Config.respond_content.anim_channel.face_channel.face_rows;
		int32 Cols = Config.respond_content.anim_channel.face_channel.face_cols;
		
		UDataSynthesizerLibrary::DecodeFaceCurve(BSData,FaceBase64, Rows, Cols);

		for (int i = 0; i < BSData.Num(); i += Cols) {
			TArray<float> TempData;
			TempData.SetNum(Cols);
			FMemory::Memcpy(TempData.GetData(), BSData.GetData() + i, Cols * sizeof(float));
			FaceBuffer.Enqueue(TempData);
		}
	}


	FString Content = Config.respond_content.chat_channel.token_content;
	if (!Content.IsEmpty()) {
		TextBuffer.Enqueue(Content);
	}

	if (!StartTimer) {
		WorldRef->GetTimerManager().SetTimer(TimeHandle, this, &ThisClass::NotifyComponent, 0.04f, true);
		StartTimer = true;
	}
	
	if (!bResponse) {
		OnResponseBegin.Broadcast();
		bResponse = true;
	}
}

void UXCharacterProtocolComponent::OnWebsocketError_CallBack(const FString& TextMessage)
{
	UE_LOG(LogXCharacterProtocolComponent, Error, TEXT("Connect Error. %s"), *TextMessage);
	OnResponseErrorDelegate.Broadcast(403, TextMessage);
}

void UXCharacterProtocolComponent::OnWebsocketClosed_CallBack(const int32& Code, const FString& Reason, bool ClosedByPeer)
{
	UE_LOG(LogXCharacterProtocolComponent, Warning, TEXT("Websocket Closed. Code: %d, Reason: %s, ClosedByPeer?: %hhd"),
		Code, *Reason, ClosedByPeer);
}

// Sets default values for this component's properties
UXCharacterProtocolComponent::UXCharacterProtocolComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
	WorldRef = GetWorld();
}

UXCharacterProtocolComponent::~UXCharacterProtocolComponent()
{
	/*if (WorldRef) {
		WorldRef->GetTimerManager().ClearTimer(TimeHandle);
	}*/
	StartTimer = false;
}

void UXCharacterProtocolComponent::NotifyComponent()
{


	TArray<uint8> AudioData;
	AudioBuffer.Dequeue(AudioData);

	if (!AudioData.IsEmpty()) {
		OnAudioClipReceiveDelegate.Broadcast(AudioData);
	}
	
	TArray<float> GestureData;
	GestureBuffer.Dequeue(GestureData);

	if (!GestureData.IsEmpty()) {
		OnGestureReceiveDelegate.Broadcast(GestureData, BoneNames);
	}

	TArray<float> BSData;
	FaceBuffer.Dequeue(BSData);

	if (!FaceBuffer.IsEmpty()) {
		OnBSCurveReceiveDelegate.Broadcast(BSData);
	}

	FString Content;
	TextBuffer.Dequeue(Content);
	if (!Content.IsEmpty()) {
		OnTextReceiveDelegate.Broadcast(Content);
	}

}

//void UXCharacterProtocolComponent::SendStartSignal(FConfig ChatConfig)
//{
//	/*if (!WebSocketHandler->IsConnected()) {
//		UE_LOG(LogXCharacterProtocolComponent, Error, TEXT("Connect Failed."));
//		return;
//	}*/
//
//	FString Message;
//	FJsonObjectConverter::UStructToJsonObjectString(ChatConfig, Message);
//	UE_LOG(LogXCharacterProtocolComponent, Warning, TEXT("SendStartSignal is : '%s'"), *Message);
//	WebSocketHandler->SendText(Message);
//}

void UXCharacterProtocolComponent::SendEndSignal()
{
	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
	RootObject->SetStringField("signal", "audio_end");

	TSharedRef<TJsonWriter<TCHAR>> JsonWriter = TJsonWriterFactory<TCHAR>::Create(&Message);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), JsonWriter);
	UE_LOG(LogXCharacterProtocolComponent, Warning, TEXT("SendEndSignal is : '%s'"), *Message);
	WebSocketHandler->SendText(Message);
}

void UXCharacterProtocolComponent::SendAudioClip()
{

}

void UXCharacterProtocolComponent::SendMessage(FRequest RequestData)
{
	Connect();
	FJsonObjectConverter::UStructToJsonObjectString(RequestData, Message);
	UE_LOG(LogXCharacterProtocolComponent, Warning, TEXT("Sent Message is : '%s'"), *Message);
}

void UXCharacterProtocolComponent::Close()
{
	WebSocketHandler->Close();
}


// Called when the game starts
void UXCharacterProtocolComponent::BeginPlay()
{
	Super::BeginPlay();
	// ...
}


void UXCharacterProtocolComponent::Connect() {
	
	
	FWebSocketOptions WSOptions{
		EWebSocketProtocol::WS,
		ServerIP
	};

	if (!WebSocketHandler) {
		WebSocketHandler = UWebSocketHandler::CreateWebSocket(GetWorld());
		WebSocketHandler->OnConnected.AddDynamic(this, &ThisClass::OnWebsocketConnect_CallBack);
		WebSocketHandler->OnMessageSent.AddDynamic(this, &ThisClass::OnWebsocketOnMessageSent_CallBack);
		WebSocketHandler->OnTextMessage.AddDynamic(this, &ThisClass::OnWebsocketTextMessage_CallBack);
		WebSocketHandler->OnClosed.AddDynamic(this, &ThisClass::OnWebsocketClosed_CallBack);
		WebSocketHandler->OnConnectionError.AddDynamic(this, &ThisClass::OnWebsocketError_CallBack);
	}

	if (!WebSocketHandler->Open(WSOptions))
	{
		UE_LOG(LogXCharacterProtocolComponent, Error, TEXT("Connect WebSocket Failed"));
		WebSocketHandler->Close();
	}
}

// Called every frame
void UXCharacterProtocolComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}




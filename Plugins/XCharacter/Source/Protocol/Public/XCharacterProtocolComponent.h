// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "ProtocolStruct.h"
#include "WebSocketHandler.h"
#include "XCharacterProtocolComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTextReceiveDelegate, const FString&, Text);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioClipReceiveDelegate, const TArray<uint8>&, Message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnResponseErrorDelegate, const int32&, ErrorCode, const FString&, ErrorMessage);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGestureReceiveDelegate, const TArray<float>&, Message, const TArray<FString>&, boneName);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBSCurveReceiveDelegate, const TArray<float>&, Message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnResponseEnd);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnResponseBegin);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROTOCOL_API UXCharacterProtocolComponent : public UActorComponent
{
	GENERATED_BODY()

private:
	UWorld* WorldRef;

	TQueue<TArray<uint8>> AudioBuffer;
	TQueue<TArray<float>> GestureBuffer;
	TQueue<TArray<float>> FaceBuffer;
	TQueue<FString> TextBuffer;
	TArray<FString> BoneNames;

public:

	/**此处服务器调用需要解耦*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
	FString ServerIP = "ws://118.25.191.134:10015/ws";
	//FString ServerIP = "wss://sit-audio.xverse.cn/server/avatar_driven";
	//FString ServerIP = "wss://sit-online.xverse.cn/audio/multi_syn_huya";

	UPROPERTY(BlueprintAssignable, Category = "Protocol|Event")
	FOnTextReceiveDelegate OnTextReceiveDelegate;
	UPROPERTY(BlueprintAssignable, Category = "Protocol|Event")
	FOnAudioClipReceiveDelegate OnAudioClipReceiveDelegate;
	UPROPERTY(BlueprintAssignable, Category = "Protocol|Event")
	FOnGestureReceiveDelegate OnGestureReceiveDelegate;
	UPROPERTY(BlueprintAssignable, Category = "Protocol|Event")
	FOnBSCurveReceiveDelegate OnBSCurveReceiveDelegate;
	UPROPERTY(BlueprintAssignable, Category = "Protocol|Event")
	FOnResponseEnd OnResponseEnd;
	UPROPERTY(BlueprintAssignable, Category = "Protocol|Event")
	FOnResponseBegin OnResponseBegin;
	UPROPERTY(BlueprintAssignable, Category = "Protocol|Event")
	FOnResponseErrorDelegate OnResponseErrorDelegate;


	UPROPERTY()
	FTimerHandle TimeHandle;

	UPROPERTY()
	class UWebSocketHandler* WebSocketHandler;

	UFUNCTION()
	void OnWebsocketConnect_CallBack();

	UFUNCTION()
	void OnWebsocketOnMessageSent_CallBack(const FString& MessageSent);

	UFUNCTION()
	void OnWebsocketTextMessage_CallBack(const FString& TextMessage);

	UFUNCTION()
	void OnWebsocketError_CallBack(const FString& TextMessage);
	
	UFUNCTION()
	void OnWebsocketClosed_CallBack(const int32& Code, const FString& Reason, bool ClosedByPeer);

public:	
	// Sets default values for this component's properties
	UXCharacterProtocolComponent();

	~UXCharacterProtocolComponent();

	UFUNCTION()
	void NotifyComponent();

	/*UFUNCTION()
	void SendStartSignal(FConfig Config);*/

	UFUNCTION()
	void SendEndSignal();

	UFUNCTION()
	void SendAudioClip();

	UFUNCTION()
	void SendMessage(FRequest RequestData);

	UFUNCTION()
	void Close();

	UFUNCTION()
	void Connect();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	int8 BufferSize{ 10 };
	int8 DataNums{ 0 };
	bool StartTimer{ false };
	bool bResponse{ false };
	double LastTimeSecond{0};
	TQueue<FString> ServerBuffer{};
	FString Message;
	
public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
		
};

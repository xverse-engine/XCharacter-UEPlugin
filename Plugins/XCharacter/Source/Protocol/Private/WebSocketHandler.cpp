// Fill out your copyright notice in the Description page of Project Settings.

#include "WebSocketHandler.h"
#include "WebSocketsModule.h"
#include "Modules/ModuleManager.h"


UWebSocketHandler::~UWebSocketHandler()
{
	if (this->Socket)
	{
		this->Socket->OnClosed().Clear();
		this->Socket->OnConnected().Clear();
		this->Socket->OnConnectionError().Clear();
		this->Socket->OnMessage().Clear();
		this->Socket->OnMessageSent().Clear();
		this->Socket->OnRawMessage().Clear();
		this->Close();
	}
}

UWebSocketHandler* UWebSocketHandler::CreateWebSocket(const UObject* WorldContext)
{
	auto Node = NewObject<UWebSocketHandler>();
	Node->WorldContextObject = WorldContext->GetWorld();
	return Node;
}

bool UWebSocketHandler::IsConnected()
{
	if (this->Socket != nullptr)
	{
		return this->Socket->IsConnected();
	}
	return false;
}

bool UWebSocketHandler::Open(const FWebSocketOptions& WebSocketOptions)
{
	if (this->Socket == nullptr || !this->Socket->IsConnected())
	{
		this->Options = WebSocketOptions;
		// protocol
		FString StrProtocol = UEnum::GetValueAsString(this->Options.Protocol);
		FString Protocol;
		StrProtocol.Split("::", &StrProtocol, &Protocol);
		FWebSocketsModule& Mod = FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));
		this->Socket = Mod.Get().CreateWebSocket(this->Options.Url, Protocol.ToLower(), this->Options.Headers);
		
		if (this->Socket == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("WebSocketHandler: Failed to initialize socket"))
				return false;
		}
		// callbacks
		this->Socket->OnConnected().AddLambda([this]() -> void
			{
				this->Buffer.Empty();
				this->ReconnectAmount = 0;
				if (this->ReconnectHandle.IsValid())
				{
					this->ReconnectHandle.Invalidate();
				}
				if (this->OnConnected.IsBound())
				{
					this->OnConnected.Broadcast();
				}
			});
		this->Socket->OnConnectionError().AddLambda([this](const FString& Error) -> void
			{
				this->Buffer.Empty();
				if (this->OnConnectionError.IsBound())
				{
					this->OnConnectionError.Broadcast(Error);
				}
				if (this->Options.ReconnectTimeout > 0 && this->ReconnectAmount < this->Options.ReconnectAmount)
				{
					if (this->WorldContextObject != nullptr)
					{
						(*this->WorldContextObject).GetTimerManager().SetTimer(this->ReconnectHandle, [this]()
							{
								this->ReconnectAmount++;
								if (this->OnConnectionRetry.IsBound())
								{
									this->OnConnectionRetry.Broadcast(
										this->ReconnectAmount);
								}
								this->Open(this->Options);
								UE_LOG(LogTemp, Warning,
									TEXT("WS: Retrying to connect %i"),
									this->ReconnectAmount);
							}
							, this->Options.ReconnectTimeout, false,
								(-1.0f));
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("WS: WorldContext invalid, cannot retry to connect"));
					}
				}
			});
		this->Socket->OnClosed().AddLambda([this](int32 StatusCode, const FString& Reason, bool bWasClean) -> void
			{
				if (this->OnClosed.IsBound())
				{
					this->OnClosed.Broadcast(StatusCode, Reason, bWasClean);
				}
			});
		this->Socket->OnMessage().AddLambda([this](const FString& Message) -> void
			{
				if (this->OnTextMessage.IsBound())
				{
					this->OnTextMessage.Broadcast(Message);
				}
			});
		this->Socket->OnRawMessage().AddLambda([this](const void* Data, SIZE_T Size, SIZE_T BytesRemaining) -> void
			{
				const uint8* Ptr = reinterpret_cast<uint8 const*>(Data);
				this->Buffer.Append(Ptr, Size);
				if (BytesRemaining == 0)
				{
					if (this->OnByteMessage.IsBound())
					{
						TArray<uint8> Bytes(this->Buffer);
						this->OnByteMessage.Broadcast(Bytes);
					}
					this->Buffer.Empty();
				}
			});
		this->Socket->OnMessageSent().AddLambda([this](const FString& MessageString) -> void
			{
				if (this->OnMessageSent.IsBound())
				{
					this->OnMessageSent.Broadcast(MessageString);
				}
			});
		// connect
		this->Socket->Connect();
		this->AddToRoot();
		return true;
	}
	UE_LOG(LogTemp, Warning, TEXT("WS: Socket connection is already open with a peer"));
	return false;
}

bool UWebSocketHandler::Close(int32 Code, const FString& Reason)
{
	if (this && this->Socket != nullptr && this->Socket->IsConnected())
	{
		this->Socket->Close(Code, Reason);
		this->Buffer.Empty();
		this->RemoveFromRoot();
		return true;
	}
	return false;
}

bool UWebSocketHandler::SendText(const FString& Data)
{
	if (this->Socket != nullptr && this->Socket->IsConnected())
	{
		this->Socket->Send(Data);
		return true;
	}
	return false;
}

bool UWebSocketHandler::SendBytes(const TArray<uint8>& Data, bool IsBinary)
{
	if (this->Socket != nullptr && this->Socket->IsConnected())
	{
		this->Socket->Send(Data.GetData(), sizeof(uint8) * Data.Num(), IsBinary);
		return true;
	}
	return false;
}

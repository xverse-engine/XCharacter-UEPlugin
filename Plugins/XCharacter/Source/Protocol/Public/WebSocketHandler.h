// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "IWebSocket.h"
#include "Engine/World.h"
#include "WebSocketHandler.generated.h"

UENUM(BlueprintType)
enum class EWebSocketProtocol : uint8
{
	WS UMETA(DisplayName = "WS"),
	WSS UMETA(DisplayName = "WSS")
};


USTRUCT(BlueprintType)
struct FWebSocketOptions
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "WebSocket")
	EWebSocketProtocol Protocol = EWebSocketProtocol::WSS;
	UPROPERTY(BlueprintReadWrite, Category = "WebSocket")
	FString Url;
	UPROPERTY(BlueprintReadWrite, Category = "WebSocket")
	TMap<FString, FString> Headers;
	UPROPERTY(BlueprintReadWrite, Category = "WebSocket")
	float ReconnectTimeout = 0;
	UPROPERTY(BlueprintReadWrite, Category = "WebSocket")
	int32 ReconnectAmount = 3;

};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnConnectedDelegate);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnClosedDelegate, const int32&, Code, const FString&, Reason, bool,
	ClosedByPeer);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConnectionErrorDelegate, const FString&, ErrorReason);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTextMessageDelegate, const FString&, Message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMessageSentDelegate, const FString&, Message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnByteMessageDelegate, const TArray<uint8>&, Message);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRetryDelegate, const int32&, RetryCount);

/**
 * 
 */
UCLASS()
class PROTOCOL_API UWebSocketHandler : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "WebSocket|Event")
	FOnConnectedDelegate OnConnected;
	UPROPERTY(BlueprintAssignable, Category = "WebSocket|Event")
	FOnClosedDelegate OnClosed;
	UPROPERTY(BlueprintAssignable, Category = "WebSocket|Event")
	FOnConnectionErrorDelegate OnConnectionError;
	UPROPERTY(BlueprintAssignable, Category = "WebSocket|Event")
	FOnTextMessageDelegate OnTextMessage;
	UPROPERTY(BlueprintAssignable, Category = "WebSocket|Event")
	FOnByteMessageDelegate OnByteMessage;
	UPROPERTY(BlueprintAssignable, Category = "WebSocket|Event")
	FOnMessageSentDelegate OnMessageSent;
	UPROPERTY(BlueprintAssignable, Category = "WebSocket|Event")
	FOnRetryDelegate OnConnectionRetry;

private:
	FWebSocketOptions Options;
	TSharedPtr<IWebSocket> Socket;
	TArray<uint8> Buffer;
	FTimerHandle ReconnectHandle;
	int32 ReconnectAmount = 0;
	UWorld* WorldContextObject = nullptr;

public:
	~UWebSocketHandler();
	UFUNCTION(BlueprintPure, Category = "WebSocket", meta = (WorldContext = "WorldContext"))
	static UWebSocketHandler* CreateWebSocket(const UObject* WorldContext);
	UFUNCTION(BlueprintPure, Category = "WebSocket")
	bool IsConnected();
	UFUNCTION(BlueprintCallable, Category = "WebSocket")
	bool Open(const FWebSocketOptions& Options);
	UFUNCTION(BlueprintCallable, Category = "WebSocket")
	bool Close(int32 Code = 1000, const FString& Reason = "");
	UFUNCTION(BlueprintCallable, Category = "WebSocket")
	bool SendText(const FString& Data);
	UFUNCTION(BlueprintCallable, Category = "WebSocket")
	bool SendBytes(const TArray<uint8>& Data, bool IsBinary = false);
	
};

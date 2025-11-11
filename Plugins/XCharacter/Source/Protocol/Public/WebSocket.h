// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "WebSocketHandler.h"
#include "WebSocket.generated.h"

UENUM(BlueprintType)
enum class EWebSocketError : uint8
{
	None UMETA(DisplayName = "None"),
	Activation UMETA(DisplayName = "Activation"),
	Connection UMETA(DisplayName = "Connection")
};

USTRUCT(BlueprintType)
struct FWebSocketResult
{
	GENERATED_BODY()
	;

public:
	UPROPERTY(BlueprintReadOnly, Category = "WebSocket")
	TArray<uint8> BytesMessage;
	UPROPERTY(BlueprintReadOnly, Category = "WebSocket")
	FString TextMessage;
	UPROPERTY(BlueprintReadOnly, Category = "WebSocket")
	int32 Code;
	UPROPERTY(BlueprintReadOnly, Category = "WebSocket")
	FString ClosedReason;
	UPROPERTY(BlueprintReadOnly, Category = "WebSocket")
	bool ClosedByPeer;
	UPROPERTY(BlueprintReadOnly, Category = "WebSocket")
	EWebSocketError ErrorReason = EWebSocketError::None;
	UPROPERTY(BlueprintReadOnly, Category = "WebSocket")
	FString Error;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWebSocketOutputPin, const FWebSocketResult&, Result);

/**
 *
 */
UCLASS()
class PROTOCOL_API UWebSocket : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FWebSocketOutputPin OnConnected;
	UPROPERTY(BlueprintAssignable)
	FWebSocketOutputPin OnTextMessage;
	UPROPERTY(BlueprintAssignable)
	FWebSocketOutputPin OnBytesMessage;
	UPROPERTY(BlueprintAssignable)
	FWebSocketOutputPin OnClosed;
	UPROPERTY(BlueprintAssignable)
	FWebSocketOutputPin OnRetry;
	UPROPERTY(BlueprintAssignable)
	FWebSocketOutputPin OnError;

private:
	const UObject* WorldContextObject = nullptr;
	bool Active = false;
	UWebSocketHandler* WebSocket = nullptr;
	FWebSocketOptions Options;

public:
	UFUNCTION(BlueprintCallable,
		meta = (BlueprintInternalUseOnly = "true", Category = "WebSocket", WorldContext = "WorldContextObject"))
	static UWebSocket* AsyncWebSocket(const UObject* WorldContextObject, const FWebSocketOptions& Options,
		UWebSocketHandler*& Result);
	virtual void Activate() override;

private:
	UFUNCTION()
	void _Failed(EWebSocketError Reason = EWebSocketError::None, FString Error = "");
	UFUNCTION()
	void _Connected();
	UFUNCTION()
	void _Closed(const int32& Code, const FString& Reason, bool ClosedByPeer);
	UFUNCTION()
	void _ConnectionError(const FString& Reason);
	UFUNCTION()
	void _TextMessage(const FString& Message);
	UFUNCTION()
	void _ByteMessage(const TArray<uint8>& Message);
	UFUNCTION()
	void _Retry(const int32& RetryCount);
};

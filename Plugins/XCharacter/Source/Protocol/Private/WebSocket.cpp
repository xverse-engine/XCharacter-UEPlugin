// Fill out your copyright notice in the Description page of Project Settings.

#include "WebSocket.h"

UWebSocket* UWebSocket::AsyncWebSocket(const UObject* WorldContextObject, const FWebSocketOptions& Options,
	UWebSocketHandler*& Result)
{
	UWebSocket* Node = NewObject<UWebSocket>();
	Node->WebSocket = UWebSocketHandler::CreateWebSocket(WorldContextObject);
	Result = Node->WebSocket;
	Node->Options = Options;
	Node->WorldContextObject = WorldContextObject;
	return Node;
}

void UWebSocket::Activate()
{
	if (nullptr == this->WorldContextObject)
	{
		FFrame::KismetExecutionMessage(
			TEXT("Invalid WorldContextObject. Cannot execute WebSocket"), ELogVerbosity::Error);
		this->_Failed(EWebSocketError::Activation, "Invalid WorldContextObject. Cannot execute WebSocket");
		return;
	}
	if (this->Active)
	{
		FFrame::KismetExecutionMessage(
			TEXT("WebSocket is already running, close websocket and restart to update options"),
			ELogVerbosity::Warning);
		return;
	}
	// on connected
	this->WebSocket->OnConnected.AddDynamic(this, &UWebSocket::_Connected);
	// on closed
	this->WebSocket->OnClosed.AddDynamic(this, &UWebSocket::_Closed);
	// on connection error
	this->WebSocket->OnConnectionError.AddDynamic(this, &UWebSocket::_ConnectionError);
	// on text message
	this->WebSocket->OnTextMessage.AddDynamic(this, &UWebSocket::_TextMessage);
	// on bytes message
	this->WebSocket->OnByteMessage.AddDynamic(this, &UWebSocket::_ByteMessage);
	// on retry
	this->WebSocket->OnConnectionRetry.AddDynamic(this, &UWebSocket::_Retry);
	// open connection
	this->WebSocket->Open(this->Options);
	this->Active = true;
}

void UWebSocket::_Connected()
{
	if (this->OnConnected.IsBound())
	{
		FWebSocketResult Result;
		this->OnConnected.Broadcast(Result);
	}
}

void UWebSocket::_Closed(const int32& Code, const FString& Reason, bool ClosedByPeer)
{
	this->Active = false;
	if (this->OnClosed.IsBound())
	{
		FWebSocketResult Result;
		Result.Code = Code;
		Result.ClosedReason = Reason;
		Result.ClosedByPeer = ClosedByPeer;
		this->OnClosed.Broadcast(Result);
	}
}

void UWebSocket::_ConnectionError(const FString& Reason)
{
	if (this->Options.ReconnectAmount <= 0 || this->Options.ReconnectTimeout <= 0)
	{
		this->Active = false;
	}
	if (this->OnError.IsBound())
	{
		FWebSocketResult Result;
		Result.Error = Reason;
		Result.ErrorReason = EWebSocketError::Connection;
		this->OnError.Broadcast(Result);
	}
}

void UWebSocket::_TextMessage(const FString& Message)
{
	if (this->OnTextMessage.IsBound())
	{
		FWebSocketResult Result;
		Result.TextMessage = Message;
		this->OnTextMessage.Broadcast(Result);
	}
}

void UWebSocket::_ByteMessage(const TArray<uint8>& Message)
{
	if (this->OnBytesMessage.IsBound())
	{
		FWebSocketResult Result;
		Result.BytesMessage = Message;
		this->OnBytesMessage.Broadcast(Result);
	}
}

void UWebSocket::_Retry(const int32& RetryCount)
{
	if (RetryCount >= this->Options.ReconnectAmount)
	{
		this->Active = false;
	}
	if (this->OnRetry.IsBound())
	{
		FWebSocketResult Result;
		this->OnRetry.Broadcast(Result);
	}
}

void UWebSocket::_Failed(EWebSocketError Reason, FString Error)
{
	if (this->OnError.IsBound())
	{
		FWebSocketResult Result;
		Result.Error = Error;
		Result.ErrorReason = Reason;
		this->OnError.Broadcast(Result);
	}
}

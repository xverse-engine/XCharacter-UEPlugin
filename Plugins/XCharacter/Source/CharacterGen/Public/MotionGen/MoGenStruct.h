#include "CoreMinimal.h"

#include <iostream>
#include <vector>
#include <string>
#include <mutex>

#ifndef STRUCT_DEFINITIONS_H
#define STRUCT_DEFINITIONS_H

enum EProcessStatus {
	INITED,
	FINISHED,
	PROCESSING,
	FAILED,
};

struct MoGenTask {
	FString traceId;
	FString taskId;
	FString appId;
	EProcessStatus status;
};

struct TTSTask {
	FString traceId;
	FString taskId;
	FString appId;
	EProcessStatus status;
};

struct CreateTaskRequest {
	FString traceId;
	FString userId;
	FString appId;
	FString taskType;
	FString data;
};


struct CreateTaskResponse {
	FString code;
	FString msg;
	FString taskId;
	FString batchId;
};

struct AuthenticateResponse {
	FString code;
	FString msg;
	FString request_id;
};

#endif // STRUCT_DEFINITIONS_H
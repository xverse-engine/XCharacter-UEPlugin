// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotionGen/MotionGenService.h"
#include "MotionGen/MoGenSetting.h"
#include "MotionGen/BVHImporter.h"
#include "MotionGen/BVHImportFactory.h"
#include "MotionGen/BVHImportSettings.h"
#include "XVCPluginSettings.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Guid.h"
#include "Containers/StringConv.h"
#include "Misc/OutputDevice.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor.h"
#include "Misc/MessageDialog.h"

FMotionGenService::FMotionGenService()
{
    Importer = new FBVHImporter();
    BVHFactory = NewObject<UBVHImportFactory>(GetTransientPackage(), UBVHImportFactory::StaticClass());
}

FMotionGenService::~FMotionGenService()
{
    if (Importer)
    {
        delete Importer;
        Importer = nullptr;
    }
}

/**
* HTTP Get - 获取远程BVH文件
*/
void FMotionGenService::GetRemoteBVHFile(FString OuterPath, FString SourcePath, UMoGenSetting* MoGenSetting,
                                         TSharedPtr<MoGenTask> Task, TSharedPtr<FProgressNotificationHandle> HandlePtr)
{
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();

    FHttpModule* Http = &FHttpModule::Get();
    if (!Http) { return; }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
    Request->SetURL(SourcePath);
    Request->SetVerb(TEXT("GET"));
    Request->SetTimeout(Settings->Timeout);

    Request->OnProcessRequestComplete().BindLambda(
        [this, MoGenSetting, OuterPath, Task, HandlePtr](FHttpRequestPtr Request, FHttpResponsePtr Response,
                                                         bool bWasSuccessful)
        {
            if (bWasSuccessful && Response.IsValid())
            {
                FString Content = Response->GetContentAsString();

                TObjectPtr<UBVHImportSettings> ImportSettings = NewObject<UBVHImportSettings>(
                    GetTransientPackage(), UBVHImportSettings::StaticClass());
                this->Importer->SetImportSetting(ImportSettings);

                this->Importer->SetMoGenSetting(MoGenSetting);
                this->Importer->CreateBVHFileByContent(Content);

                UPackage* Package = CreatePackage(*OuterPath);

                ImportSettings->MotionName = FString::Printf(
                    TEXT("MoGen_%s_%s"), *MoGenSetting->AssetName, *Task->traceId);
                this->BVHFactory->CreateAnimSequence(MoGenSetting->Skeleton, Package, this->Importer);

                Task->status = EProcessStatus::FINISHED;

                FSlateNotificationManager::Get().UpdateProgressNotification(*HandlePtr, 100);
                FSlateNotificationManager::Get().CancelProgressNotification(*HandlePtr);

                FString Notify = FString::Printf(TEXT("Successfully Download: %s"), *MoGenSetting->AssetName);
                FNotificationInfo Info(FText::FromString(Notify));
                FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(
                    SNotificationItem::CS_Success);
            }
            else
            {
                Task->status = EProcessStatus::FAILED;
                FNotificationInfo Info(
                    FText::FromString("Failed to request BVH file. Check the log for more information."));
                FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
            }
        });
    Request->ProcessRequest();
}

/**
* HTTP Get - 认证请求
*/
bool FMotionGenService::Authenticate(UMoGenSetting* MoGenSetting)
{
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();

    FHttpModule* Http = &FHttpModule::Get();
    if (!Http) { return false; }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
    Request->SetURL(FString::Printf(TEXT("%s%s"), *Settings->IP_LLM, *Settings->API_ServerAuth));
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader("Content-Type", "application/json");
    Request->SetHeader("Authorization", FString::Printf(TEXT("Bearer %s"), *Settings->SecretKey));
    Request->SetTimeout(Settings->Timeout);

    Request->OnProcessRequestComplete().BindLambda(
        [this, MoGenSetting](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (bWasSuccessful && Response.IsValid())
            {
                FString JsonString = Response->GetContentAsString();
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
                TSharedPtr<FJsonObject> JsonObject;

                bool bSerialized = FJsonSerializer::Deserialize(Reader, JsonObject);
                bool bValid = JsonObject.IsValid();

                if (bSerialized && bValid)
                {
                    int Code = JsonObject->GetIntegerField(TEXT("code"));

                    if (Code == 0)
                    {
                        CreateTask(MoGenSetting);
                    }
                    else
                    {
                        FString Msg = JsonObject->GetStringField(TEXT("msg"));
                        FString ErrorCode = FString::Printf(TEXT("Authentication failed: %s"), *Msg);
                        FNotificationInfo Info(FText::FromString(ErrorCode));
                        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(
                            SNotificationItem::CS_Fail);
                    }
                }
                else
                {
                    FNotificationInfo Info(
                        FText::FromString("Authentication request failed. Check the log for more information."));
                    FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(
                        SNotificationItem::CS_Fail);
                }
            }
            else
            {
                FNotificationInfo Info(
                    FText::FromString("Failed to request animation data. Check the log for more information."));
                FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
            }
        });

    Request->ProcessRequest();
    return true;
}

/**
* HTTP Post - 创建任务
*/
void FMotionGenService::CreateTask(UMoGenSetting* MoGenSetting)
{
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();

    if (MoGenSetting->Prompt == "")
    {
        FMessageDialog::Open(EAppMsgType::Type::Ok, FText::FromString(TEXT("The prompt cannot be empty!")));
        return;
    }

    FHttpModule* Http = &FHttpModule::Get();
    if (!Http) { return; }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
    Request->SetURL(FString::Printf(TEXT("%s%s"), *Settings->IP_Platform, *Settings->API_CreateTask));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader("Content-Type", "application/json");
    Request->SetTimeout(Settings->Timeout);

    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);

    FGuid NewGuid = FGuid::NewGuid();

    RootObject->SetStringField("userId", Settings->UserID);
    RootObject->SetStringField("traceId", NewGuid.ToString());
    RootObject->SetStringField("appId", Settings->AppID);
    RootObject->SetStringField("taskType", Settings->TaskType);

    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    JsonObject->SetStringField("prompt", MoGenSetting->Prompt);
    bool isUseRecommand = MoGenSetting->bUseRecommendLength;
    if (isUseRecommand)
    {
        JsonObject->SetNumberField("animLength", 0);
    }
    else
    {
        JsonObject->SetNumberField("animLength", FMath::Clamp<float>(MoGenSetting->AnimationLength, 1.0f, 10.0f));
    }

    JsonObject->SetNumberField("offset", MoGenSetting->OffsetLength);
    JsonObject->SetStringField("sk", Settings->SecretKey);

    FString RequestJson;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestJson);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    RootObject->SetStringField("data", RequestJson);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer2 = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer2);

    Request->SetContentAsString(OutputString);
    Request->OnProcessRequestComplete().BindLambda(
        [this, MoGenSetting, NewGuid](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();

            if (bWasSuccessful && Response.IsValid())
            {
                FString JsonString = Response->GetContentAsString();
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
                TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

                bool bSerialized = FJsonSerializer::Deserialize(Reader, JsonObject);
                bool bValid = JsonObject.IsValid();

                if (bSerialized && bValid)
                {
                    int code = JsonObject->GetIntegerField(TEXT("code"));

                    if (code == 0)
                    {
                        //add task
                        TSharedPtr<MoGenTask> task = MakeShareable(new MoGenTask);

                        task->taskId = JsonObject->GetStringField(TEXT("taskId"));
                        task->appId = Settings->AppID;
                        task->traceId = NewGuid.ToString();
                        task->status = EProcessStatus::INITED;

                        UMoGenSetting* NewMoGenSetting = DuplicateObject(MoGenSetting, GetTransientPackage());
                        TickInfo(task, NewMoGenSetting);

                        FNotificationInfo Info(
                            FText::FromString("Task has been sent to Inference Server, please wait..."));
                        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(
                            SNotificationItem::CS_Success);
                    }
                    else
                    {
                        FString Msg = JsonObject->GetStringField(TEXT("msg"));
                        FString ErrorInfo = FString::Printf(TEXT("Request failed. Error Code %d %s"), code, *Msg);
                        FNotificationInfo Info(FText::FromString(ErrorInfo));
                        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(
                            SNotificationItem::CS_Fail);
                    }
                }
                else
                {
                    FString ResponseBody = Response->GetContentAsString();
                    FString ErrorInfo = FString::Printf(
                        TEXT("Deserialize failed, mistach result. Content: %s"), *ResponseBody);
                    FNotificationInfo Info(FText::FromString(ErrorInfo));
                    FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(
                        SNotificationItem::CS_Fail);
                }
            }
            else
            {
                FString ResponseBody = Response->GetContentAsString();
                FString ErrorInfo = FString::Printf(
                    TEXT("Request failed. Please check your network and try again. Error %s"), *ResponseBody);
                FNotificationInfo Info(FText::FromString(ErrorInfo));
                FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
            }
        });

    Request->ProcessRequest();
}

/**
* HTTP Post - 获取任务信息
*/
void FMotionGenService::GetTaskInfo(TSharedPtr<MoGenTask> Task, UMoGenSetting* MoGenSetting,
                                    TSharedPtr<FTimerHandle> TimerHandle, TSharedPtr<FProgressNotificationHandle> HandlePtr)
{
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();

    Task->status = EProcessStatus::PROCESSING;

    FHttpModule* Http = &FHttpModule::Get();
    if (!Http) { return; }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
    Request->SetURL(FString::Printf(TEXT("%s%s"), *Settings->IP_Platform, *Settings->API_GetTaskInfo));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader("Content-Type", "application/json");
    Request->SetTimeout(Settings->Timeout);

    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);

    RootObject->SetStringField("traceId", Task->traceId);
    RootObject->SetStringField("taskId", Task->taskId);
    RootObject->SetStringField("appId", Task->appId);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

    Request->SetContentAsString(OutputString);

    Request->OnProcessRequestComplete().BindLambda(
        [this, Task, MoGenSetting, TimerHandle, HandlePtr](FHttpRequestPtr Request, FHttpResponsePtr Response,
                                                           bool bWasSuccessful)
        {
            const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();

            if (bWasSuccessful && Response.IsValid())
            {
                FString JsonString = Response->GetContentAsString();
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
                TSharedPtr<FJsonObject> JsonObject;

                bool bSerialized = FJsonSerializer::Deserialize(Reader, JsonObject);
                bool bValid = JsonObject.IsValid();

                if (bSerialized && bValid)
                {
                    TSharedPtr<FJsonObject> info = JsonObject->GetObjectField(TEXT("info"));
                    FString bizOutput = info->GetStringField(TEXT("bizOutput"));

                    Reader = TJsonReaderFactory<>::Create(bizOutput);
                    TSharedPtr<FJsonObject> outputJson = MakeShareable(new FJsonObject);

                    bSerialized = FJsonSerializer::Deserialize(Reader, outputJson);
                    bValid = outputJson.IsValid();

                    if (bSerialized && bValid)
                    {
                        if (outputJson->HasField(TEXT("code")))
                        {
                            int Code = outputJson->GetNumberField(TEXT("code"));
                            if (Code != 0 || info->GetStringField(TEXT("currentStatus")) == TEXT("TS_FAILED"))
                            {
                                FString Message = outputJson->GetStringField(TEXT("msg"));
                                FNotificationInfo Info(FText::FromString(Message));
                                FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(
                                    SNotificationItem::CS_Fail);
                                Task->status = EProcessStatus::FAILED;
                            }
                            else
                            {
                                if (outputJson->HasField(TEXT("animUrl")))
                                {
                                    FString Url = outputJson->GetStringField(TEXT("animUrl"));
                                    GetRemoteBVHFile(Settings->MotionGenWorkSpace, Url, MoGenSetting, Task, HandlePtr);
                                    FText Notifcation = FText::FromString(TEXT("Downloading Motion Data..."));
                                    FSlateNotificationManager::Get().UpdateProgressNotification(*HandlePtr, 70);
                                    Task->status = EProcessStatus::FINISHED;
                                    UWorld* World = GEditor->GetEditorWorldContext().World();
                                    World->GetTimerManager().ClearTimer(*TimerHandle);
                                }
                                else
                                {
                                    Task->status = EProcessStatus::INITED;
                                }
                            }
                        }
                    }
                    else
                    {
                        Task->status = EProcessStatus::INITED;
                    }
                }
                else
                {
                    FNotificationInfo Info(
                        FText::FromString("Failed to parse animation address. Check the log for more information."));
                    FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(
                        SNotificationItem::CS_Fail);
                    Task->status = EProcessStatus::FAILED;
                }
            }
            else
            {
                FNotificationInfo Info(FText::FromString("GetTaskInfo Failed. Check the log for more information."));
                FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
                Task->status = EProcessStatus::FAILED;
            }
        });

    Request->ProcessRequest();
}

/**
* HTTP Post Heart Beat - 心跳检测任务状态
*/
void FMotionGenService::TickInfo(TSharedPtr<MoGenTask> Task, UMoGenSetting* MoGenSetting)
{
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    UWorld* World = GEditor->GetEditorWorldContext().World();

    TSharedPtr<FTimerHandle> TimerHandle = MakeShared<FTimerHandle>();

    int Counter = 0;
    int MaxTime = Settings->MaxRequestTime;

    TSharedPtr<FProgressNotificationHandle> HandlePtr =
        MakeShareable(new FProgressNotificationHandle(FSlateNotificationManager::Get().
            StartProgressNotification(FText::FromString("Download Motion"), 100)));

    FSlateNotificationManager::Get().UpdateProgressNotification(*HandlePtr, 10, 100);

    World->GetTimerManager().SetTimer(*TimerHandle,
                                      [this, Task, MoGenSetting, TimerHandle, Counter, HandlePtr, MaxTime]() mutable
                                      {
                                          if (Task->status == EProcessStatus::INITED)
                                          {
                                              GetTaskInfo(Task, MoGenSetting, TimerHandle, HandlePtr);
                                          }

                                          FSlateNotificationManager::Get().UpdateProgressNotification(
                                              *HandlePtr, 10 + Counter * 2);

                                          bool isFailed = false;

                                          if (++Counter > MaxTime)
                                          {
                                              FString ErrorInfo = FString::Printf(
                                                  TEXT("Request Timeout, please try again."));
                                              FNotificationInfo Info(FText::FromString(ErrorInfo));
                                              FSlateNotificationManager::Get().AddNotification(Info)->
                                                                               SetCompletionState(
                                                                                   SNotificationItem::CS_Fail);
                                              isFailed = true;
                                          }

                                          if (Task->status == EProcessStatus::FAILED || Task->status ==
                                              EProcessStatus::FINISHED)
                                          {
                                              isFailed = true;
                                          }

                                          if (isFailed)
                                          {
                                              FSlateNotificationManager::Get().CancelProgressNotification(*HandlePtr);
                                              UWorld* World = GEditor->GetEditorWorldContext().World();
                                              World->GetTimerManager().ClearTimer(*TimerHandle);
                                          }
                                      }, Settings->HeartBeat, true);
} 
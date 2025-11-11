// Copyright Epic Games, Inc. All Rights Reserved.

#include "../../Public/TTS/TTSServer.h"
#include "XVCPluginSettings.h"
#include "TTS/TTSSetting.h"
#include "Misc/Base64.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Tools/GenTools.h"
#include "Containers/UnrealString.h"
#include "Misc/DefaultValueHelper.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Engine/World.h"
#include "Editor.h"

// 静态事件实例
static UTTSServer::FTTSAllCompleted GTTSAllCompletedDelegate;
static UTTSServer::FEnhanceAudioCompleted GEnhanceAudioCompletedDelegate;
static UTTSServer::FTTSProgressUpdate GTTSProgressUpdateDelegate;

// 本地静态缓存，避免依赖编辑器模块（需在使用前声明）
static TArray<TSharedPtr<FString>> GSpeakerDisplayOptions;
static bool GbSpeakerListLoaded = false;
static TArray<TFunction<void(const TArray<TSharedPtr<FString>>&)> > GPendingSpeakerListCallbacks;
// 维护 spkName -> spkId 的映射，便于根据显示名查找真实ID
static TMap<FString, FString> GSpkNameToId;

// TTS批次管理静态变量
static int32 GCurrentBatchIndex = 0;
static int32 GBatchSizeValue = 2;
static TArray<FString> GAllTextSegmentsArray;
static TArray<UTTSSetting*> GAllTempSettingsArray;
static UTTSSetting* GOriginalTTSSettingsPtr = nullptr;

UTTSServer::FTTSAllCompleted& UTTSServer::OnTTSAllCompleted()
{
    return GTTSAllCompletedDelegate;
}

UTTSServer::FEnhanceAudioCompleted& UTTSServer::OnEnhanceAudioCompleted()
{
    return GEnhanceAudioCompletedDelegate;
}

UTTSServer::FTTSProgressUpdate& UTTSServer::OnTTSProgressUpdate()
{
    return GTTSProgressUpdateDelegate;
}

void UTTSServer::ResetTTSBatchState()
{
    GCurrentBatchIndex = 0;
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    GBatchSizeValue = Settings->TTSBatchSize;
    GAllTextSegmentsArray.Empty();
    GAllTempSettingsArray.Empty();
    GOriginalTTSSettingsPtr = nullptr;
    UE_LOG(LogTemp, Log, TEXT("TTS批次状态已重置"));
}

void UTTSServer::CallTTSServer(UTTSSetting* TTSSettings)
{
    // 重置批次状态，确保每次开始新的合成时都从第1批开始
    ResetTTSBatchState();
    // 使用配置中的批次大小
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    CallTTSServerBatched(TTSSettings, Settings->TTSBatchSize);
}

void UTTSServer::CallTTSServerBatched(UTTSSetting* TTSSettings, int32 BatchSize)
{
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    if (!TTSSettings)
    {
        FNotificationInfo Info(FText::FromString(TEXT("TTS Settings are invalid.")));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }
    if (TTSSettings->TTSInputText.IsEmpty())
    {
        FNotificationInfo Info(FText::FromString(TEXT("TTS input text cannot be empty.")));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }
    
    // 路径准备 - 使用GenTools统一处理路径设置
    FString BasePath = Settings->TTSStoragePath;
    FString AssetPath, WavDir;
    if (!UGenTools::SetupTTSStoragePaths(BasePath, AssetPath, WavDir))
    {
        FNotificationInfo Info(FText::FromString(TEXT("设置TTS存储路径失败！")));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }
    
    // 按\n切分文本
    TArray<FString> TextSegments;
    TTSSettings->TTSInputText.ParseIntoArray(TextSegments, TEXT("\n"), true);
    
    // 过滤空文本
    TArray<FString> ValidTextSegments;
    for (const FString& Segment : TextSegments)
    {
        if (!Segment.TrimStartAndEnd().IsEmpty())
        {
            ValidTextSegments.Add(Segment.TrimStartAndEnd());
        }
    }
    
    if (ValidTextSegments.Num() == 0)
    {
        FNotificationInfo Info(FText::FromString(TEXT("TTS输入文本无有效内容。")));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }
    
    const int32 ReplicatesPerSegment = Settings->TTSReplicatesPerSegment;
    const int32 TotalSegments = ValidTextSegments.Num();
    const int32 TotalRequests = TotalSegments * ReplicatesPerSegment;
    
    // 创建共享的完成计数器和资产路径数组
    TSharedPtr<int32> CompletedCount = MakeShared<int32>(0);
    TSharedPtr<int32> TotalCount = MakeShared<int32>(TotalRequests);
    TSharedPtr<TArray<FString>> CompletedAssetPaths = MakeShared<TArray<FString>>();
    TSharedPtr<TArray<FTTSSegmentResult>> SegmentResults = MakeShared<TArray<FTTSSegmentResult>>();
    
    // 创建所有临时设置对象
    TArray<UTTSSetting*> AllTempSettings;
    for (int32 i = 0; i < TotalSegments; ++i)
    {
        UTTSSetting* TempSettings = NewObject<UTTSSetting>(GetTransientPackage());
        TempSettings->SpeakerName = TTSSettings->SpeakerName;
        TempSettings->EmotionName = TTSSettings->EmotionName;
        TempSettings->AudioSpeed = TTSSettings->AudioSpeed;
        TempSettings->AudioPitch = TTSSettings->AudioPitch;
        TempSettings->VoiceAudio = TTSSettings->VoiceAudio;
        TempSettings->EmotionAudio = TTSSettings->EmotionAudio;
        TempSettings->VoiceAudioFileName = TTSSettings->VoiceAudioFileName;
        TempSettings->CustomEmotionName = TTSSettings->CustomEmotionName;
        
        // 复制音频字节数据
        TempSettings->VoiceAudioBytes = TTSSettings->VoiceAudioBytes;
        TempSettings->EmotionAudioBytes = TTSSettings->EmotionAudioBytes;
        TempSettings->bUseCustomVoiceAudio = TTSSettings->bUseCustomVoiceAudio;
        TempSettings->bUseCustomEmotionAudio = TTSSettings->bUseCustomEmotionAudio;
        
        // 设置当前段落的文本内容，支持三列数据格式
        FString TextContent = ValidTextSegments[i];
        
        // 检查是否包含三列数据（用|分隔）
        TArray<FString> ColumnData;
        TextContent.ParseIntoArray(ColumnData, TEXT("|"), false);
        
        if (ColumnData.Num() >= 3)
        {
            // 三列数据：文本|段前静音|段后静音
            FString MainText = ColumnData[0].TrimStartAndEnd();
            FString PreSilence = ColumnData[1].TrimStartAndEnd();
            FString PostSilence = ColumnData[2].TrimStartAndEnd();
            
            UE_LOG(LogTemp, Log, TEXT("段落%d: 文本='%s', 段前静音='%s', 段后静音='%s'"), 
                i, *MainText, *PreSilence, *PostSilence);
            
            // 只使用文本部分进行TTS合成
            TempSettings->TTSInputText = MainText;
            
            UE_LOG(LogTemp, Log, TEXT("TTS合成使用纯文本: '%s' (已过滤静音信息)"), *MainText);
            
            // 静音信息将在预览界面中处理，不参与TTS合成
        }
        else
        {
            // 单列数据，直接使用
            TempSettings->TTSInputText = TextContent;
            UE_LOG(LogTemp, Log, TEXT("TTS合成使用单列文本: '%s'"), *TextContent);
        }
        
        AllTempSettings.Add(TempSettings);
    }
    
    // 更新全局批次管理变量
    GCurrentBatchIndex = 0;
    GBatchSizeValue = BatchSize;
    GAllTextSegmentsArray = ValidTextSegments;
    GAllTempSettingsArray = AllTempSettings;
    GOriginalTTSSettingsPtr = TTSSettings;
    
    // 发送第一批请求
    SendBatchRequests(&GCurrentBatchIndex, &GBatchSizeValue, &GAllTextSegmentsArray, &GAllTempSettingsArray, 
                     GOriginalTTSSettingsPtr, Settings, AssetPath, WavDir, 
                     CompletedCount, TotalCount, CompletedAssetPaths, SegmentResults, ReplicatesPerSegment);
    
    // 显示TTS合成开始通知
    FNotificationInfo StartInfo(FText::FromString(FString::Printf(TEXT("正在请求TTS服务 ..."))));
    StartInfo.bUseLargeFont = true;
    StartInfo.ExpireDuration = 5.0f;
    FSlateNotificationManager::Get().AddNotification(StartInfo);
}

void UTTSServer::SendBatchRequests(int32* CurrentBatchIndex, 
                                  int32* BatchSize, 
                                  TArray<FString>* AllTextSegments,
                                  TArray<UTTSSetting*>* AllTempSettings,
                                  UTTSSetting* OriginalTTSSettings,
                                  const UXVCPluginSettings* Settings,
                                  const FString& AssetPath,
                                  const FString& WavDir,
                                  TSharedPtr<int32> CompletedCount,
                                  TSharedPtr<int32> TotalCount,
                                  TSharedPtr<TArray<FString>> CompletedAssetPaths,
                                  TSharedPtr<TArray<FTTSSegmentResult>> SegmentResults,
                                  int32 ReplicatesPerSegment)
{
    const int32 TotalSegments = AllTextSegments->Num();
    const int32 StartSegmentIndex = (*CurrentBatchIndex) * (*BatchSize);
    const int32 EndSegmentIndex = FMath::Min(StartSegmentIndex + (*BatchSize), TotalSegments);
    
    UE_LOG(LogTemp, Log, TEXT("发送第 %d 批请求，处理段落 %d 到 %d"), *CurrentBatchIndex + 1, StartSegmentIndex, EndSegmentIndex - 1);
    
    // 发送当前批次的请求
    for (int32 SegmentIndex = StartSegmentIndex; SegmentIndex < EndSegmentIndex; ++SegmentIndex)
    {
        UTTSSetting* TempSettings = (*AllTempSettings)[SegmentIndex];
        
        // 针对每段文本进行多次合成
        for (int32 ReplicateIndex = 0; ReplicateIndex < ReplicatesPerSegment; ++ReplicateIndex)
        {
            FTTSRequestParams RequestParams(TempSettings, Settings, SegmentIndex, AssetPath, WavDir, 
                                          CompletedCount, TotalCount, CompletedAssetPaths, SegmentResults, ReplicateIndex,
                                          CurrentBatchIndex, BatchSize, AllTextSegments, AllTempSettings, OriginalTTSSettings);
            SendTTSRequest(RequestParams);
        }
    }
}

void UTTSServer::SendTTSRequest(const FTTSRequestParams& Params)
{
    // 获取音色数据（可能是字符串或字节流）
    FString SpeakerName;
    TArray<uint8> SpeakerAudioBytes;
    bool bHasSpeakerAudio = Params.LocalSettings->GetCurrentSpeakerData(SpeakerName, SpeakerAudioBytes);
    
    // 判断是否使用zero-shot接口
    bool bUseZeroShot = bHasSpeakerAudio && SpeakerAudioBytes.Num() > 0;
    
    FString Payload;
    FString TTSText = Params.LocalSettings->TTSInputText;
    Payload.Append(TEXT("tts=") + FGenericPlatformHttp::UrlEncode(TTSText));
    
    UE_LOG(LogTemp, Log, TEXT("发送TTS请求 - 段落%d: '%s'"), Params.SegmentIndex, *TTSText);
    
    // 根据是否使用zero-shot接口来构建不同的参数
    if (bUseZeroShot)
    {
        // Zero-shot接口参数
        UE_LOG(LogTemp, Log, TEXT("使用Zero-shot接口"));
        
        // 添加音色音频（Base64编码）
        FString Base64Audio = FBase64::Encode(SpeakerAudioBytes);
        Payload.Append(TEXT("&audio=") + FGenericPlatformHttp::UrlEncode(Base64Audio));
    }
    else
    {
        // 普通SFT接口参数
        FString SpkrId = SpeakerName;
        UE_LOG(LogTemp, Log, TEXT("原始SpeakerName: '%s'"), *SpeakerName);
        
        // 检查SpeakerName是否为空
        if (SpeakerName.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("SpeakerName为空，请先选择音色！"));
            FNotificationInfo Info(FText::FromString(TEXT("请先选择音色！")));
            FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
            return;
        }
        
        // 解析显示字符串 "gender|age|talking_style|spkr" 以取出 spkr
        FString ParsedSpkr = SpeakerName;
        {
            TArray<FString> Parts;
            SpeakerName.ParseIntoArray(Parts, TEXT("|"), false);
            if (Parts.Num() >= 4)
            {
                ParsedSpkr = Parts[3].TrimStartAndEnd();
            }
        }

        // 通过映射获取真正的 spkId
        if (const FString* FoundId = GSpkNameToId.Find(ParsedSpkr))
        {
            SpkrId = *FoundId;
        }
        else
        {
            // 如果映射未命中，使用原字符串最后一个分段或完整字符串
            SpkrId = ParsedSpkr;
        }

        UE_LOG(LogTemp, Log, TEXT("解析得到的spkr: '%s' -> spkId: '%s'"), *ParsedSpkr, *SpkrId);
        Payload.Append(TEXT("&role=") + FGenericPlatformHttp::UrlEncode(SpkrId));
    }
    
    Payload.Append(TEXT("&speed=" ) + FString::SanitizeFloat(Params.LocalSettings->AudioSpeed));
    Payload.Append(TEXT("&pitch=" ) + FString::SanitizeFloat(Params.LocalSettings->AudioPitch));
    
    // 获取情感数据（可能是字符串或字节流）
    FString EmotionName;
    TArray<uint8> EmotionAudioBytes;
    bool bHasEmotionAudio = Params.LocalSettings->GetCurrentEmotionData(EmotionName, EmotionAudioBytes);
    
    // 根据是否有情感音频来决定传递哪个参数
    if (bHasEmotionAudio && EmotionAudioBytes.Num() > 0)
    {
        // 使用情感音频：传递emotion_audio参数，不传递emotion参数
        FString Base64Audio = FBase64::Encode(EmotionAudioBytes);
        Payload.Append(TEXT("&emo_audio=") + FGenericPlatformHttp::UrlEncode(Base64Audio));
    }
    else
    {
        // 使用情感类型：传递emotion参数，不传递emotion_audio参数
        FString EmotionString = Params.LocalSettings->EmotionName.IsEmpty() ? TEXT("自适应") : Params.LocalSettings->EmotionName;
        Payload.Append(TEXT("&emotion=") + FGenericPlatformHttp::UrlEncode(EmotionString));
    }

    // 根据是否使用zero-shot选择不同的API端点
    FString APIEndpoint = bUseZeroShot ? Params.Settings->API_ZeroShot : Params.Settings->API_Synthesis;
    
    // 打印TTS请求基本信息
    UE_LOG(LogTemp, Log, TEXT("=== %s请求信息 ==="), bUseZeroShot ? TEXT("Zero-shot") : TEXT("TTS"));
    FString ServerIP = bUseZeroShot ? Params.Settings->IP_ZeroShot_Server : Params.Settings->IP_TTS_Server;
    UE_LOG(LogTemp, Log, TEXT("URL: %s%s"), *ServerIP, *APIEndpoint);
    UE_LOG(LogTemp, Log, TEXT("文本: %s"), *Params.LocalSettings->TTSInputText);
    UE_LOG(LogTemp, Log, TEXT("音色: %s"), *SpeakerName);
    UE_LOG(LogTemp, Log, TEXT("语速: %.2f"), Params.LocalSettings->AudioSpeed);
    UE_LOG(LogTemp, Log, TEXT("语调: %.2f"), Params.LocalSettings->AudioPitch);
    
    // 打印音频信息（不显示编码内容）
    if (bHasSpeakerAudio && SpeakerAudioBytes.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("音色音频: 已包含，大小: %d 字节"), SpeakerAudioBytes.Num());
    }
    
    if (bHasEmotionAudio && EmotionAudioBytes.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("情感音频: 已包含，大小: %d 字节"), EmotionAudioBytes.Num());
    }
    else
    {
        FString EmotionString = Params.LocalSettings->EmotionName.IsEmpty() ? TEXT("自适应") : Params.LocalSettings->EmotionName;
        UE_LOG(LogTemp, Log, TEXT("情感类型: %s"), *EmotionString);
    }
    UE_LOG(LogTemp, Log, TEXT("=== 请求信息完成 ==="));

    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();
    
    // 使用之前定义的ServerIP变量
    FString FullURL = FString::Printf(TEXT("%s%s"), *ServerIP, *APIEndpoint);
    
    UE_LOG(LogTemp, Log, TEXT("使用API端点: %s"), *APIEndpoint);
    HttpRequest->SetURL(FullURL);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded; charset=utf-8"));
    HttpRequest->SetContentAsString(Payload);
    HttpRequest->SetTimeout(Params.Settings->Timeout);
    HttpRequest->OnProcessRequestComplete().BindLambda(
        [Params](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            // 增加完成计数
            (*Params.CompletedCount)++;
            
            // 广播进度更新
            GTTSProgressUpdateDelegate.Broadcast(*Params.CompletedCount, *Params.TotalCount);
            
            if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
            {
                const TArray<uint8>& AudioData = Response->GetContent();
                if (AudioData.Num() > 0)
                {
                    // 直接保存音频数据到内存，不保存wav文件
                    if (Params.SegmentResults.IsValid())
                    {
                        FTTSSegmentResult Result;
                        Result.SegmentIndex = Params.SegmentIndex;
                        Result.ReplicateIndex = Params.ReplicateIndex;
                        Result.Text = Params.LocalSettings ? Params.LocalSettings->TTSInputText : TEXT("");
                        Result.AssetPath = TEXT(""); // 暂时为空，等用户保存时再设置
                        Result.AudioData = AudioData; // 保存音频数据用于预览播放
                        Result.bIsSavedAsAsset = false; // 标记为未保存
                        Params.SegmentResults->Add(MoveTemp(Result));
                    }
                }
                else
                {
                    FNotificationInfo Info(FText::FromString(TEXT("TTS请求成功，但返回的音频数据为空.")));
                    FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
                }
            }
            else
            {
                FString ErrorMsg = TEXT("TTS请求失败. ");
                if (Response.IsValid())
                {
                    ErrorMsg += FString::Printf(TEXT("错误码: %d, 内容: %s"), Response->GetResponseCode(), *Response->GetContentAsString());
                }
                FNotificationInfo Info(FText::FromString(ErrorMsg));
                FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
            }
            
            
            // 检查批次完成情况
            if (Params.CurrentBatchIndex && Params.BatchSize && 
                Params.AllTextSegments && Params.AllTempSettings)
            {
                // 分批处理模式
                const int32 TotalSegments = Params.AllTextSegments->Num();
                const int32 ReplicatesPerSegment = Params.Settings->TTSReplicatesPerSegment;
                const int32 CurrentBatchSize = *Params.BatchSize;
                const int32 CurrentBatchIndex = *Params.CurrentBatchIndex;
                
                // 计算当前批次应该完成的请求数量
                const int32 StartSegmentIndex = CurrentBatchIndex * CurrentBatchSize;
                const int32 EndSegmentIndex = FMath::Min(StartSegmentIndex + CurrentBatchSize, TotalSegments);
                const int32 CurrentBatchSegmentCount = EndSegmentIndex - StartSegmentIndex;
                const int32 CurrentBatchRequestCount = CurrentBatchSegmentCount * ReplicatesPerSegment;
                
                // 计算当前批次完成的目标计数
                const int32 CurrentBatchTargetCount = (CurrentBatchIndex + 1) * CurrentBatchSize * ReplicatesPerSegment;
                const int32 ActualTargetCount = FMath::Min(CurrentBatchTargetCount, TotalSegments * ReplicatesPerSegment);
                
                // 检查当前批次是否完成
                if (*Params.CompletedCount >= ActualTargetCount)
                {
                    
                    // 检查是否还有下一批
                    const int32 NextBatchIndex = CurrentBatchIndex + 1;
                    const int32 NextBatchStartSegment = NextBatchIndex * CurrentBatchSize;
                    
                    if (NextBatchStartSegment < TotalSegments)
                    {
                        // 发送下一批请求
                        (*Params.CurrentBatchIndex)++;
                        SendBatchRequests(Params.CurrentBatchIndex, Params.BatchSize, Params.AllTextSegments, 
                                        Params.AllTempSettings, Params.OriginalTTSSettings, Params.Settings, 
                                        Params.AssetPath, Params.WavDir, Params.CompletedCount, Params.TotalCount, 
                                        Params.CompletedAssetPaths, Params.SegmentResults, ReplicatesPerSegment);
                        
                    }
                    else
                    {
                        // 所有批次都完成了
                        
                        FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("所有音频合成完成！共处理 %d 个文本段，请在预览窗口中选择要保存的音频"), *Params.TotalCount)));
                        Info.ExpireDuration = 10.0f;
                        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Success);
                        
#if WITH_EDITOR
                        if (Params.SegmentResults.IsValid())
                        {
                            Params.SegmentResults->Sort([](const FTTSSegmentResult& A, const FTTSSegmentResult& B)
                            {
                                return A.SegmentIndex < B.SegmentIndex;
                            });
                            // 传递原始的TTS设置对象
                            GTTSAllCompletedDelegate.Broadcast(*Params.SegmentResults, Params.OriginalTTSSettings);
                        }
#endif
                    }
                }
            }
            else
            {
                // 非分批处理模式（原有逻辑）
                if (*Params.CompletedCount >= *Params.TotalCount)
                {
                    FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("所有音频合成完成！共处理 %d 个文本段，请在预览窗口中选择要保存的音频"), *Params.TotalCount)));
                    Info.ExpireDuration = 10.0f;
                    FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Success);
                    
#if WITH_EDITOR
                    if (Params.SegmentResults.IsValid())
                    {
                        Params.SegmentResults->Sort([](const FTTSSegmentResult& A, const FTTSSegmentResult& B)
                        {
                            return A.SegmentIndex < B.SegmentIndex;
                        });
                        // 传递原始的TTS设置对象（从第一个请求参数中获取）
                        GTTSAllCompletedDelegate.Broadcast(*Params.SegmentResults, Params.LocalSettings);
                    }
#endif
                }
            }
        });
    HttpRequest->ProcessRequest();
}


void UTTSServer::FetchSpeakerList(TFunction<void(const TArray<TSharedPtr<FString>>&)> OnComplete)
{
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    if (GbSpeakerListLoaded)
    {
        if (OnComplete)
            OnComplete(GSpeakerDisplayOptions);
        return;
    }

    // 如果正在加载，加入等待队列
    GPendingSpeakerListCallbacks.Add(OnComplete);
    if (GPendingSpeakerListCallbacks.Num() > 1)
        return; // 已有请求在进行中

    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();
    HttpRequest->SetURL(FString::Printf(TEXT("%s%s"), *Settings->IP_TTS_Server, *Settings->API_GetSpeakerList));
    HttpRequest->SetVerb(TEXT("GET"));

    HttpRequest->OnProcessRequestComplete().BindLambda(
        [](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            TArray<TSharedPtr<FString>> DisplayOptions;
            // 每次加载前清空旧的映射
            GSpkNameToId.Empty();

            if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
            {
                TSharedPtr<FJsonObject> JsonObject;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
                if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
                {
                    for (const auto& Pair : JsonObject->Values)
                    {
                        const FString& SpkId = Pair.Key;
                        const TSharedPtr<FJsonObject>* InfoObjectPtr;
                        if (Pair.Value->TryGetObject(InfoObjectPtr))
                        {
                            const TSharedPtr<FJsonObject>& InfoObject = *InfoObjectPtr;
                            FString Spkr = InfoObject->GetStringField(TEXT("spkr"));
                            FString Gender = InfoObject->GetStringField(TEXT("gender"));
                            FString Age = InfoObject->GetStringField(TEXT("age"));

                            // talking_style 取列表首个元素
                            FString TalkingStyle;
                            const TArray<TSharedPtr<FJsonValue>>* StyleArray = nullptr;
                            if (InfoObject->TryGetArrayField(TEXT("talking_style"), StyleArray))
                            {
                                if (StyleArray && StyleArray->Num() > 0 && (*StyleArray)[0].IsValid())
                                {
                                    TalkingStyle = (*StyleArray)[0]->AsString();
                                }
                            }

                            FString DisplayString = FString::Printf(TEXT("%s|%s|%s|%s"), *Gender, *Age, *TalkingStyle, *Spkr);
                            DisplayOptions.Add(MakeShared<FString>(DisplayString));

                            // 记录 spkName -> spkId
                            if (!Spkr.IsEmpty())
                            {
                                GSpkNameToId.Add(Spkr, SpkId);
                            }
                        }
                    }
                }
            }

            // 缓存
            GSpeakerDisplayOptions = DisplayOptions;
            GbSpeakerListLoaded = true;

            // 通知所有等待的回调
            for (auto& Callback : GPendingSpeakerListCallbacks)
            {
                if (Callback)
                    Callback(GSpeakerDisplayOptions);
            }
            GPendingSpeakerListCallbacks.Empty();
        });

    HttpRequest->ProcessRequest();
}

TArray<FString> UTTSServer::GetCachedSpeakerOptions()
{
    TArray<FString> Result;
    for (const TSharedPtr<FString>& Ptr : GSpeakerDisplayOptions)
    {
        if (Ptr.IsValid())
        {
            Result.Add(*Ptr);
        }
    }
    Result.Sort();
    return Result;
}

void UTTSServer::EnhanceAudio(const FString& AudioAssetPath, const FString& EnhancedAssetPath)
{
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    
    // 加载音频资产
    USoundWave* SoundWave = Cast<USoundWave>(StaticLoadObject(USoundWave::StaticClass(), nullptr, *AudioAssetPath));
    if (!SoundWave)
    {
        UE_LOG(LogTemp, Error, TEXT("无法加载音频资产: %s"), *AudioAssetPath);
        GEnhanceAudioCompletedDelegate.Broadcast(false, AudioAssetPath, EnhancedAssetPath);
        return;
    }
    
    // 获取音频数据
    TArray<uint8> AudioData;
    TArray<uint8> PCMData;
    uint16 NumChannels = 0;
    uint32 SampleRate = 0;
    
    // 尝试获取PCM数据
    if (SoundWave->GetImportedSoundWaveData(PCMData, SampleRate, NumChannels))
    {
        // 将PCM数据转换为WAV格式
        if (UTTSSetting::ConvertPCMToWAV(PCMData.GetData(), PCMData.Num(), SampleRate, NumChannels, AudioData))
        {
            UE_LOG(LogTemp, Log, TEXT("成功获取音频PCM数据，大小: %d 字节"), AudioData.Num());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("PCM转WAV失败"));
            GEnhanceAudioCompletedDelegate.Broadcast(false, AudioAssetPath, EnhancedAssetPath);
            return;
        }
    }
    else
    {
        // 尝试获取压缩数据
        FByteBulkData* CompressedBulkData = SoundWave->GetCompressedData(SoundWave->GetRuntimeFormat());
        if (CompressedBulkData && CompressedBulkData->GetBulkDataSize() > 0)
        {
            int32 CompressedDataSize = CompressedBulkData->GetBulkDataSize();
            AudioData.SetNum(CompressedDataSize);
            
            void* CompressedData = nullptr;
            CompressedBulkData->GetCopy(&CompressedData, true);
            if (CompressedData)
            {
                FMemory::Memcpy(AudioData.GetData(), CompressedData, CompressedDataSize);
                UE_LOG(LogTemp, Log, TEXT("成功获取音频压缩数据，大小: %d 字节"), AudioData.Num());
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("获取压缩数据失败"));
                GEnhanceAudioCompletedDelegate.Broadcast(false, AudioAssetPath, EnhancedAssetPath);
                return;
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("无法获取音频数据"));
            GEnhanceAudioCompletedDelegate.Broadcast(false, AudioAssetPath, EnhancedAssetPath);
            return;
        }
    }
    
    // Base64编码音频数据
    FString Base64Audio = FBase64::Encode(AudioData);
    
    // 构建请求参数
    FString Payload = TEXT("audio=") + FGenericPlatformHttp::UrlEncode(Base64Audio);
    
    // 构建URL
    FString FullURL = FString::Printf(TEXT("%s%s"), *Settings->IP_TTS_Server, *Settings->API_EnhanceAudio);
    
    UE_LOG(LogTemp, Log, TEXT("=== 音频增强请求信息 ==="));
    UE_LOG(LogTemp, Log, TEXT("URL: %s"), *FullURL);
    UE_LOG(LogTemp, Log, TEXT("音频大小: %d 字节"), AudioData.Num());
    UE_LOG(LogTemp, Log, TEXT("=== 请求信息完成 ==="));
    
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();
    
    HttpRequest->SetURL(FullURL);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded; charset=utf-8"));
    HttpRequest->SetContentAsString(Payload);
    HttpRequest->SetTimeout(Settings->Timeout);
    
    HttpRequest->OnProcessRequestComplete().BindLambda([AudioAssetPath, EnhancedAssetPath](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
        {
            const TArray<uint8>& EnhancedAudioData = Response->GetContent();
            if (EnhancedAudioData.Num() > 0)
            {
                UE_LOG(LogTemp, Log, TEXT("收到增强音频数据，大小: %d 字节"), EnhancedAudioData.Num());
                
                // 验证音频数据格式
                if (EnhancedAudioData.Num() < 44) // WAV文件头至少44字节
                {
                    UE_LOG(LogTemp, Error, TEXT("增强音频数据格式无效，数据太小"));
                    GEnhanceAudioCompletedDelegate.Broadcast(false, AudioAssetPath, EnhancedAssetPath);
                    return;
                }
                
                // 检查WAV文件头
                if (EnhancedAudioData[0] != 'R' || EnhancedAudioData[1] != 'I' || 
                    EnhancedAudioData[2] != 'F' || EnhancedAudioData[3] != 'F' ||
                    EnhancedAudioData[8] != 'W' || EnhancedAudioData[9] != 'A' || 
                    EnhancedAudioData[10] != 'V' || EnhancedAudioData[11] != 'E')
                {
                    UE_LOG(LogTemp, Error, TEXT("增强音频数据不是有效的WAV格式"));
                    GEnhanceAudioCompletedDelegate.Broadcast(false, AudioAssetPath, EnhancedAssetPath);
                    return;
                }
                
                // 保存增强后的音频
                // 正确分离路径和文件名
                FString AssetDir = FPaths::GetPath(EnhancedAssetPath);
                FString AssetName = FPaths::GetBaseFilename(EnhancedAssetPath);
                
                // 确保路径以斜杠结尾
                if (!AssetDir.EndsWith(TEXT("/")))
                {
                    AssetDir += TEXT("/");
                }
                
                UE_LOG(LogTemp, Log, TEXT("保存增强音频 - 目录: %s, 文件名: %s"), *AssetDir, *AssetName);
                
                if (UGenTools::SaveAudioAsSoundWaveAsset(EnhancedAudioData, AssetDir, AssetName, true))
                {
                    UE_LOG(LogTemp, Log, TEXT("音频增强成功，已保存到: %s"), *EnhancedAssetPath);
                    GEnhanceAudioCompletedDelegate.Broadcast(true, AudioAssetPath, EnhancedAssetPath);
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("保存增强音频失败"));
                    GEnhanceAudioCompletedDelegate.Broadcast(false, AudioAssetPath, EnhancedAssetPath);
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("音频增强请求成功，但返回的音频数据为空"));
                GEnhanceAudioCompletedDelegate.Broadcast(false, AudioAssetPath, EnhancedAssetPath);
            }
        }
        else
        {
            FString ErrorMsg = TEXT("音频增强请求失败. ");
            if (Response.IsValid())
            {
                ErrorMsg += FString::Printf(TEXT("错误码: %d, 内容: %s"), Response->GetResponseCode(), *Response->GetContentAsString());
            }
            UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMsg);
            GEnhanceAudioCompletedDelegate.Broadcast(false, AudioAssetPath, EnhancedAssetPath);
        }
    });
    
    HttpRequest->ProcessRequest();
    
    FNotificationInfo Info(FText::FromString(TEXT("正在请求音频增强服务...")));
    FSlateNotificationManager::Get().AddNotification(Info);
}

void UTTSServer::EnhanceAudioFromMemory(const TArray<uint8>& AudioData, const FString& EnhancedAssetPath, 
    TFunction<void(bool bSuccess, const TArray<uint8>& EnhancedAudioData)> OnComplete)
{
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    
    if (AudioData.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("音频数据为空，无法进行增强"));
        if (OnComplete)
        {
            OnComplete(false, TArray<uint8>());
        }
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("=== 从内存直接进行音频增强 ==="));
    UE_LOG(LogTemp, Log, TEXT("音频大小: %d 字节"), AudioData.Num());
    UE_LOG(LogTemp, Log, TEXT("目标路径: %s"), *EnhancedAssetPath);
    
    // Base64编码音频数据
    FString Base64Audio = FBase64::Encode(AudioData);
    
    // 构建请求参数（与EnhanceAudio函数保持一致）
    FString Payload = TEXT("audio=") + FGenericPlatformHttp::UrlEncode(Base64Audio);
    
    // 构建URL
    FString FullURL = FString::Printf(TEXT("%s%s"), *Settings->IP_TTS_Server, *Settings->API_EnhanceAudio);
    
    UE_LOG(LogTemp, Log, TEXT("=== 音频增强请求信息 ==="));
    UE_LOG(LogTemp, Log, TEXT("URL: %s"), *FullURL);
    UE_LOG(LogTemp, Log, TEXT("音频大小: %d 字节"), AudioData.Num());
    UE_LOG(LogTemp, Log, TEXT("Base64编码后大小: %d 字符"), Base64Audio.Len());
    UE_LOG(LogTemp, Log, TEXT("请求体大小: %d 字符"), Payload.Len());
    UE_LOG(LogTemp, Log, TEXT("=== 请求信息完成 ==="));
    
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();
    
    HttpRequest->SetURL(FullURL);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded; charset=utf-8"));
    HttpRequest->SetContentAsString(Payload);
    HttpRequest->SetTimeout(Settings->Timeout);
    
    HttpRequest->OnProcessRequestComplete().BindLambda([OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
        {
            const TArray<uint8>& EnhancedAudioData = Response->GetContent();
            if (EnhancedAudioData.Num() > 0)
            {
                UE_LOG(LogTemp, Log, TEXT("收到增强音频数据，大小: %d 字节"), EnhancedAudioData.Num());
                
                // 验证音频数据格式
                if (EnhancedAudioData.Num() < 44) // WAV文件头至少44字节
                {
                    UE_LOG(LogTemp, Error, TEXT("增强音频数据格式无效，数据太小"));
                    if (OnComplete)
                    {
                        OnComplete(false, TArray<uint8>());
                    }
                    return;
                }
                
                // 检查WAV文件头
                if (EnhancedAudioData[0] != 'R' || EnhancedAudioData[1] != 'I' || 
                    EnhancedAudioData[2] != 'F' || EnhancedAudioData[3] != 'F' ||
                    EnhancedAudioData[8] != 'W' || EnhancedAudioData[9] != 'A' || 
                    EnhancedAudioData[10] != 'V' || EnhancedAudioData[11] != 'E')
                {
                    UE_LOG(LogTemp, Error, TEXT("增强音频数据不是有效的WAV格式"));
                    if (OnComplete)
                    {
                        OnComplete(false, TArray<uint8>());
                    }
                    return;
                }
                
                UE_LOG(LogTemp, Log, TEXT("音频增强成功，返回增强后的音频数据"));
                if (OnComplete)
                {
                    OnComplete(true, EnhancedAudioData);
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("音频增强请求成功，但返回的音频数据为空"));
                if (OnComplete)
                {
                    OnComplete(false, TArray<uint8>());
                }
            }
        }
        else
        {
            FString ErrorMsg = TEXT("音频增强请求失败. ");
            if (Response.IsValid())
            {
                ErrorMsg += FString::Printf(TEXT("HTTP状态码: %d, 响应: %s"), 
                    Response->GetResponseCode(), *Response->GetContentAsString());
            }
            else
            {
                ErrorMsg += TEXT("无响应");
            }
            UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMsg);
            
            if (OnComplete)
            {
                OnComplete(false, TArray<uint8>());
            }
        }
    });
    
    HttpRequest->ProcessRequest();
    
    FNotificationInfo Info(FText::FromString(TEXT("正在请求音频增强服务...")));
    FSlateNotificationManager::Get().AddNotification(Info);
}
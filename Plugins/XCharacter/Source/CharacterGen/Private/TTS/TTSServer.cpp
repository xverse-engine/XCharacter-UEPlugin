// Copyright Epic Games, Inc. All Rights Reserved.

#include "../../Public/TTS/TTSServer.h"
#include "XVCPluginSettings.h"
#include "TTS/TTSSetting.h"
#include "Misc/Base64.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
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
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/Guid.h"

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

// 豆包TTS speaker配置结构（对应 Python 中的 x_spkr_config）
struct FDoubaoTTS_SpeakerConfig
{
    FString SpkId;
    FString Spkr; // 显示名称
    FString VoiceType; // 豆包voice_type
    FString Gender;
    FString Age;
    FString TalkingStyle;
    FString Description;
};

// 豆包TTS speaker配置列表（对应 Python 中的 x_spkr_config）
static TArray<FDoubaoTTS_SpeakerConfig> GDoubaoTTS_SpeakerConfigs = {
    {TEXT("d001"), TEXT("爽快思思"), TEXT("zh_female_shuangkuaisisi_moon_bigtts"), TEXT("女"), TEXT("青年"), TEXT(""), TEXT("爽快思思 通用场景 中文")},
    {TEXT("d002"), TEXT("温暖阿虎"), TEXT("zh_male_wennuanahu_moon_bigtts"), TEXT("男"), TEXT("青年"), TEXT(""), TEXT("温暖阿虎 通用场景 中文")},
    {TEXT("d003"), TEXT("湾湾小何"), TEXT("zh_female_wanwanxiaohe_moon_bigtts"), TEXT("女"), TEXT("青年"), TEXT(""), TEXT("湾湾小何 趣味方言 中文")},
    {TEXT("d004"), TEXT("京腔侃爷"), TEXT("zh_male_jingqiangkanye_moon_bigtts"), TEXT("男"), TEXT("青年"), TEXT(""), TEXT("京腔侃爷 趣味方言 中文")},
};

// 豆包TTS speaker ID 到 voice_type 的映射（用于快速查找）
static TMap<FString, FString> GDoubaoTTS_VoiceTypeMap;
static bool GbDoubaoTTSVoiceTypeMapInitialized = false;

// 初始化豆包TTS voice_type映射（延迟初始化，确保在首次使用时初始化）
static void EnsureDoubaoTTSVoiceTypeMapInitialized()
{
    if (!GbDoubaoTTSVoiceTypeMapInitialized)
    {
        GDoubaoTTS_VoiceTypeMap.Empty();
        for (const FDoubaoTTS_SpeakerConfig& Config : GDoubaoTTS_SpeakerConfigs)
        {
            if (!Config.SpkId.IsEmpty() && !Config.VoiceType.IsEmpty())
            {
                GDoubaoTTS_VoiceTypeMap.Add(Config.SpkId, Config.VoiceType);
            }
        }
        GbDoubaoTTSVoiceTypeMapInitialized = true;
    }
}

// TTS批次管理静态变量
static int32 GCurrentBatchIndex = 0;
static int32 GBatchSizeValue = 2;
static TArray<FString> GAllTextSegmentsArray;
static TArray<UTTSSetting*> GAllTempSettingsArray;
static UTTSSetting* GOriginalTTSSettingsPtr = nullptr;
// 静音信息映射（段落索引 -> 静音时间（秒））
static TMap<int32, float> GSegmentPreSilenceMap;
static TMap<int32, float> GSegmentPostSilenceMap;
// 预览窗口与通知触发控制
static bool GbTTSFinalNotificationShown = false;

static void DispatchTTSAllCompleted(const FTTSRequestParams& Params)
{
    if (GbTTSFinalNotificationShown)
    {
        return;
    }

    if (!Params.SegmentResults.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("DispatchTTSAllCompleted: SegmentResults 无效，无法展示预览。"));
        return;
    }

    int32 ExpectedTotalCount = 0;
    if (Params.TotalCount.IsValid())
    {
        ExpectedTotalCount = *Params.TotalCount;
    }
    else if (Params.Settings)
    {
        const int32 SegmentCount = Params.AllTextSegments ? Params.AllTextSegments->Num() : Params.SegmentResults->Num();
        ExpectedTotalCount = SegmentCount * Params.Settings->TTSReplicatesPerSegment;
    }
    else
    {
        ExpectedTotalCount = Params.SegmentResults->Num();
    }

    if (ExpectedTotalCount <= 0)
    {
        ExpectedTotalCount = Params.SegmentResults->Num();
    }

    const int32 CurrentResultCount = Params.SegmentResults->Num();
    if (CurrentResultCount < ExpectedTotalCount)
    {
        UE_LOG(LogTemp, Log, TEXT("DispatchTTSAllCompleted: 当前结果 %d / 预期 %d，继续等待剩余音频。"), CurrentResultCount, ExpectedTotalCount);
        return;
    }

    GbTTSFinalNotificationShown = true;

    const int32 TotalCountValue = (Params.TotalCount.IsValid()) ? *Params.TotalCount : CurrentResultCount;
    FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("所有音频合成完成！共处理 %d 个文本段，请在预览窗口中选择要保存的音频"), TotalCountValue)));
    Info.ExpireDuration = 10.0f;
    FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Success);

#if WITH_EDITOR
    if (Params.SegmentResults.IsValid())
    {
        Params.SegmentResults->Sort([](const FTTSSegmentResult& A, const FTTSSegmentResult& B)
        {
            return A.SegmentIndex < B.SegmentIndex;
        });

        UTTSSetting* SettingsForPreview = Params.OriginalTTSSettings ? Params.OriginalTTSSettings : Params.LocalSettings;
        GTTSAllCompletedDelegate.Broadcast(*Params.SegmentResults, SettingsForPreview);
    }
#endif
}

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
    GSegmentPreSilenceMap.Empty();
    GSegmentPostSilenceMap.Empty();
    GbTTSFinalNotificationShown = false;
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

bool UTTSServer::ValidateDoubaoTTSConfig(const UXVCPluginSettings* Settings, const FString& Context)
{
    if (!Settings)
    {
        FString ErrorMsg = Context.IsEmpty() ? TEXT("无法获取插件设置！") : FString::Printf(TEXT("%s: 无法获取插件设置！"), *Context);
        UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMsg);
        FNotificationInfo Info(FText::FromString(ErrorMsg));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return false;
    }
    
    // 验证豆包TTS API配置参数
    if (Settings->IP_DoubaoTTS_Server.IsEmpty())
    {
        FString ErrorMsg = Context.IsEmpty() ? TEXT("请配置豆包TTS服务器URL！") : FString::Printf(TEXT("%s: 请配置豆包TTS服务器URL！"), *Context);
        UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMsg);
        FNotificationInfo Info(FText::FromString(ErrorMsg));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return false;
    }
    
    if (Settings->DoubaoTTS_AppID.IsEmpty())
    {
        FString ErrorMsg = Context.IsEmpty() ? TEXT("请配置豆包AppID！") : FString::Printf(TEXT("%s: 请配置豆包AppID！"), *Context);
        UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMsg);
        FNotificationInfo Info(FText::FromString(ErrorMsg));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return false;
    }
    
    if (Settings->DoubaoTTS_AccessToken.IsEmpty())
    {
        FString ErrorMsg = Context.IsEmpty() ? TEXT("请配置豆包Access Token！") : FString::Printf(TEXT("%s: 请配置豆包Access Token！"), *Context);
        UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMsg);
        FNotificationInfo Info(FText::FromString(ErrorMsg));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return false;
    }
    
    if (Settings->DoubaoTTS_Cluster.IsEmpty())
    {
        FString ErrorMsg = Context.IsEmpty() ? TEXT("请配置豆包Cluster！") : FString::Printf(TEXT("%s: 请配置豆包Cluster！"), *Context);
        UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMsg);
        FNotificationInfo Info(FText::FromString(ErrorMsg));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return false;
    }
    
    if (Settings->DoubaoTTS_UserID.IsEmpty())
    {
        FString ErrorMsg = Context.IsEmpty() ? TEXT("请配置豆包User ID！") : FString::Printf(TEXT("%s: 请配置豆包User ID！"), *Context);
        UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMsg);
        FNotificationInfo Info(FText::FromString(ErrorMsg));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return false;
    }
    
    return true;
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
    
    // 验证豆包TTS API配置参数（在开始处理前检查）
    if (!ValidateDoubaoTTSConfig(Settings, TEXT("CallTTSServerBatched")))
    {
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
    
    // 创建所有临时设置对象和静音信息映射
    TArray<UTTSSetting*> AllTempSettings;
    TMap<int32, float> SegmentPreSilenceMap;  // 段落索引 -> 段前静音（秒）
    TMap<int32, float> SegmentPostSilenceMap; // 段落索引 -> 段后静音（秒）
    
    // 记录原始设置的语速和语调值
    UE_LOG(LogTemp, Log, TEXT("CallTTSServerBatched: 原始TTSSettings 语速=%.2f, 语调=%.2f"), 
        TTSSettings->AudioSpeed, TTSSettings->AudioPitch);
    
    for (int32 i = 0; i < TotalSegments; ++i)
    {
        UTTSSetting* TempSettings = NewObject<UTTSSetting>(GetTransientPackage());
        TempSettings->SpeakerName = TTSSettings->SpeakerName;
        TempSettings->AudioSpeed = TTSSettings->AudioSpeed;
        TempSettings->AudioPitch = TTSSettings->AudioPitch;
        
        UE_LOG(LogTemp, Log, TEXT("CallTTSServerBatched: 段落%d 创建TempSettings 语速=%.2f, 语调=%.2f"), 
            i, TempSettings->AudioSpeed, TempSettings->AudioPitch);
        
        // 设置当前段落的文本内容，支持三列数据格式
        FString TextContent = ValidTextSegments[i];
        float PreSilence = 0.0f;
        float PostSilence = 0.0f;
        
        // 检查是否包含三列数据（用|分隔）
        TArray<FString> ColumnData;
        TextContent.ParseIntoArray(ColumnData, TEXT("|"), false);
        
        if (ColumnData.Num() >= 3)
        {
            // 三列数据：文本|段前静音|段后静音
            FString MainText = ColumnData[0].TrimStartAndEnd();
            FString PreSilenceStr = ColumnData[1].TrimStartAndEnd();
            FString PostSilenceStr = ColumnData[2].TrimStartAndEnd();
            
            // 解析静音数值（单位：秒）
            if (!PreSilenceStr.IsEmpty())
            {
                FDefaultValueHelper::ParseFloat(PreSilenceStr, PreSilence);
            }
            if (!PostSilenceStr.IsEmpty())
            {
                FDefaultValueHelper::ParseFloat(PostSilenceStr, PostSilence);
            }
            
            UE_LOG(LogTemp, Log, TEXT("段落%d: 文本='%s', 段前静音=%.2f秒, 段后静音=%.2f秒"), 
                i, *MainText, PreSilence, PostSilence);
            
            // 只使用文本部分进行TTS合成
            TempSettings->TTSInputText = MainText;
            
            // 保存静音信息到映射
            SegmentPreSilenceMap.Add(i, PreSilence);
            SegmentPostSilenceMap.Add(i, PostSilence);
            
            UE_LOG(LogTemp, Log, TEXT("TTS合成使用纯文本: '%s' (已解析静音信息: 前%.2f秒, 后%.2f秒)"), 
                *MainText, PreSilence, PostSilence);
        }
        else
        {
            // 单列数据，直接使用，静音为0
            TempSettings->TTSInputText = TextContent;
            SegmentPreSilenceMap.Add(i, 0.0f);
            SegmentPostSilenceMap.Add(i, 0.0f);
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
    // 保存静音信息映射
    GSegmentPreSilenceMap = SegmentPreSilenceMap;
    GSegmentPostSilenceMap = SegmentPostSilenceMap;
    
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
    
    // 发送当前批次的请求（添加延迟以避免并发限制）
    int32 RequestIndex = 0;
    for (int32 SegmentIndex = StartSegmentIndex; SegmentIndex < EndSegmentIndex; ++SegmentIndex)
    {
        UTTSSetting* TempSettings = (*AllTempSettings)[SegmentIndex];
        
        // 获取该段落的静音信息
        float PreSilence = GSegmentPreSilenceMap.FindRef(SegmentIndex);
        float PostSilence = GSegmentPostSilenceMap.FindRef(SegmentIndex);
        
        // 针对每段文本进行多次合成
        for (int32 ReplicateIndex = 0; ReplicateIndex < ReplicatesPerSegment; ++ReplicateIndex)
        {
            FTTSRequestParams RequestParams(TempSettings, Settings, SegmentIndex, AssetPath, WavDir, 
                                          CompletedCount, TotalCount, CompletedAssetPaths, SegmentResults, ReplicateIndex,
                                          CurrentBatchIndex, BatchSize, AllTextSegments, AllTempSettings, OriginalTTSSettings,
                                          PreSilence, PostSilence);
            
            // 添加延迟以避免并发限制（每2个请求之间延迟0.1秒）
            if (RequestIndex > 0 && RequestIndex % 2 == 0)
            {
                FPlatformProcess::Sleep(0.1f);
            }
            
            SendTTSRequest(RequestParams);
            RequestIndex++;
        }
    }
}

void UTTSServer::SendTTSRequest(const FTTSRequestParams& Params)
{
    // 验证基本参数
    if (!Params.LocalSettings)
    {
        UE_LOG(LogTemp, Error, TEXT("SendTTSRequest: Params.LocalSettings为空！"));
        FNotificationInfo Info(FText::FromString(TEXT("TTS设置参数无效！")));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }
    
    // 验证豆包TTS API配置参数
    if (!ValidateDoubaoTTSConfig(Params.Settings, TEXT("SendTTSRequest")))
    {
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("SendTTSRequest: 从LocalSettings获取语速=%.2f, 语调=%.2f"), 
        Params.LocalSettings->AudioSpeed, Params.LocalSettings->AudioPitch);
    
    // 获取音色名称
    FString SpeakerName;
    TArray<uint8> SpeakerAudioBytes; // 豆包接口不需要音频数据，但保留以兼容 GetCurrentSpeakerData 接口
    Params.LocalSettings->GetCurrentSpeakerData(SpeakerName, SpeakerAudioBytes);
        
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

    // 通过映射获取真正的 spkId（用于查找豆包音色映射）
    FString SpkrId = ParsedSpkr;
        if (const FString* FoundId = GSpkNameToId.Find(ParsedSpkr))
        {
            SpkrId = *FoundId;
        }
    
    // 检查音色是否为空
    if (SpeakerName.IsEmpty() || SpkrId.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("SpeakerName为空，请先选择音色！"));
        FNotificationInfo Info(FText::FromString(TEXT("请先选择音色！")));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }
    
    // 确保voice_type映射已初始化
    EnsureDoubaoTTSVoiceTypeMapInitialized();
    
    // 获取豆包 voice_type
    FString VoiceType;
    if (const FString* FoundVoiceType = GDoubaoTTS_VoiceTypeMap.Find(SpkrId))
    {
        VoiceType = *FoundVoiceType;
    }
    else
    {
        // 如果没有找到映射，直接返回错误
        UE_LOG(LogTemp, Error, TEXT("未找到豆包音色映射: %s (音色ID: %s)"), *SpkrId, *SpeakerName);
        FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("未找到豆包音色映射: %s，请检查音色配置！"), *SpkrId)));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }
    
    FString TTSText = Params.LocalSettings->TTSInputText;
    UE_LOG(LogTemp, Log, TEXT("发送豆包TTS请求 - 段落%d: '%s'"), Params.SegmentIndex, *TTSText);
    
    // 构建JSON请求体
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
    
    // app 对象
    TSharedPtr<FJsonObject> AppObject = MakeShareable(new FJsonObject);
    AppObject->SetStringField(TEXT("appid"), Params.Settings->DoubaoTTS_AppID);
    AppObject->SetStringField(TEXT("token"), TEXT("access_token"));
    AppObject->SetStringField(TEXT("cluster"), Params.Settings->DoubaoTTS_Cluster);
    RootObject->SetObjectField(TEXT("app"), AppObject);
    
    // user 对象
    TSharedPtr<FJsonObject> UserObject = MakeShareable(new FJsonObject);
    UserObject->SetStringField(TEXT("uid"), Params.Settings->DoubaoTTS_UserID);
    RootObject->SetObjectField(TEXT("user"), UserObject);
    
    // audio 对象
    TSharedPtr<FJsonObject> AudioObject = MakeShareable(new FJsonObject);
    AudioObject->SetStringField(TEXT("voice_type"), VoiceType);
    AudioObject->SetStringField(TEXT("encoding"), TEXT("wav"));
    AudioObject->SetStringField(TEXT("language"), TEXT("zh"));
    AudioObject->SetNumberField(TEXT("speed_ratio"), Params.LocalSettings->AudioSpeed);
    AudioObject->SetNumberField(TEXT("volume_ratio"), 1.0);
    AudioObject->SetNumberField(TEXT("pitch_ratio"), Params.LocalSettings->AudioPitch);
    RootObject->SetObjectField(TEXT("audio"), AudioObject);
    
    // request 对象
    TSharedPtr<FJsonObject> RequestObject = MakeShareable(new FJsonObject);
    RequestObject->SetStringField(TEXT("reqid"), FGuid::NewGuid().ToString());
    RequestObject->SetStringField(TEXT("text"), TTSText);
    RequestObject->SetStringField(TEXT("text_type"), TEXT("plain"));
    RequestObject->SetStringField(TEXT("operation"), TEXT("query"));
    RequestObject->SetNumberField(TEXT("with_frontend"), 1);
    RequestObject->SetStringField(TEXT("frontend_type"), TEXT("unitTson"));
    RootObject->SetObjectField(TEXT("request"), RequestObject);
    
    // 序列化JSON
    FString RequestPayload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestPayload);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
    
    // 构建URL
    FString FullURL = FString::Printf(TEXT("%s%s"), *Params.Settings->IP_DoubaoTTS_Server, *Params.Settings->API_DoubaoTTS);
    
    // 打印请求信息
    UE_LOG(LogTemp, Log, TEXT("=== 豆包TTS请求信息 ==="));
    UE_LOG(LogTemp, Log, TEXT("URL: %s"), *FullURL);
    UE_LOG(LogTemp, Log, TEXT("文本: %s"), *TTSText);
    UE_LOG(LogTemp, Log, TEXT("音色ID: %s (voice_type: %s)"), *SpkrId, *VoiceType);
    UE_LOG(LogTemp, Log, TEXT("语速: %.2f (speed_ratio)"), Params.LocalSettings->AudioSpeed);
    UE_LOG(LogTemp, Log, TEXT("语调: %.2f (pitch_ratio)"), Params.LocalSettings->AudioPitch);
    UE_LOG(LogTemp, Log, TEXT("完整请求JSON: %s"), *RequestPayload);
    UE_LOG(LogTemp, Log, TEXT("=== 请求信息完成 ==="));

    // 创建HTTP请求
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();
    
    HttpRequest->SetURL(FullURL);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer;%s"), *Params.Settings->DoubaoTTS_AccessToken));
    HttpRequest->SetContentAsString(RequestPayload);
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
                // 豆包接口返回JSON格式，需要解析
                FString ResponseString = Response->GetContentAsString();
                UE_LOG(LogTemp, Log, TEXT("豆包TTS响应: %s"), *ResponseString);
                
                TArray<uint8> AudioData;
                TSharedPtr<FJsonObject> JsonObject;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);
                
                if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
                {
                    // 检查是否有 data 字段
                    FString Base64Audio;
                    if (JsonObject->TryGetStringField(TEXT("data"), Base64Audio))
                    {
                        // 解码Base64音频数据
                        if (FBase64::Decode(Base64Audio, AudioData))
                        {
                            UE_LOG(LogTemp, Log, TEXT("成功解码豆包TTS音频数据，大小: %d 字节"), AudioData.Num());
                            
                            // 应用静音到音频数据（如果存在）
                            TArray<uint8> ProcessedAudioData = AudioData;
                            float PreSilence = Params.PreSilence;
                            float PostSilence = Params.PostSilence;
                            
                            if ((PreSilence > 0.0f || PostSilence > 0.0f) && AudioData.Num() > 0)
                            {
                                // 从WAV数据创建临时SoundWave获取音频信息
                                USoundWave* TempWave = UGenTools::CreateSoundWaveFromAudioData(AudioData);
                                if (TempWave)
                                {
                                    // 获取PCM数据
                                    TArray<uint8> PCMData;
                                    uint16 NumChannels = 0;
                                    uint32 SampleRate = 0;
                                    
                                    if (TempWave->GetImportedSoundWaveData(PCMData, SampleRate, NumChannels))
                                    {
                                        // 计算静音样本数
                                        int32 PreSilenceSamples = FMath::RoundToInt(PreSilence * SampleRate * NumChannels);
                                        int32 PostSilenceSamples = FMath::RoundToInt(PostSilence * SampleRate * NumChannels);
                                        
                                        // 创建新的音频数据（包含静音）
                                        TArray<uint8> ProcessedPCMData;
                                        int32 TotalSamples = PCMData.Num() / sizeof(int16) + PreSilenceSamples + PostSilenceSamples;
                                        ProcessedPCMData.SetNum(TotalSamples * sizeof(int16));
                                        
                                        int16* ProcessedPCM = reinterpret_cast<int16*>(ProcessedPCMData.GetData());
                                        int16* OriginalPCM = reinterpret_cast<int16*>(PCMData.GetData());
                                        int32 OriginalSamples = PCMData.Num() / sizeof(int16);
                                        
                                        // 填充段前静音（0值）
                                        if (PreSilenceSamples > 0)
                                        {
                                            FMemory::Memzero(ProcessedPCM, PreSilenceSamples * sizeof(int16));
                                        }
                                        
                                        // 复制原始音频数据
                                        FMemory::Memcpy(ProcessedPCM + PreSilenceSamples, OriginalPCM, OriginalSamples * sizeof(int16));
                                        
                                        // 填充段后静音（0值）
                                        if (PostSilenceSamples > 0)
                                        {
                                            FMemory::Memzero(ProcessedPCM + PreSilenceSamples + OriginalSamples, PostSilenceSamples * sizeof(int16));
                                        }
                                        
                                        // 将处理后的PCM数据转换回WAV格式
                                        if (UGenTools::ExportPCMToWavData(ProcessedPCMData, SampleRate, NumChannels, ProcessedAudioData))
                                        {
                                            UE_LOG(LogTemp, Log, TEXT("成功应用静音: 段前静音=%.2f秒, 段后静音=%.2f秒"), PreSilence, PostSilence);
                                        }
                                        else
                                        {
                                            UE_LOG(LogTemp, Warning, TEXT("转换PCM到WAV失败，使用原始音频数据"));
                                            ProcessedAudioData = AudioData;
                                        }
                                    }
                                    else
                                    {
                                        UE_LOG(LogTemp, Warning, TEXT("无法获取PCM数据，使用原始音频数据"));
                                        ProcessedAudioData = AudioData;
                                    }
                                    
                                    // 清理临时对象
                                    TempWave->MarkAsGarbage();
                                    TempWave->ConditionalBeginDestroy();
                                }
                                else
                                {
                                    UE_LOG(LogTemp, Warning, TEXT("无法创建SoundWave，使用原始音频数据"));
                                    ProcessedAudioData = AudioData;
                                }
                            }
                            
                            // 保存音频数据到内存
                            if (ProcessedAudioData.Num() > 0 && Params.SegmentResults.IsValid())
                    {
                        FTTSSegmentResult Result;
                        Result.SegmentIndex = Params.SegmentIndex;
                        Result.ReplicateIndex = Params.ReplicateIndex;
                        Result.Text = Params.LocalSettings ? Params.LocalSettings->TTSInputText : TEXT("");
                        Result.AssetPath = TEXT(""); // 暂时为空，等用户保存时再设置
                                Result.AudioData = ProcessedAudioData; // 保存处理后的音频数据（已包含静音）
                        Result.bIsSavedAsAsset = false; // 标记为未保存
                                Result.PreSilence = Params.PreSilence; // 保存静音信息
                                Result.PostSilence = Params.PostSilence; // 保存静音信息
                        Params.SegmentResults->Add(MoveTemp(Result));
                                
                                UE_LOG(LogTemp, Log, TEXT("音频数据已保存: 段落%d, 复制%d, 段前静音=%.2f秒, 段后静音=%.2f秒"), 
                                    Params.SegmentIndex, Params.ReplicateIndex, Params.PreSilence, Params.PostSilence);
                            }
                            else
                            {
                                UE_LOG(LogTemp, Warning, TEXT("豆包TTS返回的音频数据为空"));
                                FNotificationInfo Info(FText::FromString(TEXT("豆包TTS返回的音频数据为空")));
                                FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
                    }
                }
                else
                {
                            UE_LOG(LogTemp, Error, TEXT("解码Base64音频数据失败"));
                            FNotificationInfo Info(FText::FromString(TEXT("解码豆包TTS音频数据失败")));
                    FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
                            // 继续执行，以便批次处理逻辑能够继续
                        }
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("豆包TTS响应中未找到 data 字段"));
                        FString ErrorInfo;
                        if (JsonObject->TryGetStringField(TEXT("error"), ErrorInfo))
                        {
                            UE_LOG(LogTemp, Error, TEXT("错误信息: %s"), *ErrorInfo);
                            FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("豆包TTS错误: %s"), *ErrorInfo)));
                            FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
                        }
                        else
                        {
                            FNotificationInfo Info(FText::FromString(TEXT("豆包TTS响应格式错误：未找到 data 字段")));
                            FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
                        }
                        // 继续执行，以便批次处理逻辑能够继续
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("解析豆包TTS JSON响应失败"));
                    UE_LOG(LogTemp, Error, TEXT("响应内容: %s"), *ResponseString);
                    FNotificationInfo Info(FText::FromString(TEXT("解析豆包TTS响应失败")));
                    FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
                    // 继续执行，以便批次处理逻辑能够继续
                }
            }
            else
            {
                FString ErrorMsg = TEXT("TTS请求失败. ");
                int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
                FString ResponseContent = Response.IsValid() ? Response->GetContentAsString() : TEXT("");
                
                if (Response.IsValid())
                {
                    ErrorMsg += FString::Printf(TEXT("错误码: %d, 内容: %s"), ResponseCode, *ResponseContent);
                }
                
                // 检测429错误（并发限制）并实现重试机制
                if (ResponseCode == 429 && Params.RetryCount < 3)
                {
                    // 检查错误内容是否包含并发限制信息
                    bool bIsConcurrencyError = ResponseContent.Contains(TEXT("quota exceeded")) || 
                                               ResponseContent.Contains(TEXT("concurrency"));
                    
                    if (bIsConcurrencyError)
                    {
                        // 递增重试计数
                        FTTSRequestParams RetryParams = Params;
                        RetryParams.RetryCount = Params.RetryCount + 1;
                        
                        // 计算重试延迟（指数退避：1秒、2秒、4秒）
                        float RetryDelay = FMath::Pow(2.0f, static_cast<float>(Params.RetryCount));
                        
                        UE_LOG(LogTemp, Warning, TEXT("检测到429并发限制错误，将在%.1f秒后重试（第%d次重试）"), 
                            RetryDelay, RetryParams.RetryCount);
                        
                        // 使用定时器延迟重试
                        if (UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
                        {
                            if (FTimerManager* TimerManager = &World->GetTimerManager())
                            {
                                FTimerHandle RetryTimerHandle;
                                // 使用按值捕获所有需要的参数
                                UTTSSetting* RetryLocalSettings = RetryParams.LocalSettings;
                                const UXVCPluginSettings* RetrySettings = RetryParams.Settings;
                                int32 RetrySegmentIndex = RetryParams.SegmentIndex;
                                FString RetryAssetPath = RetryParams.AssetPath;
                                FString RetryWavDir = RetryParams.WavDir;
                                TSharedPtr<int32> RetryCompletedCount = RetryParams.CompletedCount;
                                TSharedPtr<int32> RetryTotalCount = RetryParams.TotalCount;
                                TSharedPtr<TArray<FString>> RetryCompletedAssetPaths = RetryParams.CompletedAssetPaths;
                                TSharedPtr<TArray<FTTSSegmentResult>> RetrySegmentResults = RetryParams.SegmentResults;
                                int32 RetryReplicateIndex = RetryParams.ReplicateIndex;
                                int32* RetryCurrentBatchIndex = RetryParams.CurrentBatchIndex;
                                int32* RetryBatchSize = RetryParams.BatchSize;
                                TArray<FString>* RetryAllTextSegments = RetryParams.AllTextSegments;
                                TArray<UTTSSetting*>* RetryAllTempSettings = RetryParams.AllTempSettings;
                                UTTSSetting* RetryOriginalTTSSettings = RetryParams.OriginalTTSSettings;
                                float RetryPreSilence = RetryParams.PreSilence;
                                float RetryPostSilence = RetryParams.PostSilence;
                                int32 RetryCount = RetryParams.RetryCount;
                                
                                TimerManager->SetTimer(RetryTimerHandle, [RetryLocalSettings, RetrySettings, RetrySegmentIndex,
                                    RetryAssetPath, RetryWavDir, RetryCompletedCount, RetryTotalCount, RetryCompletedAssetPaths,
                                    RetrySegmentResults, RetryReplicateIndex, RetryCurrentBatchIndex, RetryBatchSize,
                                    RetryAllTextSegments, RetryAllTempSettings, RetryOriginalTTSSettings,
                                    RetryPreSilence, RetryPostSilence, RetryCount]()
                                {
                                    UE_LOG(LogTemp, Log, TEXT("开始重试TTS请求（第%d次重试）"), RetryCount);
                                    FTTSRequestParams RetryParams2(RetryLocalSettings, RetrySettings, RetrySegmentIndex,
                                        RetryAssetPath, RetryWavDir, RetryCompletedCount, RetryTotalCount, RetryCompletedAssetPaths,
                                        RetrySegmentResults, RetryReplicateIndex, RetryCurrentBatchIndex, RetryBatchSize,
                                        RetryAllTextSegments, RetryAllTempSettings, RetryOriginalTTSSettings,
                                        RetryPreSilence, RetryPostSilence);
                                    RetryParams2.RetryCount = RetryCount;
                                    UTTSServer::SendTTSRequest(RetryParams2);
                                }, RetryDelay, false);
                                
                                // 不增加完成计数，因为请求还未完成
                                return;
                            }
                        }
                    }
                }
                
                // 如果重试次数已用完或不是429错误，则记录错误
                UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorMsg);
                if (Params.RetryCount >= 3)
                {
                    UE_LOG(LogTemp, Error, TEXT("TTS请求重试次数已达上限（3次），放弃重试"));
                }
                
                // 即使失败也要增加完成计数，以便批次处理能够继续
                (*Params.CompletedCount)++;
                GTTSProgressUpdateDelegate.Broadcast(*Params.CompletedCount, *Params.TotalCount);
                
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
                        DispatchTTSAllCompleted(Params);
                    }
                }
            }
            else
            {
                // 非分批处理模式（原有逻辑）
                if (*Params.CompletedCount >= *Params.TotalCount)
                {
                    DispatchTTSAllCompleted(Params);
                }
            }
        });
    HttpRequest->ProcessRequest();
}


void UTTSServer::FetchSpeakerList(TFunction<void(const TArray<TSharedPtr<FString>>&)> OnComplete)
{
    // 如果已经加载，直接返回
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

    // 确保voice_type映射已初始化
    EnsureDoubaoTTSVoiceTypeMapInitialized();
    
    // 清空旧的映射
            GSpkNameToId.Empty();
    GSpeakerDisplayOptions.Empty();

    // 从硬编码的豆包音色配置构建音色列表（对应 Python 中的 x_spkr_config）
    TArray<TSharedPtr<FString>> DisplayOptions;
    
    for (const FDoubaoTTS_SpeakerConfig& Config : GDoubaoTTS_SpeakerConfigs)
    {
        // 构建显示字符串格式: "gender|age|talking_style|spkr"
        FString TalkingStyle = Config.TalkingStyle.IsEmpty() ? TEXT("") : Config.TalkingStyle;
        FString DisplayString = FString::Printf(TEXT("%s|%s|%s|%s"), *Config.Gender, *Config.Age, *TalkingStyle, *Config.Spkr);
                            DisplayOptions.Add(MakeShared<FString>(DisplayString));

        // 记录 spkName (spkr) -> spkId 映射（用于根据显示名称查找音色ID）
        if (!Config.Spkr.IsEmpty() && !Config.SpkId.IsEmpty())
        {
            GSpkNameToId.Add(Config.Spkr, Config.SpkId);
        }
    }

    // 按显示字符串排序
    DisplayOptions.Sort([](const TSharedPtr<FString>& A, const TSharedPtr<FString>& B)
    {
        if (!A.IsValid() || !B.IsValid())
            return false;
        return *A < *B;
    });

    // 缓存结果
            GSpeakerDisplayOptions = DisplayOptions;
            GbSpeakerListLoaded = true;

    UE_LOG(LogTemp, Log, TEXT("加载豆包TTS音色列表完成，共 %d 个音色"), DisplayOptions.Num());

            // 通知所有等待的回调
            for (auto& Callback : GPendingSpeakerListCallbacks)
            {
                if (Callback)
                    Callback(GSpeakerDisplayOptions);
            }
            GPendingSpeakerListCallbacks.Empty();
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
        if (UGenTools::ExportPCMToWavData(PCMData, SampleRate, NumChannels, AudioData))
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
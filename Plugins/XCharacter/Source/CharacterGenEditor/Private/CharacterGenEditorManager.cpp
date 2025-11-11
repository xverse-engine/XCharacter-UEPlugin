// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterGenEditorManager.h"
#include "Tools/GenTools.h"
#include "A2F/A2FService.h"
#include "EditorStyleSet.h"
#include "MotionGen/BVHImportOptions.h"
#include "MotionGen/BVHImportSettings.h"
#include "Interfaces/IMainFrameModule.h"
#include "MotionGen/BVHImporter.h"
#include "MotionGen/BVHImportFactory.h"
#include "CharacterGenEditorOptions.h"
#include "MotionGen/MoGenSetting.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "Interfaces/IPluginManager.h"
#include "TTS/TTSServer.h"
#include "TTS/AudioAssetOperation.h"
#include "Widgets/SWaveformMini.h"

#include "LevelEditor.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "AssetToolsModule.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformFile.h"
#include "Misc/CRC.h"

#if ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION <= 2)
#include "AssetRegistryModule.h"
#else
#endif

#include "AssetRegistry/AssetRegistryModule.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SComboBox.h"
#include "TTS/TTSSetting.h"
#if WITH_EDITOR
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#endif

#define LOCTEXT_NAMESPACE "CharacterGenEditorManager"

// 颜色常量定义
// TTS按钮绿色：RGB(51, 204, 102) - 用于音频选中高亮，避免与音频播放的蓝色混淆
#define TTS_GREEN_COLOR FLinearColor(0.2f, 0.8f, 0.4f, 1.0f)
// 音频播放进度条蓝色：RGB(0, 112, 224) - 用于音频播放进度显示
#define AUDIO_PLAY_COLOR FLinearColor(0.0f, 0.44f, 0.88f, 1.0f)

DEFINE_LOG_CATEGORY_STATIC(LogFBVHPluginModule, Verbose, All);

UCharacterGenEditorManager::UCharacterGenEditorManager()
{
    TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
    ToolbarExtender->AddToolBarExtension("Content", EExtensionHook::After, nullptr,
                                         FToolBarExtensionDelegate::CreateUObject(
                                             this, &UCharacterGenEditorManager::AddToolbarExtension));

    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);

    CustomStyle = MakeShareable(new FSlateStyleSet("CustomStyleSet"));

    FString PluginResourcesPath = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("XCharacter"))->GetBaseDir(),
                                                  TEXT("Resources"));
    CustomStyle->SetContentRoot(PluginResourcesPath);

    CustomStyle->Set("XVerseIcon",
                     new FSlateImageBrush(CustomStyle->RootToContentDir(TEXT("LogoThumbnail_40.png")),
                                          FVector2D(16.0f, 16.0f)));

    FSlateStyleRegistry::RegisterSlateStyle(*CustomStyle.Get());
    
    // 初始化MotionGen服务
    MotionGenService = MakeShareable(new FMotionGenService());

    // 初始化TTS预览管理器
    TTSPreviewManager = UTTSPreviewManager::Get();

    // 订阅TTS完成事件（编辑器期望弹出预览窗口）
    UTTSServer::OnTTSAllCompleted().AddUObject(this, &UCharacterGenEditorManager::OnTTSAllCompletedHandler);
    
    // 订阅音频增强完成事件
    UTTSServer::OnEnhanceAudioCompleted().AddUObject(this, &UCharacterGenEditorManager::OnEnhanceAudioCompleted);
    
    // 绑定TTSPreviewManager的OnTTSCompleted委托，让它执行CharacterGenEditorManager的OnTTSCompleted
    if (TTSPreviewManager)
    {
        TTSPreviewManager->OnTTSCompleted.BindLambda([this]()
        {
            // 当TTSPreviewManager触发完成事件时，同时触发CharacterGenEditorManager的完成事件
            OnTTSCompleted.ExecuteIfBound();
        });
    }
}


UCharacterGenEditorManager::~UCharacterGenEditorManager()
{
    FSlateStyleRegistry::UnRegisterSlateStyle(CustomStyle->GetStyleSetName());
}


void UCharacterGenEditorManager::AddToolbarExtension(FToolBarBuilder& Builder)
{
    Builder.AddToolBarButton(
        FUIAction(FExecuteAction::CreateLambda([this]()
        {
            //UE_LOG(LogFBVHPluginModule, Warning, TEXT("click"));
            
            // 如果窗口已经存在，直接将其带到前台
            if (MotionGenWindow.IsValid())
            {
                UE_LOG(LogTemp, Log, TEXT("插件窗口已存在，将其带到前台"));
                MotionGenWindow->BringToFront();
            }
            else
            {
                // 如果窗口不存在，创建新窗口
                UE_LOG(LogTemp, Log, TEXT("创建新的插件窗口"));
                ShowAnimationGenWindow();
            }
        })),
        NAME_None, FText::FromString("XCharacter"),
        FText::FromString("Open XCharacter"),
        FSlateIcon(CustomStyle->GetStyleSetName(),
                   "XVerseIcon")
    );
}


void UCharacterGenEditorManager::ShowAnimationGenWindow()
{
    //UE_LOG(LogFBVHPluginModule, Warning, TEXT("create window"));

    // 检查是否已经有插件窗口打开，如果有则关闭它
    if (MotionGenWindow.IsValid())
    {
        UE_LOG(LogTemp, Log, TEXT("检测到已有插件窗口，关闭旧窗口并创建新窗口"));
        MotionGenWindow->RequestDestroyWindow();
        MotionGenWindow.Reset();
    }

    TSharedPtr<SCharacterGenEditorOptions> Options;
    UMoGenSetting* MoGenPtr = UMoGenSetting::Get();
    UA2FSetting* A2FPtr = UA2FSetting::Get();
    UTTSSetting* TTSPtr = UTTSSetting::Get();
    UAudioAssetOperationSetting* AudioAssetOperationPtr = UAudioAssetOperationSetting::Get();
    UNpcBpAssemblerSettings* NpcAsbPtr = UNpcBpAssemblerSettings::Get();

    const float WindowHeight = 800.f;
    const float WindowWidth = 1400.f;

    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(FText::FromString(TEXT("")))
        .ClientSize(FVector2D(WindowWidth, WindowHeight))
        .IsTopmostWindow(false) // 不置顶，允许其他界面显示
        .SizingRule(ESizingRule::UserSized)
        .SupportsMaximize(true)
        .SupportsMinimize(true);

    MotionGenWindow = Window;

    // 设置窗口关闭时的回调，清理窗口引用
    Window->GetOnWindowClosedEvent().AddLambda([this](const TSharedRef<SWindow>& ClosedWindow)
    {
        UE_LOG(LogTemp, Log, TEXT("插件窗口已关闭，清理窗口引用"));
        MotionGenWindow.Reset();
    });

    Window->SetContent
    (
        SAssignNew(Options, SCharacterGenEditorOptions)
        .WidgetWindow(Window)
        .ImportSettings(MoGenPtr)
        .A2FSettings(A2FPtr)
        .TTSSettings(TTSPtr)
        .AudioAssetOperationSettings(AudioAssetOperationPtr)
        .NpcBpAssemblerSettings(NpcAsbPtr)
        .ManagerRef(this)
    );
    Options->ManagerRef = this;

    FSlateApplication::Get().AddWindow(Window);
    Window->BringToFront();
}

void UCharacterGenEditorManager::BringWindowToFront()
{
    if (MotionGenWindow.IsValid())
    {
        MotionGenWindow->BringToFront();
    }
}

UCharacterGenEditorManager* UCharacterGenEditorManager::Get()
{
    return GetMutableDefault<UCharacterGenEditorManager>();
}

void UCharacterGenEditorManager::OnTTSAllCompletedHandler(const TArray<FTTSSegmentResult>& Results, UTTSSetting* TTSSettings)
{
    // 委托给TTS预览管理器处理
    if (TTSPreviewManager)
    {
        TTSPreviewManager->OnTTSAllCompletedHandler(Results, TTSSettings);
    }
}

static FString Local_FormatSecondsText(float InSeconds)
{
    InSeconds = FMath::Max(0.0f, InSeconds);
    int32 Total = FMath::RoundToInt(InSeconds);
    int32 Minutes = Total / 60;
    int32 Seconds = Total % 60;
    return FString::Printf(TEXT("%d:%02d"), Minutes, Seconds);
}

static FString Local_FormatTotalSecondsText(float InSeconds)
{
    InSeconds = FMath::Max(0.0f, InSeconds);
    return FString::Printf(TEXT("%.1fs"), InSeconds);
}

void UCharacterGenEditorManager::ShowTTSPreviewWindow(const TArray<FTTSSegmentResult>& Results, UTTSSetting* CurrentTTSSettings)
{
    // 委托给TTS预览管理器处理
    if (TTSPreviewManager)
    {
        TTSPreviewManager->ShowTTSPreviewWindow(Results, CurrentTTSSettings);
    }
}

void UCharacterGenEditorManager::SaveSelectedTTSAudio(const TArray<FTTSSegmentResult>& Results)
{
    // 委托给TTS预览管理器处理
    if (TTSPreviewManager)
    {
        TTSPreviewManager->SaveSelectedTTSAudio(Results);
    }
}

void UCharacterGenEditorManager::OnEnhanceAudioCompleted(bool bSuccess, const FString& OriginalAssetPath, const FString& EnhancedAssetPath)
{
    // 委托给TTS预览管理器处理
    if (TTSPreviewManager)
    {
        TTSPreviewManager->OnEnhanceAudioCompleted(bSuccess, OriginalAssetPath, EnhancedAssetPath);
    }
}


bool UCharacterGenEditorManager::IsTTSPreviewWindowOpen() const
{
    // 委托给TTS预览管理器处理
    if (TTSPreviewManager)
    {
        return TTSPreviewManager->IsTTSPreviewWindowOpen();
    }
    return false;
}
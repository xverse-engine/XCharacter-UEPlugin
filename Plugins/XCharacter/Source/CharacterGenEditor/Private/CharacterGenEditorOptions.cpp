// Copyright Epic Games, Inc. All Rights Reserved.


#include "CharacterGenEditorOptions.h"
#include "SlateOptMacros.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "CharAssetProcessor/Public/NpcBpAssemblerSettings.h"
#include "MotionGen/BVHImportSettings.h"
#include "TTS/AudioAssetOperation.h"
#include "Math/UnrealMathUtility.h"
#include "TTS/TTSServer.h"
#include "Modules/ModuleManager.h"
#include "InputCoreTypes.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "EasyCsv.h"
#include "EFDFunctionLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#include "Misc/Guid.h"
#include "Containers/StringConv.h"
#include "Misc/OutputDevice.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SSeparator.h"



#define LOCTEXT_NAMESPACE "CharacterGenEditorOptions"

// 析构函数实现
SCharacterGenEditorOptions::~SCharacterGenEditorOptions()
{
    UE_LOG(LogTemp, Log, TEXT("SCharacterGenEditorOptions析构函数被调用"));
}



DEFINE_LOG_CATEGORY_STATIC(LogSCharacterGenEditorOptions, Verbose, All);

// .h 文件中声明
TSharedPtr<IDetailsView> GenerateSettingDetailsView;
TSharedPtr<IDetailsView> TTSSettingDetailsView;
TSharedPtr<IDetailsView> AudioAssetOperationSettingDetailsView;
TSharedPtr<IDetailsView> A2FDetailsView;
TSharedPtr<IDetailsView> NpcBpAssemblerDetailsView;


void SCharacterGenEditorOptions::Construct(const FArguments& InArgs)
{
    ImportSettings = InArgs._ImportSettings;
    TTSSettings = InArgs._TTSSettings;
    AudioAssetOperationSettings = InArgs._AudioAssetOperationSettings;
    A2FSettings = InArgs._A2FSettings;
	NpcBpAssemblerSettings = InArgs._NpcBpAssemblerSettings;

    WidgetWindow = InArgs._WidgetWindow;
    ManagerRef = InArgs._ManagerRef;

    FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

    // 1. MotionGen Setting View
    FDetailsViewArgs GenArgs;
    GenArgs.bAllowSearch = false;
    GenArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
    GenArgs.ColumnWidth = 0.5f;
    GenerateSettingDetailsView = PropertyEditorModule.CreateDetailView(GenArgs);
    GenerateSettingDetailsView->SetObject(ImportSettings);

    // 2. TTS Setting View
    FDetailsViewArgs TTSSettingArgs = GenArgs;
    TTSSettingArgs.bAllowSearch = false;
    TTSSettingArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
    TTSSettingArgs.ColumnWidth = 0.5f;
    TTSSettingDetailsView = PropertyEditorModule.CreateDetailView(TTSSettingArgs);
    TTSSettingDetailsView->SetObject(TTSSettings);

    // 3. AudioAssetOperation Setting View
    FDetailsViewArgs AudioAssetOperationSettingArgs = GenArgs;
    AudioAssetOperationSettingDetailsView = PropertyEditorModule.CreateDetailView(AudioAssetOperationSettingArgs);
    AudioAssetOperationSettingDetailsView->SetObject(AudioAssetOperationSettings);

    // 4. A2F Setting View
    FDetailsViewArgs A2FArgs = GenArgs;
    A2FDetailsView = PropertyEditorModule.CreateDetailView(A2FArgs);
    A2FDetailsView->SetObject(A2FSettings);

    // 5. NpcBpAssembler Setting View
	FDetailsViewArgs NpcBpAssemblerArgs = GenArgs;
    NpcBpAssemblerDetailsView = PropertyEditorModule.CreateDetailView(NpcBpAssemblerArgs);
	NpcBpAssemblerDetailsView->SetObject(NpcBpAssemblerSettings);
    
    
    // 添加TTS设置的属性变更回调
    TTSSettingDetailsView->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent& Event) {
        // 只有在特定属性变更时才需要刷新视图
        if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UTTSSetting, TTSInputText) ||
            Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UTTSSetting, SpeakerName) ||
            Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UTTSSetting, AudioSpeed) ||
            Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UTTSSetting, AudioPitch))
        {
            // TTS相关属性变更时刷新视图
            // 移除ForceRefresh以避免循环刷新
        }
    });
    


    // 添加AudioAssetOperation设置的属性变更回调
    AudioAssetOperationSettingDetailsView->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent& Event) {
        if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UAudioAssetOperationSetting, SelectedLevelSequence))
        {
            if (AudioAssetOperationSettings && AudioAssetOperationSettings->SelectedLevelSequence.IsValid())
            {
                OpenXSequencerForLevelSequence(AudioAssetOperationSettings->SelectedLevelSequence.Get());
                
                // 获取轨道列表并刷新下拉框
                AudioAssetOperationSettings->RefreshTrackOptions();
                
                AudioAssetOperationSettingDetailsView->ForceRefresh();
            }
        }
    });
    
    // 添加A2F设置的属性变更回调
    A2FDetailsView->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent& Event) {
        if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UA2FSetting, SelectedLevelSequence))
        {
            if (A2FSettings && A2FSettings->SelectedLevelSequence.IsValid())
            {
                OpenXSequencerForLevelSequence(A2FSettings->SelectedLevelSequence.Get());
                
                // 刷新A2F详情视图
                A2FDetailsView->ForceRefresh();
            }
        }
        else if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UA2FSetting, bSkeletalAnim))
        {
            // 当骨骼动画驱动状态改变时，刷新A2F详情视图
            if (A2FDetailsView.IsValid())
            {
                A2FDetailsView->ForceRefresh();
            }
        }
    });

    // 3. 创建嵌入式内容浏览器
    FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    FContentBrowserConfig Config;
    Config.bCanShowDevelopersFolder = true;
    ContentBrowserWidget = ContentBrowserModule.Get().CreateContentBrowser(
        *ManagerRef->EmbeddedContentBrowserInstanceName,
        nullptr,
        &Config
    );

    // 轨道列表初始化（放在UI构建之前）
    if (AudioAssetOperationSettings && AudioAssetOperationSettings->SelectedLevelSequence.IsValid())
    {
        TArray<FString> RawTracks = UAudioAssetOperation::GetTracksFromLevelSequence(AudioAssetOperationSettings->SelectedLevelSequence.Get());
        TrackNames.Empty();
        for (const FString& Name : RawTracks)
        {
            TrackNames.Add(MakeShared<FString>(Name));
        }
    }

    // 初始化当前视图为TTS视图
    CurrentViewType = EViewType::TTS;
    CurrentViewWidget = CreateTTSView();

    this->ChildSlot
    [
        SNew(SOverlay)
        // 主内容区域
        + SOverlay::Slot()
        [
            SNew(SHorizontalBox)
            // 左侧：侧边栏导航（固定宽度）
            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Left)
            [
                SNew(SBox)
                .WidthOverride(120.0f)
                [
                    CreateSidebarNavigation()
                ]
            ]
            // 中间：内容区域（固定宽度）
            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Left)
            [
                SNew(SBox)
                .WidthOverride(500)
                [
                    CurrentViewWidget.ToSharedRef()
                ]
            ]
            // 右侧：内容浏览器（占据剩余空间）
            + SHorizontalBox::Slot()
            .FillWidth(1)
            .HAlign(HAlign_Fill)
            [
                SNew(SBox)
                .Padding(4)
                [
                    ContentBrowserWidget.ToSharedRef()
                ]
            ]
        ]
        // 版本信息（左下角）
        + SOverlay::Slot()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Bottom)
        .Padding(8, 0, 0, 8)
        [
            SNew(STextBlock)
            .Text(FText::FromString(GetVersionString()))
            .ColorAndOpacity(FLinearColor::White)
            .Font(FCoreStyle::GetDefaultFontStyle("Small", 8))
        ]
    ];
    
    if (ManagerRef)
    {
        UTTSServer::FetchSpeakerList([this](const TArray<TSharedPtr<FString>>&)
        {
            if (TTSSettingDetailsView.IsValid())
            {
                TTSSettingDetailsView->ForceRefresh();
            }
        });
        
        // 绑定TTS合成完成回调
        ManagerRef->OnTTSCompleted = FSimpleDelegate::CreateLambda([this]()
        {
            bIsTTSGenerating = false;
            
            // 完成时设置进度为100%
            if (TTSProgressBar.IsValid())
            {
                TTSProgressBar->SetPercent(1.0f);
            }
            
            if (TTSProgressText.IsValid())
            {
                if (bUseRealProgress && TTSTotalCount > 0)
                {
                    TTSProgressText->SetText(FText::FromString(FString::Printf(TEXT("合成完成！共完成 %d/%d 个音频"), TTSCompletedCount, TTSTotalCount)));
                }
                else
                {
                    TTSProgressText->SetText(FText::FromString(TEXT("合成完成！")));
                }
            }
            
            // 延迟0.5秒后停止进度条
            if (UWorld* World = GEditor->GetEditorWorldContext().World())
            {
                if (FTimerManager* TimerManager = &World->GetTimerManager())
                {
                    FTimerHandle StopTimerHandle;
                    TimerManager->SetTimer(StopTimerHandle, [this]()
                    {
                        StopTTSProgress();
                    }, 0.5f, false);
                }
            }
        });
        
        // 绑定TTS进度更新回调
        UTTSServer::OnTTSProgressUpdate().AddLambda([this](int32 CompletedCount, int32 TotalCount)
        {
            UpdateTTSRealProgress(CompletedCount, TotalCount);
        });
    }
}

// 创建侧边栏导航
TSharedRef<SWidget> SCharacterGenEditorOptions::CreateSidebarNavigation()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(10, 20, 10, 20)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("XCharacter")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
            .ColorAndOpacity(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(5, 5, 5, 5)
        [
            CreateNavigationButton(TEXT("TTS"), EViewType::TTS, FLinearColor(0.2f, 0.8f, 0.4f, 1.0f))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(5, 5, 5, 5)
        [
            CreateNavigationButton(TEXT("A2F"), EViewType::AudioOperation, FLinearColor(0.8f, 0.4f, 0.2f, 1.0f))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(5, 5, 5, 5)
        [
            CreateNavigationButton(TEXT("MoGen"), EViewType::MotionGen, FLinearColor(0.2f, 0.6f, 1.0f, 1.0f))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(5, 5, 5, 5)
        [
            CreateNavigationButton(TEXT("NpcAsb"), EViewType::NpcBpAssembler, FLinearColor(0.2f, 0.6f, 0.6f, 0.5f))
        ];
}



// 创建导航按钮
TSharedRef<SWidget> SCharacterGenEditorOptions::CreateNavigationButton(const FString& ButtonText, EViewType ViewType, const FLinearColor& Color)
{
    bool bIsSelected = (CurrentViewType == ViewType);
    
    return SNew(SButton)
        .ButtonStyle(FCoreStyle::Get(), bIsSelected ? "FlatButton.Dark" : "Button")
        .ContentPadding(FMargin(8, 6))
        .OnClicked_Lambda([this, ViewType]() {
            return SwitchToView(ViewType);
        })
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0, 0, 10, 0)
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(Color)
                .Padding(0)
                .Content()
                [
                    SNew(SBox)
                    .WidthOverride(5)
                    .HeightOverride(20)
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(ButtonText))
                .Font(FCoreStyle::GetDefaultFontStyle(bIsSelected ? "Bold" : "Normal", 12))
                .ColorAndOpacity(bIsSelected ? FLinearColor::White : FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
            ]
        ];
}

// 切换视图的通用方法
FReply SCharacterGenEditorOptions::SwitchToView(EViewType NewViewType)
{
    CurrentViewType = NewViewType;
    
    switch (NewViewType)
    {
    case EViewType::MotionGen:
        CurrentViewWidget = CreateMotionGenView();
        break;
    case EViewType::TTS:
        CurrentViewWidget = CreateTTSView();
        break;
    case EViewType::AudioOperation:
        CurrentViewWidget = CreateAudioOperationView();
        break;
	case EViewType::NpcBpAssembler:
		CurrentViewWidget = CreateNpcBpAssemblerView();
    default:
        break;
    }
    
    // 重新构建整个界面以更新侧边栏状态
    this->ChildSlot
    [
        SNew(SOverlay)
        // 主内容区域
        + SOverlay::Slot()
        [
            SNew(SHorizontalBox)
            // 左侧：侧边栏导航
            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Left)
            [
                SNew(SBox)
                .WidthOverride(120.0f)
                [
                    CreateSidebarNavigation()
                ]
            ]
            // 中间：内容区域（固定宽度）
            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Left)
            [
                SNew(SBox)
                .WidthOverride(500)
                [
                    CurrentViewWidget.ToSharedRef()
                ]
            ]
            // 右侧：内容浏览器（占据剩余空间）
            + SHorizontalBox::Slot()
            .FillWidth(1)
            .HAlign(HAlign_Fill)
            [
                SNew(SBox)
                .Padding(4)
                [
                    ContentBrowserWidget.ToSharedRef()
                ]
            ]
        ]
        // 版本信息（左下角）
        + SOverlay::Slot()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Bottom)
        .Padding(8, 0, 0, 8)
        [
            SNew(STextBlock)
            .Text(FText::FromString(GetVersionString()))
            .ColorAndOpacity(FLinearColor::White)
            .Font(FCoreStyle::GetDefaultFontStyle("Small", 8))
        ]
    ];
    
    return FReply::Handled();
}



// 创建动作生成视图
TSharedRef<SWidget> SCharacterGenEditorOptions::CreateMotionGenView()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        .Padding(20, 20, 20, 10)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("动作生成")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20, 10, 20, 10)
        [
            GenerateSettingDetailsView.ToSharedRef()
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(20, 0, 20, 20)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("生成动作")))
            .OnClicked(this, &SCharacterGenEditorOptions::OnImport)
        ];
}

// 创建TTS视图
TSharedRef<SWidget> SCharacterGenEditorOptions::CreateTTSView()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        .Padding(20, 20, 20, 10)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("语音合成")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
        ]
        // 文本输入区域
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20, 10, 20, 10)
        [
            SNew(SVerticalBox)
            // 文本输入框
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 8)
            [
                SNew(SBox)
                .MinDesiredHeight(80)
                .MaxDesiredHeight(80)
                [
                    SNew(SMultiLineEditableTextBox)
                    .HintText(FText::FromString(TEXT("请输入合成文本（按'\\n'切割，一行一个音频）或上传CSV文件")))
                    .Text_Lambda([this]() {
                        return FText::FromString(TTSSettings ? TTSSettings->TTSInputText : TEXT(""));
                    })
                    .AutoWrapText(true)
                    .OnTextChanged_Lambda([this](const FText& NewText) {
                        if (TTSSettings)
                        {
                            TTSSettings->TTSInputText = NewText.ToString();
                            // 强制刷新TTSSettingDetailsView以更新属性显示
                            if (TTSSettingDetailsView.IsValid())
                            {
                                TTSSettingDetailsView->ForceRefresh();
                            }
                        }
                    })
                ]
            ]
            // CSV文件上传按钮
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Left)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0, 0, 8, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("上传CSV文件")))
                    .OnClicked(this, &SCharacterGenEditorOptions::OnUploadExcelFile)
                    .ToolTipText(FText::FromString(TEXT("上传或CSV文件(.csv)，自动提取第一列文本内容")))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("清空文本")))
                    .OnClicked(this, &SCharacterGenEditorOptions::OnClearText)
                    .ToolTipText(FText::FromString(TEXT("清空当前文本内容")))
                ]
            ]
        ]
        // TTS设置详情视图
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20, 10, 20, 10)
        [
            TTSSettingDetailsView.ToSharedRef()
        ]
        // 合成音频按钮
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(20, 0, 20, 20)
        [
            SAssignNew(TTSGenerateButton, SButton)
            .Text_Lambda([this]() -> FText
            {
                return bIsTTSGenerating ? FText::FromString(TEXT("合成中...")) : FText::FromString(TEXT("合成音频"));
            })
            .IsEnabled_Lambda([this]() -> bool
            {
                return !bIsTTSGenerating && !ManagerRef->IsTTSPreviewWindowOpen();
            })
            .OnClicked(this, &SCharacterGenEditorOptions::OnGenerateTTS)
        ]
        // TTS合成进度条
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20, 10, 20, 10)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(0, 0, 0, 5)
            [
                SAssignNew(TTSProgressText, STextBlock)
                .Text(FText::FromString(TEXT("正在合成语音...")))
                .Font(FCoreStyle::GetDefaultFontStyle("Normal", 12))
                .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
                .Visibility_Lambda([this]() -> EVisibility
                {
                    return bIsTTSGenerating ? EVisibility::Visible : EVisibility::Collapsed;
                })
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Fill)
            [
                SAssignNew(TTSProgressBar, SProgressBar)
                .Percent(0.0f)
                .Visibility_Lambda([this]() -> EVisibility
                {
                    return bIsTTSGenerating ? EVisibility::Visible : EVisibility::Collapsed;
                })
            ]
        ]
        // 分割线
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0, 10, 0, 10)
        [
            SNew(SSeparator)
        ]
        // 音频操作部分
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(20, 20, 20, 10)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("音频轨道操作")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(20, 10, 20, 10)
            [
                AudioAssetOperationSettingDetailsView.ToSharedRef()
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Right)
            .Padding(20, 0, 20, 10)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0, 0, 10, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("刷新轨道列表")))
                    .OnClicked_Lambda([this]() {
                        if (AudioAssetOperationSettings)
                        {
                            AudioAssetOperationSettings->RefreshTrackOptions();
                            if (AudioAssetOperationSettingDetailsView.IsValid())
                            {
                                AudioAssetOperationSettingDetailsView->ForceRefresh();
                            }
                            UE_LOG(LogTemp, Log, TEXT("手动刷新轨道列表"));
                        }
                        return FReply::Handled();
                    })
                    // .ContentPadding(FMargin(10, 5))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("MoGenOptionWindow_AudioAssetOperation", "合并轨道上的音频片段"))
                    .OnClicked(this, &SCharacterGenEditorOptions::OnMergeAudio)
                    // .ContentPadding(FMargin(20, 10))
                ]
            ]
        ];
}

// 创建A2F视图
TSharedRef<SWidget> SCharacterGenEditorOptions::CreateAudioOperationView()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        .Padding(20, 20, 20, 10)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("面部口型生成")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20, 10, 20, 10)
        [
            A2FDetailsView.ToSharedRef()
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(20, 0, 20, 20)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("生成口型")))
            .OnClicked(this, &SCharacterGenEditorOptions::OnGenerateA2F)
        ];
}

// 创建NPC蓝图装配器视图
TSharedRef<SWidget> SCharacterGenEditorOptions::CreateNpcBpAssemblerView()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(20, 20, 20, 10)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("NPC蓝图装配器")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
		]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20, 10, 20, 10)
        [
            NpcBpAssemblerDetailsView.ToSharedRef()
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(0, 0, 10, 0)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0, 0, 10, 0)
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("检查角色骨骼")))
                .OnClicked(this, &SCharacterGenEditorOptions::OnCheckSkmAsset)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("生成角色蓝图")))
                .OnClicked(this, &SCharacterGenEditorOptions::OnAssembleNpcBp)
            ]
        ];
}

bool SCharacterGenEditorOptions::CanImport() const
{
    return true;
}


void SCharacterGenEditorOptions::OpenXSequencerForLevelSequence(ULevelSequence* LevelSequence)
{
    // 在这里实现打开XSequencer的逻辑
    UE_LOG(LogTemp, Log, TEXT("打开XSequencer: %s"), *LevelSequence->GetName());
    if (!LevelSequence) return;

    // 打开LevelSequence的编辑器（XSequencer）
    if (GEditor)
    {
        UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (AssetEditorSubsystem)
        {
            AssetEditorSubsystem->OpenEditorForAsset(LevelSequence);
        }
    }
}

void SCharacterGenEditorOptions::StartTTSProgressTimer()
{
    if (UWorld* World = GEditor->GetEditorWorldContext().World())
    {
        if (FTimerManager* TimerManager = &World->GetTimerManager())
        {
            // 清除之前的定时器
            TimerManager->ClearTimer(TTSProgressTimerHandle);
            
            // 启动新的定时器，每0.1秒更新一次进度
            TimerManager->SetTimer(TTSProgressTimerHandle, [this]()
            {
                UpdateTTSProgress();
            }, 0.1f, true);
        }
    }
}

void SCharacterGenEditorOptions::UpdateTTSProgress()
{
    if (!bIsTTSGenerating)
    {
        StopTTSProgress();
        return;
    }
    
    // 如果使用真实进度，则不需要模拟进度
    if (bUseRealProgress && TTSTotalCount > 0)
    {
        // 真实进度已经在UpdateTTSRealProgress中更新，这里不需要额外处理
        return;
    }
    
    // 在真实进度开始之前，只显示"正在合成语音..."，不显示模拟进度
    if (TTSProgressText.IsValid())
    {
        TTSProgressText->SetText(FText::FromString(TEXT("正在合成语音...")));
    }
    
    // 不更新进度条，保持0%
    if (TTSProgressBar.IsValid())
    {
        TTSProgressBar->SetPercent(0.0f);
    }
}

void SCharacterGenEditorOptions::StopTTSProgress()
{
    // 清除定时器
    if (UWorld* World = GEditor->GetEditorWorldContext().World())
    {
        if (FTimerManager* TimerManager = &World->GetTimerManager())
        {
            TimerManager->ClearTimer(TTSProgressTimerHandle);
        }
    }
    
    // 重置进度
    TTSProgressValue = 0.0f;
    TTSCompletedCount = 0;
    TTSTotalCount = 0;
    bUseRealProgress = false;
    
    // 更新进度条
    if (TTSProgressBar.IsValid())
    {
        TTSProgressBar->SetPercent(0.0f);
    }
    
    // 更新进度文本
    if (TTSProgressText.IsValid())
    {
        TTSProgressText->SetText(FText::FromString(TEXT("正在合成语音...")));
    }
}

void SCharacterGenEditorOptions::UpdateTTSRealProgress(int32 CompletedCount, int32 TotalCount)
{
    // 只有在有实际进度时才更新显示
    if (TotalCount > 0 && CompletedCount > 0)
    {
        TTSCompletedCount = CompletedCount;
        TTSTotalCount = TotalCount;
        bUseRealProgress = true;
        
        float ProgressPercent = (float)TTSCompletedCount / (float)TTSTotalCount;
        
        if (TTSProgressBar.IsValid())
        {
            TTSProgressBar->SetPercent(ProgressPercent);
        }
        
        if (TTSProgressText.IsValid())
        {
            FString ProgressText = FString::Printf(TEXT("正在合成语音... %d/%d (%.0f%%)"), 
                TTSCompletedCount, TTSTotalCount, ProgressPercent * 100.0f);
            TTSProgressText->SetText(FText::FromString(ProgressText));
        }
    }
}

// CSV文件上传相关方法实现
FReply SCharacterGenEditorOptions::OnUploadExcelFile()
{
    TArray<FString> OutFileNames;
    
    // 打开文件选择对话框，支持CSV文件
    bool bSuccess = UEFDFunctionLibrary::OpenFileDialog(
        TEXT("选择CSV文件"),
        TEXT("C:\\"),
        TEXT(""),
        TEXT("*.csv"),
        EEasyFileDialogFlags::Single,
        OutFileNames
    );
    
    if (!bSuccess || OutFileNames.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("未选择文件"));
        return FReply::Handled();
    }
    
    FString SelectedFile = OutFileNames[0];
    UE_LOG(LogTemp, Log, TEXT("选择的文件: %s"), *SelectedFile);
    
    // 解析CSV文件
    TArray<FString> TextLines;
    if (ParseCSVFile(SelectedFile, TextLines))
    {
        // 将解析的文本合并为换行分隔的字符串
        FString CombinedText = FString::Join(TextLines, TEXT("\n"));
        
        // 更新TTS设置
        if (TTSSettings)
        {
            TTSSettings->TTSInputText = CombinedText;
            
            // 刷新详情视图
            if (TTSSettingDetailsView.IsValid())
            {
                TTSSettingDetailsView->ForceRefresh();
            }
            
            UE_LOG(LogTemp, Log, TEXT("成功导入 %d 行文本"), TextLines.Num());
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("解析CSV文件失败"));
    }
    
    return FReply::Handled();
}

FReply SCharacterGenEditorOptions::OnClearText()
{
    if (TTSSettings)
    {
        TTSSettings->TTSInputText = TEXT("");
        
        // 刷新详情视图
        if (TTSSettingDetailsView.IsValid())
        {
            TTSSettingDetailsView->ForceRefresh();
        }
        
        UE_LOG(LogTemp, Log, TEXT("已清空文本内容"));
    }
    
    return FReply::Handled();
}

bool SCharacterGenEditorOptions::ParseCSVFile(const FString& FilePath, TArray<FString>& OutTextLines)
{
    OutTextLines.Empty();
    
    // 检查文件是否存在
    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("文件不存在: %s"), *FilePath);
        return false;
    }
    
    FString FileExtension = FPaths::GetExtension(FilePath).ToLower();
    
    if (FileExtension == TEXT("csv"))
    {
        // 处理CSV文件
        return ParseCSVFileContent(FilePath, OutTextLines);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("只支持CSV格式文件"));
        return false;
    }
}

bool SCharacterGenEditorOptions::ParseCSVFileContent(const FString& FilePath, TArray<FString>& OutTextLines)
{
    OutTextLines.Empty();
    FString FileContent;
    
    // 读取文件内容，尝试不同的编码
    TArray<uint8> FileBytes;
    if (!FFileHelper::LoadFileToArray(FileBytes, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("无法读取文件: %s"), *FilePath);
        return false;
    }
    
    
    // 尝试不同的编码方式
    bool bSuccess = false;
    FString BestContent;
    FString BestEncoding;
    
    // 1. 首先尝试UTF-8 BOM检测
    if (FileBytes.Num() >= 3 && 
        FileBytes[0] == 0xEF && FileBytes[1] == 0xBB && FileBytes[2] == 0xBF)
    {
        // UTF-8 BOM
        FString UTF8Content = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(FileBytes.GetData() + 3)));
        if (UTF8Content.Len() > 0 && !UTF8Content.Contains(TEXT("?")))
        {
            BestContent = UTF8Content;
            BestEncoding = TEXT("UTF-8 BOM");
            bSuccess = true;
        }
    }
    
    // 2. 尝试UTF-8（无BOM）
    if (!bSuccess)
    {
        FString UTF8Content = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(FileBytes.GetData())));
        if (UTF8Content.Len() > 0 && !UTF8Content.Contains(TEXT("?")))
        {
            BestContent = UTF8Content;
            BestEncoding = TEXT("UTF-8");
            bSuccess = true;
        }
    }
    
    // 3. 尝试GBK/GB2312编码（中文常用）
    if (!bSuccess)
    {
        // 使用Windows API进行GBK转换
        #if PLATFORM_WINDOWS
        int32 WideCharLen = MultiByteToWideChar(CP_ACP, 0, 
            reinterpret_cast<const char*>(FileBytes.GetData()), 
            FileBytes.Num(), nullptr, 0);
        
        if (WideCharLen > 0)
        {
            TArray<WCHAR> WideCharBuffer;
            WideCharBuffer.SetNum(WideCharLen + 1);
            
            MultiByteToWideChar(CP_ACP, 0, 
                reinterpret_cast<const char*>(FileBytes.GetData()), 
                FileBytes.Num(), WideCharBuffer.GetData(), WideCharLen);
            
            WideCharBuffer[WideCharLen] = 0;
            FString GBKContent = FString(WideCharBuffer.GetData());
            
            if (GBKContent.Len() > 0 && !GBKContent.Contains(TEXT("?")))
            {
                BestContent = GBKContent;
                BestEncoding = TEXT("GBK/GB2312");
                bSuccess = true;
            }
        }
        #endif
    }
    
    // 3.5. 尝试使用UE的内置编码转换
    if (!bSuccess)
    {
        // 尝试使用FText::FromString的编码检测
        FString TestContent = FString(ANSI_TO_TCHAR(reinterpret_cast<const char*>(FileBytes.GetData())));
        if (TestContent.Len() > 0)
        {
            // 检查是否包含中文字符
            bool bContainsChinese = false;
            for (int32 i = 0; i < TestContent.Len(); ++i)
            {
                TCHAR Char = TestContent[i];
                if (Char >= 0x4E00 && Char <= 0x9FFF) // 中文字符范围
                {
                    bContainsChinese = true;
                    break;
                }
            }
            
            if (bContainsChinese || !TestContent.Contains(TEXT("?")))
            {
                BestContent = TestContent;
                BestEncoding = TEXT("ANSI (中文检测)");
                bSuccess = true;
            }
        }
    }
    
    // 4. 尝试UTF-16 LE BOM
    if (!bSuccess && FileBytes.Num() >= 2 && 
        FileBytes[0] == 0xFF && FileBytes[1] == 0xFE)
    {
        FString UTF16Content = FString(reinterpret_cast<const TCHAR*>(FileBytes.GetData() + 2));
        if (UTF16Content.Len() > 0)
        {
            BestContent = UTF16Content;
            BestEncoding = TEXT("UTF-16 LE BOM");
            bSuccess = true;
        }
    }
    
    // 5. 尝试UTF-16 BE BOM
    if (!bSuccess && FileBytes.Num() >= 2 && 
        FileBytes[0] == 0xFE && FileBytes[1] == 0xFF)
    {
        // 需要字节序转换
        TArray<uint8> ConvertedBytes = FileBytes;
        for (int32 i = 2; i < ConvertedBytes.Num() - 1; i += 2)
        {
            uint8 Temp = ConvertedBytes[i];
            ConvertedBytes[i] = ConvertedBytes[i + 1];
            ConvertedBytes[i + 1] = Temp;
        }
        
        FString UTF16Content = FString(reinterpret_cast<const TCHAR*>(ConvertedBytes.GetData() + 2));
        if (UTF16Content.Len() > 0)
        {
            BestContent = UTF16Content;
            BestEncoding = TEXT("UTF-16 BE BOM");
            bSuccess = true;
        }
    }
    
    // 6. 最后尝试ANSI
    if (!bSuccess)
    {
        FString ANSIContent = FString(ANSI_TO_TCHAR(reinterpret_cast<const char*>(FileBytes.GetData())));
        if (ANSIContent.Len() > 0)
        {
            BestContent = ANSIContent;
            BestEncoding = TEXT("ANSI");
            bSuccess = true;
        }
    }
    
    if (bSuccess)
    {
        FileContent = BestContent;
    }
    
    // 如果所有编码都失败，尝试使用UE的内置文件读取
    if (!bSuccess)
    {
        
        // 使用UE的内置文件读取功能
        if (FFileHelper::LoadFileToString(FileContent, *FilePath))
        {
            BestContent = FileContent;
            BestEncoding = TEXT("UE内置读取");
            bSuccess = true;
        }
    }
    
    if (!bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("无法解析文件编码: %s"), *FilePath);
        return false;
    }
    
    
    // 使用EasyCsv解析CSV内容
    TArray<TArray<FString>> CsvData = UEasyCsv::ReadCsv(FileContent);
    
    
    if (CsvData.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("CSV文件为空或格式错误"));
        return false;
    }
    
    
    // 智能判断是否有表头
    bool bHasHeader = false;
    int32 StartRow = 0;
    
    if (CsvData.Num() > 0)
    {
        // 检查第一行是否包含表头关键词
        TArray<FString> FirstRow = CsvData[0];
        bool bIsHeader = false;
        
        // 检查第一行是否包含表头关键词
        UE_LOG(LogTemp, Log, TEXT("检查第一行是否为表头，第一行内容:"));
        for (int32 j = 0; j < FirstRow.Num(); ++j)
        {
            UE_LOG(LogTemp, Log, TEXT("  列%d: '%s'"), j, *FirstRow[j]);
        }
        
        for (const FString& Cell : FirstRow)
        {
            FString LowerCell = Cell.ToLower().TrimStartAndEnd();
            UE_LOG(LogTemp, Log, TEXT("检查单元格: '%s' -> '%s'"), *Cell, *LowerCell);
            
            if (LowerCell.Contains(TEXT("文本")) || 
                LowerCell.Contains(TEXT("text")) ||
                LowerCell.Contains(TEXT("内容")) ||
                LowerCell.Contains(TEXT("content")) ||
                LowerCell.Contains(TEXT("静音")) ||
                LowerCell.Contains(TEXT("silence")) ||
                LowerCell.Contains(TEXT("段前")) ||
                LowerCell.Contains(TEXT("段后")) ||
                LowerCell.Contains(TEXT("pre")) ||
                LowerCell.Contains(TEXT("post")) ||
                LowerCell.Contains(TEXT("前")) ||
                LowerCell.Contains(TEXT("后")))
            {
                bIsHeader = true;
                break;
            }
        }
        
        // 如果第一行包含表头关键词，或者第一行只有1-2列且第二行有3列，则认为是表头
        if (bIsHeader || (FirstRow.Num() <= 2 && CsvData.Num() > 1 && CsvData[1].Num() >= 3))
        {
            bHasHeader = true;
            StartRow = 1;
        }
        else
        {
            bHasHeader = false;
            StartRow = 0;
        }
    }
    
    
    int32 DiscardedRows = 0; // 统计丢弃的行数
    
    for (int32 i = StartRow; i < CsvData.Num(); ++i)
    {
        if (CsvData[i].Num() >= 3) // 确保至少有3列
        {
            FString TextColumn = CsvData[i][0].TrimStartAndEnd();
            FString PreSilence = CsvData[i][1].TrimStartAndEnd();
            FString PostSilence = CsvData[i][2].TrimStartAndEnd();
            
            
            if (!TextColumn.IsEmpty())
            {
                // 组合三列数据，用特殊分隔符分隔
                FString CombinedText = FString::Printf(TEXT("%s|%s|%s"), *TextColumn, *PreSilence, *PostSilence);
                OutTextLines.Add(CombinedText);
            }
            else
            {
                // 文本列为空，丢弃该行（即使静音有值）
                DiscardedRows++;
            }
        }
        else if (CsvData[i].Num() > 0)
        {
            // 如果只有1列，只提取文本
            FString TextColumn = CsvData[i][0].TrimStartAndEnd();
            if (!TextColumn.IsEmpty())
            {
                FString CombinedText = FString::Printf(TEXT("%s|0|0"), *TextColumn); // 默认静音为0
                OutTextLines.Add(CombinedText);
            }
        }
        else
        {
        }
    }
    
    
    // 如果解析失败，尝试从第一行开始解析（忽略表头检测）
    if (OutTextLines.Num() == 0 && CsvData.Num() > 0)
    {
        OutTextLines.Empty();
        
        for (int32 i = 0; i < CsvData.Num(); ++i)
        {
            if (CsvData[i].Num() >= 3)
            {
                FString TextColumn = CsvData[i][0].TrimStartAndEnd();
                FString PreSilence = CsvData[i][1].TrimStartAndEnd();
                FString PostSilence = CsvData[i][2].TrimStartAndEnd();
                
                if (!TextColumn.IsEmpty())
                {
                    FString CombinedText = FString::Printf(TEXT("%s|%s|%s"), *TextColumn, *PreSilence, *PostSilence);
                    OutTextLines.Add(CombinedText);
                }
                else
                {
                    // 文本列为空，丢弃该行（即使静音有值）
                }
            }
            else if (CsvData[i].Num() > 0)
            {
                FString TextColumn = CsvData[i][0].TrimStartAndEnd();
                if (!TextColumn.IsEmpty())
                {
                    FString CombinedText = FString::Printf(TEXT("%s|0|0"), *TextColumn);
                    OutTextLines.Add(CombinedText);
                }
            }
        }
        
    }
    
    return OutTextLines.Num() > 0;
}

FString SCharacterGenEditorOptions::GetVersionString() const
{
    // 获取插件根目录路径
    FString PluginDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() / TEXT("XCharacter_UEPlugin"));
    FString VersionFilePath = PluginDir / TEXT("version");
    
    // 读取版本文件
    FString VersionContent;
    if (FFileHelper::LoadFileToString(VersionContent, *VersionFilePath))
    {
        // 解析版本信息，格式: "version == 1.3.1.1"
        FString VersionNumber;
        if (VersionContent.StartsWith(TEXT("version == ")))
        {
            VersionNumber = VersionContent.RightChop(11).TrimStartAndEnd(); // 移除 "version == " 前缀
            return FString::Printf(TEXT("v%s"), *VersionNumber);
        }
    }
    
    // 如果读取失败，返回默认版本
    return TEXT(" ");
}



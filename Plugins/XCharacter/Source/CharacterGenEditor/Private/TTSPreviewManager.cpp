// Copyright Epic Games, Inc. All Rights Reserved.

#include "TTSPreviewManager.h"
#include "Tools/GenTools.h"
#include "TTS/TTSServer.h"
#include "TTS/AudioAssetOperation.h"
#include "XVCPluginSettings.h"
#include "Widgets/SWaveformMini.h"
#include "Widgets/SWaveformInteractive.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
//#include "Widgets/Layout/SVerticalBox.h"
//#include "Widgets/Layout/SHorizontalBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"
#include "Styling/AppStyle.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Async/Async.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/CRC.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "TTSPreviewManager"

// 颜色常量定义
// TTS按钮绿色：RGB(51, 204, 102) - 用于音频选中高亮，避免与音频播放的蓝色混淆
#define TTS_GREEN_COLOR FLinearColor(0.2f, 0.8f, 0.4f, 1.0f)
// 音频播放进度条蓝色：RGB(0, 112, 224) - 用于音频播放进度显示
#define AUDIO_PLAY_COLOR FLinearColor(0.0f, 0.44f, 0.88f, 1.0f)

UTTSPreviewManager::UTTSPreviewManager()
{
    // 初始化成员变量
    bIsUpdatingFromRegeneration = false;
    bGlobalUpgradeEnabled = false;
    bIsUpgrading = false;
    TotalSelectedAudioDuration = 0.0f;
    TotalSelectedAudioDurationWithSilence = 0.0f;
    TotalRetryAttempts = 0;
    MaxRetryAttempts = 3;
    PreviewSampleRate = 0;
    PreviewNumChannels = 0;
    bPreviewPaused = false;
}

UTTSPreviewManager::~UTTSPreviewManager()
{
    // 清理所有状态
    ClearAllTTSPreviewState();
}

UTTSPreviewManager* UTTSPreviewManager::Get()
{
    return GetMutableDefault<UTTSPreviewManager>();
}

static FString TTSPreview_FormatSecondsText(float InSeconds)
{
    InSeconds = FMath::Max(0.0f, InSeconds);
    int32 Total = FMath::RoundToInt(InSeconds);
    int32 Minutes = Total / 60;
    int32 Seconds = Total % 60;
    return FString::Printf(TEXT("%d:%02d"), Minutes, Seconds);
}

static FString TTSPreview_FormatTotalSecondsText(float InSeconds)
{
    InSeconds = FMath::Max(0.0f, InSeconds);
    return FString::Printf(TEXT("%.1fs"), InSeconds);
}

// 核心预览功能
void UTTSPreviewManager::ShowTTSPreviewWindow(const TArray<FTTSSegmentResult>& Results, UTTSSetting* CurrentTTSSettings, const TMap<int32, float>& PreSilenceValues, const TMap<int32, float>& PostSilenceValues)
{
    // 保存当前结果用于动态更新
    CurrentPreviewResults = Results;
    
    // 预填充采样率缓存，避免UI更新时频繁检查
    PrefillAudioSampleRateCache();
    
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(FText::FromString(TEXT("TTS 预览 - 请选择要保存的音频")))
        .ClientSize(FVector2D(1200.f, 800.f))
        .SupportsMaximize(true)
        .SupportsMinimize(true);
    
    // 保存窗口引用
    CurrentTTSPreviewWindow = Window;
    
    // 设置窗口关闭时的回调，只有在非重新合成/升级模式下才清理状态
    Window->GetOnWindowClosedEvent().AddLambda([this](const TSharedRef<SWindow>& ClosedWindow)
    {
        UE_LOG(LogTemp, Log, TEXT("TTS预览窗口已关闭"));
        
        // 只有在非重新合成/升级模式下才清理状态
        if (!bIsUpdatingFromRegeneration)
        {
            UE_LOG(LogTemp, Log, TEXT("手动关闭窗口，清理所有状态"));
            ClearAllTTSPreviewState();
            
            // 通知主界面TTS预览窗口已关闭，允许重新合成
            UE_LOG(LogTemp, Log, TEXT("通知主界面TTS预览窗口已关闭"));
            OnTTSCompleted.ExecuteIfBound();
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("重新合成/升级模式，保留状态"));
        }
    });

    TSharedRef<SVerticalBox> VBox = SNew(SVerticalBox);
    
    // 保存UI组件引用
    CurrentPreviewVBox = VBox;
    
    // 添加使用说明
    VBox->AddSlot().AutoHeight().Padding(8.f)
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(12.f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("TTS音频预览 - 支持多条文本，每条文本生成多个音频版本")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                .ColorAndOpacity(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("使用步骤：1. 播放音频试听效果 2. 点击音频区域选择要保存的版本（选中的音频会高亮显示）3. 点击底部'保存选中音频'按钮")))
                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
                .Font(FCoreStyle::GetDefaultFontStyle("Normal", 10))
            ]
        ]
    ];
    
    // 已选总时长与时长表（段+复本 -> 秒）
    TSharedPtr<TMap<FIntPoint, float>> DurationMap = MakeShared<TMap<FIntPoint, float>>();
    // 按段索引分组
    TMap<int32, TArray<const FTTSSegmentResult*>> SegmentToItems;
    for (const FTTSSegmentResult& R : Results)
    {
        SegmentToItems.FindOrAdd(R.SegmentIndex).Add(&R);
    }
    
    // 对每个段落的复本按ReplicateIndex排序，确保显示顺序一致
    for (auto& Pair : SegmentToItems)
    {
        TArray<const FTTSSegmentResult*>& Items = Pair.Value;
        Items.Sort([](const FTTSSegmentResult& A, const FTTSSegmentResult& B) -> bool
        {
            return A.ReplicateIndex < B.ReplicateIndex;
        });
    }

    // 按段索引排序输出
    TArray<int32> SortedSegments;
    SegmentToItems.GetKeys(SortedSegments);
    SortedSegments.Sort();

    // 获取当前TTS设置，用于初始化音色和情感选择
    // 优先使用传入的TTS设置对象，如果没有则使用默认对象
    UTTSSetting* TTSSettingsToUse = CurrentTTSSettings ? CurrentTTSSettings : UTTSSetting::Get();
    FString CurrentSpeakerName = TTSSettingsToUse ? TTSSettingsToUse->SpeakerName : TEXT("");
    FString CurrentEmotionName = TTSSettingsToUse ? TTSSettingsToUse->EmotionName : TEXT("自适应");
    float CurrentSpeed = TTSSettingsToUse ? TTSSettingsToUse->AudioSpeed : 1.0f;
    float CurrentPitch = TTSSettingsToUse ? TTSSettingsToUse->AudioPitch : 1.0f;
    
    // TTS预览界面初始化
    
    // 检查音色和情感选项列表
    TArray<FString> SpeakerOptions = UTTSSetting::GetSpeakerOptions();
    TArray<FString> EmotionOptions = UTTSSetting::GetEmotionOptions();
    
    // 如果音色为空，尝试从音色选项中获取第一个作为默认值
    if (CurrentSpeakerName.IsEmpty())
    {
        if (SpeakerOptions.Num() > 0 && SpeakerOptions[0] != TEXT("未加载音色列表"))
        {
            CurrentSpeakerName = SpeakerOptions[0];
        }
    }
    
    // 段落循环处理 - 简化版本
    for (int32 SegIdx : SortedSegments)
    {
        const TArray<const FTTSSegmentResult*>& Items = SegmentToItems[SegIdx];
        
        // 添加行间距（除了第一行）
        if (SegIdx > 0)
        {
            VBox->AddSlot().AutoHeight().Padding(0,16,0,0)
            [
                SNullWidget::NullWidget
            ];
        }
        
        // 行容器：上文本、下三条音频
        TSharedRef<SVerticalBox> RowVBox = SNew(SVerticalBox);
        
        // 顶部文本（可编辑）
        if (!EditedTextBySegment.Contains(SegIdx))
        {
            EditedTextBySegment.Add(SegIdx, MakeShared<FString>(Items.Num()>0 ? *Items[0]->Text : TEXT("")));
        }
        TSharedPtr<FString> BoundText = EditedTextBySegment[SegIdx];
        
        // 初始化当前段落的音色和情感选择
        // 只有在不存在时才设置默认值，保持各段落的独立配置
        if (!SegmentSpeakerSelections.Contains(SegIdx))
        {
            SegmentSpeakerSelections.Add(SegIdx, MakeShared<FString>(CurrentSpeakerName));
        }
        // 重新合成模式下保持原有配置，不强制更新
        
        if (!SegmentEmotionSelections.Contains(SegIdx))
        {
            SegmentEmotionSelections.Add(SegIdx, MakeShared<FString>(CurrentEmotionName));
        }
        // 重新合成模式下保持原有配置，不强制更新
        
        if (!SegmentSpeedValues.Contains(SegIdx))
        {
            SegmentSpeedValues.Add(SegIdx, CurrentSpeed);
        }
        // 重新合成模式下保持原有配置，不强制更新
        
        if (!SegmentPitchValues.Contains(SegIdx))
        {
            SegmentPitchValues.Add(SegIdx, CurrentPitch);
        }
        // 重新合成模式下保持原有配置，不强制更新
        
        // 文本编辑区域
        RowVBox->AddSlot().AutoHeight().Padding(0,0,0,8)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0,0,12.f,0).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("[%d]"), SegIdx)))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                .ColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.3f, 1.0f))
            ]
            + SHorizontalBox::Slot().FillWidth(1.f)
            [
                SNew(SBox)
                .MinDesiredHeight(48.0f)
                [
                    SNew(SMultiLineEditableTextBox)
                    .Text_Lambda([BoundText]() -> FText { return FText::FromString(*BoundText); })
                    .OnTextCommitted_Lambda([BoundText](const FText& NewText, ETextCommit::Type){ *BoundText = NewText.ToString(); })
                    .Font(FCoreStyle::GetDefaultFontStyle("Normal", 10))
                    .HintText(FText::FromString(TEXT("编辑文本内容...")))
                ]
            ]
        ];

        // 准备音色和情感选项数据 - 使用Lambda动态获取，避免局部变量生命周期问题
        auto GetSpeakerOptions = []() -> const TArray<TSharedPtr<FString>>*
        {
            static TArray<TSharedPtr<FString>> SpeakerOptions;
            
            // 每次都重新获取最新的音色列表
            SpeakerOptions.Empty();
            TArray<FString> RawSpeakerOptions = UTTSSetting::GetSpeakerOptions();
            for (const FString& Option : RawSpeakerOptions)
            {
                SpeakerOptions.Add(MakeShared<FString>(Option));
            }
            
            return &SpeakerOptions;
        };
        
        auto GetEmotionOptions = []() -> const TArray<TSharedPtr<FString>>*
        {
            static TArray<TSharedPtr<FString>> EmotionOptions;
            
            // 每次都重新获取最新的情感列表
            EmotionOptions.Empty();
            TArray<FString> RawEmotionOptions = UTTSSetting::GetEmotionOptions();
            for (const FString& Option : RawEmotionOptions)
            {
                EmotionOptions.Add(MakeShared<FString>(Option));
            }
            
            return &EmotionOptions;
        };



        // TTS配置区域（一行布局）
        RowVBox->AddSlot().AutoHeight().Padding(0,8,0,8)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(12.f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("TTS配置")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                    .ColorAndOpacity(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    // 情感
                    + SHorizontalBox::Slot().FillWidth(1.f).Padding(0,0,8.f,0)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("情感:")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Normal", 9))
                    ]
                        + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SComboBox<TSharedPtr<FString>>)
                            .OptionsSource(GetEmotionOptions())
                            .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) -> TSharedRef<SWidget>
                            {
                                return SNew(STextBlock).Text(FText::FromString(*Item));
                            })
                            .OnSelectionChanged_Lambda([this, SegIdx](TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
                            {
                                if (NewSelection.IsValid())
                                {
                                    if (!SegmentEmotionSelections.Contains(SegIdx))
                                    {
                                        SegmentEmotionSelections.Add(SegIdx, MakeShared<FString>());
                                    }
                                    *SegmentEmotionSelections[SegIdx] = *NewSelection;
                                }
                            })
                        .Content()
                        [
                            SNew(STextBlock)
                                .Text_Lambda([this, SegIdx]() -> FText
                            {
                                    // 检查SegmentEmotionSelections是否包含当前段落
                                if (SegmentEmotionSelections.Contains(SegIdx))
                                {
                                        FString SelectedValue = *(*SegmentEmotionSelections.Find(SegIdx));
                                        if (!SelectedValue.IsEmpty())
                                        {
                                            return FText::FromString(SelectedValue);
                                        }
                                    }
                                    return FText::FromString(TEXT("选择情感"));
                                })
                            ]
                        ]
                    ]
                    // 语速
                    + SHorizontalBox::Slot().FillWidth(1.f).Padding(0,0,8.f,0)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("语速:")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Normal", 9))
                    ]
                        + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SSpinBox<float>)
                            .Value_Lambda([this, SegIdx]() -> float
                            {
                                if (const float* Found = SegmentSpeedValues.Find(SegIdx))
                                {
                                    return *Found;
                                }
                                return 1.0f;
                        })
                        .OnValueChanged_Lambda([this, SegIdx](float NewValue)
                        {
                            SegmentSpeedValues.Add(SegIdx, NewValue);
                        })
                            .MinValue(0.5f)
                            .MaxValue(2.0f)
                            .Delta(0.1f)
                        ]
                    ]
                    // 音高
                    + SHorizontalBox::Slot().FillWidth(1.f).Padding(0,0,8.f,0)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("音高:")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Normal", 9))
                    ]
                        + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SSpinBox<float>)
                            .Value_Lambda([this, SegIdx]() -> float
                            {
                                if (const float* Found = SegmentPitchValues.Find(SegIdx))
                                {
                                    return *Found;
                                }
                                return 1.0f;
                        })
                        .OnValueChanged_Lambda([this, SegIdx](float NewValue)
                        {
                            SegmentPitchValues.Add(SegIdx, NewValue);
                        })
                            .MinValue(0.5f)
                            .MaxValue(2.0f)
                            .Delta(0.1f)
                        ]
                    ]
                    // 段前静音
                    + SHorizontalBox::Slot().FillWidth(1.f).Padding(0,0,8.f,0)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("段前静音:")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Normal", 9))
                    ]
                        + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SSpinBox<float>)
                        .Value_Lambda([this, SegIdx]() -> float
                        {
                                if (const float* Found = SegmentPreSilenceValues.Find(SegIdx))
                                {
                                    return *Found;
                                }
                                return 0.0f;
                        })
                        .OnValueChanged_Lambda([this, SegIdx](float NewValue)
                        {
                            SegmentPreSilenceValues.Add(SegIdx, NewValue);
                                // 重新计算总时长（包括静音）
                                RecalculateSelectedAudioDuration();
                            })
                            .MinValue(0.0f)
                            .MaxValue(1000.0f)
                            .Delta(0.1f)
                        ]
                    ]
                    // 段后静音
                    + SHorizontalBox::Slot().FillWidth(1.f)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("段后静音:")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Normal", 9))
                    ]
                        + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SSpinBox<float>)
                        .Value_Lambda([this, SegIdx]() -> float
                        {
                                if (const float* Found = SegmentPostSilenceValues.Find(SegIdx))
                                {
                                    return *Found;
                                }
                                return 0.0f;
                        })
                        .OnValueChanged_Lambda([this, SegIdx](float NewValue)
                        {
                            SegmentPostSilenceValues.Add(SegIdx, NewValue);
                                // 重新计算总时长（包括静音）
                                RecalculateSelectedAudioDuration();
                        })
                            .MinValue(0.0f)
                            .MaxValue(1000.0f)
                            .Delta(0.1f)
                        ]
                    ]
                ]
            ]
        ];

        // 底部并排四个音频 + 重新合成按钮
        TSharedRef<SHorizontalBox> LineHBox = SNew(SHorizontalBox);
        bool bFirstItem = true;
        for (const FTTSSegmentResult* ItemPtr : Items)
        {
            const FTTSSegmentResult& Item = *ItemPtr;
            TSharedPtr<float> SharePercent = MakeShared<float>(0.0f);
            TSharedPtr<float> ShareDuration = MakeShared<float>(0.0f);
            TSharedPtr<bool> bIsPlaying = MakeShared<bool>(false);
            TSharedPtr<bool> bIsPaused = MakeShared<bool>(false);
            const int32 KeyLocal = Item.SegmentIndex * 100 + Item.ReplicateIndex;
            const FString AssetPathLocal = Item.AssetPath;

            // 从内存中的音频数据或WAV文件获取时长
            float Duration = 0.0f;
            
            // 首先尝试从内存中的音频数据创建SoundWave获取时长
            if (Item.AudioData.Num() > 0)
            {
                if (USoundWave* TempWave = UGenTools::CreateSoundWaveFromAudioData(Item.AudioData))
                {
                    Duration = TempWave->GetDuration();
                }
            }
            
            if (Duration > 0.0f && FMath::IsFinite(Duration))
            {
                *ShareDuration = Duration;
                DurationMap->Add(FIntPoint(Item.SegmentIndex, Item.ReplicateIndex), Duration);
            }

            if (!bFirstItem)
            {
                LineHBox->AddSlot().AutoWidth().Padding(6.f,0)
                [
                    SNew(SSeparator)
                    .Orientation(Orient_Vertical)
                ];
            }
            bFirstItem = false;

                         LineHBox->AddSlot().FillWidth(1.f).Padding(8.f,0).VAlign(VAlign_Center)
            [
                SNew(SBorder)
                 .Padding_Lambda([this, SegIdx, Rep=Item.ReplicateIndex]() -> FMargin
                 {
                     // 检查是否被选中，如果是则使用更大的外边框
                     if (const int32* Found = SelectedReplicateBySegment.Find(SegIdx))
                     {
                         if (*Found == Rep)
                         {
                             return FMargin(3.f); // 选中时使用3像素外边框，形成强烈对比
                         }
                     }
                     return FMargin(1.f); // 未选中时使用1像素外边框
                 })
                 .BorderImage_Lambda([this, SegIdx, Rep=Item.ReplicateIndex]() -> const FSlateBrush*
                 {
                     // 检查是否被选中，如果是则使用高亮边框
                     if (const int32* Found = SelectedReplicateBySegment.Find(SegIdx))
                     {
                         if (*Found == Rep)
                         {
                             return FAppStyle::GetBrush("NoBrush"); // 选中时使用白色边框，形成强烈对比
                         }
                     }
                     return FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"); // 未选中时使用深色边框
                 })
                 .ColorAndOpacity_Lambda([this, SegIdx, Rep=Item.ReplicateIndex]() -> FLinearColor
                 {
                     // 检查是否被选中，如果是则使用高亮颜色
                     if (const int32* Found = SelectedReplicateBySegment.Find(SegIdx))
                     {
                         if (*Found == Rep)
                         {
                             return FColor(0,112,224,255); // 高亮：rgba(0,112,224,255)
                         }
                     }
                     return FLinearColor(0.95f, 0.95f, 0.95f, 1.0f); // 未选中：与TTS预览界面主背景色保持一致
                 })
                 [
                     SNew(SBorder)
                     .Padding(FMargin(12.f)) // 固定内边距，避免大小变化
                     .BorderImage_Lambda([this, SegIdx, Rep=Item.ReplicateIndex]() -> const FSlateBrush*
                     {
                         // 检查是否被选中，如果是则使用高亮边框
                         if (const int32* Found = SelectedReplicateBySegment.Find(SegIdx))
                         {
                             if (*Found == Rep)
                             {
                                 return FAppStyle::GetBrush("WhiteBrush"); // 选中时使用白色边框
                             }
                         }
                         return FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"); // 未选中时使用深色边框
                     })


                 .OnMouseButtonDown_Lambda([this, SegIdx, Rep=Item.ReplicateIndex, DurationMap](const FGeometry& Geometry, const FPointerEvent& MouseEvent) -> FReply
                 {
                     if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
                     {
                         // 检查是否已经选中
                         if (const int32* Found = SelectedReplicateBySegment.Find(SegIdx))
                         {
                             if (*Found == Rep)
                             {
                                 // 如果已经选中，则取消选中
                                 SelectedReplicateBySegment.Remove(SegIdx);
                             }
                             else
                             {
                                 // 如果选中了其他版本，则切换到当前版本
                                 SelectedReplicateBySegment.Add(SegIdx, Rep);
                             }
                         }
                         else
                         {
                             // 如果没有选中任何版本，则选中当前版本
                             SelectedReplicateBySegment.Add(SegIdx, Rep);
                         }
                         
                         // 重算总时长
                         RecalculateSelectedAudioDuration();
                         
                            return FReply::Handled();
                     }
                     return FReply::Unhandled();
                        })
                        [
                 SNew(SVerticalBox)
                                   + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)
                            [
                                SNew(STextBlock)
                       .Text_Lambda([SharePercent, ShareDuration]() -> FText
                       {
                           const float Duration = *ShareDuration;
                           const float Current = FMath::Clamp(*SharePercent, 0.0f, 1.0f) * (Duration > 0.0f ? Duration : 0.0f);
                           return FText::FromString(TTSPreview_FormatSecondsText(Current) + TEXT(" / ") + TTSPreview_FormatSecondsText(Duration));
                       })
                       .Font(FCoreStyle::GetDefaultFontStyle("Normal", 9))
                       .ColorAndOpacity_Lambda([this, SegIdx, Rep=Item.ReplicateIndex]() -> FSlateColor
                       {
                           // 检查是否被选中，如果是则使用白色文字
                           if (const int32* Found = SelectedReplicateBySegment.Find(SegIdx))
                           {
                               if (*Found == Rep)
                               {
                                   return FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)); // 高亮时使用白色文字
                               }
                           }
                           return FSlateColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f)); // 未选中时使用深色文字
                       })
                       .ColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f))) // 强制设置默认颜色，防止被覆盖
                       
                   ]
                 + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)
                 [
                     SNew(SProgressBar)
                     .Percent_Lambda([SharePercent]() -> TOptional<float>
                     {
                         return TOptional<float>(*SharePercent);
                     })
                     .FillColorAndOpacity(AUDIO_PLAY_COLOR)  // 使用音频播放蓝色
                 ]
                                   + SVerticalBox::Slot().AutoHeight()
                  [
                      SNew(SHorizontalBox)
                      + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                          .ToolTipText_Lambda([bIsPlaying]() -> FText { return *bIsPlaying ? FText::FromString(TEXT("暂停")) : FText::FromString(TEXT("播放")); })
                          .ContentPadding(FMargin(8.f,4.f))
                          [
                              SNew(STextBlock)
                              .Text_Lambda([bIsPlaying]() -> FText
                              {
                                  // 使用ASCII字符，最大兼容性
                                  return *bIsPlaying ? FText::FromString(TEXT("||")) : FText::FromString(TEXT("▶"));
                              })
                              .Justification(ETextJustify::Center)
                              .Font(FCoreStyle::GetDefaultFontStyle("Normal", 10))
                              .ColorAndOpacity_Lambda([this, SegIdx, Rep=Item.ReplicateIndex]() -> FSlateColor
                              {
                                  // 检查是否被选中，如果是则使用白色文字
                                  if (const int32* Found = SelectedReplicateBySegment.Find(SegIdx))
                                  {
                                      if (*Found == Rep)
                                      {
                                          return FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)); // 高亮时使用白色文字
                                      }
                                  }
                                  return FSlateColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f)); // 未选中时使用深色文字
                              })
                          ]
                          .OnClicked_Lambda([this, KeyLocal, AssetPathLocal, SharePercent, ShareDuration, bIsPlaying, bIsPaused, SegIdx, Rep=Item.ReplicateIndex]() -> FReply
                          {
                              if (TWeakObjectPtr<UAudioComponent>* Found = PlayingComponents.Find(KeyLocal))
                              {
                                  if (Found->IsValid())
                                  {
                                      UAudioComponent* Comp = Found->Get();
                                      if (*bIsPlaying)
                                      {
                                          // 暂停
                                          Comp->SetPaused(true);
                                          *bIsPlaying = false;
                                          *bIsPaused = true;
                                      }
                                      else
                                      {
                                          // 从暂停恢复
                                          Comp->SetPaused(false);
                                          *bIsPlaying = true;
                                          *bIsPaused = false;
                                      }
                return FReply::Handled();
                                  }
                                  // 组件失效则移除，按无组件处理
                                  PlayingComponents.Remove(KeyLocal);
                              }

                                                             // 无已有组件：创建并开始播放
                               // 首先尝试从内存中的音频数据创建SoundWave
                               USoundWave* SoundWave = nullptr;
                               
                               // 查找对应的音频结果
                               for (const FTTSSegmentResult& Result : CurrentPreviewResults)
                               {
                                   if (Result.SegmentIndex == SegIdx && Result.ReplicateIndex == Rep)
                                   {
                                       // 如果音频数据存在，从内存创建SoundWave
                                       if (Result.AudioData.Num() > 0)
                                       {
                                           SoundWave = UGenTools::CreateSoundWaveFromAudioData(Result.AudioData);
                                       }
                                       break;
                                   }
                               }
                               
                               // 如果内存中没有数据，尝试从WAV文件加载
                               if (!SoundWave)
                               {
                                   // 查找对应的音频结果获取WAV文件路径
                    for (const FTTSSegmentResult& Result : CurrentPreviewResults)
                    {
                                       if (Result.SegmentIndex == SegIdx && Result.ReplicateIndex == Rep)
                                       {
                                           if (Result.AudioData.Num() > 0)
                                           {
                                               // 从内存数据创建SoundWave
                                               SoundWave = UGenTools::CreateSoundWaveFromAudioData(Result.AudioData);
                                           }
                            break;
                        }
                    }
                }
                
                               if (SoundWave)
                               {
                                   UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
                                   if (World)
                                   {
                                       UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(World, SoundWave, 1.0f, 1.0f, 0.0f, nullptr, true);
                                       if (Comp)
                                       {
                                           const float Dur = SoundWave->GetDuration();
                                           if (Dur > 0.0f && FMath::IsFinite(Dur))
                                           {
                                               *ShareDuration = Dur;
                                           }
                                           *SharePercent = 0.0f;
                                           *bIsPlaying = true;
                                           *bIsPaused = false;
                                           Comp->OnAudioPlaybackPercentNative.AddLambda([SharePercent](const UAudioComponent* AC, const USoundWave* SW, const float InPercent)
                                           {
                                               *SharePercent = FMath::Clamp(InPercent, 0.0f, 1.0f);
                                           });
                                           Comp->OnAudioFinishedNative.AddLambda([this, KeyLocal, bIsPlaying, bIsPaused, SharePercent](UAudioComponent* AC)
                                           {
                                               if (TWeakObjectPtr<UAudioComponent>* FoundFinish = PlayingComponents.Find(KeyLocal))
                                               {
                                                   if (FoundFinish->Get() == AC)
                                                   {
                                                       PlayingComponents.Remove(KeyLocal);
                                                   }
                                               }
                                               *bIsPlaying = false;
                                               *bIsPaused = false;
                                               *SharePercent = 1.0f;
                                           });
                                           Comp->Play();
                                           PlayingComponents.Add(KeyLocal, Comp);
                                       }
                                   }
                               }
                            return FReply::Handled();
                        })
                      ]
                       
                  ]
                    ]
                ]
            ];
        }

        // 在音频区域右侧添加重新合成按钮和进度条
        LineHBox->AddSlot().AutoWidth().Padding(8.f,0,0,0).VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SButton)
                .Text_Lambda([this, SegIdx]() -> FText
                {
                    // 如果正在重新合成，显示"合成中..."
                    if (RegeneratingSegments.Contains(SegIdx) && RegeneratingSegments[SegIdx])
                    {
                        return FText::FromString(TEXT("合成中..."));
                    }
                    return FText::FromString(TEXT("重新合成"));
                })
                .ToolTipText(FText::FromString(TEXT("使用当前配置重新合成此段文本的音频")))
                .ContentPadding(FMargin(12.f,6.f))
                .IsEnabled_Lambda([this, SegIdx]() -> bool
                {
                    // 如果正在重新合成，禁用按钮
                    return !(RegeneratingSegments.Contains(SegIdx) && RegeneratingSegments[SegIdx]);
                })
                .OnClicked_Lambda([this, SegIdx, BoundText]() -> FReply
                {
                    // 获取当前段落的配置
                    FString SpeakerName = TEXT("");
                    FString EmotionName = TEXT("");
                    float Speed = 1.0f;
                    float Pitch = 1.0f;
                    
                    if (const TSharedPtr<FString>* Found = SegmentSpeakerSelections.Find(SegIdx))
                    {
                        SpeakerName = **Found;
                    }
                    if (const TSharedPtr<FString>* Found = SegmentEmotionSelections.Find(SegIdx))
                    {
                        EmotionName = **Found;
                    }
                    if (const float* Found = SegmentSpeedValues.Find(SegIdx))
                    {
                        Speed = *Found;
                    }
                    if (const float* Found = SegmentPitchValues.Find(SegIdx))
                    {
                        Pitch = *Found;
                    }
                    
                    // 设置重新合成状态
                RegeneratingSegments.Add(SegIdx, true);
                
                    // 调用重新合成功能
                    RegenerateTTSForSegment(SegIdx, *BoundText, SpeakerName, EmotionName, Speed, Pitch);
                        return FReply::Handled();
                    })
                ]
            + SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
            [
                SNew(SProgressBar)
                .Visibility_Lambda([this, SegIdx]() -> EVisibility
                {
                    // 如果正在重新合成，显示进度条
                    if (RegeneratingSegments.Contains(SegIdx) && RegeneratingSegments[SegIdx])
                    {
                        return EVisibility::Visible;
                    }
                    return EVisibility::Collapsed;
                })
                                 .Percent(0.5f)  // 显示50%进度，表示正在处理
                 .FillColorAndOpacity(TTS_GREEN_COLOR)  // 使用TTS绿色，与插件主界面TTS按钮颜色一致
            ]
        ];

        RowVBox->AddSlot().AutoHeight()[ LineHBox ];

        // 添加单段时长计算器
        TSharedRef<STextBlock> SegmentTotalText = SNew(STextBlock)
            .Text_Lambda([this, SegIdx, DurationMap]() -> FText
            {
                // 计算当前段落的选中音频时长
                float SegmentAudioDuration = 0.0f;
                float SegmentSilenceDuration = 0.0f;
                
                // 获取选中的复本索引
                if (const int32* SelectedReplicate = SelectedReplicateBySegment.Find(SegIdx))
                {
                    int32 UseReplicate = *SelectedReplicate;
                    
                    // 获取选中音频的时长
                    if (const float* Duration = DurationMap->Find(FIntPoint(SegIdx, UseReplicate)))
                    {
                        SegmentAudioDuration = *Duration;
                    }
                    
                    // 获取段前静音时长
                    if (const float* Silence = SegmentPreSilenceValues.Find(SegIdx))
                    {
                        SegmentSilenceDuration += *Silence;
                    }

                    // 获取段后静音时长
                    if (const float* Silence = SegmentPostSilenceValues.Find(SegIdx))
                    {
                        SegmentSilenceDuration += *Silence;
                    }
                }
                
                float TotalSegmentDuration = SegmentAudioDuration + SegmentSilenceDuration;
                
                FString Text = FString::Printf(TEXT("段落 %d: 音频 %.1fs + 静音 %.1fs = 总计 %.1fs"), 
                    SegIdx, SegmentAudioDuration, SegmentSilenceDuration, TotalSegmentDuration);
                return FText::FromString(Text);
            })
            .Font(FCoreStyle::GetDefaultFontStyle("Normal", 10))
            .ColorAndOpacity(FLinearColor(0.0f, 0.44f, 0.88f, 1.0f));

        RowVBox->AddSlot().AutoHeight().Padding(8.f, 4.f, 8.f, 4.f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(8.f)
            .ColorAndOpacity(FLinearColor(0.98f, 0.98f, 0.98f, 1.0f))
            [
                SegmentTotalText
            ]
        ];

        VBox->AddSlot().AutoHeight().Padding(0,0,0,12)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
            .Padding(16.f)
            .ColorAndOpacity(FLinearColor(0.95f, 0.95f, 0.95f, 1.0f))
            [
                RowVBox
            ]
        ];
    }

    // 底部总时长显示和保存按钮
    TSharedRef<STextBlock> TotalText = SNew(STextBlock)
        .Text_Lambda([this]() -> FText
        {
            FString Text = FString::Printf(TEXT("已选择: %d 个音频 | 总时长: %s | 补充静音后时长: %s"), 
                SelectedReplicateBySegment.Num(), 
                *TTSPreview_FormatTotalSecondsText(TotalSelectedAudioDuration),
                *TTSPreview_FormatTotalSecondsText(TotalSelectedAudioDurationWithSilence));
            return FText::FromString(Text);
        });

    // 包裹整体+底部条
    TSharedRef<SVerticalBox> Root = SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(1.f)
        [
            SAssignNew(CurrentPreviewScrollBox, SScrollBox)
            .ScrollBarAlwaysVisible(true)
            + SScrollBox::Slot()
            [
                VBox
            ]
        ]
        // 合并预览波形区域
        + SVerticalBox::Slot().AutoHeight().Padding(8.f, 0, 8.f, 8.f)
        [
            SAssignNew(PreviewArea, SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("预览：未生成")))
                .Font(FCoreStyle::GetDefaultFontStyle("Normal", 9))
                .ColorAndOpacity(FLinearColor(0.5f,0.5f,0.5f,1.f))
                .Visibility(EVisibility::Visible)
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(8.f)
            [
                SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
            [
                TotalText
            ]

            // 升级音质选项已移至保存对话框中

            // 预览音频按钮（将所有段落按顺序合并含静音，仅用于预览）
            + SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0, 0, 0).VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("预览音频")))
                .ToolTipText(FText::FromString(TEXT("将选中高亮的音频片段（含段前/段后静音）临时合并为一条音频并波形预览，不保存资产")))
                    .OnClicked_Lambda([this, Results]() -> FReply
                    {
                        PreviewAllTTSAudio(Results);
                        return FReply::Handled();
                    })
                ]

            + SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0, 0, 0).VAlign(VAlign_Center)
                [
                    SNew(SButton)
                .Text_Lambda([this]() -> FText
                {
                    return bIsUpgrading ? FText::FromString(TEXT("升级中...")) : FText::FromString(TEXT("合并选中音频"));
                })
                .IsEnabled_Lambda([this]() -> bool
                {
                    return !bIsUpgrading;
                })
                .ToolTipText(FText::FromString(TEXT("将选中的音频保存为资产并同步到内容浏览器")))
                    .OnClicked_Lambda([this, Results]() -> FReply
                    {
                        ShowSaveDialog();
                        return FReply::Handled();
                    })
                ]
            
            
            // 音质升级进度条
            + SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0, 0, 0).VAlign(VAlign_Center)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                .Padding(0, 0, 0, 2)
                [
                    SAssignNew(UpgradeProgressText, STextBlock)
                    .Text(FText::FromString(TEXT("正在升级音质...")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Normal", 9))
                    .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
                    .Visibility_Lambda([this]() -> EVisibility
                    {
                        return bIsUpgrading ? EVisibility::Visible : EVisibility::Collapsed;
                    })
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Fill)
                [
                    SAssignNew(UpgradeProgressBar, SProgressBar)
                    .Percent(0.0f)
                    .Visibility_Lambda([this]() -> EVisibility
                    {
                        return bIsUpgrading ? EVisibility::Visible : EVisibility::Collapsed;
                    })
                ]
            ]
        ];

    Window->SetContent(Root);
    FSlateApplication::Get().AddWindow(Window);
    
    // 初始化时设置默认高亮（为所有段落设置第0个复本为默认高亮）
    for (const FTTSSegmentResult& Result : Results)
    {
        if (Result.ReplicateIndex == 0 && !SelectedReplicateBySegment.Contains(Result.SegmentIndex))
        {
            SelectedReplicateBySegment.Add(Result.SegmentIndex, 0);
        }
    }
    
    // 设置静音信息（在UI创建完成后）
    if (PreSilenceValues.Num() > 0 || PostSilenceValues.Num() > 0)
    {
        SegmentPreSilenceValues = PreSilenceValues;
        SegmentPostSilenceValues = PostSilenceValues;
        
        // 强制刷新UI以显示静音值
        if (PreviewArea.IsValid())
        {
            PreviewArea->Invalidate(EInvalidateWidget::Layout);
        }
    }
    
    // 初始化时计算默认高亮音频的时长（在设置静音信息之后）
    RecalculateSelectedAudioDuration();
    
    Window->BringToFront();
}

void UTTSPreviewManager::OnTTSAllCompletedHandler(const TArray<FTTSSegmentResult>& Results, UTTSSetting* TTSSettings)
{
    // 检查是否是重试模式
    bool bIsRetryMode = RetryingSegments.Num() > 0;
    
    if (bIsRetryMode)
    {
        // 重试模式：处理重试结果
        UE_LOG(LogTemp, Log, TEXT("检测到重试模式，处理重试结果"));
        OnRetryTTSCompleted(Results, TTSSettings);
        return;
    }
    
    // 检查是否已经有预览窗口打开
    if (CurrentTTSPreviewWindow.IsValid())
    {
        // 检查是否是重新合成模式（通过检查是否有正在重新合成的段落）
        bool bIsRegenerationMode = RegeneratingSegments.Num() > 0;
        
        if (bIsRegenerationMode)
        {
            // 重新合成模式：保存状态并更新窗口
            UE_LOG(LogTemp, Log, TEXT("检测到已有预览窗口，准备更新现有窗口（重新合成模式）"));
            
            // 设置重新合成标志
            bIsUpdatingFromRegeneration = true;
            
            // 清除所有重新合成状态
            RegeneratingSegments.Empty();
            
            // 保存当前窗口的所有状态
            SavePreviewWindowState();
            
            // 更新当前预览结果（合并新结果和旧结果）
            MergePreviewResults(Results);
            
            // 保存旧窗口的引用
            TSharedPtr<SWindow> OldWindow = CurrentTTSPreviewWindow;
            
            // 创建新窗口（使用合并后的结果）
            ShowTTSPreviewWindow(CurrentPreviewResults, TTSSettings);
            
            // 然后关闭旧窗口
            if (OldWindow.IsValid())
            {
                OldWindow->RequestDestroyWindow();
            }
            
            // 恢复窗口状态
            RestorePreviewWindowState();
            
            // 重置重新合成标志
            bIsUpdatingFromRegeneration = false;
            
            // 显示重新合成完成通知
            FNotificationInfo FinalInfo(FText::FromString(TEXT("重新合成完成！预览窗口已更新")));
            FinalInfo.bUseLargeFont = true;
            FinalInfo.ExpireDuration = 3.0f;
            FSlateNotificationManager::Get().AddNotification(FinalInfo)->SetCompletionState(SNotificationItem::CS_Success);
        }
        else
        {
            // 初始TTS合成模式：关闭旧窗口，创建新窗口，不保存状态
            UE_LOG(LogTemp, Log, TEXT("检测到已有预览窗口，关闭旧窗口并创建新窗口（初始TTS合成模式）"));
            
            // 设置重新合成标志为true，防止旧窗口关闭时清理状态
            bIsUpdatingFromRegeneration = true;
            
            // 保存旧窗口的引用
            TSharedPtr<SWindow> OldWindow = CurrentTTSPreviewWindow;
            
            // 清理所有状态（包括窗口引用，但保留OldWindow）
            ClearAllTTSPreviewState();
            
            // 创建新窗口（使用新的结果）
            ShowTTSPreviewWindow(Results, TTSSettings);
            
            // 然后关闭旧窗口
            if (OldWindow.IsValid())
            {
                OldWindow->RequestDestroyWindow();
            }
            
            // 重置重新合成标志
            bIsUpdatingFromRegeneration = false;
            
            // 显示初始合成完成通知
            FNotificationInfo FinalInfo(FText::FromString(TEXT("TTS合成完成！预览窗口已更新")));
            FinalInfo.bUseLargeFont = true;
            FinalInfo.ExpireDuration = 3.0f;
            FSlateNotificationManager::Get().AddNotification(FinalInfo)->SetCompletionState(SNotificationItem::CS_Success);
            
            // 通知TTS合成完成
            OnTTSCompleted.ExecuteIfBound();
        }
        
        return;
    }
    
    // 只有在没有预览窗口时才创建新窗口（初始TTS合成模式）
    // 确保清理任何可能残留的状态
    UE_LOG(LogTemp, Log, TEXT("初始TTS合成模式，清理旧状态"));
    ClearAllTTSPreviewState();
    
    // 验证TTS结果，检查是否有失败的语音
    TArray<FString> OriginalTexts;
    if (TTSSettings && !TTSSettings->TTSInputText.IsEmpty())
    {
        TTSSettings->TTSInputText.ParseIntoArray(OriginalTexts, TEXT("\n"), true);
    }
    
    // 解析并存储静音信息
    ParseAndStoreSilenceInfo(OriginalTexts);
    
    ValidateTTSResults(Results, OriginalTexts);
    
    // 如果有失败的语音且重试次数未达到上限，则自动重试
    if (HasFailedSegments() && TotalRetryAttempts < MaxRetryAttempts)
    {
        UE_LOG(LogTemp, Warning, TEXT("检测到失败的语音，开始自动重试（第 %d 次）"), TotalRetryAttempts + 1);
        RetryFailedTTS(OriginalTexts, TTSSettings);
        return;
    }
    
    // 显示预览窗口
    ShowTTSPreviewWindow(Results, TTSSettings, SegmentPreSilenceValues, SegmentPostSilenceValues);
    
    // 通知TTS合成完成
    OnTTSCompleted.ExecuteIfBound();
}

void UTTSPreviewManager::PreviewAllTTSAudio(const TArray<FTTSSegmentResult>& Results)
{
    // 停止当前播放的音频，避免重新装填波形时音频仍在播放
    if (PreviewAudioComponent.IsValid())
    {
        // 先清理回调，避免异步回调干扰状态重置
        PreviewAudioComponent->OnAudioPlaybackPercentNative.Clear();
        PreviewAudioComponent->OnAudioFinishedNative.Clear();
        
        PreviewAudioComponent->Stop();
        PreviewAudioComponent.Reset();
    }
    
    // 重置所有播放相关状态
    bPreviewPaused = false;
    if (PreviewPlayPercent.IsValid()) { *PreviewPlayPercent = 0.0f; }
    
    // 重置片段播放状态
    bIsPlayingSegment = false;
    CurrentPlayingSegmentIndex = -1;
    CurrentSegmentStartTime = 0.0f;
    CurrentSegmentDuration = 0.0f;
    
    // 收集文本与静音（使用当前编辑缓存）并保持界面顺序
    TArray<int32> SegmentIndices;
    for (const FTTSSegmentResult& R : Results)
    {
        if (!SegmentIndices.Contains(R.SegmentIndex))
        {
            SegmentIndices.Add(R.SegmentIndex);
        }
    }
    SegmentIndices.Sort();

    TArray<FTTSSegmentResult> OrderedResults;
    TArray<FString> Texts;
    TArray<float> PreSilences;
    TArray<float> PostSilences;

    // 只处理有高亮状态的段落
    TArray<int32> HighlightedSegments;
    
    // 收集所有有高亮状态的段落索引
    for (const auto& Pair : SelectedReplicateBySegment)
    {
        HighlightedSegments.Add(Pair.Key);
    }
    
    // 按段落索引排序，保持界面显示顺序
    HighlightedSegments.Sort();

    for (int32 SegmentIndex : HighlightedSegments)
    {
        // 获取选中的复本索引
        int32 UseReplicate = SelectedReplicateBySegment[SegmentIndex];
        
        const FTTSSegmentResult* Found = nullptr;
        for (const FTTSSegmentResult& R : Results)
        {
            if (R.SegmentIndex == SegmentIndex && R.ReplicateIndex == UseReplicate)
            {
                Found = &R; break;
            }
        }
        if (!Found)
        {
            // 退化为查找该段任意结果
            for (const FTTSSegmentResult& R : Results)
            {
                if (R.SegmentIndex == SegmentIndex) { Found = &R; break; }
            }
        }
        if (!Found) { continue; }

        OrderedResults.Add(*Found);
        FString TextToUse = EditedTextBySegment.Contains(SegmentIndex) ? *EditedTextBySegment[SegmentIndex] : Found->Text;
        Texts.Add(TextToUse);
        PreSilences.Add(SegmentPreSilenceValues.Contains(SegmentIndex) ? SegmentPreSilenceValues[SegmentIndex] : 0.0f);
        PostSilences.Add(SegmentPostSilenceValues.Contains(SegmentIndex) ? SegmentPostSilenceValues[SegmentIndex] : 0.0f);
    }

    if (OrderedResults.Num() == 0)
    {
        FNotificationInfo Info(FText::FromString(TEXT("没有选中音频片段！请先选择要预览的音频")));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        
        // 清空预览数据并更新UI显示提示信息
        PreviewSoundWave.Reset();
        PreviewPCMData.Empty();
        PreviewSampleRate = 0;
        PreviewNumChannels = 0;
        CachedPreviewWaveformSegments.Empty();
        UpdatePreviewUI();
        return;
    }

    // 合并到内存（不保存），要求使用 FromMemoryNoSave 版本以获取字节数据
    TArray<FAudioDataWrapper> AudioDataArray;
    for (const FTTSSegmentResult& R : OrderedResults)
    {
        FAudioDataWrapper W; W.AudioData = R.AudioData; AudioDataArray.Add(W);
    }

    TArray<uint8> MergedAudioData; int32 SampleRate = 0; int32 NumChannels = 0;
    const bool bOk = UAudioAssetOperation::ProcessMultipleAudioWithSilenceFromMemoryNoSave(AudioDataArray, Texts, PreSilences, PostSilences, MergedAudioData, SampleRate, NumChannels);
    if (!bOk || MergedAudioData.Num() == 0)
    {
        FNotificationInfo Info(FText::FromString(TEXT("预览合并失败！")));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }

    // 从字节创建瞬态 SoundWave，并将波形与进度嵌入原窗口
    USoundWave* TempWave = UGenTools::CreateSoundWaveFromAudioData(MergedAudioData);
    if (!TempWave)
    {
        FNotificationInfo Info(FText::FromString(TEXT("创建临时音频失败！")));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }

    PreviewSoundWave = TempWave;
    PreviewPCMData = MergedAudioData;
    PreviewSampleRate = (uint32)SampleRate;
    PreviewNumChannels = (uint16)NumChannels;
    PreviewPlayPercent = MakeShared<float>(0.0f);

    // 创建音频片段信息用于波形标记
    TArray<FAudioSegmentInfo> WaveformSegments;
    CreateWaveformSegments(OrderedResults, Texts, PreSilences, PostSilences, WaveformSegments);
    
    // 保存到缓存中
    CachedPreviewWaveformSegments = WaveformSegments;
    
    UE_LOG(LogTemp, Log, TEXT("创建了 %d 个波形片段标记"), WaveformSegments.Num());
    
    // 验证片段信息的有效性
    int32 ValidSegments = 0;
    for (int32 i = 0; i < WaveformSegments.Num(); ++i)
    {
        const FAudioSegmentInfo& Segment = WaveformSegments[i];
        
        // 检查片段信息的有效性
        bool bIsValid = !FMath::IsNaN(Segment.StartTime) && !FMath::IsNaN(Segment.EndTime) &&
                       FMath::IsFinite(Segment.StartTime) && FMath::IsFinite(Segment.EndTime) &&
                       Segment.StartTime >= 0.0f && Segment.EndTime >= Segment.StartTime &&
                       Segment.Label.Len() > 0 && Segment.Label.Len() < 1000;
        
        if (bIsValid)
        {
            ValidSegments++;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("片段[%d]: 无效数据 - StartTime=%.2f, EndTime=%.2f, LabelLen=%d"), 
                   i, Segment.StartTime, Segment.EndTime, Segment.Label.Len());
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("有效片段数量: %d/%d"), ValidSegments, WaveformSegments.Num());

    // 更新UI显示
    UpdatePreviewUI();
}

void UTTSPreviewManager::ParseAndStoreSilenceInfo(const TArray<FString>& OriginalTexts)
{
    // 清空现有的静音信息
    SegmentPreSilenceValues.Empty();
    SegmentPostSilenceValues.Empty();
    
    for (int32 i = 0; i < OriginalTexts.Num(); ++i)
    {
        const FString& TextLine = OriginalTexts[i];
        float PreSilenceValue = 0.0f;
        float PostSilenceValue = 0.0f;
        
        // 检查是否包含三列数据（用|分隔）
        TArray<FString> ColumnData;
        TextLine.ParseIntoArray(ColumnData, TEXT("|"), false);
        
        if (ColumnData.Num() >= 3)
        {
            // 解析段前静音
            if (!ColumnData[1].IsEmpty())
            {
                PreSilenceValue = FCString::Atof(*ColumnData[1]);
            }
            
            // 解析段后静音
            if (!ColumnData[2].IsEmpty())
            {
                PostSilenceValue = FCString::Atof(*ColumnData[2]);
            }
        }
        
        // 存储静音信息
        SegmentPreSilenceValues.Add(i, PreSilenceValue);
        SegmentPostSilenceValues.Add(i, PostSilenceValue);
    }
}

void UTTSPreviewManager::SetSilenceInfo(const TMap<int32, float>& PreSilenceValues, const TMap<int32, float>& PostSilenceValues)
{
    SegmentPreSilenceValues = PreSilenceValues;
    SegmentPostSilenceValues = PostSilenceValues;
}

void UTTSPreviewManager::UpdatePreviewUI()
{
    if (PreviewArea.IsValid())
    {
        PreviewArea->ClearChildren();
        
        if (!PreviewSoundWave.IsValid() || PreviewPCMData.Num() == 0 || PreviewSampleRate == 0)
        {
            // 没有预览数据时显示提示信息
            PreviewArea->AddSlot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("预览：未生成")))
                .Font(FCoreStyle::GetDefaultFontStyle("Normal", 9))
                .ColorAndOpacity(FLinearColor(0.5f,0.5f,0.5f,1.f))
                .Visibility(EVisibility::Visible)
            ];
            return;
        }

        // 有预览数据时显示波形
        PreviewArea->AddSlot().AutoHeight()
        [
            SAssignNew(PreviewInfoText, STextBlock)
            .Text(FText::FromString(FString::Printf(TEXT("预览：%0.2fs  %dHz  x%d"), PreviewSoundWave->Duration, (int32)PreviewSampleRate, (int32)PreviewNumChannels)))
            .Font(FCoreStyle::GetDefaultFontStyle("Normal", 9))
        ];
        PreviewArea->AddSlot().AutoHeight().Padding(0,4,0,4)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(8.f)
            [
                SNew(SVerticalBox)
                // 波形显示区域（固定窗口）
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SBox)
                    .HeightOverride(140.0f) // 固定高度（增加20像素用于时间刻度线）
                    .WidthOverride(800.0f)  // 固定宽度
            [
                SNew(SWaveformInteractive)
                .Samples(reinterpret_cast<const int16*>(PreviewPCMData.GetData()))
                .NumSamples(PreviewPCMData.Num() / sizeof(int16))
                .SampleRate(PreviewSampleRate)
                        .Segments(TAttribute<TArray<FAudioSegmentInfo>>::Create([this]() -> TArray<FAudioSegmentInfo>
                        {
                            // 使用缓存的波形片段信息
                            return CachedPreviewWaveformSegments;
                        }))
                        .PlayProgress(TAttribute<float>::Create([this]() -> float
                        {
                            // 如果正在播放片段，返回片段的相对进度
                            if (bIsPlayingSegment && CurrentSegmentDuration > 0.0f && PreviewPlayPercent.IsValid())
                            {
                                // 计算片段内的相对进度，然后映射到全局时间
                                float SegmentProgress = *PreviewPlayPercent; // 0.0-1.0
                                float GlobalProgress = (CurrentSegmentStartTime + SegmentProgress * CurrentSegmentDuration) / ((float)PreviewPCMData.Num() / sizeof(int16) / (float)PreviewSampleRate);
                                return FMath::Clamp(GlobalProgress, 0.0f, 1.0f);
                            }
                            // 否则返回全局播放进度
                            if (!PreviewPlayPercent.IsValid()) return 0.0f;
                            return *PreviewPlayPercent;
                        }))
                        .SelectedSegmentIndex(TAttribute<int32>::Create([this]() -> int32
                        {
                            return SelectedWaveformSegmentIndex;
                        }))
                        .HoveredSegmentIndex(TAttribute<int32>::Create([this]() -> int32
                        {
                            return HoveredWaveformSegmentIndex;
                        }))
                        .OnSegmentClicked(FOnSegmentClicked::CreateUObject(this, &UTTSPreviewManager::OnWaveformSegmentClicked))
                        .OnSegmentHovered(FOnSegmentHovered::CreateUObject(this, &UTTSPreviewManager::OnWaveformSegmentHovered))
                    ]
                ]
            ]
        ];
        PreviewArea->AddSlot().AutoHeight().Padding(0,4,0,0)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
            [
                SNew(SButton)
                .Text_Lambda([this]() -> FText
                {
                    if (PreviewAudioComponent.IsValid())
                    {
                        return bPreviewPaused ? FText::FromString(TEXT("继续")) : FText::FromString(TEXT("暂停"));
                    }
                    return FText::FromString(TEXT("播放"));
                })
                .OnClicked_Lambda([this]() -> FReply
                {
                    if (!PreviewSoundWave.IsValid()) return FReply::Handled();
                    
                    if (PreviewAudioComponent.IsValid())
                    {
                        // 如果音频组件存在，切换暂停/继续状态
                        bPreviewPaused = !bPreviewPaused;
                        PreviewAudioComponent->SetPaused(bPreviewPaused);
                    }
                    else
                    {
                        // 如果音频组件不存在，开始播放
                        if (UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
                        {
                            UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(World, PreviewSoundWave.Get(), 1.0f, 1.0f, 0.0f, nullptr, true);
                            if (Comp)
                            {
                                PreviewAudioComponent = Comp;
                                bPreviewPaused = false;
                                if (!PreviewPlayPercent.IsValid()) { PreviewPlayPercent = MakeShared<float>(0.0f); } else { *PreviewPlayPercent = 0.0f; }
                                Comp->OnAudioPlaybackPercentNative.AddLambda([this](const UAudioComponent* AC, const USoundWave* SW, const float InPercent)
                                {
                                    if (PreviewPlayPercent.IsValid()) { *PreviewPlayPercent = FMath::Clamp(InPercent, 0.0f, 1.0f); }
                                });
                                Comp->OnAudioFinishedNative.AddLambda([this](UAudioComponent* AC)
                                {
                                    if (PreviewPlayPercent.IsValid()) { *PreviewPlayPercent = 1.0f; }
                                    bPreviewPaused = false; // 播放完成后重置暂停状态
                                });
                                Comp->Play();
                            }
                        }
                    }
                    return FReply::Handled();
                })
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("重置")))
                .OnClicked_Lambda([this]() -> FReply
                {
                    // 粗暴停止：不管有没有音频在播放，都停止并重置
                    if (PreviewAudioComponent.IsValid())
                    {
                        // 先清理回调，避免异步回调干扰状态重置
                        PreviewAudioComponent->OnAudioPlaybackPercentNative.Clear();
                        PreviewAudioComponent->OnAudioFinishedNative.Clear();
                        
                        PreviewAudioComponent->Stop();
                        PreviewAudioComponent.Reset();
                    }
                    
                    // 重置所有播放相关状态
                    bPreviewPaused = false; // 停止时重置暂停状态
                    if (PreviewPlayPercent.IsValid()) { *PreviewPlayPercent = 0.0f; }
                    
                    // 重置片段播放状态
                    bIsPlayingSegment = false;
                    CurrentPlayingSegmentIndex = -1;
                    CurrentSegmentStartTime = 0.0f;
                    CurrentSegmentDuration = 0.0f;
                    
                    return FReply::Handled();
                })
            ]
        ];
    }
}

// 音频处理相关
void UTTSPreviewManager::ProcessAndSaveSingleAudio(const FTTSSegmentResult& Result, const FString& Text, float PreSilence, float PostSilence, bool bStereo)
{
    // 直接处理内存中的音频数据，不保存原始资产
    USoundWave* OutProcessedAudio = nullptr;
    bool bSuccess = false;
    
    if (Result.AudioData.Num() > 0)
    {
        // 从内存中的音频数据直接处理
        bSuccess = UAudioAssetOperation::ProcessSingleAudioWithSilenceFromMemory(Result.AudioData, Text, PreSilence, PostSilence, OutProcessedAudio, bStereo);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("音频数据为空，无法处理"));
        return;
    }
    
    if (bSuccess && OutProcessedAudio)
    {
        // 确保资产已经完全加载和注册
        OutProcessedAudio->AddToRoot();
        FAssetRegistryModule::AssetCreated(OutProcessedAudio);
        Sleep(1);
        // 同步到内容浏览器
        FString ProcessedAssetPath = OutProcessedAudio->GetPathName();
        UGenTools::SyncToAsset({ProcessedAssetPath});
        
        FString NotificationText = bStereo ? TEXT("单个音频处理完成（双声道）！") : TEXT("单个音频处理完成！");
        FNotificationInfo Info(FText::FromString(NotificationText));
        Info.bUseLargeFont = false;
        Info.ExpireDuration = 3.0f;
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Success);
        
        UE_LOG(LogTemp, Log, TEXT("单个音频处理完成 - 文本: %s, 段前静音: %.2f秒, 段后静音: %.2f秒, 双声道: %s"), 
               *Text, PreSilence, PostSilence, bStereo ? TEXT("是") : TEXT("否"));
        UE_LOG(LogTemp, Log, TEXT("单个音频资产路径: %s"), *ProcessedAssetPath);
    }
    else
    {
        FNotificationInfo Info(FText::FromString(TEXT("单个音频处理失败！")));
        Info.bUseLargeFont = false;
        Info.ExpireDuration = 3.0f;
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
    }
}

void UTTSPreviewManager::ProcessAndSaveMergedAudio(const TArray<FTTSSegmentResult>& Results, const TArray<FString>& Texts, const TArray<float>& PreSilences, const TArray<float>& PostSilences, const FString& CustomFileName, bool bStereo)
{
    // 直接处理内存中的音频数据，不保存原始资产
    USoundWave* OutMergedAudio = nullptr;
    bool bSuccess = false;
    
    // 收集音频数据
    TArray<FAudioDataWrapper> AudioDataArray;
    TArray<FString> FilePaths;
    TArray<FString> AssetPaths;
    
    for (const FTTSSegmentResult& Result : Results)
    {
        if (Result.AudioData.Num() > 0)
        {
            FAudioDataWrapper Wrapper;
            Wrapper.AudioData = Result.AudioData;
            AudioDataArray.Add(Wrapper);
            FilePaths.Add(TEXT(""));  // 内存数据
            AssetPaths.Add(TEXT("")); // 内存数据
        }
        // 移除WavFilePath分支，只使用内存数据或资产路径
        else
        {
            FAudioDataWrapper Wrapper;
            Wrapper.AudioData.Empty();
            AudioDataArray.Add(Wrapper);  // 空数据
            FilePaths.Add(TEXT(""));              // 空路径
            AssetPaths.Add(Result.AssetPath);     // 资产路径
        }
    }
    
    // 检查是否有有效的音频数据
    bool bHasValidAudioData = false;
    for (const FAudioDataWrapper& Wrapper : AudioDataArray)
    {
        if (Wrapper.AudioData.Num() > 0)
        {
            bHasValidAudioData = true;
            break;
        }
    }
    
    if (bHasValidAudioData)
    {
        // 所有音频都在内存中，使用内存处理方法
        bSuccess = UAudioAssetOperation::ProcessMultipleAudioWithSilenceFromMemory(AudioDataArray, Texts, PreSilences, PostSilences, OutMergedAudio, CustomFileName, bStereo);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("没有有效的音频数据，无法处理"));
        return;
    }
    
    if (bSuccess && OutMergedAudio)
    {
        // 确保资产已经完全加载和注册
        OutMergedAudio->AddToRoot();
        FAssetRegistryModule::AssetCreated(OutMergedAudio);
        
        // 等待资产完全注册到资产注册表，并强制刷新
        Sleep(0.2f);
        
        // 强制刷新资产注册表，确保资产被正确识别
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        AssetRegistryModule.Get().ScanPathsSynchronous({OutMergedAudio->GetPackage()->GetPathName()});
        
        // 再次等待以确保扫描完成
        Sleep(0.1f);
        
        // 同步到内容浏览器
        FString MergedAssetPath = OutMergedAudio->GetPathName();
        UGenTools::SyncToAsset({MergedAssetPath});
        
        FString NotificationText;
        if (!CustomFileName.IsEmpty())
        {
            NotificationText = FString::Printf(TEXT("音频已保存: %s%s"), *CustomFileName, bStereo ? TEXT("（双声道）") : TEXT(""));
        }
        else
        {
            NotificationText = FString::Printf(TEXT("已合并 %d 个音频片段%s！"), Results.Num(), bStereo ? TEXT("（双声道）") : TEXT(""));
        }
        
        FNotificationInfo Info(FText::FromString(NotificationText));
        Info.bUseLargeFont = !CustomFileName.IsEmpty();
        Info.ExpireDuration = 3.0f;
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Success);
        
        UE_LOG(LogTemp, Log, TEXT("音频合并完成 - 合并了 %d 个音频片段"), Results.Num());
        UE_LOG(LogTemp, Log, TEXT("合并音频资产路径: %s"), *MergedAssetPath);
        UE_LOG(LogTemp, Log, TEXT("最终合并顺序（按界面显示顺序）："));
        for (int32 i = 0; i < Texts.Num(); ++i)
        {
            float PreSilence = (i < PreSilences.Num()) ? PreSilences[i] : 0.0f;
            float PostSilence = (i < PostSilences.Num()) ? PostSilences[i] : 0.0f;
            UE_LOG(LogTemp, Log, TEXT("  [%d] 段落索引: %d, 文本: %s, 段前静音: %.2f秒, 段后静音: %.2f秒"), 
                   i, Results[i].SegmentIndex, *Texts[i], PreSilence, PostSilence);
        }
    }
    else
    {
        FNotificationInfo Info(FText::FromString(TEXT("音频合并失败！")));
        Info.bUseLargeFont = false;
        Info.ExpireDuration = 3.0f;
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
    }
}

void UTTSPreviewManager::ProcessAndSaveMergedAudioWithUpgrade(const TArray<FTTSSegmentResult>& Results, const TArray<FString>& Texts, const TArray<float>& PreSilences, const TArray<float>& PostSilences, const FString& CustomFileName, bool bStereo)
{
    // 设置升级状态
    bIsUpgrading = true;
    
    // 更新进度条
    if (UpgradeProgressBar.IsValid())
    {
        UpgradeProgressBar->SetPercent(0.1f);
    }
    if (UpgradeProgressText.IsValid())
    {
        UpgradeProgressText->SetText(FText::FromString(TEXT("正在合并音频...")));
    }
    
    // 第一步：先合并音频（添加静音）
    USoundWave* OutMergedAudio = nullptr;
    bool bMergeSuccess = false;
    
    // 收集音频数据
    TArray<FAudioDataWrapper> AudioDataArray;
    TArray<FString> FilePaths;
    TArray<FString> AssetPaths;
    
    for (const FTTSSegmentResult& Result : Results)
    {
        if (Result.AudioData.Num() > 0)
        {
            FAudioDataWrapper Wrapper;
            Wrapper.AudioData = Result.AudioData;
            AudioDataArray.Add(Wrapper);
            FilePaths.Add(TEXT(""));  // 内存数据
            AssetPaths.Add(TEXT("")); // 内存数据
        }
        // 移除WavFilePath分支，只使用内存数据或资产路径
        else
        {
            FAudioDataWrapper Wrapper;
            Wrapper.AudioData.Empty();
            AudioDataArray.Add(Wrapper);  // 空数据
            FilePaths.Add(TEXT(""));              // 空路径
            AssetPaths.Add(Result.AssetPath);     // 资产路径
        }
    }
    
    // 根据数据类型选择处理方法进行合并（不保存资产，只获取音频数据）
    TArray<uint8> MergedAudioData;
    int32 OutSampleRate = 0;
    int32 OutNumChannels = 0;
    
    if (AudioDataArray[0].AudioData.Num() > 0)
    {
        // 所有音频都在内存中，使用内存处理方法（不保存资产）
        bMergeSuccess = UAudioAssetOperation::ProcessMultipleAudioWithSilenceFromMemoryNoSave(AudioDataArray, Texts, PreSilences, PostSilences, MergedAudioData, OutSampleRate, OutNumChannels);
    }
    else
    {
        // 对于文件和资产，暂时不支持，显示错误
        FNotificationInfo Info(FText::FromString(TEXT("音质升级功能目前只支持内存中的音频数据！")));
        Info.bUseLargeFont = false;
        Info.ExpireDuration = 3.0f;
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }
    
    if (!bMergeSuccess || MergedAudioData.Num() == 0)
    {
        FNotificationInfo Info(FText::FromString(TEXT("音频合并失败！")));
        Info.bUseLargeFont = false;
        Info.ExpireDuration = 3.0f;
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("成功合并音频数据，大小: %d 字节"), MergedAudioData.Num());
    
    // 更新进度条
    if (UpgradeProgressBar.IsValid())
    {
        UpgradeProgressBar->SetPercent(0.3f);
    }
    if (UpgradeProgressText.IsValid())
    {
        UpgradeProgressText->SetText(FText::FromString(TEXT("正在升级音质...")));
    }
    
    // 第三步：升级音质
    FString EnhancedAssetPath = FString::Printf(TEXT("/Game/TTS/Asset/TTS_Merged_%s_Enhanced"), 
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
    
    UTTSServer::EnhanceAudioFromMemory(MergedAudioData, EnhancedAssetPath, 
        [this, Results, CustomFileName, bStereo](bool bSuccess, const TArray<uint8>& EnhancedAudioData)
            {
                if (bSuccess && EnhancedAudioData.Num() > 0)
                {
                    // 第四步：保存升级后的音频
                    USoundWave* EnhancedSoundWave = UGenTools::CreateSoundWaveFromAudioData(EnhancedAudioData, bStereo);
                    if (EnhancedSoundWave)
                    {
                        // 生成最终的资产路径
                        FString AssetName;
                        if (!CustomFileName.IsEmpty())
                        {
                            AssetName = CustomFileName;
                        }
                        else
                        {
                            AssetName = FString::Printf(TEXT("TTS_Merged_%s_Enhanced"), 
                                *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
                        }
                        
                        // 使用正确的存储路径
                        const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
                        FString BasePath = Settings->TTSStoragePath;
                        FString AssetPathDir, WavDir;
                        if (!UGenTools::SetupTTSStoragePaths(BasePath, AssetPathDir, WavDir))
                        {
                            UE_LOG(LogTemp, Error, TEXT("设置TTS存储路径失败"));
                            FNotificationInfo Info(FText::FromString(TEXT("设置存储路径失败！")));
                            Info.bUseLargeFont = false;
                            Info.ExpireDuration = 3.0f;
                            FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
                            return;
                        }
                        
                        // 直接保存为资产，让SaveAudioAsSoundWaveAsset处理双声道转换
                        bool bSaveSuccess = UGenTools::SaveAudioAsSoundWaveAsset(EnhancedAudioData, AssetPathDir, AssetName, true, bStereo);
                        
                        if (bSaveSuccess)
                        {
                            // 确保资产已经完全加载和注册
                            EnhancedSoundWave->AddToRoot();
                            FAssetRegistryModule::AssetCreated(EnhancedSoundWave);
                            
                            Sleep(1);
                            // 同步到内容浏览器
                            FString FinalAssetPathName = EnhancedSoundWave->GetPathName();
                            UGenTools::SyncToAsset({FinalAssetPathName});
                            
                            // 更新进度条为完成状态
                            if (UpgradeProgressBar.IsValid())
                            {
                                UpgradeProgressBar->SetPercent(1.0f);
                            }
                            if (UpgradeProgressText.IsValid())
                            {
                                UpgradeProgressText->SetText(FText::FromString(TEXT("升级完成！")));
                            }
                            
                            FString NotificationText;
                            if (!CustomFileName.IsEmpty())
                            {
                                NotificationText = FString::Printf(TEXT("音频已保存: %s%s"), *AssetName, bStereo ? TEXT("（双声道）") : TEXT(""));
                            }
                            else
                            {
                                NotificationText = FString::Printf(TEXT("已合并并升级 %d 个音频片段%s！"), Results.Num(), bStereo ? TEXT("（双声道）") : TEXT(""));
                            }
                            
                            FNotificationInfo Info(FText::FromString(NotificationText));
                            Info.bUseLargeFont = !CustomFileName.IsEmpty();
                            Info.ExpireDuration = 3.0f;
                            FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Success);
                            
                            UE_LOG(LogTemp, Log, TEXT("音频合并并升级完成 - 处理了 %d 个音频片段"), Results.Num());
                            UE_LOG(LogTemp, Log, TEXT("最终合并顺序（按界面显示顺序）："));
                            for (int32 i = 0; i < Results.Num(); ++i)
                            {
                                UE_LOG(LogTemp, Log, TEXT("  [%d] 段落索引: %d, 复本索引: %d"), 
                                       i, Results[i].SegmentIndex, Results[i].ReplicateIndex);
                            }
                            
                            // 延迟重置升级状态
                            if (UWorld* World = GEditor->GetEditorWorldContext().World())
                            {
                                if (FTimerManager* TimerManager = &World->GetTimerManager())
                                {
                                    FTimerHandle ResetTimerHandle;
                                    TimerManager->SetTimer(ResetTimerHandle, [this]()
                                    {
                                        bIsUpgrading = false;
                                        if (UpgradeProgressBar.IsValid())
                                        {
                                            UpgradeProgressBar->SetPercent(0.0f);
                                        }
                                        if (UpgradeProgressText.IsValid())
                                        {
                                            UpgradeProgressText->SetText(FText::FromString(TEXT("正在升级音质...")));
                                        }
                                    }, 1.0f, false);
                                }
                            }
                            UE_LOG(LogTemp, Log, TEXT("最终音频资产路径: %s"), *FinalAssetPathName);
                        }
                        else
                        {
                            bIsUpgrading = false;
                            FNotificationInfo Info(FText::FromString(TEXT("升级后音频保存失败！")));
                            Info.bUseLargeFont = false;
                            Info.ExpireDuration = 3.0f;
                            FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
                        }
                    }
                    else
                    {
                        bIsUpgrading = false;
                        FNotificationInfo Info(FText::FromString(TEXT("创建升级音频对象失败！")));
                        Info.bUseLargeFont = false;
                        Info.ExpireDuration = 3.0f;
                        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
                    }
                }
                else
                {
                    bIsUpgrading = false;
                    FNotificationInfo Info(FText::FromString(TEXT("音频升级失败！")));
                    Info.bUseLargeFont = false;
                    Info.ExpireDuration = 3.0f;
                    FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
                }
            });
}

void UTTSPreviewManager::SaveSelectedTTSAudio(const TArray<FTTSSegmentResult>& Results, const FString& CustomFileName, bool bStereo)
{
    if (SelectedReplicateBySegment.Num() == 0)
    {
        FNotificationInfo Info(FText::FromString(TEXT("请先选择要保存的音频！")));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }

    TArray<FTTSSegmentResult> SelectedResults;
    TArray<FString> SelectedTexts;
    TArray<float> PreSilences;
    TArray<float> PostSilences;
    
    // 按照段落索引顺序收集选中的音频结果和静音设置
    TArray<int32> SelectedSegmentIndices;
    for (const auto& Pair : SelectedReplicateBySegment)
    {
        SelectedSegmentIndices.Add(Pair.Key);
    }
    SelectedSegmentIndices.Sort(); // 按段落索引排序
    
    // 按照排序后的段落索引顺序收集数据
    for (int32 SegmentIndex : SelectedSegmentIndices)
    {
        int32 ReplicateIndex = SelectedReplicateBySegment[SegmentIndex];
        
        // 查找对应的结果
        for (const FTTSSegmentResult& Result : Results)
        {
            if (Result.SegmentIndex == SegmentIndex && Result.ReplicateIndex == ReplicateIndex)
            {
                SelectedResults.Add(Result);
                
                // 获取编辑后的文本（如果有的话）
                FString TextToUse = Result.Text;
                if (EditedTextBySegment.Contains(SegmentIndex))
                {
                    TextToUse = *EditedTextBySegment[SegmentIndex];
                }
                SelectedTexts.Add(TextToUse);
                
                // 获取静音设置
                float PreSilence = SegmentPreSilenceValues.Contains(SegmentIndex) ? SegmentPreSilenceValues[SegmentIndex] : 0.0f;
                float PostSilence = SegmentPostSilenceValues.Contains(SegmentIndex) ? SegmentPostSilenceValues[SegmentIndex] : 0.0f;
                PreSilences.Add(PreSilence);
                PostSilences.Add(PostSilence);
                
                UE_LOG(LogTemp, Log, TEXT("按显示顺序收集音频 - 段落索引: %d, 复本索引: %d, 文本: %s"), 
                       SegmentIndex, ReplicateIndex, *TextToUse);
                
                break;
            }
        }
    }
    
    if (SelectedResults.Num() == 0)
    {
        FNotificationInfo Info(FText::FromString(TEXT("没有找到选中的音频！")));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }
    
    // 根据是否勾选升级音质进行不同的处理
    if (bGlobalUpgradeEnabled)
    {
        // 勾选了升级音质：先合并音频，然后整体升级音质，最后保存
        ProcessAndSaveMergedAudioWithUpgrade(SelectedResults, SelectedTexts, PreSilences, PostSilences, CustomFileName, bStereo);
    }
    else
    {
        // 未勾选升级音质：直接保存（原有逻辑）
        if (SelectedResults.Num() == 1)
        {
            // 单个音频：只添加静音，不合并
            ProcessAndSaveSingleAudio(SelectedResults[0], SelectedTexts[0], PreSilences[0], PostSilences[0], bStereo);
        }
        else
        {
            // 多个音频：添加静音并合并
            // 直接调用合并音频函数，支持自定义文件名
            ProcessAndSaveMergedAudio(SelectedResults, SelectedTexts, PreSilences, PostSilences, CustomFileName, bStereo);
        }
    }
}

// 重新合成功能
void UTTSPreviewManager::RegenerateTTSForSegment(int32 SegmentIndex, const FString& Text, const FString& SpeakerName, const FString& EmotionName, float Speed, float Pitch)
{
    // 清除相关段落的采样率缓存，因为音频将重新生成
    for (auto It = CachedAudioSampleRates.CreateIterator(); It; ++It)
    {
        if (It.Key().X == SegmentIndex)
        {
            It.RemoveCurrent();
        }
    }
    
    // 清除相关段落的哈希值缓存
    for (auto It = CachedAudioDataHashes.CreateIterator(); It; ++It)
    {
        if (It.Key().X == SegmentIndex)
        {
            It.RemoveCurrent();
        }
    }
    
    // 暂时移除音频替换功能，只保留状态管理
    UE_LOG(LogTemp, Log, TEXT("重新合成功能已暂时禁用音频替换"));

    // 创建临时的TTS设置对象
    UTTSSetting* TempSettings = NewObject<UTTSSetting>(GetTransientPackage());
    
    // 应用新的设置
    TempSettings->SpeakerName = SpeakerName;
    TempSettings->EmotionName = EmotionName;
    TempSettings->AudioSpeed = Speed;
    TempSettings->AudioPitch = Pitch;
    
    // 重要：设置要合成的文本为当前编辑的文本，而不是原始文本
    TempSettings->TTSInputText = Text;
    
    // 显示重新合成开始的通知
    FNotificationInfo StartInfo(FText::FromString(FString::Printf(TEXT("正在重新合成第 %d 段文本..."), SegmentIndex)));
    FSlateNotificationManager::Get().AddNotification(StartInfo);
    
    // 获取插件设置
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    
    // 路径准备
    FString BasePath = Settings->TTSStoragePath;
    FString AssetPath, WavDir;
    if (!UGenTools::SetupTTSStoragePaths(BasePath, AssetPath, WavDir))
    {
        FNotificationInfo ErrorInfo(FText::FromString(TEXT("设置TTS存储路径失败！")));
        FSlateNotificationManager::Get().AddNotification(ErrorInfo)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }
    
    // 创建重新合成专用的参数
    TSharedPtr<int32> CompletedCount = MakeShared<int32>(0);
    TSharedPtr<int32> TotalCount = MakeShared<int32>(Settings->TTSReplicatesPerSegment);
    TSharedPtr<TArray<FString>> CompletedAssetPaths = MakeShared<TArray<FString>>();
    TSharedPtr<TArray<FTTSSegmentResult>> SegmentResults = MakeShared<TArray<FTTSSegmentResult>>();
    
    // 创建重新合成进度跟踪
    TSharedPtr<TSet<int32>> CompletedReplicates = MakeShared<TSet<int32>>();
    
    // 针对每段文本进行多次合成
    for (int32 ReplicateIndex = 0; ReplicateIndex < Settings->TTSReplicatesPerSegment; ++ReplicateIndex)
    {
        FTTSRequestParams RequestParams(TempSettings, Settings, SegmentIndex, AssetPath, WavDir, 
                                       CompletedCount, TotalCount, CompletedAssetPaths, SegmentResults, ReplicateIndex);
        UTTSServer::SendTTSRequest(RequestParams);
    }
    
    // 重新合成完成后会自动触发OnTTSAllCompletedHandler
    // 该函数会检测到已有预览窗口，然后关闭并重新创建窗口
}

void UTTSPreviewManager::OnEnhanceAudioCompleted(bool bSuccess, const FString& OriginalAssetPath, const FString& EnhancedAssetPath)
{
    // 音频增强完成回调 - 主要用于处理从资产路径升级的情况
    // 从内存升级的情况现在通过新的回调函数直接处理
    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("音频增强成功 - 原始: %s, 增强: %s"), *OriginalAssetPath, *EnhancedAssetPath);
        
        // 查找并更新对应的音频结果
        bool bFound = false;
        int32 FoundSegmentIndex = -1;
        int32 FoundReplicateIndex = -1;
        
        // 从资产路径升级的情况：通过原始资产路径查找
        for (FTTSSegmentResult& Result : CurrentPreviewResults)
        {
            if (Result.AssetPath == OriginalAssetPath)
            {
                // 更新为增强后的音频路径
                Result.AssetPath = EnhancedAssetPath;
                bFound = true;
                FoundSegmentIndex = Result.SegmentIndex;
                FoundReplicateIndex = Result.ReplicateIndex;
                UE_LOG(LogTemp, Log, TEXT("从资产路径升级：已更新音频结果 - 段落: %d, 复本: %d"), Result.SegmentIndex, Result.ReplicateIndex);
                break;
            }
        }
        
        if (bFound)
        {
            // 清除对应音频的升级状态（已移除EnhancingAudio状态管理）
            UE_LOG(LogTemp, Log, TEXT("音频升级完成 - 段落: %d, 复本: %d"), FoundSegmentIndex, FoundReplicateIndex);
            
            // 使用与重新合成相同的安全更新逻辑
            // 检查是否已经有预览窗口打开
            if (CurrentTTSPreviewWindow.IsValid())
            {
                UE_LOG(LogTemp, Log, TEXT("检测到已有预览窗口，准备更新现有窗口（音频升级模式）"));
                
                // 设置重新合成标志
                bIsUpdatingFromRegeneration = true;
                
                // 保存当前窗口的所有状态
                SavePreviewWindowState();
                
                // 保存旧窗口的引用
                TSharedPtr<SWindow> OldWindow = CurrentTTSPreviewWindow;
                
                // 创建新窗口（使用更新后的结果）
                ShowTTSPreviewWindow(CurrentPreviewResults, nullptr);
                
                // 然后关闭旧窗口
                if (OldWindow.IsValid())
                {
                    OldWindow->RequestDestroyWindow();
                }
                
                // 恢复窗口状态
                RestorePreviewWindowState();
                
                // 重置重新合成标志
                bIsUpdatingFromRegeneration = false;
                
                UE_LOG(LogTemp, Log, TEXT("音频升级UI更新完成"));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("没有检测到预览窗口，跳过UI更新"));
            }
            
            // 显示成功通知
            FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("音频升级成功！已保存到: %s"), *FPaths::GetBaseFilename(EnhancedAssetPath))));
            Info.bUseLargeFont = false;
            Info.ExpireDuration = 3.0f;
            FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Success);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("未找到对应的音频结果进行更新"));
            FNotificationInfo Info(FText::FromString(TEXT("音频升级成功，但未找到对应音频进行更新")));
            Info.bUseLargeFont = false;
            Info.ExpireDuration = 3.0f;
            FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("音频增强失败 - 原始: %s"), *OriginalAssetPath);
        
        // 显示失败通知
        FNotificationInfo Info(FText::FromString(TEXT("音频升级失败，请重试")));
        Info.bUseLargeFont = false;
        Info.ExpireDuration = 3.0f;
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
    }
}

// 状态管理
void UTTSPreviewManager::SavePreviewWindowState()
{
    // 保存预览窗口的所有状态
    // 注意：EditedTextBySegment, SegmentSpeakerSelections, SegmentEmotionSelections, 
    // SegmentSpeedValues, SegmentPitchValues, SelectedReplicateBySegment 等状态
    // 已经在类的成员变量中保存，不需要额外保存
    
    UE_LOG(LogTemp, Log, TEXT("保存预览窗口状态完成"));
    UE_LOG(LogTemp, Log, TEXT("已保存 %d 个段落的文本编辑状态"), EditedTextBySegment.Num());
    UE_LOG(LogTemp, Log, TEXT("已保存 %d 个段落的音色选择状态"), SegmentSpeakerSelections.Num());
    UE_LOG(LogTemp, Log, TEXT("已保存 %d 个段落的情感选择状态"), SegmentEmotionSelections.Num());
    UE_LOG(LogTemp, Log, TEXT("已保存 %d 个段落的音频选择状态"), SelectedReplicateBySegment.Num());
}

void UTTSPreviewManager::MergePreviewResults(const TArray<FTTSSegmentResult>& NewResults)
{
    // 合并新的音频结果到现有结果中
    // 策略：对于重新合成的段落，用新结果替换旧结果；对于其他段落，保持不变
    
    // 找出重新合成的段落索引
    TSet<int32> RegeneratedSegments;
    for (const FTTSSegmentResult& NewResult : NewResults)
    {
        RegeneratedSegments.Add(NewResult.SegmentIndex);
    }
    
    // 保留非重新合成段落的结果
    TArray<FTTSSegmentResult> MergedResults;
    for (const FTTSSegmentResult& OldResult : CurrentPreviewResults)
    {
        if (!RegeneratedSegments.Contains(OldResult.SegmentIndex))
        {
            MergedResults.Add(OldResult);
        }
    }
    
    // 添加新结果
    MergedResults.Append(NewResults);
    
    // 按段落索引和复本索引排序
    MergedResults.Sort([](const FTTSSegmentResult& A, const FTTSSegmentResult& B)
    {
        if (A.SegmentIndex != B.SegmentIndex)
        {
            return A.SegmentIndex < B.SegmentIndex;
        }
        return A.ReplicateIndex < B.ReplicateIndex;
    });
    
    // 更新当前预览结果
    CurrentPreviewResults = MergedResults;
    
    UE_LOG(LogTemp, Log, TEXT("合并预览结果完成"));
    UE_LOG(LogTemp, Log, TEXT("原有结果数量: %d"), CurrentPreviewResults.Num() - NewResults.Num());
    UE_LOG(LogTemp, Log, TEXT("新结果数量: %d"), NewResults.Num());
    UE_LOG(LogTemp, Log, TEXT("合并后总数量: %d"), CurrentPreviewResults.Num());
    
    // 清除重新合成段落的音频选择状态（因为音频已更新）
    for (int32 SegmentIndex : RegeneratedSegments)
    {
        SelectedReplicateBySegment.Remove(SegmentIndex);
        UE_LOG(LogTemp, Log, TEXT("已清除段落 %d 的音频选择状态"), SegmentIndex);
    }
}

void UTTSPreviewManager::RestorePreviewWindowState()
{
    // 恢复预览窗口的状态
    // 注意：由于我们在ShowTTSPreviewWindow中使用了Lambda表达式来绑定状态，
    // 这些状态会自动从类的成员变量中恢复，所以这里主要是日志记录
    
    UE_LOG(LogTemp, Log, TEXT("恢复预览窗口状态完成"));
    UE_LOG(LogTemp, Log, TEXT("已恢复 %d 个段落的文本编辑状态"), EditedTextBySegment.Num());
    UE_LOG(LogTemp, Log, TEXT("已恢复 %d 个段落的音色选择状态"), SegmentSpeakerSelections.Num());
    UE_LOG(LogTemp, Log, TEXT("已恢复 %d 个段落的情感选择状态"), SegmentEmotionSelections.Num());
    UE_LOG(LogTemp, Log, TEXT("已恢复 %d 个段落的音频选择状态"), SelectedReplicateBySegment.Num());
    
    // 重新计算选中音频的总时长
    RecalculateSelectedAudioDuration();
}

void UTTSPreviewManager::ClearAllTTSPreviewState()
{
    UE_LOG(LogTemp, Log, TEXT("开始清理所有TTS预览状态"));
    
    // 清理所有状态
    SelectedReplicateBySegment.Empty();
    EditedTextBySegment.Empty();
    SegmentSpeakerSelections.Empty();
    SegmentEmotionSelections.Empty();
    SegmentSpeedValues.Empty();
    SegmentPitchValues.Empty();
    SegmentPreSilenceValues.Empty();
    SegmentPostSilenceValues.Empty();
    RegeneratingSegments.Empty();
    TotalSelectedAudioDuration = 0.0f;
    TotalSelectedAudioDurationWithSilence = 0.0f;
    
    // 清理重试状态
    ClearRetryState();
    
    // 清理采样率缓存
    CachedAudioSampleRates.Empty();
    
    // 清理哈希值缓存
    CachedAudioDataHashes.Empty();
    
    // 清理窗口引用
    CurrentTTSPreviewWindow.Reset();
    CurrentPreviewVBox.Reset();
    CurrentPreviewScrollBox.Reset();
    CurrentPreviewResults.Empty();
    
    // 停止所有正在播放的音频
    for (auto& Pair : PlayingComponents)
    {
        if (Pair.Value.IsValid())
        {
            Pair.Value->Stop();
        }
    }
    PlayingComponents.Empty();
    
    UE_LOG(LogTemp, Log, TEXT("TTS预览状态清理完成"));
}

// 重试机制
void UTTSPreviewManager::ValidateTTSResults(const TArray<FTTSSegmentResult>& Results, const TArray<FString>& OriginalTexts)
{
    UE_LOG(LogTemp, Log, TEXT("开始验证TTS结果"));
    
    // 清空之前的失败记录
    FailedSegments.Empty();
    
    // 获取插件设置
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    const int32 ReplicatesPerSegment = Settings->TTSReplicatesPerSegment;
    
    // 按段落索引分组结果
    TMap<int32, TArray<const FTTSSegmentResult*>> SegmentToResults;
    for (const FTTSSegmentResult& Result : Results)
    {
        SegmentToResults.FindOrAdd(Result.SegmentIndex).Add(&Result);
    }
    
    // 检查每个段落是否有足够的音频结果
    for (int32 SegmentIndex = 0; SegmentIndex < OriginalTexts.Num(); ++SegmentIndex)
    {
        if (OriginalTexts[SegmentIndex].TrimStartAndEnd().IsEmpty())
        {
            continue; // 跳过空文本
        }
        
        TArray<int32> FailedReplicates;
        
        // 检查该段落是否有足够的复本
        if (const TArray<const FTTSSegmentResult*>* SegmentResults = SegmentToResults.Find(SegmentIndex))
        {
            // 统计该段落有多少个有效的复本
            int32 ValidReplicateCount = 0;
            
            // 检查每个复本是否有效
            for (int32 ReplicateIndex = 0; ReplicateIndex < ReplicatesPerSegment; ++ReplicateIndex)
            {
                bool bFoundValidResult = false;
                
                for (const FTTSSegmentResult* Result : *SegmentResults)
                {
                    if (Result->ReplicateIndex == ReplicateIndex)
                    {
                        // 检查音频数据是否有效
                        bool bHasValidAudio = false;
                        
                        if (Result->AudioData.Num() > 0)
                        {
                            // 检查音频数据大小（至少应该有WAV文件头）
                            if (Result->AudioData.Num() >= 44)
                            {
                                // 检查WAV文件头
                                if (Result->AudioData[0] == 'R' && Result->AudioData[1] == 'I' && 
                                    Result->AudioData[2] == 'F' && Result->AudioData[3] == 'F' &&
                                    Result->AudioData[8] == 'W' && Result->AudioData[9] == 'A' && 
                                    Result->AudioData[10] == 'V' && Result->AudioData[11] == 'E')
                                {
                                    bHasValidAudio = true;
                                }
                            }
                        }
                        
                        if (bHasValidAudio)
                        {
                            bFoundValidResult = true;
                            ValidReplicateCount++;
                            break;
                        }
                    }
                }
                
                if (!bFoundValidResult)
                {
                    FailedReplicates.Add(ReplicateIndex);
                    UE_LOG(LogTemp, Warning, TEXT("段落 %d 的复本 %d 音频无效"), SegmentIndex, ReplicateIndex);
                }
            }
            
            // 只有当该段落完全没有有效音频时才需要重试
            if (ValidReplicateCount == 0)
            {
                FailedSegments.Add(SegmentIndex, FailedReplicates);
                UE_LOG(LogTemp, Warning, TEXT("段落 %d 完全没有有效音频，需要重试"), SegmentIndex);
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("段落 %d 有 %d 个有效音频，无需重试"), SegmentIndex, ValidReplicateCount);
            }
        }
        else
        {
            // 整个段落都没有结果
            for (int32 ReplicateIndex = 0; ReplicateIndex < ReplicatesPerSegment; ++ReplicateIndex)
            {
                FailedReplicates.Add(ReplicateIndex);
            }
            FailedSegments.Add(SegmentIndex, FailedReplicates);
            UE_LOG(LogTemp, Warning, TEXT("段落 %d 完全没有音频结果，需要重试"), SegmentIndex);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("TTS结果验证完成，发现 %d 个段落有失败的语音"), FailedSegments.Num());
}

void UTTSPreviewManager::RetryFailedTTS(const TArray<FString>& OriginalTexts, UTTSSetting* TTSSettings)
{
    if (FailedSegments.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("没有失败的语音需要重试"));
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("开始重试失败的TTS语音"));
    
    // 增加重试次数
    TotalRetryAttempts++;
    
    // 设置重试状态
    for (const auto& Pair : FailedSegments)
    {
        RetryingSegments.Add(Pair.Key, true);
    }
    
    // 显示重试通知
    FNotificationInfo RetryInfo(FText::FromString(FString::Printf(TEXT("检测到 %d 个段落完全没有语音，正在重试（第 %d 次）..."), 
        FailedSegments.Num(), TotalRetryAttempts)));
    RetryInfo.bUseLargeFont = true;
    RetryInfo.ExpireDuration = 5.0f;
    FSlateNotificationManager::Get().AddNotification(RetryInfo);
    
    // 获取插件设置
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    
    // 路径准备
    FString BasePath = Settings->TTSStoragePath;
    FString AssetPath, WavDir;
    if (!UGenTools::SetupTTSStoragePaths(BasePath, AssetPath, WavDir))
    {
        FNotificationInfo ErrorInfo(FText::FromString(TEXT("设置TTS存储路径失败！")));
        FSlateNotificationManager::Get().AddNotification(ErrorInfo)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }
    
    // 创建重试专用的参数
    TSharedPtr<int32> CompletedCount = MakeShared<int32>(0);
    TSharedPtr<int32> TotalCount = MakeShared<int32>(0);
    TSharedPtr<TArray<FString>> CompletedAssetPaths = MakeShared<TArray<FString>>();
    TSharedPtr<TArray<FTTSSegmentResult>> SegmentResults = MakeShared<TArray<FTTSSegmentResult>>();
    
    // 计算需要重试的总数
    for (const auto& Pair : FailedSegments)
    {
        *TotalCount += Pair.Value.Num();
    }
    
    // 为每个失败的段落发送重试请求（重试所有复本，因为该段落完全没有有效音频）
    for (const auto& Pair : FailedSegments)
    {
        int32 SegmentIndex = Pair.Key;
        
        if (SegmentIndex >= OriginalTexts.Num())
        {
            UE_LOG(LogTemp, Error, TEXT("段落索引 %d 超出原始文本范围"), SegmentIndex);
            continue;
        }
        
        FString TextToRetry = OriginalTexts[SegmentIndex].TrimStartAndEnd();
        if (TextToRetry.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("段落 %d 的文本为空，跳过重试"), SegmentIndex);
            continue;
        }
        
        // 创建临时的TTS设置对象
        UTTSSetting* TempSettings = NewObject<UTTSSetting>(GetTransientPackage());
        TempSettings->SpeakerName = TTSSettings->SpeakerName;
        TempSettings->EmotionName = TTSSettings->EmotionName;
        TempSettings->AudioSpeed = TTSSettings->AudioSpeed;
        TempSettings->AudioPitch = TTSSettings->AudioPitch;
        TempSettings->VoiceAudio = TTSSettings->VoiceAudio;
        TempSettings->EmotionAudio = TTSSettings->EmotionAudio;
        TempSettings->VoiceAudioFileName = TTSSettings->VoiceAudioFileName;
        TempSettings->CustomEmotionName = TTSSettings->CustomEmotionName;
        TempSettings->VoiceAudioBytes = TTSSettings->VoiceAudioBytes;
        TempSettings->EmotionAudioBytes = TTSSettings->EmotionAudioBytes;
        TempSettings->bUseCustomVoiceAudio = TTSSettings->bUseCustomVoiceAudio;
        TempSettings->bUseCustomEmotionAudio = TTSSettings->bUseCustomEmotionAudio;
        TempSettings->TTSInputText = TextToRetry;
        
        // 重试该段落的所有复本（因为该段落完全没有有效音频）
        for (int32 ReplicateIndex = 0; ReplicateIndex < Settings->TTSReplicatesPerSegment; ++ReplicateIndex)
        {
            FTTSRequestParams RequestParams(TempSettings, Settings, SegmentIndex, AssetPath, WavDir, 
                                           CompletedCount, TotalCount, CompletedAssetPaths, SegmentResults, ReplicateIndex);
            UTTSServer::SendTTSRequest(RequestParams);
            
            UE_LOG(LogTemp, Log, TEXT("发送重试请求 - 段落: %d, 复本: %d, 文本: %s"), 
                   SegmentIndex, ReplicateIndex, *TextToRetry);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("重试请求已发送，等待结果"));
}

void UTTSPreviewManager::OnRetryTTSCompleted(const TArray<FTTSSegmentResult>& Results, UTTSSetting* TTSSettings)
{
    UE_LOG(LogTemp, Log, TEXT("重试TTS完成，处理结果"));
    
    // 清除重试状态
    RetryingSegments.Empty();
    
    // 验证重试结果
    TArray<FString> OriginalTexts;
    if (TTSSettings && !TTSSettings->TTSInputText.IsEmpty())
    {
        TTSSettings->TTSInputText.ParseIntoArray(OriginalTexts, TEXT("\n"), true);
    }
    
    ValidateTTSResults(Results, OriginalTexts);
    
    // 检查是否还有失败的语音
    if (FailedSegments.Num() > 0 && TotalRetryAttempts < MaxRetryAttempts)
    {
        UE_LOG(LogTemp, Warning, TEXT("重试后仍有失败的语音，继续重试（第 %d 次）"), TotalRetryAttempts + 1);
        RetryFailedTTS(OriginalTexts, TTSSettings);
        return;
    }
    
    // 重试完成，显示结果
    if (FailedSegments.Num() > 0)
    {
        // 达到最大重试次数，显示警告
        FNotificationInfo WarningInfo(FText::FromString(FString::Printf(TEXT("重试 %d 次后仍有 %d 个段落完全没有语音，请检查网络连接或稍后重试"), 
            MaxRetryAttempts, FailedSegments.Num())));
        WarningInfo.bUseLargeFont = true;
        WarningInfo.ExpireDuration = 8.0f;
        FSlateNotificationManager::Get().AddNotification(WarningInfo)->SetCompletionState(SNotificationItem::CS_Fail);
    }
    else
    {
        // 重试成功
        FNotificationInfo SuccessInfo(FText::FromString(FString::Printf(TEXT("重试成功！所有段落都有语音了（共重试 %d 次）"), TotalRetryAttempts)));
        SuccessInfo.bUseLargeFont = true;
        SuccessInfo.ExpireDuration = 5.0f;
        FSlateNotificationManager::Get().AddNotification(SuccessInfo)->SetCompletionState(SNotificationItem::CS_Success);
    }
    
    // 显示预览窗口
    ShowTTSPreviewWindow(Results, TTSSettings);
    
    // 通知TTS合成完成
    OnTTSCompleted.ExecuteIfBound();
}

bool UTTSPreviewManager::HasFailedSegments() const
{
    return FailedSegments.Num() > 0;
}

void UTTSPreviewManager::ClearRetryState()
{
    FailedSegments.Empty();
    RetryingSegments.Empty();
    TotalRetryAttempts = 0;
    UE_LOG(LogTemp, Log, TEXT("重试状态已清除"));
}

// 工具函数
void UTTSPreviewManager::RecalculateSelectedAudioDuration()
{
    // 重新计算选中音频的总时长
    float Sum = 0.0f;
    float SumWithSilence = 0.0f;
    
    // 计算所有选中段落的时长
    for (const TPair<int32,int32>& P : SelectedReplicateBySegment)
    {
        int32 SegmentIndex = P.Key;
        int32 ReplicateIndex = P.Value;
        
        // 获取静音设置
        float PreSilence = SegmentPreSilenceValues.Contains(SegmentIndex) ? SegmentPreSilenceValues[SegmentIndex] : 0.0f;
        float PostSilence = SegmentPostSilenceValues.Contains(SegmentIndex) ? SegmentPostSilenceValues[SegmentIndex] : 0.0f;
        
        // 查找对应的音频结果
        for (const FTTSSegmentResult& Result : CurrentPreviewResults)
        {
            if (Result.SegmentIndex == SegmentIndex && Result.ReplicateIndex == ReplicateIndex)
            {
                // 从内存中的音频数据或WAV文件获取时长
                float Duration = 0.0f;
                
                // 从内存中的音频数据创建SoundWave获取时长
                if (Result.AudioData.Num() > 0)
                {
                    if (USoundWave* TempWave = UGenTools::CreateSoundWaveFromAudioData(Result.AudioData))
                    {
                        Duration = TempWave->GetDuration();
                    }
                }
                
                if (Duration > 0.0f && FMath::IsFinite(Duration))
                {
                    Sum += Duration;
                    SumWithSilence += Duration + PreSilence + PostSilence;
                }
                break;
            }
        }
    }
    
    TotalSelectedAudioDuration = Sum;
    TotalSelectedAudioDurationWithSilence = SumWithSilence;
}

void UTTSPreviewManager::PrefillAudioSampleRateCache()
{
    // 预填充所有音频的采样率缓存，避免UI更新时频繁检查
    for (const FTTSSegmentResult& Result : CurrentPreviewResults)
    {
        FIntPoint AudioKey(Result.SegmentIndex, Result.ReplicateIndex);
        
        // 计算当前音频数据的哈希值，用于检测数据是否变化
        uint32 CurrentDataHash = 0;
        if (Result.AudioData.Num() > 0)
        {
            // 使用音频数据的前100字节计算简单哈希
            int32 HashSize = FMath::Min(100, Result.AudioData.Num());
            for (int32 i = 0; i < HashSize; i++)
            {
                CurrentDataHash = ((CurrentDataHash << 5) + CurrentDataHash) + Result.AudioData[i];
            }
            CurrentDataHash += Result.AudioData.Num(); // 加入长度信息
        }
        // 移除WavFilePath分支，只使用内存数据或资产路径
        else if (!Result.AssetPath.IsEmpty())
        {
            // 使用资产路径作为哈希
            CurrentDataHash = FCrc::StrCrc32(*Result.AssetPath);
        }
        
        // 检查缓存中的哈希值是否匹配
        if (CachedAudioDataHashes.Contains(AudioKey) && CachedAudioSampleRates.Contains(AudioKey))
        {
            uint32 CachedHash = CachedAudioDataHashes[AudioKey];
            if (CachedHash == CurrentDataHash)
            {
                // 数据没有变化，跳过重新计算
                continue;
            }
        }
        
        // 数据有变化或缓存未命中，重新计算采样率
        uint32 SampleRate = 0;
        
        // 检查内存中的音频数据
        if (Result.AudioData.Num() > 0)
        {
            // 从内存中的音频数据创建临时SoundWave检查采样率
            if (USoundWave* TempWave = UGenTools::CreateSoundWaveFromAudioData(Result.AudioData))
            {
                TArray<uint8> DummyData;
                uint16 OutChannels = 0;
                uint32 OutSampleRate = 0;
                if (TempWave->GetImportedSoundWaveData(DummyData, OutSampleRate, OutChannels))
                {
                    SampleRate = OutSampleRate;
                }
                
                // 清理临时对象
                TempWave->MarkAsGarbage();
                TempWave->ConditionalBeginDestroy();
            }
        }
        // 如果都没有，检查资产路径
        else if (!Result.AssetPath.IsEmpty())
        {
            USoundWave* SoundWave = Cast<USoundWave>(StaticLoadObject(USoundWave::StaticClass(), nullptr, *Result.AssetPath));
            if (SoundWave)
            {
                TArray<uint8> DummyData;
                uint16 OutChannels = 0;
                uint32 OutSampleRate = 0;
                if (SoundWave->GetImportedSoundWaveData(DummyData, OutSampleRate, OutChannels))
                {
                    SampleRate = OutSampleRate;
                }
            }
        }
        
        // 更新缓存
        if (SampleRate > 0)
        {
            CachedAudioSampleRates.Add(AudioKey, SampleRate);
            CachedAudioDataHashes.Add(AudioKey, CurrentDataHash);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("预填充采样率缓存完成，共缓存 %d 个音频的采样率"), CachedAudioSampleRates.Num());
}

void UTTSPreviewManager::CreateWaveformSegments(const TArray<FTTSSegmentResult>& Results, const TArray<FString>& Texts, const TArray<float>& PreSilences, const TArray<float>& PostSilences, TArray<FAudioSegmentInfo>& OutSegments)
{
    OutSegments.Empty();
    
    if (Results.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("CreateWaveformSegments: No results provided"));
        return;
    }
    
    if (PreviewSampleRate == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("CreateWaveformSegments: Invalid sample rate: %d"), PreviewSampleRate);
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("CreateWaveformSegments: Processing %d results with sample rate %d"), Results.Num(), PreviewSampleRate);
    
    // 计算总音频时长，用于确定最小显示阈值
    float TotalAudioDuration = 0.0f;
    for (int32 i = 0; i < Results.Num(); ++i)
    {
        const FTTSSegmentResult& Result = Results[i];
        float AudioDuration = 0.0f;
        
        if (Result.AudioData.Num() > 0)
        {
            int32 NumSamples = Result.AudioData.Num() / sizeof(int16);
            AudioDuration = (float)NumSamples / (float)PreviewSampleRate;
        }
        else if (!Result.AssetPath.IsEmpty())
        {
            USoundWave* SoundWave = Cast<USoundWave>(StaticLoadObject(USoundWave::StaticClass(), nullptr, *Result.AssetPath));
            if (SoundWave)
            {
                AudioDuration = SoundWave->GetDuration();
            }
        }
        
        TotalAudioDuration += AudioDuration;
    }
    
    // 根据总时长和窗口宽度计算最小显示阈值
    // 动态计算最小显示阈值，确保文本有足够空间显示
    const float WindowWidth = 800.0f; // 波形控件默认宽度
    const float MinTextWidth = 80.0f; // 最小文本宽度（包含"前静音"或"后静音"）
    const float MinTextWidthShort = 40.0f; // 短文本最小宽度（只包含时长）
    
    // 计算最小静音时长阈值
    const float MinSilenceRatio = MinTextWidth / WindowWidth;
    const float MinSilenceDuration = TotalAudioDuration * MinSilenceRatio;
    
    // 计算短文本阈值（用于只显示时长的情况）
    const float MinSilenceRatioShort = MinTextWidthShort / WindowWidth;
    const float MinSilenceDurationShort = TotalAudioDuration * MinSilenceRatioShort;
    
    UE_LOG(LogTemp, Log, TEXT("CreateWaveformSegments: Total duration=%.2fs, Min silence duration=%.2fs, Min short duration=%.2fs"), 
           TotalAudioDuration, MinSilenceDuration, MinSilenceDurationShort);
    
    float CurrentTime = 0.0f;
    
    for (int32 i = 0; i < Results.Num(); ++i)
    {
        const FTTSSegmentResult& Result = Results[i];
        const FString& Text = (i < Texts.Num()) ? Texts[i] : Result.Text;
        float PreSilence = (i < PreSilences.Num()) ? PreSilences[i] : 0.0f;
        float PostSilence = (i < PostSilences.Num()) ? PostSilences[i] : 0.0f;
        
        // 验证输入数据的有效性
        if (FMath::IsNaN(PreSilence) || FMath::IsNaN(PostSilence) || PreSilence < 0.0f || PostSilence < 0.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("CreateWaveformSegments: Invalid silence values for segment %d - PreSilence=%.2f, PostSilence=%.2f"), i, PreSilence, PostSilence);
            PreSilence = FMath::Max(0.0f, PreSilence);
            PostSilence = FMath::Max(0.0f, PostSilence);
        }
        
        // 计算音频时长
        float AudioDuration = 0.0f;
        if (Result.AudioData.Num() > 0)
        {
            // 从内存中的音频数据计算时长
            // 假设是16位PCM数据，需要除以2得到样本数
            int32 NumSamples = Result.AudioData.Num() / sizeof(int16);
            AudioDuration = (float)NumSamples / (float)PreviewSampleRate;
        }
        else if (!Result.AssetPath.IsEmpty())
        {
            // 从资产路径计算时长
            USoundWave* SoundWave = Cast<USoundWave>(StaticLoadObject(USoundWave::StaticClass(), nullptr, *Result.AssetPath));
            if (SoundWave)
            {
                AudioDuration = SoundWave->GetDuration();
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("片段[%d]: 无法获取音频数据 - AudioData.Num()=%d, AssetPath=%s"), 
                   i, Result.AudioData.Num(), *Result.AssetPath);
        }
        
        // 添加段前静音区域
        if (PreSilence > 0.0f)
        {
            FString PreSilenceLabel;
            if (PreSilence >= MinSilenceDuration)
            {
                // 静音时长足够，显示完整标签
                PreSilenceLabel = FString::Printf(TEXT("前静音[%.1fs]"), PreSilence);
            }
            else if (PreSilence >= MinSilenceDurationShort)
            {
                // 静音时长中等，只显示时长
                PreSilenceLabel = FString::Printf(TEXT("[%.1fs]"), PreSilence);
            }
            else
            {
                // 静音时长太短，不显示文本（只显示标记线）
                PreSilenceLabel = TEXT("");
            }
            
            FAudioSegmentInfo PreSilenceSegment(
                CurrentTime,
                CurrentTime + PreSilence,
                PreSilenceLabel,
                FLinearColor(0.7f, 0.7f, 0.7f, 0.8f),
                true
            );
            OutSegments.Add(PreSilenceSegment);
            CurrentTime += PreSilence;
        }
        
        // 添加音频内容区域
        if (AudioDuration > 0.0f)
        {
            FString SegmentLabel;
            if (!Text.IsEmpty())
            {
                // 截取文本前15个字符作为标签，并清理特殊字符
                FString CleanText = Text;
                CleanText = CleanText.Replace(TEXT("\n"), TEXT(" "));
                CleanText = CleanText.Replace(TEXT("\r"), TEXT(" "));
                CleanText = CleanText.Replace(TEXT("\t"), TEXT(" "));
                
                FString ShortText = CleanText.Len() > 15 ? CleanText.Left(15) + TEXT("...") : CleanText;
                SegmentLabel = FString::Printf(TEXT("%s[%.1fs]"), *ShortText, AudioDuration);
            }
            else
            {
                SegmentLabel = FString::Printf(TEXT("段落%d[%.1fs]"), i, AudioDuration);
            }
            
            FAudioSegmentInfo AudioSegment(
                CurrentTime,
                CurrentTime + AudioDuration,
                SegmentLabel,
                FLinearColor(0.0f, 0.6f, 1.0f, 0.8f), // 蓝色，与波形颜色一致
                false
            );
            OutSegments.Add(AudioSegment);
            CurrentTime += AudioDuration;
        }
        
        // 添加段后静音区域
        if (PostSilence > 0.0f)
        {
            FString PostSilenceLabel;
            if (PostSilence >= MinSilenceDuration)
            {
                // 静音时长足够，显示完整标签
                PostSilenceLabel = FString::Printf(TEXT("后静音[%.1fs]"), PostSilence);
            }
            else if (PostSilence >= MinSilenceDurationShort)
            {
                // 静音时长中等，只显示时长
                PostSilenceLabel = FString::Printf(TEXT("[%.1fs]"), PostSilence);
            }
            else
            {
                // 静音时长太短，不显示文本（只显示标记线）
                PostSilenceLabel = TEXT("");
            }
            
            FAudioSegmentInfo PostSilenceSegment(
                CurrentTime,
                CurrentTime + PostSilence,
                PostSilenceLabel,
                FLinearColor(0.7f, 0.7f, 0.7f, 0.8f),
                true
            );
            OutSegments.Add(PostSilenceSegment);
            CurrentTime += PostSilence;
        }
    }
    
    // 验证片段信息的有效性
    for (int32 i = 0; i < OutSegments.Num(); ++i)
    {
        const FAudioSegmentInfo& Segment = OutSegments[i];
        if (FMath::IsNaN(Segment.StartTime) || FMath::IsNaN(Segment.EndTime) || 
            Segment.StartTime < 0.0f || Segment.EndTime < Segment.StartTime)
        {
            UE_LOG(LogTemp, Error, TEXT("无效的片段信息[%d]: StartTime=%.2f, EndTime=%.2f"), 
                   i, Segment.StartTime, Segment.EndTime);
        }
    }
}

bool UTTSPreviewManager::IsTTSPreviewWindowOpen() const
{
    return CurrentTTSPreviewWindow.IsValid();
}

void UTTSPreviewManager::OnWaveformSegmentClicked(int32 SegmentIndex)
{
    if (SegmentIndex >= 0 && SegmentIndex < CachedPreviewWaveformSegments.Num())
    {
        SelectedWaveformSegmentIndex = SegmentIndex;
        
        const FAudioSegmentInfo& Segment = CachedPreviewWaveformSegments[SegmentIndex];
        
        // 如果点击的不是静音区域，则播放该片段
        if (!Segment.bIsSilence)
        {
            PlayWaveformSegment(SegmentIndex);
        }
        
        // 不需要调用UpdatePreviewUI()，因为SelectedSegmentIndex是通过TAttribute绑定的
        // 控件会自动更新显示状态，保持缩放和平移状态
    }
}

void UTTSPreviewManager::OnWaveformSegmentHovered(int32 SegmentIndex, bool bIsHovered)
{
    if (bIsHovered)
    {
        HoveredWaveformSegmentIndex = SegmentIndex;
        if (SegmentIndex >= 0 && SegmentIndex < CachedPreviewWaveformSegments.Num())
        {
            // 悬停逻辑（无需日志输出）
        }
    }
    else
    {
        HoveredWaveformSegmentIndex = -1;
    }
}

void UTTSPreviewManager::PlayWaveformSegment(int32 SegmentIndex)
{
    if (SegmentIndex < 0 || SegmentIndex >= CachedPreviewWaveformSegments.Num())
    {
        return;
    }
    
    const FAudioSegmentInfo& Segment = CachedPreviewWaveformSegments[SegmentIndex];
    
    // 如果是静音区域，不播放
    if (Segment.bIsSilence)
    {
        return;
    }
    
    // 检查是否是重复点击相同的片段
    if (CurrentPlayingSegmentIndex == SegmentIndex && PreviewAudioComponent.IsValid())
    {
        // 如果是重复点击相同的片段，切换播放/暂停状态
        if (!bPreviewPaused)
        {
            // 当前正在播放，暂停播放
            PreviewAudioComponent->SetPaused(true);
            bPreviewPaused = true;
        }
        else
        {
            // 当前已暂停，继续播放
            PreviewAudioComponent->SetPaused(false);
            bPreviewPaused = false;
        }
        return;
    }
    
    // 先设置新的片段播放状态，避免停止播放时的状态重置
    CurrentPlayingSegmentIndex = SegmentIndex;
    CurrentSegmentStartTime = Segment.StartTime;
    CurrentSegmentDuration = Segment.EndTime - Segment.StartTime;
    bIsPlayingSegment = true;
    
    // 停止当前播放（在设置新状态之后）
    if (PreviewAudioComponent.IsValid() && PreviewAudioComponent->IsPlaying())
    {
        PreviewAudioComponent->Stop();
    }
    
    // 计算片段在合并音频中的时间范围
    float SegmentStartTime = Segment.StartTime;
    float SegmentEndTime = Segment.EndTime;
    
    // 如果有合并的音频数据，播放对应的时间段
    if (PreviewPCMData.Num() > 0 && PreviewSampleRate > 0)
    {
        // 计算片段的开始和结束样本位置
        int32 StartSample = FMath::RoundToInt(SegmentStartTime * PreviewSampleRate);
        int32 EndSample = FMath::RoundToInt(SegmentEndTime * PreviewSampleRate);
        
        // 确保样本位置在有效范围内
        int32 MaxSamples = PreviewPCMData.Num() / sizeof(int16);
        StartSample = FMath::Clamp(StartSample, 0, MaxSamples - 1);
        EndSample = FMath::Clamp(EndSample, StartSample, MaxSamples - 1);
        
        // 创建片段音频数据
        int32 SegmentSampleCount = EndSample - StartSample;
        TArray<uint8> SegmentAudioData;
        SegmentAudioData.SetNum(SegmentSampleCount * sizeof(int16));
        
        // 复制音频数据
        const int16* SourceSamples = reinterpret_cast<const int16*>(PreviewPCMData.GetData());
        int16* DestSamples = reinterpret_cast<int16*>(SegmentAudioData.GetData());
        FMemory::Memcpy(DestSamples, &SourceSamples[StartSample], SegmentSampleCount * sizeof(int16));
        
        // 创建临时SoundWave并播放
        if (SegmentAudioData.Num() > 0)
        {
            USoundWave* SegmentSoundWave = NewObject<USoundWave>();
            if (SegmentSoundWave)
            {
                // 设置音频数据
                SegmentSoundWave->RawPCMDataSize = SegmentAudioData.Num();
                SegmentSoundWave->RawPCMData = static_cast<uint8*>(FMemory::Malloc(SegmentAudioData.Num()));
                FMemory::Memcpy(SegmentSoundWave->RawPCMData, SegmentAudioData.GetData(), SegmentAudioData.Num());
                
                // 设置音频属性
                SegmentSoundWave->Duration = SegmentEndTime - SegmentStartTime;
                SegmentSoundWave->SetSampleRate(PreviewSampleRate);
                SegmentSoundWave->NumChannels = 1; // 假设是单声道
                
                // 音频数据已设置
                
                // 播放音频
                if (UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr)
                {
                    UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(World, SegmentSoundWave, 1.0f, 1.0f, 0.0f, nullptr, true);
                    if (Comp)
                    {
                        PreviewAudioComponent = Comp;
                        
                        // 设置播放进度回调
                        if (!PreviewPlayPercent.IsValid()) 
                        { 
                            PreviewPlayPercent = MakeShared<float>(0.0f); 
                        } 
                        else 
                        { 
                            *PreviewPlayPercent = 0.0f; 
                        }
                        
                        Comp->OnAudioPlaybackPercentNative.AddLambda([this](const UAudioComponent* AC, const USoundWave* SW, const float InPercent)
                        {
                            if (PreviewPlayPercent.IsValid()) 
                            { 
                                *PreviewPlayPercent = FMath::Clamp(InPercent, 0.0f, 1.0f); 
                            }
                        });
                        
                        Comp->OnAudioFinishedNative.AddLambda([this, SegmentIndex](UAudioComponent* AC)
                        {
                            // 只有在播放的是当前片段时才处理播放完成
                            if (CurrentPlayingSegmentIndex == SegmentIndex)
                            {
                                // 片段播放完成，进度条应该停留在片段的末尾
                                // 计算片段末尾在全局时间中的位置
                                if (PreviewPlayPercent.IsValid() && CurrentSegmentDuration > 0.0f)
                                {
                                    float TotalDuration = (float)PreviewPCMData.Num() / sizeof(int16) / (float)PreviewSampleRate;
                                    float SegmentEndProgress = (CurrentSegmentStartTime + CurrentSegmentDuration) / TotalDuration;
                                    *PreviewPlayPercent = FMath::Clamp(SegmentEndProgress, 0.0f, 1.0f);
                                }
                                
                                // 重置片段播放状态
                                bIsPlayingSegment = false;
                                CurrentPlayingSegmentIndex = -1;
                                CurrentSegmentStartTime = 0.0f;
                                CurrentSegmentDuration = 0.0f;
                            }
                        });
                        
                        Comp->Play();
                    }
                }
            }
        }
    }
}

void UTTSPreviewManager::SaveWaveformZoomState(float ZoomLevel, float PanOffset)
{
    SavedZoomLevel = ZoomLevel;
    SavedPanOffset = PanOffset;
    bHasSavedZoomState = true;
}

void UTTSPreviewManager::RestoreWaveformZoomState()
{
    if (bHasSavedZoomState)
    {
        // 这里需要在控件创建后设置状态
        // 由于控件是动态创建的，我们需要在创建后立即设置状态
        // 这个函数将在UpdatePreviewUI中被调用
    }
}

// 新的保存对话框相关方法实现
void UTTSPreviewManager::ShowSaveDialog()
{
    // 检查是否有选中的音频
    if (SelectedReplicateBySegment.Num() == 0)
    {
        FNotificationInfo Info(FText::FromString(TEXT("请先选择要保存的音频！")));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }

    // 生成默认文件名
    FString DefaultFileName = TEXT("TTS_Audio_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));

    // 创建美观的保存对话框
    TSharedRef<SWindow> DialogWindow = SNew(SWindow)
        .Title(FText::FromString(TEXT("保存TTS音频")))
        .ClientSize(FVector2D(450, 220))
        .SupportsMaximize(false)
        .SupportsMinimize(false)
        .SizingRule(ESizingRule::FixedSize)
        .IsTopmostWindow(true);

    TSharedPtr<SEditableTextBox> FileNameTextBox;
    TSharedPtr<SCheckBox> UpgradeCheckBox;
    TSharedPtr<SCheckBox> StereoCheckBox;

    DialogWindow->SetContent(
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(24.0f)
        [
            SNew(SVerticalBox)
            // 文件名输入区域
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 12)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("文件名")))
                .Font(FCoreStyle::GetDefaultFontStyle("Normal", 10))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 20)
            [
                SAssignNew(FileNameTextBox, SEditableTextBox)
                .Text(FText::FromString(TEXT("")))
                .HintText(FText::FromString(DefaultFileName))
                .Font(FCoreStyle::GetDefaultFontStyle("Normal", 10))
                .Padding(FMargin(8, 6))
            ]
            
            // 选项区域
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 12)
            [
                SAssignNew(UpgradeCheckBox, SCheckBox)
                .Content()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("升级音质")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Normal", 9))
                    .ColorAndOpacity(FSlateColor::UseForeground())
                ]
                .ToolTipText(FText::FromString(TEXT("将音频质量提升至48kHz，提供更好的音质效果")))
            ]
            
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 24)
            [
                SAssignNew(StereoCheckBox, SCheckBox)
                .Content()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("立体音")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Normal", 9))
                    .ColorAndOpacity(FSlateColor::UseForeground())
                ]
                .ToolTipText(FText::FromString(TEXT("保存为双声道音频")))
            ]
            
            // 按钮区域
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNullWidget::NullWidget
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(8, 0, 0, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("取消")))
                    .ButtonStyle(FAppStyle::Get(), "Button")
                    .OnClicked_Lambda([DialogWindow]()
                    {
                        DialogWindow->RequestDestroyWindow();
                        return FReply::Handled();
                    })
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(8, 0, 0, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("保存")))
                    .ButtonStyle(FAppStyle::Get(), "PrimaryButton")
                    .OnClicked_Lambda([this, DialogWindow, FileNameTextBox, UpgradeCheckBox, StereoCheckBox, DefaultFileName]()
                    {
                        FString FileName = FileNameTextBox->GetText().ToString();
                        bool bUpgrade = (UpgradeCheckBox->IsChecked());
                        bool bStereo = (StereoCheckBox->IsChecked());
                        
                        // 如果用户没有输入任何内容，使用默认文件名
                        if (FileName.IsEmpty())
                        {
                            FileName = DefaultFileName;
                        }
                        
                        // 清理文件名
                        FileName = FileName.Replace(TEXT(" "), TEXT("_"));
                        FileName = FileName.Replace(TEXT("/"), TEXT("_"));
                        FileName = FileName.Replace(TEXT("\\"), TEXT("_"));
                        FileName = FileName.Replace(TEXT(":"), TEXT("_"));
                        FileName = FileName.Replace(TEXT("*"), TEXT("_"));
                        FileName = FileName.Replace(TEXT("?"), TEXT("_"));
                        FileName = FileName.Replace(TEXT("\""), TEXT("_"));
                        FileName = FileName.Replace(TEXT("<"), TEXT("_"));
                        FileName = FileName.Replace(TEXT(">"), TEXT("_"));
                        FileName = FileName.Replace(TEXT("|"), TEXT("_"));
                        
                        // 检查文件是否已存在
                        if (CheckFileExistsAndConfirmOverwrite(FileName, DialogWindow, bUpgrade, bStereo))
                        {
                            // 用户确认覆盖，执行保存
                            OnSaveDialogConfirmed(FileName, bUpgrade, bStereo);
                            DialogWindow->RequestDestroyWindow();
                        }
                        // 如果用户取消覆盖，保持对话框打开
                        
                        return FReply::Handled();
                    })
                ]
            ]
        ]
    );

    FSlateApplication::Get().AddModalWindow(DialogWindow, nullptr);
}

void UTTSPreviewManager::OnSaveDialogConfirmed(const FString& FileName, bool bUpgradeQuality, bool bStereo)
{
    // 临时保存当前状态
    bool bOriginalUpgradeEnabled = bGlobalUpgradeEnabled;
    
    // 设置升级音质选项
    bGlobalUpgradeEnabled = bUpgradeQuality;
    
    // 直接调用 SaveSelectedTTSAudio，传入自定义文件名和双声道选项
    SaveSelectedTTSAudio(CurrentPreviewResults, FileName, bStereo);
    
    // 恢复原始状态
    bGlobalUpgradeEnabled = bOriginalUpgradeEnabled;
}

void UTTSPreviewManager::OnSaveDialogCancelled()
{
    UE_LOG(LogTemp, Log, TEXT("用户取消了保存操作"));
}

bool UTTSPreviewManager::CheckFileExistsAndConfirmOverwrite(const FString& FileName, TSharedPtr<SWindow> DialogWindow, bool bUpgrade, bool bStereo)
{
    // 使用GenTools中的函数检查资产是否已存在
    if (UGenTools::CheckAudioAssetExists(FileName))
    {
        // 文件已存在，显示覆盖确认对话框
        TSharedRef<SWindow> OverwriteDialog = SNew(SWindow)
            .Title(FText::FromString(TEXT("文件已存在")))
            .ClientSize(FVector2D(400, 150))
            .SupportsMaximize(false)
            .SupportsMinimize(false)
            .SizingRule(ESizingRule::FixedSize)
            .IsTopmostWindow(true);
        
        OverwriteDialog->SetContent(
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(24.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 16)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("文件 \"%s\" 已存在，是否要覆盖？"), *FileName)))
                    .Font(FCoreStyle::GetDefaultFontStyle("Normal", 10))
                    .ColorAndOpacity(FSlateColor::UseForeground())
                    .AutoWrapText(true)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    [
                        SNullWidget::NullWidget
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(8, 0, 0, 0)
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("取消")))
                        .ButtonStyle(FAppStyle::Get(), "Button")
                        .OnClicked_Lambda([OverwriteDialog]()
                        {
                            OverwriteDialog->RequestDestroyWindow();
                            return FReply::Handled();
                        })
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(8, 0, 0, 0)
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("覆盖")))
                        .ButtonStyle(FAppStyle::Get(), "PrimaryButton")
                        .OnClicked_Lambda([this, OverwriteDialog, DialogWindow, FileName, bUpgrade, bStereo]()
                        {
                            OverwriteDialog->RequestDestroyWindow();
                            // 用户确认覆盖，执行保存
                            OnSaveDialogConfirmed(FileName, bUpgrade, bStereo);
                            DialogWindow->RequestDestroyWindow();
                            return FReply::Handled();
                        })
                    ]
                ]
            ]
        );
        
        FSlateApplication::Get().AddModalWindow(OverwriteDialog, DialogWindow);
        return false; // 返回false，因为保存操作在确认对话框中处理
    }
    
    // 文件不存在，可以直接保存
    return true;
}


#undef LOCTEXT_NAMESPACE


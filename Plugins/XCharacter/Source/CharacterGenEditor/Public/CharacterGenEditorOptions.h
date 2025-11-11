// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Input/SComboBox.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#include "MotionGen/MoGenSetting.h"
#include "A2F/A2FSetting.h"
#include "TTS/TTSSetting.h"
#include "TTS/AudioAssetOperationSetting.h"
#include "CharAssetProcessor/Public/NpcBpAssemblerSettings.h"
#include "MotionGen/MoGenStruct.h"
#include "CharacterGenEditorManager.h"
#include "XVCPluginSettings.h"
#include "TTS/TTSServer.h"
#include "A2F/A2FService.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Editor.h"
#include "TimerManager.h"
#include "Sound/SoundWave.h"

// 定义视图枚举
enum class EViewType : uint8
{
    MainMenu,       // 主菜单（显示三个按钮）
    MotionGen,      // 动作生成视图
    TTS,            // TTS视图
    AudioOperation,  // 音频操作视图
	NpcBpAssembler // NPC蓝图装配器试图
};

/**
 * 
 */
class SCharacterGenEditorOptions : public SCompoundWidget
{

public:
	SLATE_BEGIN_ARGS(SCharacterGenEditorOptions)
		: _ImportSettings(nullptr)
		, _A2FSettings(nullptr)
		, _TTSSettings(nullptr)
		, _AudioAssetOperationSettings(nullptr)
		, _NpcBpAssemblerSettings(nullptr)
		, _WidgetWindow()

		{}

		SLATE_ARGUMENT(UMoGenSetting*, ImportSettings)
		SLATE_ARGUMENT(UA2FSetting*, A2FSettings)
		SLATE_ARGUMENT(UTTSSetting*, TTSSettings)
		SLATE_ARGUMENT(UAudioAssetOperationSetting*, AudioAssetOperationSettings)
		SLATE_ARGUMENT(UNpcBpAssemblerSettings*, NpcBpAssemblerSettings)
		SLATE_ARGUMENT(TWeakPtr<SWindow>, WidgetWindow)
		SLATE_ARGUMENT(UCharacterGenEditorManager*, ManagerRef)


	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual ~SCharacterGenEditorOptions();

	 // 嵌入式内容浏览器相关
    TSharedPtr<SWidget> ContentBrowserWidget;

	UCharacterGenEditorManager* ManagerRef;

	// 视图切换相关方法
	TSharedRef<SWidget> CreateMotionGenView();
	TSharedRef<SWidget> CreateTTSView();
	TSharedRef<SWidget> CreateAudioOperationView();
	TSharedRef<SWidget> CreateNpcBpAssemblerView();
	
	// 侧边栏导航相关方法
	TSharedRef<SWidget> CreateSidebarNavigation();
	TSharedRef<SWidget> CreateNavigationButton(const FString& ButtonText, EViewType ViewType, const FLinearColor& Color);
	FReply SwitchToView(EViewType NewViewType);
	
	// 版本信息相关方法
	FString GetVersionString() const;
	
	FReply OnImport()
	{
		bShouldImport = true;
		if (WidgetWindow.IsValid())
		{	
			if(ManagerRef && ManagerRef->MotionGenService.IsValid()){
				ManagerRef->MotionGenService->Authenticate(ImportSettings);
			}
			
			//SendHttpRequest(ImportSettings);
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	FReply OnCancel()
	{
		bShouldImport = false;
		if (WidgetWindow.IsValid())
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			return OnCancel();
		}

		return FReply::Unhandled();
	}

	bool ShouldImport() const
	{
		return bShouldImport;
	}

	FReply OnGenerateTTS()
	{
		if (WidgetWindow.IsValid())
		{	
			UE_LOG(LogTemp, Log, TEXT("Generate TTS按钮被点击"));
			if (ManagerRef && TTSSettings)
			{
				// 验证TTS输入文本
				if (TTSSettings->TTSInputText.IsEmpty())
				{
					FNotificationInfo Info(FText::FromString(TEXT("请输入要合成的文本！")));
					Info.bUseLargeFont = true;
					Info.ExpireDuration = 3.0f;
					FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
					return FReply::Handled();
				}
				
				// 验证音色选择
				if (TTSSettings->SpeakerName.IsEmpty())
				{
					FNotificationInfo Info(FText::FromString(TEXT("请先选择音色！")));
					Info.bUseLargeFont = true;
					Info.ExpireDuration = 3.0f;
					FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
					return FReply::Handled();
				}
				
				// 设置按钮为合成中状态
				bIsTTSGenerating = true;
				TTSProgressValue = 0.0f;
				
				// 初始化真实进度跟踪
				TTSCompletedCount = 0;
				TTSTotalCount = 0;
				bUseRealProgress = false;
				
				// 启动进度条更新定时器
				StartTTSProgressTimer();
				
				// 保存当前窗口引用，以便在TTS完成后恢复焦点
				CurrentTTSWindow = WidgetWindow;
				TTSSettings->SynthesizeTTSAudio();
			}
		}
		
		return FReply::Handled();
	}
	
	// 启动TTS进度条更新定时器
	void StartTTSProgressTimer();
	
	// 更新TTS进度条
	void UpdateTTSProgress();
	
	// 停止TTS进度条
	void StopTTSProgress();
	
	// 更新TTS真实进度
	void UpdateTTSRealProgress(int32 CompletedCount, int32 TotalCount);



	FReply OnMergeAudio()
	{
		// 在这里实现音频操作的逻辑
		UE_LOG(LogTemp, Log, TEXT("音频操作按钮被点击"));

		if (AudioAssetOperationSettings && ManagerRef)
		{
			if (AudioAssetOperationSettings->MergeAudioSegments())
			{
				if (AudioAssetOperationSettings->ApplyMergedAudioToTrack())
				{
					UE_LOG(LogTemp, Log, TEXT("音频合并并应用成功!"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("音频合并或应用到轨道失败！"));
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("音频片段合并失败！"));
			}
		}

		return FReply::Handled();
	}

	FReply OnGenerateA2F()
	{
		if (WidgetWindow.IsValid())
		{	
			if(ManagerRef){
	                // 保存当前窗口引用，以便在TTS完成后恢复焦点
				CurrentA2FWindow = WidgetWindow;
				UA2FService* A2FService = NewObject<UA2FService>();
				A2FService->ExecuteA2F(A2FSettings);
			}
		}
		
		return FReply::Handled();
	}
	


	FReply OnCheckSkmAsset()
	{
		if (WidgetWindow.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("开始检查骨骼网格体资产"));
			if (ManagerRef)
			{
				CurrentNpcBpAssemblerWindow = WidgetWindow;
				NpcBpAssemblerSettings->CheckSkmAsset(true);
				CurrentNpcBpAssemblerWindow.Pin()->BringToFront();
			}
		}

		return FReply::Handled();
	}

	FReply OnAssembleNpcBp()
	{
		if (WidgetWindow.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("开始组装角色蓝图"));
			if (ManagerRef)
			{
				CurrentNpcBpAssemblerWindow = WidgetWindow;
				NpcBpAssemblerSettings->AssembleNpcBp();
				CurrentNpcBpAssemblerWindow.Pin()->BringToFront();
			}
		}
		return FReply::Handled();
	}

	void OpenXSequencerForLevelSequence(ULevelSequence* LevelSequence);
	
	// CSV文件上传相关方法
	FReply OnUploadExcelFile();
	FReply OnClearText();
	bool ParseCSVFile(const FString& FilePath, TArray<FString>& OutTextLines);
	bool ParseCSVFileContent(const FString& FilePath, TArray<FString>& OutTextLines);

	SCharacterGenEditorOptions()
		: ImportSettings(nullptr)
		, A2FSettings(nullptr)
		, TTSSettings(nullptr)
		, AudioAssetOperationSettings(nullptr)
		, bShouldImport(false)
		, CurrentViewType(EViewType::TTS)
	{}

private:
	bool CanImport() const;

private:
	UMoGenSetting* ImportSettings;
	UA2FSetting* A2FSettings;
	UTTSSetting* TTSSettings;
	UAudioAssetOperationSetting* AudioAssetOperationSettings;
	UNpcBpAssemblerSettings* NpcBpAssemblerSettings;
	TWeakPtr< SWindow > WidgetWindow;

	bool			bShouldImport;
	
	// Details View 成员变量
	TSharedPtr<IDetailsView> GenerateSettingDetailsView;
	TSharedPtr<IDetailsView> TTSSettingDetailsView;
	TSharedPtr<IDetailsView> AudioAssetOperationSettingDetailsView;
	TSharedPtr<IDetailsView> A2FDetailsView;
	TSharedPtr<IDetailsView> NpcBpAssemblerDetailsView;

	TArray<TSharedPtr<FString>> TrackNames;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> TrackComboBox;

	// 视图切换相关成员变量
	EViewType CurrentViewType;
	TSharedPtr<SWidget> CurrentViewWidget;
	
	// TTS窗口焦点管理
	TWeakPtr<SWindow> CurrentTTSWindow;
    // A2F窗口焦点管理
	TWeakPtr<SWindow> CurrentA2FWindow;
	// NPC蓝图装配器窗口焦点管理
	TWeakPtr<SWindow> CurrentNpcBpAssemblerWindow;
	
	// TTS合成按钮状态管理
	TSharedPtr<SButton> TTSGenerateButton;
	bool bIsTTSGenerating = false;
	
	// TTS合成进度条管理
	TSharedPtr<SProgressBar> TTSProgressBar;
	TSharedPtr<STextBlock> TTSProgressText;
	FTimerHandle TTSProgressTimerHandle;
	float TTSProgressValue = 0.0f;
	
	// TTS真实进度跟踪
	int32 TTSCompletedCount = 0;
	int32 TTSTotalCount = 0;
	bool bUseRealProgress = false;
	
};


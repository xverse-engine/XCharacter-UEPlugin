// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"

#include "MotionGen/MoGenSetting.h"
#include "TTS/TTSSetting.h"
#include "TTS/AudioAssetOperationSetting.h"
#include "A2F/A2FSetting.h"
#include "A2F/A2FService.h"
#include "CharAssetProcessor/Public/NpcBpAssemblerSettings.h"
#include "Animation/AnimSequence.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "MotionGen/MoGenStruct.h"
#include "XVCPluginSettings.h"
#include "Tools/GenTools.h"
#include "TTS/TTSServer.h"
class UAudioComponent;
class USoundWave;

// 前向声明
class SVerticalBox;
class SScrollBox;

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Input/SComboBox.h"

#include "Styling/SlateStyle.h"

#include "MotionGen/MotionGenService.h"
#include "TTSPreviewManager.h"

#include "CharacterGenEditorManager.generated.h"


//For managing text2motion generation
UCLASS()
class CHARACTERGENEDITOR_API UCharacterGenEditorManager : public UObject
{
	GENERATED_BODY()

public:
	static UCharacterGenEditorManager* Get();
	FBVHImporter* Importer;
	UMoGenSetting* InputMoGenSetting;
	UPROPERTY()
	UBVHImportFactory* BVHFactory{
		NewObject<UBVHImportFactory>(
			GetTransientPackage(), UBVHImportFactory::StaticClass())
	};
	TSharedPtr<FSlateStyleSet> CustomStyle{};

public:
	UCharacterGenEditorManager();
	~UCharacterGenEditorManager();

	void AddToolbarExtension(FToolBarBuilder& Builder);
	void ShowAnimationGenWindow();
	void BringWindowToFront();  // 重新激活窗口到前面

public:
	// MotionGen服务实例
	TSharedPtr<FMotionGenService> MotionGenService;
	
	// TTS预览管理器实例
	UPROPERTY()
	UTTSPreviewManager* TTSPreviewManager;
	
	void ImportWavAsSoundAsset(const FString& WavFilePath, const FString& AssetName);
	bool SaveAudioAsSoundWaveAsset(const TArray<uint8>& AudioData);
	
	// A2F功能接口
	void ExecuteA2F(UA2FSetting* A2FSettings);
	
	// 资产同步接口
	void SyncToAsset(const FAssetData& AssetData);

	// TTS预览功能接口（委托给TTSPreviewManager）
	void OnTTSAllCompletedHandler(const TArray<FTTSSegmentResult>& Results, UTTSSetting* TTSSettings);
	void ShowTTSPreviewWindow(const TArray<FTTSSegmentResult>& Results, UTTSSetting* CurrentTTSSettings);
	void SaveSelectedTTSAudio(const TArray<FTTSSegmentResult>& Results);
	void OnEnhanceAudioCompleted(bool bSuccess, const FString& OriginalAssetPath, const FString& EnhancedAssetPath);
	bool IsTTSPreviewWindowOpen() const;
	
	
	// TTS完成回调委托
	FSimpleDelegate OnTTSCompleted;

	TArray<TSharedPtr<FString>> CachedSpeakerDisplayOptions;
	TArray<FString> CachedSpeakerIDOptions;
	bool bSpeakerListLoaded = false;
	TArray<TFunction<void(const TArray<TSharedPtr<FString>>&)> > PendingSpeakerListCallbacks;
	
	    // 保存操作界面窗口引用，用于重新激活
    TSharedPtr<SWindow> MotionGenWindow;
    


private:

	// 播放中音频组件（按段索引管理）
	TMap<int32, TWeakObjectPtr<UAudioComponent>> PlayingComponents;
	
public:
	FString EmbeddedContentBrowserInstanceName = TEXT("MoGen_EmbeddedContentBrowser");
};


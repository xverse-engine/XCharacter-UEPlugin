// Fill out your copyright notice in the Description page of Project Settings.

#include "A2F/A2FService.h"
#include "XSequencerDefines.h"
#include "JsonObjectConverter.h"
#include "XSequencer.h"
#include "PackageTools.h"
#include "UObject/SavePackage.h"
#include "Serialization/JsonSerializer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Sound/SoundWave.h"
#include "Factories/AnimSequenceFactory.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "HAL/Platform.h"

// 添加缺少的头文件
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "A2F/A2FSetting.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "XVCPluginSettings.h"

// RuntimeAudioImporter 相关头文件
#include "RuntimeAudioImporterLibrary.h"
#include "RuntimeAudioExporter.h"

// ContentBrowser 相关头文件
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

// GenTools 相关头文件
#include "Tools/GenTools.h"

// 添加延迟恢复窗口焦点所需的头文件
#include "Engine/World.h"
#include "Editor.h"

#if ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION == 5 || ENGINE_MINOR_VERSION == 2 || ENGINE_MINOR_VERSION == 1)
#include "Misc/Base64.h"
#endif

// ARKit blendshape names
const TArray<FName> ArkitBlendshapeNameList{
	TEXT("browDownLeft"),
	TEXT("browDownRight"),
	TEXT("browInnerUp"),
	TEXT("browOuterUpLeft"),
	TEXT("browOuterUpRight"),
	TEXT("cheekPuff"),
	TEXT("cheekSquintLeft"),
	TEXT("cheekSquintRight"),
	TEXT("eyeBlinkLeft"),
	TEXT("eyeBlinkRight"),
	TEXT("eyeLookDownLeft"),
	TEXT("eyeLookDownRight"),
	TEXT("eyeLookInLeft"),
	TEXT("eyeLookInRight"),
	TEXT("eyeLookOutLeft"),
	TEXT("eyeLookOutRight"),
	TEXT("eyeLookUpLeft"),
	TEXT("eyeLookUpRight"),
	TEXT("eyeSquintLeft"),
	TEXT("eyeSquintRight"),
	TEXT("eyeWideLeft"),
	TEXT("eyeWideRight"),
	TEXT("jawForward"),
	TEXT("jawLeft"),
	TEXT("jawOpen"),
	TEXT("jawRight"),
	TEXT("mouthClose"),
	TEXT("mouthDimpleLeft"),
	TEXT("mouthDimpleRight"),
	TEXT("mouthFrownLeft"),
	TEXT("mouthFrownRight"),
	TEXT("mouthFunnel"),
	TEXT("mouthLeft"),
	TEXT("mouthLowerDownLeft"),
	TEXT("mouthLowerDownRight"),
	TEXT("mouthPressLeft"),
	TEXT("mouthPressRight"),
	TEXT("mouthPucker"),
	TEXT("mouthRight"),
	TEXT("mouthRollLower"),
	TEXT("mouthRollUpper"),
	TEXT("mouthShrugLower"),
	TEXT("mouthShrugUpper"),
	TEXT("mouthSmileLeft"),
	TEXT("mouthSmileRight"),
	TEXT("mouthStretchLeft"),
	TEXT("mouthStretchRight"),
	TEXT("mouthUpperUpLeft"),
	TEXT("mouthUpperUpRight"),
	TEXT("noseSneerLeft"),
	TEXT("noseSneerRight")
};

// 情感类型转换函数
FString ConvertEmotionTypeToString(EA2FEmotionType EmotionType)
{
	switch (EmotionType)
	{
	case EA2FEmotionType::Neutral:
		return TEXT("Neutral");
	case EA2FEmotionType::Happy:
		return TEXT("Happy");
	case EA2FEmotionType::Sad:
		return TEXT("Sad");
	case EA2FEmotionType::Disgust:
		return TEXT("Disgust");
	case EA2FEmotionType::Anger:
		return TEXT("Anger");
	case EA2FEmotionType::Surprise:
		return TEXT("Surprise");
	case EA2FEmotionType::Fear:
		return TEXT("Fear");
	default:
		return TEXT("Neutral");
	}
}

#pragma region PublicFunctions

UA2FService::UA2FService()
{
	// 初始化表情帧数据
	ExpressionFrames.Empty();
}

bool UA2FService::GenerateAnimationFile(UA2FSetting* A2FSettings,const FString& FacialAnimationFolder, const FString& AnimBaseName, bool bSkeletalAnim, int32 AnimDataFPS)
{
	UE_LOG(LogTemp,
	       Warning,
	       TEXT("Start Generating Expression Animation File, Read %d frames of data"),
	       ExpressionFrames.Num());

	// 确保路径以 /Game/ 开头
	FString BasePath = FacialAnimationFolder;
	if (!BasePath.StartsWith(TEXT("/Game/")))
	{
		BasePath = TEXT("/Game/") + BasePath;
	}
	
	// 移除末尾的斜杠（如果有）
	if (BasePath.EndsWith(TEXT("/")))
	{
		BasePath = BasePath.LeftChop(1);
	}
	
	const FString AnimationPackageName = BasePath + TEXT("/") + AnimBaseName;

	// --------------------- Create Anim Sequence using AnimSequenceFactory -----------------------------------------

	UPackage* Package = CreatePackage(*AnimationPackageName);
	Package->Modify();

	auto AnimSequenceFactory = NewObject<UAnimSequenceFactory>();
	auto Model = A2FSettings->InputMeshModel.Get();
	AnimSequenceFactory->TargetSkeleton = Model->GetSkeleton();
	AnimSequenceFactory->PreviewSkeletalMesh = Model;

	UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimSequenceFactory->FactoryCreateNew(
		UAnimSequence::StaticClass(),
		Package,
		*AnimBaseName,
		RF_Standalone | RF_Public,
		nullptr,
		GWarn));

	AnimSequence->AdditiveAnimType = EAdditiveAnimationType::AAT_LocalSpaceBase;
	AnimSequence->RefPoseType = EAdditiveBasePoseType::ABPT_RefPose;
	AnimSequence->ImportFileFramerate = DEFAULT_ANIMATION_FPS;
	AnimSequence->ImportResampleFramerate = DEFAULT_ANIMATION_FPS;
	AnimSequence->GetController().SetFrameRate(FFrameRate{DEFAULT_ANIMATION_FPS, 1}, false);
	AnimSequence->GetController().OpenBracket(FText::FromString("Blendshape"));

	int32 OriginFrameNumber = ExpressionFrames.Num();
	
	// 安全检查：确保AnimDataFPS不为0或负数
	if (AnimDataFPS <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid AnimDataFPS: %d, using default value %d"), AnimDataFPS, DEFAULT_ANIMATION_FPS);
		AnimDataFPS = DEFAULT_ANIMATION_FPS;
	}
	
	auto TargetFrameNumber = static_cast<int32>(static_cast<float>(OriginFrameNumber) / static_cast<float>(AnimDataFPS) * DEFAULT_ANIMATION_FPS);
	
	// 安全检查：确保TargetFrameNumber在合理范围内
	if (TargetFrameNumber <= 0 || TargetFrameNumber > MAX_FRAME_COUNT)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid TargetFrameNumber: %d, using OriginFrameNumber instead"), TargetFrameNumber);
		TargetFrameNumber = OriginFrameNumber;
	}

	UE_LOG(LogTemp, Warning, TEXT("Origin Frame Number: %d, Target Frame Number: %d"),
	       OriginFrameNumber,
	       TargetFrameNumber);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 1
	AnimSequence->GetController().SetPlayLength(static_cast<float>(TargetFrameNumber) / DEFAULT_ANIMATION_FPS);
#else
	AnimSequence->GetController().SetNumberOfFrames(FFrameNumber{TargetFrameNumber});
#endif

	// --------------------- Calculate Arkit Bone Transform Map if Use anim -----------------------------------------

	if (bSkeletalAnim)
	{
		// ---------------------------------- Skeletal Anim -----------------------------------------
		// 一个骨骼名-每一帧骨骼 Transform 的 map
		USkeletalMeshComponent* SkMeshComp = NewObject<USkeletalMeshComponent>(
			USkeletalMeshComponent::StaticClass());
		SkMeshComp->SetSkinnedAsset(Model);

		// 一个骨骼名-每一帧骨骼 Transform 的 map
		TMap<FName, TArray<FTransform>> ArkitBoneTransformMap;

		TArray<FName> BoneNames;
		SkMeshComp->GetBoneNames(BoneNames);
		UE_LOG(LogTemp, Warning, TEXT("BoneNames Num: %d"), BoneNames.Num());

		for (int32 SocketIdx = 1; SocketIdx < BoneNames.Num(); SocketIdx += 1)
		{
			TArray<FTransform> Result;
			for (int32 i = 1; i < 52; i++)
			{
#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
				FTransform CurrentFrameTransform = FTransform::Identity;
				int32 BoneTrackIndex = A2FSettings->InputSkeleAnim->GetDataModel()->GetBoneTrackByName(
												 BoneNames[SocketIdx]).
											 BoneTreeIndex;
				A2FSettings->InputSkeleAnim->GetBoneTransform(CurrentFrameTransform, BoneTrackIndex,
																 static_cast<float>(i) / 30.0,
																 true);
#else
				auto CurrentFrameTransform = A2FSettings->InputSkeleAnim->GetDataModel()->GetBoneTrackTransform(
					BoneNames[SocketIdx],
					FFrameNumber{i});
#endif

				Result.Push(CurrentFrameTransform);
			}

			// 如果一个骨骼每帧数据都相同，则不再加入 Map，不再参与后续计算
			auto FirstFrame = Result[0];
			bool ShouldAddToMap = false;
			for (const auto& Transform : Result)
			{
				if (!Transform.Equals(FirstFrame))
				{
					ShouldAddToMap = true;
					break;
				}
			}
			if (ShouldAddToMap)
			{
				ArkitBoneTransformMap.Add(BoneNames[SocketIdx], Result);
			}
		}

		// 通过 Expression Frames 数据生成表情动画
		for (const auto& Name_Data : ArkitBoneTransformMap)
		{
			FName BoneName = Name_Data.Key;

			FRawAnimSequenceTrack RawTrack;

#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
			FTransform OriginTransform = FTransform::Identity;
			int32 BoneTrackIndex = A2FSettings->InputSkeleAnim->GetDataModel()->GetBoneTrackByName(BoneName).
										 BoneTreeIndex;
			A2FSettings->InputSkeleAnim->GetBoneTransform(OriginTransform, BoneTrackIndex, 0.0f, true);
#else
			FTransform OriginTransform = A2FSettings->InputSkeleAnim->GetDataModel()->GetBoneTrackTransform(
				BoneName,
				FFrameNumber{0});
#endif

			for (int32 FrameIdx = 0; FrameIdx < TargetFrameNumber; FrameIdx += 1)
			{
				auto CurrentExpressionFrame = GetResampledExpressionFrame(FrameIdx, TargetFrameNumber, AnimDataFPS);
				auto BoneTransform = CalcBoneCurrentFrame(Name_Data.Value,
				                                          CurrentExpressionFrame,
				                                          OriginTransform);

				RawTrack.ScaleKeys.Add(FVector3f(BoneTransform.GetScale3D()));
				RawTrack.PosKeys.Add(FVector3f(BoneTransform.GetTranslation()));
				RawTrack.RotKeys.Add(FQuat4f(BoneTransform.GetRotation()));
			}

			//add new track
#if ENGINE_MAJOR_VERSION == 5 &&  ENGINE_MINOR_VERSION == 1
			AnimSequence->GetController().AddBoneTrack(BoneName);
#else
			AnimSequence->GetController().AddBoneCurve(BoneName);
#endif

			AnimSequence->GetController().SetBoneTrackKeys(BoneName,
			                                               RawTrack.PosKeys,
			                                               RawTrack.RotKeys,
			                                               RawTrack.ScaleKeys);
		}
	}
	else
	{
		// ---------------------------------- Morph Target Anim -----------------------------------------

		for (uint16 MorphIdx = 0; MorphIdx < ARKIT_BLENDSHAPE_COUNT && MorphIdx < ArkitBlendshapeNameList.Num(); MorphIdx++)
		{
#if ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION == 2 || ENGINE_MINOR_VERSION == 1)
			FAnimationCurveIdentifier Curve{
				FSmartName{ArkitBlendshapeNameList[MorphIdx], MorphIdx}, ERawCurveTrackTypes::RCT_Float
			};
#else
			FAnimationCurveIdentifier Curve{ArkitBlendshapeNameList[MorphIdx], ERawCurveTrackTypes::RCT_Float};
#endif
			AnimSequence->GetController().AddCurve(Curve, false);

			TArray<FRichCurveKey> Keys{};
			for (int32 FrameIdx = 0; FrameIdx < TargetFrameNumber; FrameIdx++)
			{
				auto CurrentExpressionFrame = this->GetResampledExpressionFrame(FrameIdx, TargetFrameNumber, AnimDataFPS);
				if (CurrentExpressionFrame.Num() > MorphIdx)
				{
					FRichCurveKey Key(static_cast<float>(FrameIdx) / DEFAULT_ANIMATION_FPS, CurrentExpressionFrame[MorphIdx]);
					Keys.Add(Key);
				}
			}

			AnimSequence->GetController().SetCurveKeys(Curve, Keys, false);
		}
	}

	// ---------------------------------- Finish Creating Asset -----------------------------------------
	AnimSequence->GetController().UpdateAttributesFromSkeleton(Model->GetSkeleton());
	AnimSequence->GetController().NotifyPopulated();

	AnimSequence->GetController().CloseBracket();
	AnimSequence->AddToRoot();
	AnimSequence->Modify();

	FAssetRegistryModule::AssetCreated(AnimSequence);

	Package->Modify();
	FString PackageFileName = FPackageName::LongPackageNameToFilename(AnimationPackageName,
	                                                                  FPackageName::GetAssetPackageExtension());
	bool bSaved = UPackage::SavePackage(Package, AnimSequence, *PackageFileName, FSavePackageArgs{});

	// 资产保存成功后，高亮显示新生成的动画资产
	if (bSaved)
	{
		UGenTools::SyncToAsset(AnimationPackageName);
	}

	return UPackageTools::ReloadPackages(TArray<UPackage*>{Package});
}

TArray<float> UA2FService::GetResampledExpressionFrame(const int32 CurrentFrame, const int32 TargetFrameLength,
                                                              int32 OriginFPS)
{
	// 安全检查：确保ExpressionFrames不为空
	if (ExpressionFrames.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ExpressionFrames is empty"));
		return TArray<float>();
	}

	if (OriginFPS == DEFAULT_ANIMATION_FPS)
	{
		// 安全检查：确保CurrentFrame在有效范围内
		if (CurrentFrame >= 0 && CurrentFrame < ExpressionFrames.Num())
		{
			return this->ExpressionFrames[CurrentFrame];
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("CurrentFrame %d out of range [0, %d)"), CurrentFrame, ExpressionFrames.Num());
			return this->ExpressionFrames.Last();
		}
	}

	const float ScaledCurrentFrame = static_cast<float>(CurrentFrame) / static_cast<float>(TargetFrameLength)
		* static_cast<float>(this->ExpressionFrames.Num());

	if (ScaledCurrentFrame >= this->ExpressionFrames.Num())
	{
		return this->ExpressionFrames.Last();
	}
	const int32 Ceil = FMath::CeilToInt32(ScaledCurrentFrame);
	const int32 Floor = FMath::FloorToInt32(ScaledCurrentFrame);

	if (Ceil >= this->ExpressionFrames.Num() || Floor >= this->ExpressionFrames.Num())
	{
		return this->ExpressionFrames.Last();
	}

	const TArray<float>& CeilExpressionFrames = this->ExpressionFrames[Ceil];
	const TArray<float>& FloorExpressionFrames = this->ExpressionFrames[Floor];

	const float Lerp = ScaledCurrentFrame - Floor;

	TArray<float> Result;
	for (int i = 0; i < CeilExpressionFrames.Num(); i += 1)
	{
		const auto Number = FMath::Lerp(FloorExpressionFrames[i], CeilExpressionFrames[i], Lerp);
		const auto FinalNumber = FMath::Clamp(Number, 0.0, 1.0);
		Result.Push(FinalNumber);
	}

	return Result;
}

#pragma endregion

#pragma region UtilityFunctions

FTransform UA2FService::CalcBoneCurrentFrame(const TArray<FTransform>& ArkitBoneTransforms,
                                                    const TArray<float>& CurrentExpressionFrame,
                                                    const FTransform& OriginTransform)
{
	FTransform OutTransform = FTransform::Identity;
	FQuat ResRotation = OriginTransform.GetRotation();
	FVector ResLocation{0.f};

	for (int32 i = 0; i < ARKIT_BLENDSHAPE_COUNT; i++)
	{
		auto OriginRot = OriginTransform.GetRotation();
		auto TargetRot = ArkitBoneTransforms[i].GetRotation();

		// Must Normalize!!!
		OriginRot.Normalize();
		TargetRot.Normalize();

		auto DeltaQuat = OriginRot.Inverse() * TargetRot;
		DeltaQuat.Normalize();
		ResRotation *= FQuat::Slerp(FQuat::Identity,
		                            DeltaQuat,
		                            CurrentExpressionFrame[i]);

		auto DeltaLoc = ArkitBoneTransforms[i].GetLocation() - OriginTransform.GetLocation();
		ResLocation += DeltaLoc * CurrentExpressionFrame[i];
	}
	const auto ResLoc = ResLocation + OriginTransform.GetLocation();

	ResRotation.Normalize();

	OutTransform.SetLocation(ResLoc);
	OutTransform.SetRotation(ResRotation);

	return OutTransform;
}

TArray<float> UA2FService::Base64ToFloatArray(const FString& Base64String)
{
	TArray<uint8> Data;
	FBase64::Decode(Base64String, Data);
	TArray<float> Result;
	
	// 安全检查：确保数据长度是4的倍数（每个float占4字节）
	if (Data.Num() % 4 != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Base64 decoded data length %d is not a multiple of 4"), Data.Num());
		return Result;
	}
	
	for (int32 i = 0; i < Data.Num() - 3; i += 4)
	{
		float Fa = 0.f;
		uint8 Temp[4];
		Temp[0] = Data[i];
		Temp[1] = Data[i + 1];
		Temp[2] = Data[i + 2];
		Temp[3] = Data[i + 3];
		FMemory::Memcpy(&Fa, Temp, 4);

		Result.Push(Fa);
	}
	return Result;
}

FString UA2FService::ConvertPCMBufferToBase64String(const TArray<uint8>& PcmBuffer)
{
	return FBase64::Encode(PcmBuffer);
}

TArray<uint8> UA2FService::ConvertPCMBufferToWavBuffer(const TArray<uint8>& PcmBuffer,
                                                              const uint32 AudioSampleRate)
{
	TArray<uint8> BytesArr;

	TArray<uint8> AudioHeader;
	constexpr uint16 BitDepth = 16; // How to retrieve information directly from the SoundWave ??

	AudioHeader.Append(FStringToBytes("RIFF")); // 1-4
	AudioHeader.Append(Uint32ToBytes(PcmBuffer.Num() + 36)); // 5-8
	AudioHeader.Append(FStringToBytes("WAVEfmt ")); // 9-16
	AudioHeader.Append(Uint32ToBytes(16)); // 17-20
	AudioHeader.Append(Uint16ToBytes(1)); // 21-22
	AudioHeader.Append(Uint16ToBytes(1)); // 23-24 Num of channels
	AudioHeader.Append(Uint32ToBytes(AudioSampleRate)); // 25-28
	AudioHeader.Append(Uint32ToBytes(AudioSampleRate * BitDepth * 1 / 8)); // 29-32
	AudioHeader.Append(Uint16ToBytes(BitDepth * 1 / 8)); // 33-34
	AudioHeader.Append(Uint16ToBytes(BitDepth)); // 35-36
	AudioHeader.Append(FStringToBytes("data")); // 37-40
	AudioHeader.Append(Uint32ToBytes(PcmBuffer.Num())); // 41-44

	BytesArr.Append(AudioHeader);
	BytesArr.Append(PcmBuffer);

	return BytesArr;
}

FString UA2FService::ConvertWavBufferToBase64String(const TArray<uint8>& WavBuffer)
{
	return FBase64::Encode(WavBuffer);
}

TArray<uint8> UA2FService::FStringToBytes(const FString& String)
{
	TArray<uint8> OutBytes;

	// Handle empty strings
	if (String.Len() > 0)
	{
		FTCHARToUTF8 Converted(*String); // Convert to UTF8
		OutBytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
	}

	return OutBytes;
}

TArray<uint8> UA2FService::Uint32ToBytes(const uint32 Value, bool UseLittleEndian)
{
	TArray<uint8> OutBytes;
	if (UseLittleEndian)
	{
		OutBytes.Add(Value >> 0 & 0xFF);
		OutBytes.Add(Value >> 8 & 0xFF);
		OutBytes.Add(Value >> 16 & 0xFF);
		OutBytes.Add(Value >> 24 & 0xFF);
	}
	else
	{
		OutBytes.Add(Value >> 24 & 0xFF);
		OutBytes.Add(Value >> 16 & 0xFF);
		OutBytes.Add(Value >> 8 & 0xFF);
		OutBytes.Add(Value >> 0 & 0xFF);
	}
	return OutBytes;
}

TArray<uint8> UA2FService::Uint16ToBytes(const uint16 Value, bool UseLittleEndian)
{
	TArray<uint8> OutBytes;
	if (UseLittleEndian)
	{
		OutBytes.Add(Value >> 0 & 0xFF);
		OutBytes.Add(Value >> 8 & 0xFF);
	}
	else
	{
		OutBytes.Add(Value >> 8 & 0xFF);
		OutBytes.Add(Value >> 0 & 0xFF);
	}
	return OutBytes;
}

#pragma endregion


// A2F功能实现
void UA2FService::ExecuteA2F(UA2FSetting* A2FSettings)
{
    if (!A2FSettings)
    {
        FNotificationInfo Info(FText::FromString(TEXT("A2F设置无效")));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }
    
    // 检查输入参数
    if (A2FSettings->InputAudio.IsValid() && !A2FSettings->InputAudio.Get())
    {
        A2FSettings->InputAudio.LoadSynchronous();
    }
    
    // 尝试通过路径直接加载
    if (!A2FSettings->InputAudio.Get() && !A2FSettings->InputAudio.ToString().IsEmpty())
    {
        FSoftObjectPath AudioPath = A2FSettings->InputAudio.ToSoftObjectPath();
        if (AudioPath.IsValid())
        {
            USoundWave* LoadedAudio = Cast<USoundWave>(AudioPath.TryLoad());
            if (LoadedAudio)
            {
                A2FSettings->InputAudio = TSoftObjectPtr<USoundWave>(LoadedAudio);
            }
        }
    }
    
    if (!A2FSettings->InputAudio.Get())
    {
        FNotificationInfo Info(FText::FromString(TEXT("请选择输入音频")));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }
    
    // 尝试同步加载Mesh模型
    if (A2FSettings->InputMeshModel.IsValid() && !A2FSettings->InputMeshModel.Get())
    {
        A2FSettings->InputMeshModel.LoadSynchronous();
    }
    
    // 尝试通过路径加载Mesh模型
    if (!A2FSettings->InputMeshModel.Get() && !A2FSettings->InputMeshModel.ToString().IsEmpty())
    {
        FSoftObjectPath MeshPath = A2FSettings->InputMeshModel.ToSoftObjectPath();
        if (MeshPath.IsValid())
        {
            USkeletalMesh* LoadedMesh = Cast<USkeletalMesh>(MeshPath.TryLoad());
            if (LoadedMesh)
            {
                A2FSettings->InputMeshModel = TSoftObjectPtr<USkeletalMesh>(LoadedMesh);
            }
        }
    }
    
    if (!A2FSettings->InputMeshModel.Get())
    {
        FNotificationInfo Info(FText::FromString(TEXT("请选择输入Mesh模型")));
        FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
        return;
    }
    
    // 只有在启用骨骼动画驱动时才验证表情动画
    if (A2FSettings->bSkeletalAnim)
    {
        // 尝试同步加载表情动画
        if (A2FSettings->InputSkeleAnim.IsValid() && !A2FSettings->InputSkeleAnim.Get())
        {
            A2FSettings->InputSkeleAnim.LoadSynchronous();
        }
        
        // 尝试通过路径加载表情动画
        if (!A2FSettings->InputSkeleAnim.Get() && !A2FSettings->InputSkeleAnim.ToString().IsEmpty())
        {
            FSoftObjectPath AnimPath = A2FSettings->InputSkeleAnim.ToSoftObjectPath();
            if (AnimPath.IsValid())
            {
                UAnimSequence* LoadedAnim = Cast<UAnimSequence>(AnimPath.TryLoad());
                if (LoadedAnim)
                {
                    A2FSettings->InputSkeleAnim = TSoftObjectPtr<UAnimSequence>(LoadedAnim);
                }
            }
        }
        
        if (!A2FSettings->InputSkeleAnim.Get())
        {
            FNotificationInfo Info(FText::FromString(TEXT("请选择输入表情基骨骼动画")));
            FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Fail);
            return;
        }
    }

    // 显示开始处理通知
    FNotificationInfo StartInfo(FText::FromString(TEXT("正在请求A2F服务...")));
    FSlateNotificationManager::Get().AddNotification(StartInfo);

    // 创建HTTP请求
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

    // 设置请求URL
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    FString BasePath = Settings->A2FStoragePath;
    
    // 确保路径以 /Game/ 开头
    if (!BasePath.StartsWith(TEXT("/Game/")))
    {
        BasePath = TEXT("/Game/") + BasePath;
    }
    
    // 移除末尾的斜杠（如果有）
    if (BasePath.EndsWith(TEXT("/")))
    {
        BasePath = BasePath.LeftChop(1);
    }
    
    // 创建目录（如果需要）
    FString ProjectContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
    FString RelativePath = BasePath;
    if (RelativePath.StartsWith(TEXT("/Game/")))
    {
        RelativePath = RelativePath.RightChop(6); // 移除 "/Game/"
    }
    FString AbsoluteFolderPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ProjectContentDir, RelativePath));
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*AbsoluteFolderPath))
    {
        PlatformFile.CreateDirectory(*AbsoluteFolderPath);
    }

    FString A2FServerURL = FString::Printf(TEXT("%s%s"), *Settings->IP_A2F_Server, *Settings->API_Audio2Face);
    HttpRequest->SetURL(A2FServerURL);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetTimeout(Settings->Timeout);

	// ------------------------------------ Audio To Base64 To Chunks -----------------------------------------
	TArray<uint8> AudioRawData;
	uint32 AudioImportSampleRate;
	uint16 NumChannels;
	if (const bool Res = A2FSettings->InputAudio.Get()->GetImportedSoundWaveData(AudioRawData, AudioImportSampleRate, NumChannels); !Res)
	{
		FNotificationInfo ErrorInfo(FText::FromString(TEXT("无法获取音频数据")));
		FSlateNotificationManager::Get().AddNotification(ErrorInfo)->SetCompletionState(SNotificationItem::CS_Fail);
		return;
	}

	// 简单的音频重采样：如果采样率不是16000，进行基本处理
	if (AudioImportSampleRate != 16000)
	{
        // First Resample audio to 16000 and 1 channel
		const auto ImportedSoundWave = URuntimeAudioImporterLibrary::ConvertRegularToImportedSoundWave(A2FSettings->InputAudio.Get());

		FRuntimeAudioExportOverrideOptions ResampleOptions;
		ResampleOptions.SampleRate = 16000;
		ResampleOptions.NumOfChannels = 1;

		const TArray<uint8> ExportedSoundWave = URuntimeAudioExporter::ExportSoundWaveToRAWBuffer(ImportedSoundWave,
			ERuntimeRAWAudioFormat::Int16,
			ResampleOptions);

		// AudioRawData = TArray(&ExportedSoundWave.GetData()[44], ExportedSoundWave.Num() - 44);
		AudioRawData = ExportedSoundWave;
	}

	// Convert Audio Raw Data to WAV Buffer, then to Base64 String
	const TArray<uint8> WavBuffer = UA2FService::ConvertPCMBufferToWavBuffer(AudioRawData, 16000);
	const FString AudioBase64 = UA2FService::ConvertWavBufferToBase64String(WavBuffer);

	// 构造请求体
	FA2FRequest A2FRequest;
	A2FRequest.trace_id = FGuid::NewGuid().ToString();
	A2FRequest.audio = AudioBase64;
	
	// 设置眨眼参数
	A2FRequest.blink_params.add_blink = A2FSettings->addBlink;
	A2FRequest.blink_params.blink_random_bottom = A2FSettings->BlinkRandomBottom;
	A2FRequest.blink_params.blink_random_top = A2FSettings->BlinkRandomTop;
	A2FRequest.blink_params.blink_time = A2FSettings->BlinkTime;
	A2FRequest.blink_params.blink_amplitude = A2FSettings->BlinkAmplitude;
	
	// 设置张嘴参数
	A2FRequest.mouth_params.add_mouth_scale = A2FSettings->addMouthScale;
	A2FRequest.mouth_params.mouth_amplitude_scale = A2FSettings->MouthAmplitudeScale;
	
	// 设置情感参数
	A2FRequest.emotion = ConvertEmotionTypeToString(A2FSettings->EmotionType);
	// 当emotion为Neutral时，intensity固定为1.0
	A2FRequest.intensity = (A2FSettings->EmotionType == EA2FEmotionType::Neutral) ? 1.0f : A2FSettings->EmotionIntensity;

	FString RequestJson;
	FJsonObjectConverter::UStructToJsonObjectString(A2FRequest, RequestJson, 0, 0);

	HttpRequest->SetContentAsString(RequestJson);

    // 设置响应回调
    HttpRequest->OnProcessRequestComplete().BindLambda(
        [this, A2FSettings, BasePath](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
            {
                FString ResponseContent = Response->GetContentAsString();

                // 解析响应数据
                TSharedPtr<FJsonObject> ResponseObject;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
                
                if (FJsonSerializer::Deserialize(Reader, ResponseObject) && ResponseObject.IsValid())
                {
                    // 检查响应状态
                    if (ResponseObject->HasField(TEXT("code")))
                    {
                        int32 ResponseCode = ResponseObject->GetIntegerField(TEXT("code"));
                        if (ResponseCode == 0) // 成功
                        {
                            // 获取表情参数数据
                            if (ResponseObject->HasField(TEXT("data")))
                            {
                                // 解析表情数据
                                TArray<float> FaceDataBase;
                                const TArray<TSharedPtr<FJsonValue>>* DataArray;
                                if (ResponseObject->TryGetArrayField(TEXT("data"), DataArray))
                                {
                                    FaceDataBase.Reserve(DataArray->Num());
                                    for (const auto& Value : *DataArray)
                                    {
                                        if (Value->Type == EJson::Number)
                                        {
                                            FaceDataBase.Add(static_cast<float>(Value->AsNumber()));
                                        }
                                    }
                                }
                                else
                                {
                                    UE_LOG(LogTemp, Error, TEXT("No data field found in response"));
                                    return;
                                }

                                // 处理表情帧数据
                                ExpressionFrames.Empty();
                                
                                // 安全检查：确保数据长度是51的倍数
                                const int32 ExpectedFrameCount = FaceDataBase.Num() / ARKIT_BLENDSHAPE_COUNT;
                                if (FaceDataBase.Num() % ARKIT_BLENDSHAPE_COUNT != 0)
                                {
                                    UE_LOG(LogTemp, Warning, TEXT("Face data length %d is not a multiple of %d"), FaceDataBase.Num(), ARKIT_BLENDSHAPE_COUNT);
                                }
                                
                                for (int32 i = 0; i < ExpectedFrameCount; i++)
                                {
                                    TArray<float> SingleFrameData{};
                                    for (int32 j = 0; j < ARKIT_BLENDSHAPE_COUNT; j++)
                                    {
                                        const int32 DataIndex = i * ARKIT_BLENDSHAPE_COUNT + j;
                                        if (DataIndex < FaceDataBase.Num())
                                        {
                                            float Value = FaceDataBase[DataIndex] >= 0.f ? FaceDataBase[DataIndex] : 0;
                                            SingleFrameData.Add(Value);
                                        }
                                        else
                                        {
                                            // 如果数据不足，用0填充
                                            SingleFrameData.Add(0.0f);
                                        }
                                    }
                                    ExpressionFrames.Add(SingleFrameData);
                                }
                                
                                // 保存表情参数到本地文件
                                // 获取音频文件名
                                FString AudioFileName = A2FSettings->InputAudio.Get()->GetName();
                                // 获取currentTime
                                FString CurrentTime = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
                                const FString AnimationBaseName = "FACE_" + AudioFileName + "_" + CurrentTime;
                                bool bGenerateAnimSuccess{false};
                                bGenerateAnimSuccess = this->GenerateAnimationFile(A2FSettings, BasePath, AnimationBaseName, A2FSettings->bSkeletalAnim, 30);

                                FNotificationInfo SuccessInfo(FText::FromString(TEXT("A2F功能执行成功，表情参数已保存")));
                                FSlateNotificationManager::Get().AddNotification(SuccessInfo)->SetCompletionState(SNotificationItem::CS_Success);
                            }
                            else
                            {
                                FNotificationInfo ErrorInfo(FText::FromString(TEXT("服务器响应中缺少表情数据")));
                                FSlateNotificationManager::Get().AddNotification(ErrorInfo)->SetCompletionState(SNotificationItem::CS_Fail);
                            }
                        }
                        else
                        {
                            FString ErrorMessage = ResponseObject->GetStringField(TEXT("msg"));
                            FNotificationInfo ErrorInfo(FText::FromString(FString::Printf(TEXT("A2F请求失败: %s"), *ErrorMessage)));
                            FSlateNotificationManager::Get().AddNotification(ErrorInfo)->SetCompletionState(SNotificationItem::CS_Fail);
                        }
                    }
                    else
                    {
                        FNotificationInfo ErrorInfo(FText::FromString(TEXT("服务器响应格式错误")));
                        FSlateNotificationManager::Get().AddNotification(ErrorInfo)->SetCompletionState(SNotificationItem::CS_Fail);
                    }
                }
                else
                {
                    FNotificationInfo ErrorInfo(FText::FromString(TEXT("无法解析服务器响应")));
                    FSlateNotificationManager::Get().AddNotification(ErrorInfo)->SetCompletionState(SNotificationItem::CS_Fail);
                }
            }
            else
            {
                FString ErrorMsg = TEXT("A2F请求失败. ");
                if (Response.IsValid())
                {
                    ErrorMsg += FString::Printf(TEXT("错误码: %d, 内容: %s"), Response->GetResponseCode(), *Response->GetContentAsString());
                }
                FNotificationInfo ErrorInfo(FText::FromString(ErrorMsg));
                FSlateNotificationManager::Get().AddNotification(ErrorInfo)->SetCompletionState(SNotificationItem::CS_Fail);
            }

            // 延迟恢复窗口焦点，避免A2F服务执行导致的失焦
            UGenTools::BringPluginWindowToFront();
        });

    // 发送请求    
    HttpRequest->ProcessRequest();
}
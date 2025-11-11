// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Evaluation/Blending/MovieSceneBlendType.h"
#include "Curves/RealCurve.h"
#include "TransformAndKeyframeBPLib.generated.h"

class UMaterialInstanceConstant;
class UMovieScenePrimitiveMaterialSection;
class UMovieScenePrimitiveMaterialTrack;
class ULevelSequence;
class UMovieSceneSection;
class UMovieScene3DTransformTrack;
class UMovieScene3DTransformSection;
class UMaterialParameterCollection;

/**
 *
 */
UCLASS()
class XSEQUENCER_API UTransformAndKeyframeBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#pragma region TransformTrack

	/**
	 * 获取 Sequence 中指定 Actor 的 Transform Track
	 * @param LevelSeqPath			Sequence 的储存路径
	 * @param ActorGuid				所指定 Actor 在 Sequence 中的 GUID
	 * @return 返回所获取到的 Transform Track
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | TransformTrack")
	static UMovieScene3DTransformTrack* GetTransformTrackFromActor(FString LevelSeqPath,
	                                                               FGuid ActorGuid);

	/**
	 * 向 Sequence 中指定 Actor 添加 Transform Track
	 * @param LevelSeqPath			Sequence 的储存路径
	 * @param ActorGuid				所指定 Actor 在 Sequence 中的 GUID
	 * @return 返回所添加的 Transform Track，如果添加前已经存在则返回该 Track
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | TransformTrack")
	static UMovieScene3DTransformTrack* AddTransformTrackToActor(FString LevelSeqPath,
	                                                             FGuid ActorGuid);

	/**
	 * 从指定 Sequence 中移除指定 Actor 的 Transform Track
	 * @param LevelSeqPath			Level Sequence 路径
	 * @param ActorGuid				所指定 Actor 在 Sequence 中的 GUID
	 * @return 移除是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | TransformTrack")
	static bool RemoveTransformTrackFromActor(FString LevelSeqPath, FGuid ActorGuid);

	/**
	 * 通过索引获取 Sequence 中指定 Actor 的 Transform Section
	 * @param LevelSeqPath			Level Sequence 路径
	 * @param ActorGuid				所指定 Actor 在 Sequence 中的 GUID
	 * @param SectionIndex			所查找的 Transform Section 索引
	 * @return 所查找的 Transform Section
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | TransformSection")
	static UMovieScene3DTransformSection* GetTransformSectionInActor(FString LevelSeqPath,
	                                                                 FGuid ActorGuid,
	                                                                 int SectionIndex);

	/**
	 * 在 Sequence 中给指定 Actor 添加 Transform Section
	 * @param LevelSeqPath			Level Sequence 路径
	 * @param ActorGuid				所指定 Actor 在 Sequence 中的 GUID
	 * @param StartFrame			Section 起始帧
	 * @param EndFrame				Section 结束帧
	 * @param BlendType				Section 指定 Blend 类型
	 * @return 返回所添加的 Transform Section，如果添加前已经存在则返回该 Sections
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | TransformSection")
	static UMovieScene3DTransformSection* AddTransformSectionToActor(FString LevelSeqPath,
	                                                                 FGuid ActorGuid,
	                                                                 int StartFrame,
	                                                                 int EndFrame,
	                                                                 EMovieSceneBlendType BlendType);

	/**
	 * 在 Sequence 中移除指定 Actor 的 Transform Section
	 * @param LevelSeqPath			Level Sequence 路径
	 * @param ActorGuid				所指定 Actor 在 Sequence 中的 GUID
	 * @param SectionIndex			Section 索引
	 * @return 移除是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | TransformSection")
	static bool RemoveTransformSectionFromActor(FString LevelSeqPath, FGuid ActorGuid,
	                                            int SectionIndex);

#pragma endregion

#pragma region KeyFrame

	/**
	 * 添加 Sequence 中指定 Actor 的 Transform 关键帧
	 * @param LevelSeqPath			Level Sequence 路径
	 * @param ActorGuid				所指定 Actor 在 Sequence 中的 GUID
	 * @param SectionIndex			所在 Section 索引
	 * @param Frame					所在帧数
	 * @param Transform				插入的关键帧的 Transform
	 * @param KeyInterpolation		关键帧过渡类型：0-Cubic、1-Linear、2-Constant
	 * @return						添加是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | KeyFrame")
	static bool AddTransformKeyframeForActor(FString LevelSeqPath, FGuid ActorGuid, int SectionIndex, int Frame,
	                                         FTransform Transform, int KeyInterpolation);

	/**
	 * 移除 Sequence 中指定 Actor 的 Transform 关键帧
	 * @param LevelSeqPath		Level Sequence 路径
	 * @param ActorGuid				所指定 Actor 在 Sequence 中的 GUID
	 * @param SectionIndex			所在 Section 索引
	 * @param Frame					所在帧数
	 * @return						移除是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | KeyFrame")
	static bool RemoveTransformKeyframeForActor(FString LevelSeqPath, FGuid ActorGuid, int SectionIndex,
	                                            int Frame);

	/**
	 * 添加关键帧的 Double Channel
	 * @param Section				所在的 Transform Section
	 * @param ChannelIndex			所在 Channel 索引
	 * @param Frame					需要添加的关键帧帧数
	 * @param Value					设定该关键帧的值
	 * @param KeyInterpolation		关键帧过渡类型：0-Cubic、1-Linear、2-Constant
	 * @return						添加是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | KeyFrame")
	static bool AddKeyframeToDoubleChannel(UMovieSceneSection* Section, int ChannelIndex, int Frame, double Value,
	                                       int KeyInterpolation);

	/**
	 * 移除关键帧的 Double Channel
	 * @param Section				需要移除的关键帧所在的 Transform Section
	 * @param ChannelIndex			关键帧所在的 Channel 索引
	 * @param Frame					需要移除的关键帧的索引
	 * @return 移除是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | KeyFrame")
	static bool RemoveKeyframeFromDoubleChannel(UMovieSceneSection* Section, int ChannelIndex, int Frame);

#pragma endregion

#pragma region Interpolation

	/**
	 * 修改指定 Sequence 中指定 Transform Section 中关键帧的插值类型
	 * @param LevelSeqPath			Level Sequence 路径
	 * @param ActorGuid				所指定 Actor 在 Sequence 中的 GUID
	 * @param SectionIndex			Transform Track 中指定 Section 的索引
	 * @param ChannelIndex			指定 Section 中的 Channel 索引
	 * @param Frame					需要移除的关键帧帧数
	 * @param Value					设定该关键帧的值
	 * @param InterpMode			设定该关键帧的插值方法：1-Cubic、2-Linear、3-Constant
	 * @param TangentMode			设定该关键帧的切线类型：0-Auto、1-User、2-Break
	 * @param ArriveTangent			设定到达切线的值
	 * @param LeaveTangent			设定离去切线的值
	 * @return 是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | KeyFrame")
	static bool SetKeyFrameInterpInTransformTrack(const FString& LevelSeqPath,
	                                              const FGuid& ActorGuid,
	                                              int SectionIndex,
	                                              int ChannelIndex,
	                                              int Frame,
	                                              float Value,
	                                              ERichCurveInterpMode InterpMode,
	                                              ERichCurveTangentMode TangentMode,
	                                              float ArriveTangent,
	                                              float LeaveTangent);

#pragma endregion

#pragma region Visibility

	/**
	 * 在 Sequence 中添加指定 Actor 的可见性关键帧
	 * @param LevelSeqPath			Level Sequence 路径
	 * @param ActorGuid				所指定 Actor 在 Sequence 中的 GUID
	 * @param Frame					帧数
	 * @param Visibility			可见性
	 * @return 是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | KeyFrame")
	static bool AddVisibilityKeyframeForActor(const FString& LevelSeqPath,
	                                          const FGuid& ActorGuid,
	                                          int Frame,
	                                          bool Visibility);

	/**
	 * 在 Sequence 中移除指定 Actor 的可见性关键帧
	 * @param LevelSeqPath			Level Sequence 路径
	 * @param ActorGuid				所指定 Actor 在 Sequence 中的 GUID
	 * @param Frame					帧数
	 * @return 是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | KeyFrame")
	static bool RemoveVisibilityKeyframe(const FString& LevelSeqPath,
	                                     const FGuid& ActorGuid,
	                                     int Frame);

#pragma endregion

#pragma region MatParameter

	/**
	 * 在 Level Sequence 中添加 MaterialParameterCollection 中含有的材质参数关键帧
	 * @param LevelSeqPath							Level Sequence 路径
	 * @param MaterialParameterCollection			指定所需要增加关键帧的 MPC
	 * @param Parameter								指定需要的材质变量
	 * @param Frame									帧数
	 * @param bColor								是否是 Color 变量
	 * @param ScalarValue							如果是浮点变量则在此处指定数值
	 * @param ColorValue							如果是颜色变量则在此处指定对应颜色
	 * @return 是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Material")
	static bool AddMPCKeyframe(const FString& LevelSeqPath,
	                           UMaterialParameterCollection* MaterialParameterCollection,
	                           const FString& Parameter,
	                           int Frame,
	                           bool bColor,
	                           float ScalarValue = 0.f,
	                           FLinearColor ColorValue = FLinearColor(1.f, 1.f, 1.f));

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Material")
	static UMovieSceneSection* AddMISwitcherSection(const FString& LevelSeqPath,
	                                                FGuid ExistingMeshGuid,
	                                                int32 MaterialIndex);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Material")
	static bool AddKeyframeToMISwitcherSection(UMovieSceneSection* Section,
	                                                 int32 Frame,
	                                                 UMaterialInstanceConstant* MaterialInstanceConstant);

#pragma endregion

#pragma region BPVariables

	/**
	 * 在 Level Sequence 中添加指定 Actor 的成员变量关键帧，需要在蓝图中设置该变量的 Detail 中的 Instance Editable 以及 Expose to Cinematic 为 True，目前只支持 bool 和 float 两种类型，后续考虑加入泛型支持更多变量类型
	 * @param LevelSeqPath				Level Sequence 路径
	 * @param ActorGuid					所指定 Actor 在 Sequence 中的 GUID
	 * @param Frame						帧数
	 * @param VariableName				需要增加关键帧的变量名
	 * @param VariableType				变量类型，0-Bool、1-Float
	 * @param BoolPropertyValue			如果是 Bool 变量，该变量的关键帧设置
	 * @param FloatPropertyValue		如果是 Float 变量，该变量的关键帧设置
	 * @return 是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | KeyFrame")
	static bool AddBPVariableKeyframe(const FString& LevelSeqPath,
	                                  const FGuid& ActorGuid,
	                                  int Frame,
	                                  const FString& VariableName,
	                                  int VariableType,
	                                  bool BoolPropertyValue = false,
	                                  float FloatPropertyValue = 0.f);

#pragma endregion
};

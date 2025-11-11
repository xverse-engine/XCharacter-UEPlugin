// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SequenceBPLib.generated.h"

class ULevelSequence;
class UMovieSceneCameraCutTrack;
class UMovieSceneSection;

/**
 * 
 */
UCLASS()
class XSEQUENCER_API USequenceBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
    * 返回传入的 Level Sequence 路径对应的 Sequence
    * @param SequencePath		Level Sequence 路径
    * @return 对应的 Level Sequence
    */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Main")
	static ULevelSequence* GetLevelSequenceByPath(FString& SequencePath);

	/**
	 * 创建一个 Level Sequence，并在当前关卡中打开
	 * @param CreatedSequencePath		返回创建的 Level Sequence 路径
	 * @param OpenInEditor				创建之后是否在 Editor 中打开该 Sequence
	 * @param Prefix					所创建的 Sequence 文件前缀名，一般分为 Main 和 Sub
	 * @return 是否创建成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Main")
	static bool CreateLevelSequence(FString& CreatedSequencePath,
	                                bool OpenInEditor,
	                                const FString& Prefix = "Sub");

	/**
	 * 修改 Level Sequence 的渲染范围
	 * 
	 * @param LevelSeqPath			Sequence 文件路径
	 * @param StartTime				起始时间，默认为 0
	 * @param EndTime				结束时间
	 * @param FrameRate				设定该 Sequence 渲染帧率，默认 60
	 * @return 是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Main")
	static bool SetSequenceRenderParameters(const FString& LevelSeqPath,
	                                        float StartTime,
	                                        float EndTime,
	                                        int32 FrameRate = 60);

	/**
	 * 增加人物角色到指定 Sequence
	 * @param LevelSeqPath			Sequence 路径
	 * @param CharName				角色名字以及添加到 Sequence 中的名字
	 * @param MeshPath				角色模型路径
	 * @return 返回加入到 Sequence 中的角色 GUID，失败则为空
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Main")
	static FGuid AddCharacterToSequenceByPath(const FString& LevelSeqPath,
	                                          const FString& CharName,
	                                          const FString& MeshPath);

	/**
 	 * 增加人物角色到指定 Sequence
 	 * @param LevelSeqPath			Sequence 路径
 	 * @param CharName				角色名字以及添加到 Sequence 中的名字
 	 * @param Mesh					角色模型
 	 * @return 返回加入到 Sequence 中的角色 GUID，失败则为空
 	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | Main")
	static FGuid AddCharacterToSequenceByObject(const FString& LevelSeqPath,
	                                            const FString& CharName,
	                                            USkeletalMesh* Mesh);

#pragma region GetGuid|Add|Remove

	/**
	 * 获取指定 Level Sequence 中所有的 GUID
	 * @param LevelSeqPath		Level Sequence 路径
	 * @return 该 Sequence 中所有 GUID 所组成的 TArray，获取失败则为空
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | GUID")
	static TArray<FGuid> GetAllGuidsFromSequence(const FString& LevelSeqPath);

	/**
	 * 通过 GUID 获取 Level Sequence 中该 GUID 所对应的 Actor 指针
	 * @param LevelSeqPath		Level Sequence 路径
	 * @param ActorGuid			所需要获得的 Actor 的 GUID
	 * @return Actor 指针，获取失败则返回 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | GUID")
	static AActor* GetPossessableActorUsingGuidFromSequence(const FString& LevelSeqPath,
	                                                        const FGuid& ActorGuid);

	/**
	 * 获取指定 Level Sequence 中所添加的 Spawnable Actor 的 GUID，Spawnable Actor 相当于从 Content 中直接拖入 Sequencer
	 * @param LevelSeqPath				Level Sequence 路径
	 * @param SpawnableName				在 Sequence 中生成的 Spawnable Actor 名称
	 * @return 返回所获取的 Sequence 中的 Actor 的 GUID，失败则为空 FGuid
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | GUID")
	static FGuid GetSpawnableGuidFromSequence(const FString& LevelSeqPath,
	                                          const FString& SpawnableName);

	/**
	 * 向 Level Sequence 中添加 Spawnable Actor，Spawnable Actor 相当于从 Content 中直接拖入 Sequencer
	 * @param LevelSeqPath				Level Sequence 路径
	 * @param AssetPath					所需要引用的素材路径，需要是可以直接拉入场景中的类
	 * @param SpawnableName				在 Sequence 中生成的 Spawnable Actor 名称
	 * @return 所添加对象的 GUID
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | GUID")
	static FGuid AddSpawnableToSequence(const FString& LevelSeqPath,
	                                    const FString& AssetPath,
	                                    const FString& SpawnableName);

	/**
 	 * 向 Level Sequence 中添加 Spawnable Actor，Spawnable Actor 相当于从 Content 中直接拖入 Sequencer
 	 * @param LevelSeqPath				Level Sequence 路径
 	 * @param Object					所需要引用的素材引用
 	 * @param SpawnableName				在 Sequence 中生成的 Spawnable Actor 名称
 	 * @return 所添加对象的 GUID
 	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | GUID")
	static FGuid AddSpawnableToSequenceByObject(const FString& LevelSeqPath,
	                                            UObject* Object,
	                                            const FString& SpawnableName);

	/**
	 * 从 Level Sequence 中移除 Spawnable Actor
	 * @param LevelSeqPath				Level Sequence 路径
	 * @param SpawnableName				需要移除的 Spawnable Actor 名称
	 * @return 移除是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | GUID")
	static bool RemoveSpawnableFromSequence(const FString& LevelSeqPath,
	                                        const FString& SpawnableName);

	/**
	 * 获取指定 Level Sequence 中所添加的 Possessable Actor 的 GUID，Possessable Actor 即为在场景中存在的 Actor
	 * @param LevelSeqPath				Level Sequence 路径
	 * @param Actor						需要查找的 Actor 引用
	 * @return 返回所获取的 Sequence 中的 Actor 的 GUID，失败则为空 FGuid
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | GUID")
	static FGuid GetPossessableGuidFromSeq(const FString& LevelSeqPath,
	                                       AActor* Actor);

	/**
    * 获取指定 Level Sequence 中所添加的 Possessable Actor 的 GUID，Possessable Actor 即为在场景中存在的 Actor
    * @param LevelSeqPath				Level Sequence 路径
    * @param Object						需要查找的 Object 引用
    * @return 返回所获取的 Sequence 中的 Object 的 GUID，失败则为空 FGuid
    */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | GUID")
	static FGuid GetPossessableGuidFromSeqUObject(const FString& LevelSeqPath,
	                                              UObject* Object);

	/**
	 * 向 Level Sequence 中添加 Possessable Actor，Possessable Actor 即为在场景中存在的 Actor
	 * @param LevelSeqPath				Level Sequence 路径
	 * @param Actor						需要加入 Sequence 的 Actor 引用
	 * @return 所添加对象的 GUID
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | GUID")
	static FGuid AddPossessableToSequence(const FString& LevelSeqPath,
	                                      AActor* Actor);

	/**
    * 向 Level Sequence 中添加 Possessable UObject
    * @param LevelSeqPath				Level Sequence 路径
    * @param Object						需要加入 Sequence 的 UObject 引用
    * @return 所添加对象的 GUID
    */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | GUID")
	static FGuid AddPossessableToSequenceUObject(const FString& LevelSeqPath,
	                                             UObject* Object);

	/**
	 * 从 Level Sequence 中移除 Possessable Actor
	 * @param LevelSeqPath				Level Sequence 路径
	 * @param Actor						需要从 Sequence 移除的 Actor 引用
	 * @return 移除是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | GUID")
	static bool RemovePossessableFromSequence(const FString& LevelSeqPath,
	                                          AActor* Actor);
#pragma endregion

#pragma region AddAnimAudio

	/**
	 * 向 Sequence 已有的 Actor 轨道中增加动画轨道以及动画
	 * @param LevelSeqPath				Level Sequence 路径
	 * @param AnimPath					需要添加的 Animation Sequence 的所在路径
	 * @param ActorGuid					指定需要添加动画的 Actor 的 GUID
	 * @param StartFrame				动画在 Sequence 中的起始帧
	 * @param EndFrame					动画在 Sequence 中的结束帧，默认 -1 即保持动画原本长度，需要改变长度则手动指定结束帧
	 * @param bUseAutoEase				是否使用动画自动淡入淡出
	 * @param EaseInTime				淡入时间，默认 0.5 秒
	 * @param EaseOutTime				淡出时间，默认 1.5 秒
	 * @return 动画添加是否成功
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | AddThings")
	static bool AddAnimationToLevelSequence(const FString& LevelSeqPath,
	                                        const FString& AnimPath,
	                                        const FGuid& ActorGuid,
	                                        int32 StartFrame = 0,
	                                        int32 EndFrame = -1,
	                                        bool bUseAutoEase = true,
	                                        float EaseInTime = 0.5f,
	                                        float EaseOutTime = 1.5f);

	/**
	 * 向 Sequence 中添加音频轨道以及音频
	 * @param LevelSeqPath				Level Sequence 路径
	 * @param AudioPath					需要添加的 Sound Wave 的所在路径
	 * @param StartFrame				音频在 Sequence 中的起始帧
	 * @param EndFrame					音频在 Sequence 中的结束帧，默认 -1 即保持动画原本长度，需要改变长度则手动指定结束帧
	 * @return 所添加音频的时长，添加失败则返回 0
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | AddThings")
	static float AddAudioToLevelSequence(
		const FString& LevelSeqPath,
		const FString& AudioPath,
		int32 StartFrame = 0,
		int32 EndFrame = -1);

	UFUNCTION(BlueprintCallable, Category = "AutoSequencer | AddThings")
	static float AddSoundWaveToLevelSequence(
		const FString& LevelSeqPath,
		USoundWave* SoundWave,
		int32 StartFrame = 0,
		int32 EndFrame = -1);

#pragma endregion
};

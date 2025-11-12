// Copyright Epic Games, Inc. All Rights Reserved.

#include "TTS/AudioAssetOperation.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Sections/MovieSceneSubSection.h"
#include "MovieSceneFolder.h"
#include "MovieSceneNameableTrack.h"
#include "Tools/GenTools.h"
#include "Math/UnrealMathUtility.h"
#include "Interfaces/IPluginManager.h"
#include "AssetToolsModule.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "XVCPluginSettings.h"

#if ENGINE_MAJOR_VERSION == 5 && (ENGINE_MINOR_VERSION <= 2)
#include "AssetRegistryModule.h"
#else
#endif

#include "AssetRegistry/AssetRegistryModule.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

// RuntimeAudioImporter 相关头文件（用于高质量音频重采样）
#include "RuntimeAudioImporterLibrary.h"
#include "RuntimeAudioExporter.h"

#if WITH_EDITOR
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#endif

// 获取所有音频轨道信息（包括嵌套轨道）
TArray<FAudioTrackInfo> UAudioAssetOperation::GetAllAudioTracksInfo(ULevelSequence* LevelSequence)
{
    TArray<FAudioTrackInfo> TrackInfos;
    if (LevelSequence && LevelSequence->GetMovieScene())
    {
        UMovieScene* MovieScene = LevelSequence->GetMovieScene();
        GetAllAudioTracksRecursive(MovieScene, TrackInfos, TEXT(""), 0);
        
        // 重新编号轨道，确保编号顺序与显示顺序一致
        RenumberTracksByDisplayOrder(TrackInfos);
    }
    return TrackInfos;
}

// 递归获取所有音频轨道信息
void UAudioAssetOperation::GetAllAudioTracksRecursive(UMovieScene* MovieScene, TArray<FAudioTrackInfo>& OutTrackInfos, const FString& ParentPath, int32 CurrentLevel)
{
    if (!MovieScene)
    {
        return;
    }

    // 注：命名规范依赖于文件夹/对象绑定/根级别上下文，
    // 不在此处先行生成条目，避免统一被标记为 root。

    // 2. 处理文件夹中的轨道（UE的文件夹分组机制）
    TArrayView<UMovieSceneFolder* const> RootFolders = MovieScene->GetRootFolders();
    for (UMovieSceneFolder* Folder : RootFolders)
    {
        if (Folder)
        {
            UE_LOG(LogTemp, Log, TEXT("处理文件夹分组: %s"), *Folder->GetFolderName().ToString());
            ProcessFolderTracks(Folder, MovieScene, OutTrackInfos, ParentPath, CurrentLevel);
        }
    }

    // 3. 处理根级别的音频轨道（不在文件夹中的轨道）
    const TArray<UMovieSceneTrack*>& RootTracks = MovieScene->GetTracks();
    int32 RootAudioCount = 0;
    for (UMovieSceneTrack* RootTrack : RootTracks)
    {
        if (Cast<UMovieSceneAudioTrack>(RootTrack))
        {
            ++RootAudioCount;
        }
    }

    int32 RootAudioOrdinal = 0;
    for (int32 RootIdx = 0; RootIdx < RootTracks.Num(); ++RootIdx)
    {
        UMovieSceneTrack* RootTrack = RootTracks[RootIdx];
        if (UMovieSceneAudioTrack* AudioTrack = Cast<UMovieSceneAudioTrack>(RootTrack))
        {
            // 检查是否已经在视觉嵌套处理中处理过了
            bool bAlreadyProcessed = false;
            for (const FAudioTrackInfo& ExistingTrack : OutTrackInfos)
            {
                if (ExistingTrack.AudioTrack == AudioTrack)
                {
                    bAlreadyProcessed = true;
                    break;
                }
            }
            
            if (!bAlreadyProcessed)
            {
                int32 Ordinal = -1;
                if (RootAudioCount > 1)
                {
                    ++RootAudioOrdinal;
                    Ordinal = RootAudioOrdinal;
                }
                AddAudioTrackEntries(AudioTrack, MovieScene, ParentPath, CurrentLevel, OutTrackInfos, TEXT("root"), TEXT(""), Ordinal);
            }
        }
    }

    // 4. 处理对象绑定中的音频轨道（UE的对象绑定分组机制）
    for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
    {
        FGuid ObjectBinding = Binding.GetObjectGuid();
        if (ObjectBinding.IsValid())
        {
            UE_LOG(LogTemp, Log, TEXT("处理对象绑定分组: %s"), *ObjectBinding.ToString());
            const FString BindingDisplayName = GetBindingNameByGuid(MovieScene, ObjectBinding);

            // 获取该ObjectBinding的所有音频轨道
            const TArray<UMovieSceneTrack*>& ActorTracks = MovieScene->FindTracks(UMovieSceneAudioTrack::StaticClass(), ObjectBinding);
            
            // 如果同一个对象绑定中有多个音频轨道，将它们组织为组轨道
            if (ActorTracks.Num() > 1)
            {
                FString GroupPath = ParentPath.IsEmpty() ? BindingDisplayName : ParentPath + TEXT(" > ") + BindingDisplayName;
                
                // 处理该对象绑定下的每个音频轨道（不再创建虚拟组节点）
                for (int32 i = 0; i < ActorTracks.Num(); ++i)
                {
                    if (UMovieSceneAudioTrack* AudioTrack = Cast<UMovieSceneAudioTrack>(ActorTracks[i]))
                    {
                        // 检查是否已经在视觉嵌套处理中处理过了
                        bool bAlreadyProcessed = false;
                        for (const FAudioTrackInfo& ExistingTrack : OutTrackInfos)
                        {
                            if (ExistingTrack.AudioTrack == AudioTrack)
                            {
                                bAlreadyProcessed = true;
                                break;
                            }
                        }
                        
                        if (!bAlreadyProcessed)
                        {
                            // 传入对象绑定显示名并按序号编号（多于1条时编号）
                            AddAudioTrackEntries(AudioTrack, MovieScene, GroupPath, CurrentLevel + 1, OutTrackInfos, TEXT(""), BindingDisplayName, i + 1);
                        }
                    }
                }
            }
            else if (ActorTracks.Num() == 1)
            {
                // 单个音频轨道，作为普通轨道处理
                if (UMovieSceneAudioTrack* AudioTrack = Cast<UMovieSceneAudioTrack>(ActorTracks[0]))
                {
                    // 检查是否已经在视觉嵌套处理中处理过了
                    bool bAlreadyProcessed = false;
                    for (const FAudioTrackInfo& ExistingTrack : OutTrackInfos)
                    {
                        if (ExistingTrack.AudioTrack == AudioTrack)
                        {
                            bAlreadyProcessed = true;
                            break;
                        }
                    }
                    
                    if (!bAlreadyProcessed)
                    {
                        // 单条时不编号
                        AddAudioTrackEntries(AudioTrack, MovieScene, ParentPath, CurrentLevel, OutTrackInfos, TEXT(""), BindingDisplayName, -1);
                    }
                }
            }
        }
    }
}

// 处理文件夹中的轨道
void UAudioAssetOperation::ProcessFolderTracks(UMovieSceneFolder* Folder, UMovieScene* MovieScene, TArray<FAudioTrackInfo>& OutTrackInfos, const FString& ParentPath, int32 CurrentLevel)
{
    if (!Folder)
    {
        return;
    }

    FString FolderName = Folder->GetFolderName().ToString().TrimStartAndEnd();
    FString CurrentPath = ParentPath.IsEmpty() ? FolderName : ParentPath + TEXT(" > ") + FolderName;
    
    UE_LOG(LogTemp, Log, TEXT("处理文件夹分组: %s (层级: %d)"), *FolderName, CurrentLevel);

    // 1. 处理文件夹中的对象绑定（UE的文件夹分组机制）
    for (const FGuid& ObjectBinding : Folder->GetChildObjectBindings())
    {
        const TArray<UMovieSceneTrack*>& FolderTracks = MovieScene->FindTracks(UMovieSceneAudioTrack::StaticClass(), ObjectBinding);
        const FString BindingDisplayName = GetBindingNameByGuid(MovieScene, ObjectBinding);
        
        // 统计同名轨道的数量，用于编号
        TMap<FString, int32> TrackNameCounts;
        for (int32 i = 0; i < FolderTracks.Num(); ++i)
        {
            if (UMovieSceneAudioTrack* AudioTrack = Cast<UMovieSceneAudioTrack>(FolderTracks[i]))
            {
                FString TrackName = AudioTrack->GetDisplayName().ToString();
                TrackNameCounts.FindOrAdd(TrackName)++;
            }
        }
        
        // 为每个轨道分配编号
        TMap<FString, int32> TrackNameOrdinals;
        for (int32 i = 0; i < FolderTracks.Num(); ++i)
        {
            if (UMovieSceneAudioTrack* AudioTrack = Cast<UMovieSceneAudioTrack>(FolderTracks[i]))
            {
                FString TrackName = AudioTrack->GetDisplayName().ToString();
                int32& Ordinal = TrackNameOrdinals.FindOrAdd(TrackName);
                Ordinal++;
                
                // 如果同名轨道只有一个，不编号；如果有多个，按顺序编号
                int32 FinalOrdinal = (TrackNameCounts[TrackName] > 1) ? Ordinal : -1;
                AddAudioTrackEntries(AudioTrack, MovieScene, CurrentPath, CurrentLevel + 1, OutTrackInfos, FolderName, BindingDisplayName, FinalOrdinal);
            }
        }
    }

    // 1.5 处理文件夹中的主轨（Master Tracks）
    // 注意：GetChildMasterTracks() 方法在 UE5.5 中已被移除
    // 如果需要处理主轨，可能需要使用其他 API 或暂时跳过
    
    {
        // UE API: GetChildMasterTracks() 返回该文件夹直接包含的主轨集合
        const auto& ChildMasterTracks = Folder->GetChildMasterTracks();
        
        // 统计同名轨道的数量，用于编号
        TMap<FString, int32> MasterTrackNameCounts;
        for (UMovieSceneTrack* Track : ChildMasterTracks)
        {
            if (UMovieSceneAudioTrack* AudioTrack = Cast<UMovieSceneAudioTrack>(Track))
            {
                FString TrackName = AudioTrack->GetDisplayName().ToString();
                MasterTrackNameCounts.FindOrAdd(TrackName)++;
            }
        }
        
        // 为每个主轨分配编号
        TMap<FString, int32> MasterTrackNameOrdinals;
        for (UMovieSceneTrack* Track : ChildMasterTracks)
        {
            if (UMovieSceneAudioTrack* AudioTrack = Cast<UMovieSceneAudioTrack>(Track))
            {
                FString TrackName = AudioTrack->GetDisplayName().ToString();
                int32& Ordinal = MasterTrackNameOrdinals.FindOrAdd(TrackName);
                Ordinal++;
                
                // 如果同名轨道只有一个，不编号；如果有多个，按顺序编号
                int32 FinalOrdinal = (MasterTrackNameCounts[TrackName] > 1) ? Ordinal : -1;
                AddAudioTrackEntries(AudioTrack, MovieScene, CurrentPath, CurrentLevel + 1, OutTrackInfos, FolderName, TEXT(""), FinalOrdinal);
            }
        }
    }
    

    // 2. 递归处理子文件夹（UE的嵌套文件夹分组机制）
    TArrayView<UMovieSceneFolder* const> ChildFolders = Folder->GetChildFolders();
    for (UMovieSceneFolder* ChildFolder : ChildFolders)
    {
        if (ChildFolder)
        {
            ProcessFolderTracks(ChildFolder, MovieScene, OutTrackInfos, CurrentPath, CurrentLevel + 1);
        }
    }
}


// 获取轨道列表（兼容旧版本）
TArray<FString> UAudioAssetOperation::GetTracksFromLevelSequence(ULevelSequence* LevelSequence)
{
    TArray<FString> TrackNames;
    TArray<FAudioTrackInfo> TrackInfos = GetAllAudioTracksInfo(LevelSequence);
    
    for (const FAudioTrackInfo& TrackInfo : TrackInfos)
    {
        TrackNames.Add(TrackInfo.DisplayName);
    }
    
    return TrackNames;
}

// 工具：通过对象绑定 Guid 获取其显示名称
FString UAudioAssetOperation::GetBindingNameByGuid(UMovieScene* MovieScene, const FGuid& ObjectBinding)
{
    if (!MovieScene)
    {
        return FString();
    }

    for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
    {
        if (Binding.GetObjectGuid() == ObjectBinding)
        {
            const FString Name = Binding.GetName().TrimStartAndEnd();
            return Name.IsEmpty() ? ObjectBinding.ToString() : Name;
        }
    }

    return ObjectBinding.ToString();
}

// 创建新的音轨并应用合并后的音频（保留原有音频）
bool UAudioAssetOperation::CreateNewTrackWithMergedAudio(ULevelSequence* LevelSequence, const FString& SelectedTrack, USoundWave* MergedAudio)
{
    if (!LevelSequence || SelectedTrack.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("LevelSequence或SelectedTrack无效"));
        return false;
    }

    if (!MergedAudio)
    {
        UE_LOG(LogTemp, Error, TEXT("MergedAudio为空"));
        return false;
    }

    if (!LevelSequence->GetMovieScene())
    {
        UE_LOG(LogTemp, Error, TEXT("LevelSequence无效"));
        return false;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    FFrameRate TickResolution = MovieScene->GetTickResolution();
    
    // 获取所有音频轨道信息（包含按 Row 拆分后的条目）
    TArray<FAudioTrackInfo> TrackInfos = UAudioAssetOperation::GetAllAudioTracksInfo(LevelSequence);
    
    // 查找匹配的音频轨道，获取其信息用于创建新轨道
    FAudioTrackInfo* SourceTrackInfo = nullptr;
    for (FAudioTrackInfo& TrackInfo : TrackInfos)
    {
        if (TrackInfo.DisplayName == SelectedTrack && TrackInfo.AudioTrack)
        {
            SourceTrackInfo = &TrackInfo;
            break;
        }
    }
    
    if (!SourceTrackInfo)
    {
        UE_LOG(LogTemp, Error, TEXT("未找到匹配的音频轨道: %s"), *SelectedTrack);
        return false;
    }
    
    UMovieSceneAudioTrack* SourceAudioTrack = SourceTrackInfo->AudioTrack;
    if (!SourceAudioTrack)
    {
        UE_LOG(LogTemp, Error, TEXT("源音频轨道无效"));
        return false;
    }
    
    // 创建新行的名称（基于选中的行）
    FString NewRowName = TEXT("Merged");
    if (SourceTrackInfo->RowIndex != INDEX_NONE)
    {
        // 获取该行的自定义名称
        FString RowName = GetRowDisplayName(SourceAudioTrack, SourceTrackInfo->RowIndex);
        if (!RowName.IsEmpty())
        {
            // 使用行名称创建新行名称
            NewRowName = FString::Printf(TEXT("%s_Merged"), *RowName);
        }
        else
        {
            // 使用行索引创建新行名称
            NewRowName = FString::Printf(TEXT("Row_%d_Merged"), SourceTrackInfo->RowIndex);
        }
    }
    
    // 将合并后的音频添加到源轨道的新行中
    bool bSectionAdded = false;
    
    // 确定新片段的Row索引
    int32 NewRowIndex = 0;
    if (SourceTrackInfo->RowIndex != INDEX_NONE)
    {
        // 如果源轨道有特定的Row，新片段使用下一个可用的Row
        NewRowIndex = SourceTrackInfo->RowIndex + 1;
        
        // 检查该Row是否已被占用，如果被占用则找到下一个可用Row
        TArray<UMovieSceneSection*> AllSections = SourceAudioTrack->GetAllSections();
        TSet<int32> UsedRows;
        for (UMovieSceneSection* Section : AllSections)
        {
            if (Section)
            {
                UsedRows.Add(Section->GetRowIndex());
            }
        }
        
        // 找到下一个可用的Row索引
        while (UsedRows.Contains(NewRowIndex))
        {
            NewRowIndex++;
        }
    }
    else
    {
        // 如果源轨道没有特定的Row，新片段使用Row 1（Row 0通常用于原始内容）
        NewRowIndex = 1;
    }
    
    // 直接在源轨道上添加新的音频片段，使用新的Row索引
    UMovieSceneAudioSection* NewSection = NewObject<UMovieSceneAudioSection>(SourceAudioTrack, NAME_None, RF_Transactional);
    if (NewSection)
    {
        // 设置音频
        NewSection->SetSound(MergedAudio);
        
        // 设置音频片段的时长范围
        FFrameNumber StartFrame(0);
        int32 EndFrameValue = FMath::RoundToInt(MergedAudio->Duration * TickResolution.AsDecimal());
        FFrameNumber EndFrame(EndFrameValue);
        NewSection->SetRange(TRange<FFrameNumber>(StartFrame, EndFrame));
        
        // 设置到新的Row索引
        NewSection->SetRowIndex(NewRowIndex);
        
        // 将新片段添加到源轨道
        SourceAudioTrack->AddSection(*NewSection);
        
        // 为新行设置自定义名称
        // 使用UMovieSceneNameableTrack的SetTrackRowDisplayName方法
        if (UMovieSceneNameableTrack* NameableTrack = Cast<UMovieSceneNameableTrack>(SourceAudioTrack))
        {
            NameableTrack->SetTrackRowDisplayName(FText::FromString(NewRowName), NewRowIndex);
            UE_LOG(LogTemp, Log, TEXT("成功为Row %d 设置自定义名称: %s"), NewRowIndex, *NewRowName);
        }
        
        UE_LOG(LogTemp, Log, TEXT("成功在轨道 '%s' 的Row %d 上添加合并后的音频片段，行名称: %s"), 
               *SourceAudioTrack->GetDisplayName().ToString(), NewRowIndex, *NewRowName);
        
        NewSection->Modify();
        bSectionAdded = true;
    }
    
    if (!bSectionAdded)
    {
        UE_LOG(LogTemp, Error, TEXT("无法创建新的音频片段"));
        return false;
    }
    
    // 标记为已修改
    LevelSequence->Modify();
    MovieScene->Modify();
    
    UE_LOG(LogTemp, Log, TEXT("成功在轨道 '%s' 中添加合并后的音频，新行名称: %s"), 
           *SourceAudioTrack->GetDisplayName().ToString(), *NewRowName);
    
    // 自动保存LevelSequence
    FString PackageFileName = FPackageName::LongPackageNameToFilename(LevelSequence->GetPackage()->GetName(), FPackageName::GetAssetPackageExtension());
    bool bSaved = UPackage::SavePackage(LevelSequence->GetPackage(), LevelSequence, *PackageFileName, FSavePackageArgs{});
    if (bSaved)
    {
        UE_LOG(LogTemp, Log, TEXT("LevelSequence已自动保存"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("LevelSequence自动保存失败"));
    }
    
    return true;
}

// 获取当前轨道上的音频片段数量
int32 UAudioAssetOperation::GetAudioSegmentCount(ULevelSequence* LevelSequence, const FString& SelectedTrack)
{
    TArray<FAudioSegment> Segments = GetAudioSegmentsFromTrack(LevelSequence, SelectedTrack);
    return Segments.Num();
}

// 获取指定索引的音频片段
bool UAudioAssetOperation::GetAudioSegmentByIndex(ULevelSequence* LevelSequence, const FString& SelectedTrack, int32 Index, FAudioSegment& OutSegment)
{
    TArray<FAudioSegment> Segments = GetAudioSegmentsFromTrack(LevelSequence, SelectedTrack);
    if (Index >= 0 && Index < Segments.Num())
    {
        OutSegment = Segments[Index];
        return true;
    }
    return false;
}

// 获取Camera Cut Track的时间范围
TArray<FCameraCutSegment> UAudioAssetOperation::GetCameraCutSegments(ULevelSequence* LevelSequence)
{
    return GetCameraCutSegmentsInternal(LevelSequence);
}

// 获取Camera Cut Track的时间范围（内部使用）
TArray<FCameraCutSegment> UAudioAssetOperation::GetCameraCutSegmentsInternal(ULevelSequence* LevelSequence)
{
    TArray<FCameraCutSegment> CameraCutSegments;
    if (!LevelSequence || !LevelSequence->GetMovieScene())
    {
        return CameraCutSegments;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    UMovieSceneTrack* CameraCutTrackRaw = MovieScene->GetCameraCutTrack();
    UMovieSceneCameraCutTrack* CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(CameraCutTrackRaw);
    
    if (!CameraCutTrack)
    {
        UE_LOG(LogTemp, Warning, TEXT("No Camera Cut Track found in LevelSequence"));
        return CameraCutSegments;
    }

    // 获取所有Camera Cut片段
    TArray<UMovieSceneSection*> AllSections = CameraCutTrack->GetAllSections();
    if (AllSections.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No Camera Cut sections found in Camera Cut Track"));
        return CameraCutSegments;
    }

    for (UMovieSceneSection* Section : AllSections)
    {
        UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(Section);
        if (CameraCutSection)
        {
            FCameraCutSegment Segment;
            TRange<FFrameNumber> SectionRange = Section->GetTrueRange();
            Segment.StartTime = SectionRange.GetLowerBoundValue();
            Segment.EndTime = SectionRange.GetUpperBoundValue();
            CameraCutSegments.Add(Segment);
            
            UE_LOG(LogTemp, Log, TEXT("Camera Cut Segment: Start=%d, End=%d"), 
                   Segment.StartTime.Value, Segment.EndTime.Value);
        }
    }

    // 对Camera Cut片段按开始时间排序
    CameraCutSegments.Sort([](const FCameraCutSegment& A, const FCameraCutSegment& B) {
        return A.StartTime < B.StartTime;
    });

    UE_LOG(LogTemp, Log, TEXT("Found %d Camera Cut segments"), CameraCutSegments.Num());
    
    // 如果只有一个片段，输出详细信息
    if (CameraCutSegments.Num() == 1)
    {
        const FCameraCutSegment& Segment = CameraCutSegments[0];
        float Duration = (Segment.EndTime.Value - Segment.StartTime.Value) / MovieScene->GetTickResolution().AsDecimal();
        UE_LOG(LogTemp, Log, TEXT("Single Camera Cut: Duration=%.2f seconds, StartFrame=%d, EndFrame=%d"), 
               Duration, Segment.StartTime.Value, Segment.EndTime.Value);
    }

    return CameraCutSegments;
}

// 获取当前轨道上的所有音频片段（内部使用）
TArray<FAudioSegment> UAudioAssetOperation::GetAudioSegmentsFromTrack(ULevelSequence* LevelSequence, const FString& SelectedTrack)
{
    TArray<FAudioSegment> Segments;
    if (!LevelSequence || SelectedTrack.IsEmpty())
    {
        return Segments;
    }

    if (!LevelSequence->GetMovieScene())
    {
        return Segments;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    FFrameRate TickResolution = MovieScene->GetTickResolution();

    // 获取所有音频轨道信息（包含按 Row 拆分后的条目）
    TArray<FAudioTrackInfo> TrackInfos = GetAllAudioTracksInfo(LevelSequence);
    
    UE_LOG(LogTemp, Log, TEXT("GetAudioSegmentsFromTrack: 查找轨道 '%s'，总轨道数: %d"), *SelectedTrack, TrackInfos.Num());

    // 查找匹配的音频轨道
    for (const FAudioTrackInfo& TrackInfo : TrackInfos)
    {
        UE_LOG(LogTemp, Log, TEXT("检查轨道: '%s' vs '%s', RowIndex: %d"), *TrackInfo.DisplayName, *SelectedTrack, TrackInfo.RowIndex);
        
        if (TrackInfo.DisplayName == SelectedTrack && TrackInfo.AudioTrack)
        {
            UE_LOG(LogTemp, Log, TEXT("找到匹配轨道: %s, RowIndex: %d"), *TrackInfo.DisplayName, TrackInfo.RowIndex);
            
            UMovieSceneAudioTrack* AudioTrack = TrackInfo.AudioTrack;
            
            // 如果指定了具体的 Row，只获取该 Row 的 Sections
            if (TrackInfo.RowIndex != INDEX_NONE)
            {
            for (UMovieSceneSection* Section : AudioTrack->GetAllSections())
                {
                    if (Section && Section->GetRowIndex() == TrackInfo.RowIndex)
            {
                UMovieSceneAudioSection* AudioSection = Cast<UMovieSceneAudioSection>(Section);
                if (AudioSection)
                {
                    FAudioSegment Segment;
                    TRange<FFrameNumber> SectionRange = Section->GetTrueRange();
                    Segment.StartTime = SectionRange.GetLowerBoundValue();
                    Segment.EndTime = SectionRange.GetUpperBoundValue();
                    Segment.SoundWave = Cast<USoundWave>(AudioSection->GetSound());
                    
                    // 获取音频片段的偏移信息（音频在时间轴上的实际开始时间）
                    // 这可以帮助我们确定用户想要保留音频的哪一部分
                    FFrameNumber AudioOffset = AudioSection->GetStartOffset();
                    Segment.AudioOffset = AudioOffset;
                    
                    Segments.Add(Segment);
                        }
                    }
                }
            }
            else
            {
                // 获取所有 Sections，按时间顺序排序
                TArray<UMovieSceneSection*> AllSections;
                for (UMovieSceneSection* Section : AudioTrack->GetAllSections())
                {
                    if (Section)
                    {
                        AllSections.Add(Section);
                    }
                }
                
                // 按开始时间排序
                AllSections.Sort([](const UMovieSceneSection& A, const UMovieSceneSection& B) {
                    return A.GetRange().GetLowerBoundValue() < B.GetRange().GetLowerBoundValue();
                });
                
                for (UMovieSceneSection* Section : AllSections)
                {
                    UMovieSceneAudioSection* AudioSection = Cast<UMovieSceneAudioSection>(Section);
                    if (AudioSection)
                    {
                        FAudioSegment Segment;
                        TRange<FFrameNumber> SectionRange = Section->GetTrueRange();
                        Segment.StartTime = SectionRange.GetLowerBoundValue();
                        Segment.EndTime = SectionRange.GetUpperBoundValue();
                        Segment.SoundWave = Cast<USoundWave>(AudioSection->GetSound());
                        
                        // 获取音频片段的偏移信息
                        FFrameNumber AudioOffset = AudioSection->GetStartOffset();
                        Segment.AudioOffset = AudioOffset;
                        
                        Segments.Add(Segment);
                    }
                }
            }
            break;
        }
    }

    // 对音频片段按开始时间排序
    Segments.Sort([](const FAudioSegment& A, const FAudioSegment& B) {
        return A.StartTime < B.StartTime;
    });

    return Segments;
}

// 合并音频片段（自动上采样到最高采样率）
bool UAudioAssetOperation::MergeAudioSegments(ULevelSequence* LevelSequence, const FString& SelectedTrack, USoundWave*& OutMergedAudio, int32 UserTargetSampleRate)
{
    // 1. 获取音频片段
    UE_LOG(LogTemp, Log, TEXT("MergeAudioSegments: 开始处理轨道 '%s'"), *SelectedTrack);
    TArray<FAudioSegment> Segments = GetAudioSegmentsFromTrack(LevelSequence, SelectedTrack);
    UE_LOG(LogTemp, Log, TEXT("MergeAudioSegments: 获取到 %d 个音频片段"), Segments.Num());
    
    if (Segments.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("No audio segments found on selected track!"));
        return false;
    }

    // 2. 获取Camera Cut片段
    TArray<FCameraCutSegment> CameraCutSegments = GetCameraCutSegmentsInternal(LevelSequence);
    
    // 3. 处理不同的情况
    if (CameraCutSegments.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("No Camera Cut segments found! Using original audio segments."));
        // 如果没有Camera Cut片段，使用原始音频片段逻辑
        if (Segments.Num() == 1)
        {
            UE_LOG(LogTemp, Log, TEXT("Only one audio segment found, no need to merge."));
            OutMergedAudio = Segments[0].SoundWave;
            return true;
        }
    }
    else if (CameraCutSegments.Num() == 1) {
        UE_LOG(LogTemp, Log, TEXT("Found single Camera Cut segment - will align audio to this duration."));
        // 单个Camera Cut片段，音频将对齐到这个时长
        if (Segments.Num() == 1)
        {
            UE_LOG(LogTemp, Log, TEXT("Single audio segment will be aligned to Camera Cut duration."));
        }
    }
    else {
        UE_LOG(LogTemp, Log, TEXT("Found %d Camera Cut segments - will align audio to total duration."), CameraCutSegments.Num());
        // 多个Camera Cut片段，音频将对齐到总时长
    }

    if (Segments.Num() > 1)
    {
        UE_LOG(LogTemp, Log, TEXT("Found %d audio segments to merge"), Segments.Num());
    }

    if (CameraCutSegments.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Found %d Camera Cut segments for alignment"), CameraCutSegments.Num());
    }

    // 创建合并后的音频
    if (!LevelSequence || !LevelSequence->GetMovieScene())
    {
        UE_LOG(LogTemp, Error, TEXT("LevelSequence 无效！"));
        return false;
    }

    FFrameRate TickResolution = LevelSequence->GetMovieScene()->GetTickResolution();

    // 4. 合并为PCM数据（与Camera Cut对齐）
    TArray<uint8> AudioData;
    uint32 SampleRate = 0;
    uint16 NumChannels = 0;
    if (!MergeAudioSegmentsToPCM(Segments, CameraCutSegments, AudioData, SampleRate, NumChannels, TickResolution, UserTargetSampleRate)) {
        UE_LOG(LogTemp, Error, TEXT("合并音频片段失败！"));
        return false;
    }

    // 5. 导出为WAV格式数据
    TArray<uint8> WavData;
    if (!UGenTools::ExportPCMToWavData(AudioData, SampleRate, NumChannels, WavData)) {
        UE_LOG(LogTemp, Error, TEXT("导出WAV数据失败！"));
        return false;
    }

    // 6. 保存为音频资产 - 使用GenTools统一处理路径设置
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    FString BasePath = Settings->TTSStoragePath;
    FString AssetPath, WavDir;
    if (!UGenTools::SetupTTSStoragePaths(BasePath, AssetPath, WavDir))
    {
        UE_LOG(LogTemp, Error, TEXT("设置TTS存储路径失败！"));
        return false;
    }
    
    FString AssetName = TEXT("Merged_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    
    // 直接保存为音频资产，不保存wav文件
    bool bSaved = UGenTools::SaveAudioAsSoundWaveAsset(WavData, AssetPath, AssetName);
    if (!bSaved)
    {
        UE_LOG(LogTemp, Error, TEXT("保存音频资产失败！"));
        return false;
    }
    UE_LOG(LogTemp, Log, TEXT("音频合并并保存为资产成功！"));
    UGenTools::BringPluginWindowToFront();
    
    // 7. 从保存的资产路径读取并赋值给OutMergedAudio
    FString FullPackageName = AssetPath + AssetName;
    FString AssetPathForLoad = FString::Printf(TEXT("%s.%s"), *FullPackageName, *AssetName);
    
    // 等待一小段时间确保资产保存完成
    FPlatformProcess::Sleep(0.1f);
    
    // 尝试多次加载资产
    USoundWave* LoadedSoundWave = nullptr;
    for (int32 RetryCount = 0; RetryCount < 5; ++RetryCount)
    {
        LoadedSoundWave = LoadObject<USoundWave>(nullptr, *AssetPathForLoad);
        if (LoadedSoundWave)
        {
            break;
        }
        UE_LOG(LogTemp, Warning, TEXT("尝试加载合并音频资产失败，重试 %d/5: %s"), RetryCount + 1, *AssetPathForLoad);
        FPlatformProcess::Sleep(0.2f); // 等待200ms后重试
    }
    
    if (LoadedSoundWave)
    {
        OutMergedAudio = LoadedSoundWave;
        UE_LOG(LogTemp, Log, TEXT("成功加载合并音频资产: %s"), *AssetPathForLoad);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("无法加载合并音频资产: %s"), *AssetPathForLoad);
        return false;
    }
}

// 合并音频片段为PCM数据（与Camera Cut对齐，自动上采样到最高采样率）
bool UAudioAssetOperation::MergeAudioSegmentsToPCM(const TArray<FAudioSegment>& Segments, const TArray<FCameraCutSegment>& CameraCutSegments, TArray<uint8>& OutAudioData, uint32& OutSampleRate, uint16& OutNumChannels, const FFrameRate& TickResolution, int32 UserTargetSampleRate)
{
    if (Segments.Num() == 0) return false;
    
    // 1. 获取声道数和采样率，并找到最高采样率
    OutNumChannels = 0;
    uint32 MaxSampleRate = 0;
    uint32 FirstSegmentSampleRate = 0;
    bool bHasMultiChannel = false;
    bool bHasSingleChannel = false;
    
    // 遍历所有片段，找到最高采样率和声道数信息
    for (const auto& Segment : Segments) {
        if (Segment.SoundWave) {
            TArray<uint8> DummyData;
            uint16 OutChannels = 0;
            uint32 OutSampleRateTmp = 0;
            if (Segment.SoundWave->GetImportedSoundWaveData(DummyData, OutSampleRateTmp, OutChannels)) {
                if (OutNumChannels == 0) {
                    OutNumChannels = OutChannels; // 使用第一个有效片段的声道数
                    FirstSegmentSampleRate = OutSampleRateTmp;
                }
                if (OutSampleRateTmp > MaxSampleRate) {
                    MaxSampleRate = OutSampleRateTmp;
                }
                
                // 检测声道数类型
                if (OutChannels == 1) {
                    bHasSingleChannel = true;
                } else if (OutChannels > 1) {
                    bHasMultiChannel = true;
                }
            }
        }
    }
    
    // 如果同时存在单声道和多声道音频，将输出声道数设置为2（双声道）
    if (bHasSingleChannel && bHasMultiChannel) {
        OutNumChannels = 2; // 强制使用双声道
        UE_LOG(LogTemp, Log, TEXT("检测到单声道和多声道混合，输出将使用双声道"));
    }
    if (OutNumChannels == 0 || MaxSampleRate == 0) {
        UE_LOG(LogTemp, Error, TEXT("未能获取有效的声道数或采样率！"));
        return false;
    }
    
    // 确定目标采样率
    if (UserTargetSampleRate > 0) {
        // 用户指定了采样率，使用用户指定的采样率
        OutSampleRate = static_cast<uint32>(UserTargetSampleRate);
        UE_LOG(LogTemp, Log, TEXT("用户指定采样率模式: 所有音频片段将重采样到 %d Hz"), OutSampleRate);
    } else {
        // 自动模式：使用最高采样率，确保音质最佳
        OutSampleRate = MaxSampleRate;
        UE_LOG(LogTemp, Log, TEXT("自动上采样模式: 所有音频片段将重采样到最高采样率 %d Hz (从 %d Hz 到 %d Hz)"), 
               OutSampleRate, FirstSegmentSampleRate, MaxSampleRate);
    }

    // 2. 确定总时长
    int32 TotalSamplePoints = 0;
    
    if (CameraCutSegments.Num() > 0)
    {
        // 使用Camera Cut Track的总时长
        FFrameNumber MaxEndTime = 0;
        FFrameNumber MinStartTime = TNumericLimits<int32>::Max();
        
        for (const auto& CameraCutSegment : CameraCutSegments)
        {
            if (CameraCutSegment.EndTime > MaxEndTime)
            {
                MaxEndTime = CameraCutSegment.EndTime;
            }
            if (CameraCutSegment.StartTime < MinStartTime)
            {
                MinStartTime = CameraCutSegment.StartTime;
            }
        }
        
        float TotalDuration = (MaxEndTime.Value - MinStartTime.Value) / TickResolution.AsDecimal();
        TotalSamplePoints = FMath::RoundToInt(TotalDuration * OutSampleRate);
        
        UE_LOG(LogTemp, Log, TEXT("Camera Cut Track: StartFrame=%d, EndFrame=%d"), MinStartTime.Value, MaxEndTime.Value);
        UE_LOG(LogTemp, Log, TEXT("使用Camera Cut Track总时长: %.2f秒, 采样点数: %d"), TotalDuration, TotalSamplePoints);
        
        // 如果是单个Camera Cut片段，输出更详细的信息
        if (CameraCutSegments.Num() == 1)
        {
            const FCameraCutSegment& SingleSegment = CameraCutSegments[0];
            UE_LOG(LogTemp, Log, TEXT("单个Camera Cut片段: 开始时间=%.2f秒, 结束时间=%.2f秒"), 
                   SingleSegment.StartTime.Value / TickResolution.AsDecimal(),
                   SingleSegment.EndTime.Value / TickResolution.AsDecimal());
        }
    }
    else
    {
        // 使用音频片段的最大结束时间
        for (const auto& Segment : Segments) {
            float EndSec = Segment.EndTime.Value / TickResolution.AsDecimal();
            int32 EndSample = FMath::RoundToInt(EndSec * OutSampleRate);
            if (EndSample > TotalSamplePoints)
                TotalSamplePoints = EndSample;
        }
        UE_LOG(LogTemp, Log, TEXT("使用音频片段总时长，采样点数: %d"), TotalSamplePoints);
    }

    const int32 TotalSamples = TotalSamplePoints * OutNumChannels;
    const int32 TotalBytes = TotalSamples * sizeof(int16);
    OutAudioData.SetNumZeroed(TotalBytes);

    // 3. 合并所有音频片段（支持重叠混音）
    UE_LOG(LogTemp, Log, TEXT("开始合并 %d 个音频片段"), Segments.Num());
    
    // 预处理：收集所有音频片段的数据和格式信息
    TArray<TArray<uint8>> ProcessedSegmentData;
    TArray<int32> SegmentStartSamples;
    TArray<int32> SegmentEndSamples;
    TArray<int32> SegmentSourceSampleCounts;
    
    for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
    {
        const auto& Segment = Segments[SegmentIndex];
        if (!Segment.SoundWave || Segment.StartTime >= Segment.EndTime)
        {
            UE_LOG(LogTemp, Warning, TEXT("跳过无效的音频片段 %d"), SegmentIndex);
            continue;
        }
            
        // 获取音频片段的原始时长
        float OriginalDuration = Segment.SoundWave->Duration;
        if (OriginalDuration <= 0.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("片段 %d 音频时长为0，跳过！"), SegmentIndex);
            continue;
        }
        
        // 计算音频片段在时间轴上的位置和时长
        float StartSec = Segment.StartTime.Value / TickResolution.AsDecimal();
        float EndSec = Segment.EndTime.Value / TickResolution.AsDecimal();
        float SegmentDuration = EndSec - StartSec;
        
        // 计算在合并音频中的采样位置
        int32 StartSample = FMath::RoundToInt(StartSec * OutSampleRate);
        int32 EndSample = FMath::RoundToInt(EndSec * OutSampleRate);
        
        TArray<uint8> SegmentData;
        uint16 SegmentChannels = 0;
        uint32 SegmentSampleRate = 0;
        if (!Segment.SoundWave->GetImportedSoundWaveData(SegmentData, SegmentSampleRate, SegmentChannels)) {
            UE_LOG(LogTemp, Warning, TEXT("片段 %d 数据获取失败，跳过！"), SegmentIndex);
            continue;
        }
        
        // 格式转换
        bool bNeedConversion = (SegmentChannels != OutNumChannels) || (SegmentSampleRate != OutSampleRate);
        if (bNeedConversion) {
            TArray<uint8> ConvertedData;
            if (!ConvertAudioFormat(SegmentData, SegmentSampleRate, SegmentChannels, 
                                   OutSampleRate, OutNumChannels, ConvertedData)) {
                UE_LOG(LogTemp, Error, TEXT("片段 %d 格式转换失败，跳过！"), SegmentIndex);
                continue;
            }
            SegmentData = ConvertedData;
        }
        
        int32 SourceSampleCount = SegmentData.Num() / sizeof(int16) / OutNumChannels;
        
        // 处理音频片段的裁剪：根据拖动后的时间范围裁剪音频数据
        TArray<uint8> CroppedSegmentData;
        int32 ProcessedSampleCount = SourceSampleCount;
        
        // 处理音频片段的时长变化：可能是裁剪、重复或正常播放
        if (SegmentDuration < OriginalDuration)
        {
            // 情况1：片段时长小于原始音频时长，需要裁剪音频数据
            float CropRatio = SegmentDuration / OriginalDuration;
            ProcessedSampleCount = FMath::RoundToInt(SourceSampleCount * CropRatio);
            
            // 精确裁剪：根据音频片段的偏移信息来裁剪音频数据
            int32 CropBytes = ProcessedSampleCount * sizeof(int16) * OutNumChannels;
            CroppedSegmentData.SetNum(CropBytes);
            
            // 计算裁剪起始位置
            int32 CropStartByte = 0;
            
            // 使用AudioOffset来确定音频的裁剪起始位置
            if (Segment.AudioOffset.Value > 0)
            {
                // 如果AudioOffset大于0，说明用户拖动了音频片段的开始位置
                float OffsetRatio = Segment.AudioOffset.Value / TickResolution.AsDecimal() / OriginalDuration;
                int32 OffsetSamples = FMath::RoundToInt(OffsetRatio * SourceSampleCount);
                CropStartByte = OffsetSamples * sizeof(int16) * OutNumChannels;
                
                // 确保不会超出原始数据范围
                int32 MaxCropBytes = SegmentData.Num() - CropStartByte;
                if (CropBytes > MaxCropBytes)
                {
                    CropBytes = MaxCropBytes;
                    ProcessedSampleCount = CropBytes / (sizeof(int16) * OutNumChannels);
                }
                
                UE_LOG(LogTemp, Log, TEXT("音频片段 %d 从偏移位置裁剪: 偏移比例=%.2f, 偏移样本数=%d, 裁剪起始字节=%d"), 
                       SegmentIndex, OffsetRatio, OffsetSamples, CropStartByte);
            }
            else
            {
                // 如果AudioOffset为0，说明用户只拖动了结束位置，从开头开始裁剪
                UE_LOG(LogTemp, Log, TEXT("音频片段 %d 从开头裁剪: 无偏移, 裁剪比例=%.2f"), 
                       SegmentIndex, CropRatio);
            }
            
            // 执行裁剪
            FMemory::Memcpy(CroppedSegmentData.GetData(), 
                           SegmentData.GetData() + CropStartByte, 
                           CropBytes);
            
            UE_LOG(LogTemp, Log, TEXT("音频片段 %d 被裁剪: 原始时长=%.2f秒, 片段时长=%.2f秒, 裁剪比例=%.2f, 裁剪字节数=%d"), 
                   SegmentIndex, OriginalDuration, SegmentDuration, CropRatio, CropBytes);
        }
        else if (SegmentDuration > OriginalDuration)
        {
            // 情况2：片段时长大于原始音频时长，需要重复播放音频
            // 计算需要重复的次数
            float RepeatRatio = SegmentDuration / OriginalDuration;
            int32 RepeatCount = FMath::CeilToInt(RepeatRatio);
            
            // 计算重复后的总样本数
            ProcessedSampleCount = FMath::RoundToInt(SourceSampleCount * RepeatRatio);
            int32 RepeatTotalBytes = ProcessedSampleCount * sizeof(int16) * OutNumChannels;
            CroppedSegmentData.SetNum(RepeatTotalBytes);
            
            // 计算裁剪起始位置（如果有AudioOffset）
            int32 CropStartByte = 0;
            int32 OffsetSamples = 0;
            if (Segment.AudioOffset.Value > 0)
            {
                float OffsetRatio = Segment.AudioOffset.Value / TickResolution.AsDecimal() / OriginalDuration;
                OffsetSamples = FMath::RoundToInt(OffsetRatio * SourceSampleCount);
                CropStartByte = OffsetSamples * sizeof(int16) * OutNumChannels;
                
                // 确保偏移不会超出音频数据范围
                if (OffsetSamples >= SourceSampleCount)
                {
                    OffsetSamples = 0;
                    CropStartByte = 0;
                    UE_LOG(LogTemp, Warning, TEXT("音频片段 %d 偏移超出范围，重置为0"), SegmentIndex);
                }
                
                UE_LOG(LogTemp, Log, TEXT("音频片段 %d 重复播放从偏移位置开始: 偏移比例=%.2f, 偏移样本数=%d"), 
                       SegmentIndex, OffsetRatio, OffsetSamples);
            }
            
            // 执行精确重复播放
            int16* DestPCM = reinterpret_cast<int16*>(CroppedSegmentData.GetData());
            int16* SrcPCM = reinterpret_cast<int16*>(SegmentData.GetData());
            int32 SrcSampleCount = SourceSampleCount;
            
            int32 CurrentDestSample = 0;
            int32 SrcStartSample = OffsetSamples; // 从偏移位置开始
            
            // 计算完整的重复周期数
            int32 FullRepeatCount = 0;
            int32 RemainingSamples = ProcessedSampleCount;
            
            // 第一次：从偏移位置播放到音频结束
            if (SrcStartSample < SrcSampleCount && RemainingSamples > 0)
            {
                int32 FirstPartSamples = SrcSampleCount - SrcStartSample;
                int32 SamplesToCopy = FMath::Min(FirstPartSamples, RemainingSamples);
                
                if (SamplesToCopy > 0)
                {
                    FMemory::Memcpy(DestPCM + CurrentDestSample * OutNumChannels,
                                   SrcPCM + SrcStartSample * OutNumChannels,
                                   SamplesToCopy * sizeof(int16) * OutNumChannels);
                    CurrentDestSample += SamplesToCopy;
                    RemainingSamples -= SamplesToCopy;
                }
                
                UE_LOG(LogTemp, Log, TEXT("音频片段 %d 第一次播放: 从偏移位置%d播放%d个样本"), 
                       SegmentIndex, SrcStartSample, SamplesToCopy);
            }
            
            // 计算剩余需要播放的完整音频周期数
            if (RemainingSamples > 0)
            {
                FullRepeatCount = RemainingSamples / SrcSampleCount;
                int32 PartialSamples = RemainingSamples % SrcSampleCount;
                
                // 播放完整的重复周期
                for (int32 i = 0; i < FullRepeatCount; ++i)
                {
                    FMemory::Memcpy(DestPCM + CurrentDestSample * OutNumChannels,
                                   SrcPCM,
                                   SrcSampleCount * sizeof(int16) * OutNumChannels);
                    CurrentDestSample += SrcSampleCount;
                    RemainingSamples -= SrcSampleCount;
                }
                
                // 播放最后的部分音频（从开头开始）
                if (PartialSamples > 0)
                {
                    FMemory::Memcpy(DestPCM + CurrentDestSample * OutNumChannels,
                                   SrcPCM,
                                   PartialSamples * sizeof(int16) * OutNumChannels);
                    CurrentDestSample += PartialSamples;
                    RemainingSamples -= PartialSamples;
                }
                
                UE_LOG(LogTemp, Log, TEXT("音频片段 %d 完整重复: %d次完整播放 + %d个部分样本"), 
                       SegmentIndex, FullRepeatCount, PartialSamples);
            }
            
            // 如果还有剩余空间，用静音填充
            if (CurrentDestSample < ProcessedSampleCount)
            {
                int32 SilenceSamples = ProcessedSampleCount - CurrentDestSample;
                FMemory::Memset(DestPCM + CurrentDestSample * OutNumChannels, 
                               0, SilenceSamples * sizeof(int16) * OutNumChannels);
                
                UE_LOG(LogTemp, Log, TEXT("音频片段 %d 重复播放后用静音填充: 剩余样本数=%d"), 
                       SegmentIndex, SilenceSamples);
            }
            
            UE_LOG(LogTemp, Log, TEXT("音频片段 %d 重复播放: 原始时长=%.2f秒, 片段时长=%.2f秒, 重复比例=%.2f, 重复次数=%d"), 
                   SegmentIndex, OriginalDuration, SegmentDuration, RepeatRatio, RepeatCount);
        }
        else
        {
            // 情况3：片段时长等于原始音频时长，正常播放
            CroppedSegmentData = SegmentData;
            UE_LOG(LogTemp, Log, TEXT("音频片段 %d 正常播放: 时长=%.2f秒"), 
                   SegmentIndex, OriginalDuration);
        }
        
        // 存储处理后的数据
        ProcessedSegmentData.Add(CroppedSegmentData);
        SegmentStartSamples.Add(StartSample);
        SegmentEndSamples.Add(EndSample);
        SegmentSourceSampleCounts.Add(ProcessedSampleCount);
        
        UE_LOG(LogTemp, Log, TEXT("音频片段 %d: 时间范围=[%.2f-%.2f]秒, 采样范围=[%d-%d], 处理后采样点数=%d"), 
               SegmentIndex, StartSec, EndSec, StartSample, EndSample, ProcessedSampleCount);
    }
    
    // 4. 执行重叠混音
    int16* DestPCM = reinterpret_cast<int16*>(OutAudioData.GetData());
    int32 TotalOverlapCount = 0;
    
    for (int32 SampleIndex = 0; SampleIndex < TotalSamplePoints; ++SampleIndex)
    {
        // 收集在当前采样点时间有音频的所有片段
        TArray<int32> ActiveSegments;
        TArray<int32> SegmentOffsets; // 在各自片段中的偏移
        
        for (int32 SegmentIndex = 0; SegmentIndex < ProcessedSegmentData.Num(); ++SegmentIndex)
        {
            int32 StartSample = SegmentStartSamples[SegmentIndex];
            int32 EndSample = SegmentEndSamples[SegmentIndex];
            int32 SourceSampleCount = SegmentSourceSampleCounts[SegmentIndex];
            
            if (SampleIndex >= StartSample && SampleIndex < EndSample)
            {
                int32 SegmentOffset = SampleIndex - StartSample;
                if (SegmentOffset < SourceSampleCount)
                {
                    ActiveSegments.Add(SegmentIndex);
                    SegmentOffsets.Add(SegmentOffset);
                }
            }
        }
        
        // 处理当前采样点的混音
        for (int32 ch = 0; ch < OutNumChannels; ++ch)
        {
            int32 DestIndex = SampleIndex * OutNumChannels + ch;
            
            if (ActiveSegments.Num() == 0)
            {
                // 没有音频片段，保持为0
                DestPCM[DestIndex] = 0;
            }
            else if (ActiveSegments.Num() == 1)
            {
                // 只有一个音频片段，直接复制
                int32 SegmentIndex = ActiveSegments[0];
                int32 SegmentOffset = SegmentOffsets[0];
                int16* SrcPCM = reinterpret_cast<int16*>(ProcessedSegmentData[SegmentIndex].GetData());
                int32 SrcIndex = SegmentOffset * OutNumChannels + ch;
                DestPCM[DestIndex] = SrcPCM[SrcIndex];
            }
            else
            {
                // 多个音频片段重叠，进行混音
                int16 MixedSample = 0;
                for (int32 i = 0; i < ActiveSegments.Num(); ++i)
                {
                    int32 SegmentIndex = ActiveSegments[i];
                    int32 SegmentOffset = SegmentOffsets[i];
                    int16* SrcPCM = reinterpret_cast<int16*>(ProcessedSegmentData[SegmentIndex].GetData());
                    int32 SrcIndex = SegmentOffset * OutNumChannels + ch;
                    
                    if (i == 0)
                    {
                        MixedSample = SrcPCM[SrcIndex];
                    }
                    else
                    {
                        // 使用交叉调制混音算法
                        MixedSample = MixAudioSamples(MixedSample, SrcPCM[SrcIndex], 1.0f, 1.0f);
                    }
                }
                DestPCM[DestIndex] = MixedSample;
                TotalOverlapCount++;
            }
        }
    }
    
    // 输出重叠统计信息
    if (TotalOverlapCount > 0) {
        UE_LOG(LogTemp, Log, TEXT("检测到 %d 个重叠采样点，已进行混音处理"), TotalOverlapCount);
    }
    
    UE_LOG(LogTemp, Log, TEXT("音频合并完成，总采样点数: %d"), TotalSamplePoints);
    return true;
}

// 音频格式转换函数
bool UAudioAssetOperation::ConvertAudioFormat(const TArray<uint8>& InputData, uint32 InputSampleRate, uint16 InputChannels,
                                              uint32 OutputSampleRate, uint16 OutputChannels, TArray<uint8>& OutputData)
{
    if (InputData.Num() == 0) return false;
    
    // 简单的格式转换实现
    // 注意：这是一个基础实现，实际项目中可能需要更复杂的音频处理库
    
    // 1. 声道数转换（简单的复制或丢弃）
    TArray<uint8> ChannelConvertedData;
    if (InputChannels != OutputChannels) {
        if (!ConvertChannels(InputData, InputChannels, OutputChannels, ChannelConvertedData)) {
            UE_LOG(LogTemp, Error, TEXT("声道数转换失败"));
            return false;
        }
    } else {
        ChannelConvertedData = InputData;
    }
    
    // 2. 采样率转换（简单的线性插值）
    if (InputSampleRate != OutputSampleRate) {
        if (!ConvertSampleRate(ChannelConvertedData, InputSampleRate, OutputSampleRate, OutputChannels, OutputData)) {
            UE_LOG(LogTemp, Error, TEXT("采样率转换失败"));
            return false;
        }
    } else {
        OutputData = ChannelConvertedData;
    }
    
    return true;
}

// 声道数转换
bool UAudioAssetOperation::ConvertChannels(const TArray<uint8>& InputData, uint16 InputChannels, uint16 OutputChannels, TArray<uint8>& OutputData)
{
    if (InputChannels == OutputChannels) {
        OutputData = InputData;
        return true;
    }
    
    int32 InputSamples = InputData.Num() / sizeof(int16);
    int32 InputSamplesPerChannel = InputSamples / InputChannels;
    
    if (OutputChannels == 1) {
        // 多声道转单声道（取平均值）
        OutputData.SetNum(InputSamplesPerChannel * sizeof(int16));
        int16* OutputPCM = reinterpret_cast<int16*>(OutputData.GetData());
        int16* InputPCM = reinterpret_cast<int16*>(const_cast<uint8*>(InputData.GetData()));
        
        for (int32 i = 0; i < InputSamplesPerChannel; ++i) {
            int32 Sum = 0;
            for (int32 ch = 0; ch < InputChannels; ++ch) {
                Sum += InputPCM[i * InputChannels + ch];
            }
            OutputPCM[i] = static_cast<int16>(Sum / InputChannels);
        }
    } else if (InputChannels == 1) {
        // 单声道转多声道（复制到所有声道）
        OutputData.SetNum(InputSamplesPerChannel * OutputChannels * sizeof(int16));
        int16* OutputPCM = reinterpret_cast<int16*>(OutputData.GetData());
        int16* InputPCM = reinterpret_cast<int16*>(const_cast<uint8*>(InputData.GetData()));
        
        for (int32 i = 0; i < InputSamplesPerChannel; ++i) {
            for (int32 ch = 0; ch < OutputChannels; ++ch) {
                OutputPCM[i * OutputChannels + ch] = InputPCM[i];
            }
        }
        
        UE_LOG(LogTemp, Log, TEXT("单声道转多声道: %d -> %d 声道"), InputChannels, OutputChannels);
    } else {
        // 其他声道数转换（取前N个声道或填充0）
        OutputData.SetNum(InputSamplesPerChannel * OutputChannels * sizeof(int16));
        int16* OutputPCM = reinterpret_cast<int16*>(OutputData.GetData());
        int16* InputPCM = reinterpret_cast<int16*>(const_cast<uint8*>(InputData.GetData()));
        
        for (int32 i = 0; i < InputSamplesPerChannel; ++i) {
            for (int32 ch = 0; ch < OutputChannels; ++ch) {
                if (ch < InputChannels) {
                    OutputPCM[i * OutputChannels + ch] = InputPCM[i * InputChannels + ch];
                } else {
                    OutputPCM[i * OutputChannels + ch] = 0; // 填充0
                }
            }
        }
    }
    
    return true;
}

// 采样率转换（支持多种重采样算法）
bool UAudioAssetOperation::ConvertSampleRate(const TArray<uint8>& InputData, uint32 InputSampleRate, uint32 OutputSampleRate, uint16 Channels, TArray<uint8>& OutputData)
{
    if (InputSampleRate == OutputSampleRate) {
        OutputData = InputData;
        return true;
    }
    
    // 优先使用RuntimeAudioImporter进行高质量重采样
    if (RuntimeAudioResample(InputData, InputSampleRate, Channels, OutputSampleRate, Channels, OutputData)) {
        UE_LOG(LogTemp, Log, TEXT("ConvertSampleRate: Using RuntimeAudioImporter for high-quality resampling"));
        return true;
    }
    
    // 如果RuntimeAudioImporter失败，回退到自定义算法
    UE_LOG(LogTemp, Warning, TEXT("ConvertSampleRate: RuntimeAudioImporter failed, falling back to custom algorithms"));
    
    int32 InputSamples = InputData.Num() / sizeof(int16);
    int32 InputSamplesPerChannel = InputSamples / Channels;
    int32 OutputSamplesPerChannel = FMath::RoundToInt(InputSamplesPerChannel * (float)OutputSampleRate / InputSampleRate);
    
    OutputData.SetNum(OutputSamplesPerChannel * Channels * sizeof(int16));
    int16* OutputPCM = reinterpret_cast<int16*>(OutputData.GetData());
    int16* InputPCM = reinterpret_cast<int16*>(const_cast<uint8*>(InputData.GetData()));
    
    // 选择重采样算法
    // 对于降采样（输出采样率 < 输入采样率），使用更高质量的算法
    // 对于升采样（输出采样率 > 输入采样率），可以使用较简单的算法
    bool bIsDownsampling = OutputSampleRate < InputSampleRate;
    
    if (bIsDownsampling) {
        // 降采样：使用Lanczos重采样（高质量）
        return LanczosResample(InputPCM, InputSamplesPerChannel, OutputPCM, OutputSamplesPerChannel, 
                              InputSampleRate, OutputSampleRate, Channels);
    } else {
        // 升采样：使用三次样条插值（平衡质量和性能）
        return CubicSplineResample(InputPCM, InputSamplesPerChannel, OutputPCM, OutputSamplesPerChannel, 
                                  InputSampleRate, OutputSampleRate, Channels);
    }
}

// Lanczos重采样（高质量，适用于降采样）
bool UAudioAssetOperation::LanczosResample(int16* InputPCM, int32 InputSamplesPerChannel, 
                                          int16* OutputPCM, int32 OutputSamplesPerChannel,
                                          uint32 InputSampleRate, uint32 OutputSampleRate, uint16 Channels)
{
    const int32 LanczosWindowSize = 3;
    
    for (int32 ch = 0; ch < Channels; ++ch) {
        for (int32 i = 0; i < OutputSamplesPerChannel; ++i) {
            float InputIndex = i * (float)InputSampleRate / OutputSampleRate;
            
            float Sum = 0.0f;
            float WeightSum = 0.0f;
            
            int32 StartIndex = FMath::Max(0, FMath::FloorToInt(InputIndex - LanczosWindowSize));
            int32 EndIndex = FMath::Min(InputSamplesPerChannel - 1, FMath::CeilToInt(InputIndex + LanczosWindowSize));
            
            for (int32 j = StartIndex; j <= EndIndex; ++j) {
                float Distance = InputIndex - j;
                float Weight = LanczosKernel(Distance, LanczosWindowSize);
                
                int16 Sample = InputPCM[j * Channels + ch];
                Sum += Sample * Weight;
                WeightSum += Weight;
            }
            
            int16 ResampledSample = 0;
            if (WeightSum > 0.0f) {
                ResampledSample = static_cast<int16>(FMath::Clamp(Sum / WeightSum, -32768.0f, 32767.0f));
            }
            
            OutputPCM[i * Channels + ch] = ResampledSample;
        }
    }
    
    return true;
}

// 三次样条插值重采样（平衡质量和性能，适用于升采样）
bool UAudioAssetOperation::CubicSplineResample(int16* InputPCM, int32 InputSamplesPerChannel,
                                              int16* OutputPCM, int32 OutputSamplesPerChannel,
                                              uint32 InputSampleRate, uint32 OutputSampleRate, uint16 Channels)
{
    for (int32 ch = 0; ch < Channels; ++ch) {
        for (int32 i = 0; i < OutputSamplesPerChannel; ++i) {
            float InputIndex = i * (float)InputSampleRate / OutputSampleRate;
            
            int32 Index1 = FMath::FloorToInt(InputIndex);
            int32 Index0 = FMath::Max(0, Index1 - 1);
            int32 Index2 = FMath::Min(InputSamplesPerChannel - 1, Index1 + 1);
            int32 Index3 = FMath::Min(InputSamplesPerChannel - 1, Index1 + 2);
            
            float Fraction = InputIndex - Index1;
            
            // 获取四个控制点
            int16 P0 = InputPCM[Index0 * Channels + ch];
            int16 P1 = InputPCM[Index1 * Channels + ch];
            int16 P2 = InputPCM[Index2 * Channels + ch];
            int16 P3 = InputPCM[Index3 * Channels + ch];
            
            // 三次样条插值
            float t = Fraction;
            float t2 = t * t;
            float t3 = t2 * t;
            
            // Catmull-Rom样条系数
            float c0 = -0.5f * t3 + t2 - 0.5f * t;
            float c1 = 1.5f * t3 - 2.5f * t2 + 1.0f;
            float c2 = -1.5f * t3 + 2.0f * t2 + 0.5f * t;
            float c3 = 0.5f * t3 - 0.5f * t2;
            
            float InterpolatedValue = c0 * P0 + c1 * P1 + c2 * P2 + c3 * P3;
            int16 ResampledSample = static_cast<int16>(FMath::Clamp(InterpolatedValue, -32768.0f, 32767.0f));
            
            OutputPCM[i * Channels + ch] = ResampledSample;
        }
    }
    
    return true;
}

// Lanczos内核函数
float UAudioAssetOperation::LanczosKernel(float x, int32 a)
{
    if (FMath::Abs(x) >= a) {
        return 0.0f;
    }
    
    if (FMath::Abs(x) < 0.0001f) {
        return 1.0f;
    }
    
    float pi_x = PI * x;
    float pi_x_a = pi_x / a;
    
    return (FMath::Sin(pi_x) * FMath::Sin(pi_x_a)) / (pi_x * pi_x_a);
}

// 使用RuntimeAudioImporter进行高质量音频重采样
bool UAudioAssetOperation::RuntimeAudioResample(const TArray<uint8>& InputData, uint32 InputSampleRate, uint16 InputChannels,
                                               uint32 OutputSampleRate, uint16 OutputChannels, TArray<uint8>& OutputData)
{
    if (InputData.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("RuntimeAudioResample: Input data is empty"));
        return false;
    }
    
    if (InputSampleRate == OutputSampleRate && InputChannels == OutputChannels) {
        // 无需重采样，直接复制数据
        OutputData = InputData;
        return true;
    }
    
    try {
        // 创建临时的SoundWave对象用于重采样
        USoundWave* TempSoundWave = NewObject<USoundWave>();
        if (!TempSoundWave) {
            UE_LOG(LogTemp, Error, TEXT("RuntimeAudioResample: Failed to create temporary SoundWave"));
            return false;
        }
        
        // 将PCM数据转换为ImportedSoundWave
        UImportedSoundWave* ImportedSoundWave = URuntimeAudioImporterLibrary::ConvertRegularToImportedSoundWave(TempSoundWave);
        if (!ImportedSoundWave) {
            UE_LOG(LogTemp, Error, TEXT("RuntimeAudioResample: Failed to convert to ImportedSoundWave"));
            return false;
        }
        
        // 设置重采样选项
        FRuntimeAudioExportOverrideOptions ResampleOptions;
        ResampleOptions.SampleRate = OutputSampleRate;
        ResampleOptions.NumOfChannels = OutputChannels;
        
        // 执行重采样
        const TArray<uint8> ResampledData = URuntimeAudioExporter::ExportSoundWaveToRAWBuffer(
            ImportedSoundWave,
            ERuntimeRAWAudioFormat::Int16,
            ResampleOptions
        );
        
        if (ResampledData.Num() == 0) {
            UE_LOG(LogTemp, Error, TEXT("RuntimeAudioResample: Resampling failed, output data is empty"));
            return false;
        }
        
        OutputData = ResampledData;
        
        UE_LOG(LogTemp, Log, TEXT("RuntimeAudioResample: Successfully resampled from %dHz/%dch to %dHz/%dch, data size: %d -> %d"), 
               InputSampleRate, InputChannels, OutputSampleRate, OutputChannels, InputData.Num(), OutputData.Num());
        
        return true;
    }
    catch (const std::exception& e) {
        UE_LOG(LogTemp, Error, TEXT("RuntimeAudioResample: Exception occurred: %s"), UTF8_TO_TCHAR(e.what()));
        return false;
    }
    catch (...) {
        UE_LOG(LogTemp, Error, TEXT("RuntimeAudioResample: Unknown exception occurred"));
        return false;
    }
}

// 应用合并后的音频到轨道
bool UAudioAssetOperation::ApplyMergedAudioToTrack(ULevelSequence* LevelSequence, const FString& SelectedTrack, USoundWave* MergedAudio)
{
    if (!LevelSequence || SelectedTrack.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("LevelSequence或SelectedTrack无效"));
        return false;
    }

    if (!MergedAudio)
    {
        UE_LOG(LogTemp, Error, TEXT("MergedAudio为空"));
        return false;
    }

    if (!LevelSequence->GetMovieScene())
    {
        UE_LOG(LogTemp, Error, TEXT("LevelSequence无效"));
        return false;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    FFrameRate TickResolution = MovieScene->GetTickResolution();
    
    // 获取所有音频轨道信息（包含按 Row 拆分后的条目）
    TArray<FAudioTrackInfo> TrackInfos = GetAllAudioTracksInfo(LevelSequence);
    
    // 查找匹配的音频轨道
    for (const FAudioTrackInfo& TrackInfo : TrackInfos)
    {
        if (TrackInfo.DisplayName == SelectedTrack && TrackInfo.AudioTrack)
        {
            UMovieSceneAudioTrack* AudioTrack = TrackInfo.AudioTrack;
            
            // 清空现有音频片段
            if (TrackInfo.RowIndex == INDEX_NONE)
            {
                // 对整条轨道清空
            AudioTrack->RemoveAllAnimationData();
            }
            else
            {
                // 仅移除目标 Row 的 Section，保留其他行
                TArray<UMovieSceneSection*> AllSections = AudioTrack->GetAllSections();
                for (UMovieSceneSection* Sec : AllSections)
                {
                    if (Sec && Sec->GetRowIndex() == TrackInfo.RowIndex)
                    {
                        AudioTrack->RemoveSection(*Sec);
                    }
                }
            }
            
            // 创建新的音频片段
            UMovieSceneAudioSection* NewSection = NewObject<UMovieSceneAudioSection>(AudioTrack, NAME_None, RF_Transactional);
            if (NewSection)
            {
                // 设置音频
                NewSection->SetSound(MergedAudio);
                
                // 设置音频片段的时长范围
                FFrameNumber StartFrame(0);
                int32 EndFrameValue = FMath::RoundToInt(MergedAudio->Duration * TickResolution.AsDecimal());
                FFrameNumber EndFrame(EndFrameValue);
                NewSection->SetRange(TRange<FFrameNumber>(StartFrame, EndFrame));
                
                // 写入到对应 Row
                if (TrackInfo.RowIndex != INDEX_NONE)
                {
                    NewSection->SetRowIndex(TrackInfo.RowIndex);
                }
                
                // 手动添加到轨道
                AudioTrack->AddSection(*NewSection);
                
                // 标记轨道为已修改
                AudioTrack->Modify();
                NewSection->Modify();
                
                // 刷新LevelSequence
                LevelSequence->Modify();
                MovieScene->Modify();
                
                UE_LOG(LogTemp, Log, TEXT("音频片段应用成功，轨道片段数量: %d"), AudioTrack->GetAllSections().Num());
                
                // 自动保存LevelSequence
                FString PackageFileName = FPackageName::LongPackageNameToFilename(LevelSequence->GetPackage()->GetName(), FPackageName::GetAssetPackageExtension());
                bool bSaved = UPackage::SavePackage(LevelSequence->GetPackage(), LevelSequence, *PackageFileName, FSavePackageArgs{});
                if (bSaved)
                {
                    UE_LOG(LogTemp, Log, TEXT("LevelSequence已自动保存"));
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("LevelSequence自动保存失败"));
                }
                
                return true;
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("创建音频片段失败"));
                return false;
            }
        }
    }

    UE_LOG(LogTemp, Error, TEXT("未找到匹配的音频轨道: %s"), *SelectedTrack);
    return false;
} 

// 清理静态变量（用于重置名称计数）
void UAudioAssetOperation::ClearStaticVariables()
{
    // 清理名称计数的静态映射（保留用于兼容性）
    static TMap<FString, int32> NameCounts;
    NameCounts.Empty();
    
    UE_LOG(LogTemp, Log, TEXT("已清理静态变量"));
}

// 调试打印所有轨道信息
void UAudioAssetOperation::DebugPrintAllTracksInfo(ULevelSequence* LevelSequence)
{
    if (!LevelSequence)
    {
        UE_LOG(LogTemp, Warning, TEXT("LevelSequence is null"));
        return;
    }

    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    if (!MovieScene)
    {
        UE_LOG(LogTemp, Warning, TEXT("MovieScene is null"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("=== 轨道信息调试 ==="));
    
    // 统计信息
    TArrayView<UMovieSceneFolder* const> RootFolders = MovieScene->GetRootFolders();
    const TArray<UMovieSceneTrack*>& RootTracks = MovieScene->GetTracks();
    UE_LOG(LogTemp, Log, TEXT("根文件夹: %d, 根轨道: %d"), RootFolders.Num(), RootTracks.Num());
    
    // 统计音频轨道
    int32 AudioTrackCount = 0;
    for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
    {
        const TArray<UMovieSceneTrack*>& BindingTracks = MovieScene->FindTracks(UMovieSceneAudioTrack::StaticClass(), Binding.GetObjectGuid());
        AudioTrackCount += BindingTracks.Num();
    }
    UE_LOG(LogTemp, Log, TEXT("音频轨道总数: %d"), AudioTrackCount);
}


// 获取Row的自定义名称
FString UAudioAssetOperation::GetRowDisplayName(UMovieSceneAudioTrack* AudioTrack, int32 RowIndex)
{
    if (!AudioTrack)
    {
        return FString();
    }

    // 使用UE提供的GetTrackRowDisplayName API获取Row的显示名称
    // 注意：UMovieSceneAudioTrack继承自UMovieSceneNameableTrack，所以可以使用这个方法
    FText RowDisplayName = AudioTrack->GetTrackRowDisplayName(RowIndex);
    FString RowName = RowDisplayName.ToString().TrimStartAndEnd();
    
    // 如果Row有自定义名称，返回该名称；否则返回空字符串（会使用数字编号）
    return RowName;
}

// 音频混音函数（使用交叉调制算法，支持音量控制）
int16 UAudioAssetOperation::MixAudioSamples(int16 Sample1, int16 Sample2, float Volume1, float Volume2)
{
    // 应用音量控制
    float Sig1 = static_cast<float>(Sample1) * Volume1;
    float Sig2 = static_cast<float>(Sample2) * Volume2;
    
    // 使用交叉调制混音算法
    // 当两个信号都为负时，使用不同的调制公式
    float SigOut;
    if (Sig1 < 0.0f && Sig2 < 0.0f) {
        // 两个信号都为负：sig1 + sig2 - (sig1 * sig2 / -32767)
        SigOut = Sig1 + Sig2 - (Sig1 * Sig2 / -32767.0f);
    } else {
        // 其他情况：sig1 + sig2 - (sig1 * sig2 / 32767)
        SigOut = Sig1 + Sig2 - (Sig1 * Sig2 / 32767.0f);
    }
    
    // 确保在有效范围内
    SigOut = FMath::Clamp(SigOut, -32768.0f, 32767.0f);
    
    return static_cast<int16>(SigOut);
}

// 工具：针对一个音频轨道，按 Row 拆分并填充条目
void UAudioAssetOperation::AddAudioTrackEntries(UMovieSceneAudioTrack* AudioTrack,
                                                UMovieScene* MovieScene,
                                                const FString& ParentPath,
                                                int32 CurrentLevel,
                                                TArray<FAudioTrackInfo>& OutTrackInfos,
                                                const FString& DisplayFolderName,
                                                const FString& DisplayActorName,
                                                int32 Ordinal)
{
    if (!AudioTrack) return;

    FString TrackBaseName = AudioTrack->GetDisplayName().ToString().TrimStartAndEnd();
    // 前缀：FolderName_ActorName_ or FolderName_ or ActorName_
    if (!DisplayFolderName.IsEmpty() && !DisplayActorName.IsEmpty())
    {
        // 文件夹中的Actor轨道：FolderName_ActorName_TrackName
        TrackBaseName = FString::Printf(TEXT("%s_%s_%s"), *DisplayFolderName, *DisplayActorName, *TrackBaseName);
    }
    else if (!DisplayFolderName.IsEmpty())
    {
        // 文件夹中的直接轨道：FolderName_TrackName
        TrackBaseName = FString::Printf(TEXT("%s_%s"), *DisplayFolderName, *TrackBaseName);
    }
    else if (!DisplayActorName.IsEmpty())
    {
        // Actor轨道：ActorName_TrackName
        TrackBaseName = FString::Printf(TEXT("%s_%s"), *DisplayActorName, *TrackBaseName);
    }
    else if (ParentPath.IsEmpty())
    {
        // 根级别：以 root 作为前缀
        TrackBaseName = FString::Printf(TEXT("%s_%s"), TEXT("root"), *TrackBaseName);
    }

    // 注意：编号逻辑已移至 RenumberTracksByDisplayOrder 函数中统一处理
    // 这里不再添加编号，避免编号顺序与显示顺序不一致的问题
    const FString BasePath = ParentPath.IsEmpty() ? TrackBaseName : ParentPath + TEXT(" > ") + TrackBaseName;

    // 收集该 Track 使用到的 Row 索引
    TSet<int32> UsedRows;
    for (UMovieSceneSection* Section : AudioTrack->GetAllSections())
    {
        if (Section)
        {
            UsedRows.Add(Section->GetRowIndex());
        }
    }

    if (UsedRows.Num() <= 1)
    {
        // 单行或未设置行：作为一个条目
        FAudioTrackInfo Info;
        Info.DisplayName = TrackBaseName;
        Info.FullPath = BasePath;
        Info.AudioTrack = AudioTrack;
        Info.HierarchyLevel = CurrentLevel;
        Info.TrackIndex = OutTrackInfos.Num();
        Info.RowIndex = INDEX_NONE; // 单行轨道不设置具体的 RowIndex
        Info.RowName = TEXT(""); // 单行轨道不设置Row名称
        OutTrackInfos.Add(Info);
        return;
    }

    // 多行：为每个 Row 生成一个条目，优先使用Row的自定义名称
    TArray<int32> RowsArray = UsedRows.Array();
    RowsArray.Sort(); // 按 Row 索引排序
    
    for (int32 RowIndex = 0; RowIndex < RowsArray.Num(); ++RowIndex)
    {
        int32 OriginalRowIndex = RowsArray[RowIndex];
        
        // 获取该Row的自定义名称
        FString RowName = GetRowDisplayName(AudioTrack, OriginalRowIndex);
        
        FAudioTrackInfo Info;
        Info.AudioTrack = AudioTrack;
        Info.HierarchyLevel = CurrentLevel;
        Info.TrackIndex = OutTrackInfos.Num();
        Info.RowIndex = OriginalRowIndex; // 保存原始的 row_index 用于后续操作
        Info.RowName = RowName;
        
        // 生成显示名称：优先使用Row自定义名称，否则使用数字编号
        if (!RowName.IsEmpty())
        {
            Info.DisplayName = FString::Printf(TEXT("%s_[%s]"), *TrackBaseName, *RowName);
            Info.FullPath = FString::Printf(TEXT("%s > %s"), *BasePath, *RowName);
        }
        else
        {
            int32 DynamicRowNumber = RowIndex + 1; // 动态编号（1, 2, 3...）
            Info.DisplayName = FString::Printf(TEXT("%s_[sub_%d]"), *TrackBaseName, DynamicRowNumber);
            Info.FullPath = FString::Printf(TEXT("%s > 行 %d"), *BasePath, DynamicRowNumber);
        }
        
        OutTrackInfos.Add(Info);
        
        UE_LOG(LogTemp, Log, TEXT("创建多行轨道条目: %s"), *Info.DisplayName);
    }
}


// 重新编号轨道，确保编号顺序与显示顺序一致
void UAudioAssetOperation::RenumberTracksByDisplayOrder(TArray<FAudioTrackInfo>& TrackInfos)
{
    // 按轨道名称分组，统计同名轨道的数量
    TMap<FString, TArray<int32>> TrackNameGroups;
    
    for (int32 i = 0; i < TrackInfos.Num(); ++i)
    {
        const FAudioTrackInfo& TrackInfo = TrackInfos[i];
        FString BaseName = TrackInfo.DisplayName;
        
        // 移除现有的编号后缀（数字编号）
        int32 LastUnderscoreIndex = BaseName.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        if (LastUnderscoreIndex != INDEX_NONE)
        {
            FString Suffix = BaseName.Mid(LastUnderscoreIndex + 1);
            // 检查是否是数字后缀
            if (Suffix.IsNumeric())
            {
                BaseName = BaseName.Left(LastUnderscoreIndex);
            }
        }
        
        // 移除 [sub_X] 或 [SectionName] 后缀
        int32 SubIndex = BaseName.Find(TEXT("_[sub_"));
        if (SubIndex != INDEX_NONE)
        {
            BaseName = BaseName.Left(SubIndex);
        }
        else
        {
            // 检查是否有 [SectionName] 格式的后缀
            int32 SectionIndex = BaseName.Find(TEXT("_["));
            if (SectionIndex != INDEX_NONE)
            {
                int32 EndBracketIndex = BaseName.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SectionIndex);
                if (EndBracketIndex != INDEX_NONE)
                {
                    BaseName = BaseName.Left(SectionIndex);
                }
            }
        }
        
        TrackNameGroups.FindOrAdd(BaseName).Add(i);
    }
    
    // 为每个同名轨道组重新编号
    for (auto& GroupPair : TrackNameGroups)
    {
        const FString& BaseName = GroupPair.Key;
        TArray<int32>& Indices = GroupPair.Value;
        
        if (Indices.Num() > 1)
        {
            // 按显示顺序排序索引
            Indices.Sort([&TrackInfos](int32 A, int32 B) {
                return TrackInfos[A].TrackIndex < TrackInfos[B].TrackIndex;
            });
            
            // 重新编号
            for (int32 i = 0; i < Indices.Num(); ++i)
            {
                int32 TrackIndex = Indices[i];
                FAudioTrackInfo& TrackInfo = TrackInfos[TrackIndex];
                
                // 获取原始名称并移除所有编号
                FString OriginalName = TrackInfo.DisplayName;
                
                // 移除数字编号后缀
                int32 LastUnderscoreIndex = OriginalName.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
                if (LastUnderscoreIndex != INDEX_NONE)
                {
                    FString Suffix = OriginalName.Mid(LastUnderscoreIndex + 1);
                    if (Suffix.IsNumeric())
                    {
                        OriginalName = OriginalName.Left(LastUnderscoreIndex);
                    }
                }
                
                // 检查是否有 [sub_X] 或 [SectionName] 后缀
                int32 SubIndex = OriginalName.Find(TEXT("_[sub_"));
                FString SubSuffix;
                if (SubIndex != INDEX_NONE)
                {
                    // 提取 [sub_X] 后缀
                    SubSuffix = OriginalName.Mid(SubIndex);
                    // 移除 [sub_X] 后缀，只保留基础名称
                    OriginalName = OriginalName.Left(SubIndex);
                }
                else
                {
                    // 检查是否有 [SectionName] 格式的后缀
                    int32 SectionIndex = OriginalName.Find(TEXT("_["));
                    if (SectionIndex != INDEX_NONE)
                    {
                        int32 EndBracketIndex = OriginalName.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SectionIndex);
                        if (EndBracketIndex != INDEX_NONE)
                        {
                            // 提取 [SectionName] 后缀
                            SubSuffix = OriginalName.Mid(SectionIndex);
                            // 移除 [SectionName] 后缀，只保留基础名称
                            OriginalName = OriginalName.Left(SectionIndex);
                        }
                    }
                }
                
                // 构建新的显示名称：基础名称 + 新编号 + 后缀
                FString NewDisplayName;
                if (SubSuffix.IsEmpty())
                {
                    // 没有后缀，直接添加编号
                    NewDisplayName = FString::Printf(TEXT("%s_%d"), *OriginalName, i + 1);
                }
                else
                {
                    // 有后缀，在基础名称后添加编号，然后加上后缀
                    NewDisplayName = FString::Printf(TEXT("%s_%d%s"), *OriginalName, i + 1, *SubSuffix);
                }
                
                TrackInfo.DisplayName = NewDisplayName;
                
                UE_LOG(LogTemp, Log, TEXT("重新编号轨道: %s -> %s"), *OriginalName, *NewDisplayName);
            }
        }
    }
}

// TTS音频处理：添加静音并保存单个音频
bool UAudioAssetOperation::ProcessSingleAudioWithSilence(const FString& AssetPath, const FString& Text, float PreSilence, float PostSilence, USoundWave*& OutProcessedAudio, bool bForceStereo)
{
    // 加载音频资产
    USoundWave* SoundWave = Cast<USoundWave>(StaticLoadObject(USoundWave::StaticClass(), nullptr, *AssetPath));
    if (!SoundWave)
    {
        UE_LOG(LogTemp, Error, TEXT("无法加载音频资产: %s"), *AssetPath);
        return false;
    }

    // 获取音频数据
    TArray<uint8> AudioData;
    uint16 NumChannels = 0;
    uint32 SampleRate = 0;
    
    if (!SoundWave->GetImportedSoundWaveData(AudioData, SampleRate, NumChannels))
    {
        UE_LOG(LogTemp, Error, TEXT("无法获取音频数据: %s"), *AssetPath);
        return false;
    }

    // 计算静音样本数
    int32 PreSilenceSamples = FMath::RoundToInt(PreSilence * SampleRate * NumChannels);
    int32 PostSilenceSamples = FMath::RoundToInt(PostSilence * SampleRate * NumChannels);
    
    // 创建新的音频数据（包含静音）
    TArray<uint8> ProcessedAudioData;
    int32 TotalSamples = AudioData.Num() / sizeof(int16) + PreSilenceSamples + PostSilenceSamples;
    ProcessedAudioData.SetNum(TotalSamples * sizeof(int16));
    
    int16* ProcessedPCM = reinterpret_cast<int16*>(ProcessedAudioData.GetData());
    int16* OriginalPCM = reinterpret_cast<int16*>(AudioData.GetData());
    int32 OriginalSamples = AudioData.Num() / sizeof(int16);
    
    // 填充段前静音（0值）
    for (int32 i = 0; i < PreSilenceSamples; ++i)
    {
        ProcessedPCM[i] = 0;
    }
    
    // 复制原始音频数据
    FMemory::Memcpy(ProcessedPCM + PreSilenceSamples, OriginalPCM, OriginalSamples * sizeof(int16));
    
    // 填充段后静音（0值）
    for (int32 i = 0; i < PostSilenceSamples; ++i)
    {
        ProcessedPCM[PreSilenceSamples + OriginalSamples + i] = 0;
    }

    // 保存为WAV格式i
    TArray<uint8> WavData;
    if (!UGenTools::ExportPCMToWavData(ProcessedAudioData, SampleRate, NumChannels, WavData, bForceStereo))
    {
        UE_LOG(LogTemp, Error, TEXT("导出WAV数据失败"));
        return false;
    }

    // 保存为音频资产
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    FString BasePath = Settings->TTSStoragePath;
    FString AssetPathDir, WavDir;
    if (!UGenTools::SetupTTSStoragePaths(BasePath, AssetPathDir, WavDir))
    {
        UE_LOG(LogTemp, Error, TEXT("设置TTS存储路径失败"));
        return false;
    }
    
    FString AssetName = TEXT("TTS_Single_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    
    // 直接保存为音频资产，不保存wav文件
    bool bSaved = UGenTools::SaveAudioAsSoundWaveAsset(WavData, AssetPathDir, AssetName);
    if (!bSaved)
    {
        UE_LOG(LogTemp, Error, TEXT("保存音频资产失败"));
        return false;
    }
    
    // 加载保存的资产
    FString FullPackageName = AssetPathDir + AssetName;
    FString AssetPathForLoad = FString::Printf(TEXT("%s.%s"), *FullPackageName, *AssetName);
    
    FPlatformProcess::Sleep(0.1f);
    
    USoundWave* LoadedSoundWave = nullptr;
    for (int32 RetryCount = 0; RetryCount < 5; ++RetryCount)
    {
        LoadedSoundWave = LoadObject<USoundWave>(nullptr, *AssetPathForLoad);
        if (LoadedSoundWave)
        {
            break;
        }
        FPlatformProcess::Sleep(0.2f);
    }
    
    if (LoadedSoundWave)
    {
        OutProcessedAudio = LoadedSoundWave;
        UE_LOG(LogTemp, Log, TEXT("单个音频处理完成 - 文本: %s, 段前静音: %.2f秒, 段后静音: %.2f秒"), 
               *Text, PreSilence, PostSilence);
        return true;
    }
    
    UE_LOG(LogTemp, Error, TEXT("无法加载处理后的音频资产"));
    return false;
}

// TTS音频处理：添加静音并合并多个音频
bool UAudioAssetOperation::ProcessMultipleAudioWithSilence(const TArray<FString>& AssetPaths, const TArray<FString>& Texts, const TArray<float>& PreSilences, const TArray<float>& PostSilences, USoundWave*& OutMergedAudio, const FString& CustomFileName, bool bForceStereo)
{
    if (AssetPaths.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("没有音频资产需要处理"));
        return false;
    }

    // 1. 加载所有音频并获取最高采样率
    TArray<USoundWave*> SoundWaves;
    uint32 MaxSampleRate = 0;
    uint16 MaxChannels = 0;
    
    for (const FString& AssetPath : AssetPaths)
    {
        USoundWave* SoundWave = Cast<USoundWave>(StaticLoadObject(USoundWave::StaticClass(), nullptr, *AssetPath));
        if (!SoundWave)
        {
            UE_LOG(LogTemp, Error, TEXT("无法加载音频资产: %s"), *AssetPath);
            return false;
        }
        
        TArray<uint8> DummyData;
        uint16 Channels = 0;
        uint32 SampleRate = 0;
        if (SoundWave->GetImportedSoundWaveData(DummyData, SampleRate, Channels))
        {
            MaxSampleRate = FMath::Max(MaxSampleRate, SampleRate);
            MaxChannels = FMath::Max(MaxChannels, Channels);
        }
        
        SoundWaves.Add(SoundWave);
    }
    
    if (MaxSampleRate == 0 || MaxChannels == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("无法获取有效的音频参数"));
        return false;
    }

    // 2. 处理每个音频片段
    TArray<TArray<uint8>> ProcessedAudioSegments;
    TArray<int32> SegmentSampleCounts;
    
    for (int32 i = 0; i < SoundWaves.Num(); ++i)
    {
        USoundWave* SoundWave = SoundWaves[i];
        TArray<uint8> AudioData;
        uint16 Channels = 0;
        uint32 SampleRate = 0;
        
        if (!SoundWave->GetImportedSoundWaveData(AudioData, SampleRate, Channels))
        {
            UE_LOG(LogTemp, Error, TEXT("无法获取音频数据: %s"), *AssetPaths[i]);
            return false;
        }
        
        // 如果需要，进行格式转换
        if (SampleRate != MaxSampleRate || Channels != MaxChannels)
        {
            TArray<uint8> ConvertedData;
            if (!ConvertAudioFormat(AudioData, SampleRate, Channels, MaxSampleRate, MaxChannels, ConvertedData))
            {
                UE_LOG(LogTemp, Error, TEXT("音频格式转换失败: %s"), *AssetPaths[i]);
                return false;
            }
            AudioData = ConvertedData;
        }
        
        // 添加静音
        float PreSilence = (i < PreSilences.Num()) ? PreSilences[i] : 0.0f;
        float PostSilence = (i < PostSilences.Num()) ? PostSilences[i] : 0.0f;
        
        int32 PreSilenceSamples = FMath::RoundToInt(PreSilence * MaxSampleRate * MaxChannels);
        int32 PostSilenceSamples = FMath::RoundToInt(PostSilence * MaxSampleRate * MaxChannels);
        
        TArray<uint8> ProcessedSegment;
        int32 OriginalSamples = AudioData.Num() / sizeof(int16);
        int32 TotalSamples = OriginalSamples + PreSilenceSamples + PostSilenceSamples;
        ProcessedSegment.SetNum(TotalSamples * sizeof(int16));
        
        int16* ProcessedPCM = reinterpret_cast<int16*>(ProcessedSegment.GetData());
        int16* OriginalPCM = reinterpret_cast<int16*>(AudioData.GetData());
        
        // 填充段前静音
        for (int32 j = 0; j < PreSilenceSamples; ++j)
        {
            ProcessedPCM[j] = 0;
        }
        
        // 复制原始音频
        FMemory::Memcpy(ProcessedPCM + PreSilenceSamples, OriginalPCM, OriginalSamples * sizeof(int16));
        
        // 填充段后静音
        for (int32 j = 0; j < PostSilenceSamples; ++j)
        {
            ProcessedPCM[PreSilenceSamples + OriginalSamples + j] = 0;
        }
        
        ProcessedAudioSegments.Add(ProcessedSegment);
        SegmentSampleCounts.Add(TotalSamples);
    }

    // 3. 合并所有音频片段
    int32 TotalSamples = 0;
    for (int32 SampleCount : SegmentSampleCounts)
    {
        TotalSamples += SampleCount;
    }
    
    TArray<uint8> MergedAudioData;
    MergedAudioData.SetNum(TotalSamples * sizeof(int16));
    int16* MergedPCM = reinterpret_cast<int16*>(MergedAudioData.GetData());
    
    int32 CurrentSampleOffset = 0;
    for (int32 i = 0; i < ProcessedAudioSegments.Num(); ++i)
    {
        const TArray<uint8>& Segment = ProcessedAudioSegments[i];
        int32 SegmentSamples = SegmentSampleCounts[i];
        
        FMemory::Memcpy(MergedPCM + CurrentSampleOffset, Segment.GetData(), SegmentSamples * sizeof(int16));
        CurrentSampleOffset += SegmentSamples;
    }

    // 4. 保存为WAV格式
    TArray<uint8> WavData;
    if (!UGenTools::ExportPCMToWavData(MergedAudioData, MaxSampleRate, MaxChannels, WavData, bForceStereo))
    {
        UE_LOG(LogTemp, Error, TEXT("导出WAV数据失败"));
        return false;
    }

    // 5. 保存为音频资产
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    FString BasePath = Settings->TTSStoragePath;
    FString AssetPathDir, WavDir;
    if (!UGenTools::SetupTTSStoragePaths(BasePath, AssetPathDir, WavDir))
    {
        UE_LOG(LogTemp, Error, TEXT("设置TTS存储路径失败"));
        return false;
    }
    
    FString AssetName;
    if (!CustomFileName.IsEmpty())
    {
        AssetName = CustomFileName;
    }
    else
    {
        AssetName = TEXT("TTS_Merged_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    }
    
    // 直接保存为音频资产，不保存wav文件
    bool bSaved = UGenTools::SaveAudioAsSoundWaveAsset(WavData, AssetPathDir, AssetName);
    if (!bSaved)
    {
        UE_LOG(LogTemp, Error, TEXT("保存音频资产失败"));
        return false;
    }
    
    // 6. 加载保存的资产
    FString FullPackageName = AssetPathDir + AssetName;
    FString AssetPathForLoad = FString::Printf(TEXT("%s.%s"), *FullPackageName, *AssetName);
    
    FPlatformProcess::Sleep(0.1f);
    
    USoundWave* LoadedSoundWave = nullptr;
    for (int32 RetryCount = 0; RetryCount < 5; ++RetryCount)
    {
        LoadedSoundWave = LoadObject<USoundWave>(nullptr, *AssetPathForLoad);
        if (LoadedSoundWave)
        {
            break;
        }
        FPlatformProcess::Sleep(0.2f);
    }
    
    if (LoadedSoundWave)
    {
        OutMergedAudio = LoadedSoundWave;
        UE_LOG(LogTemp, Log, TEXT("多个音频合并完成 - 合并了 %d 个音频片段"), AssetPaths.Num());
        for (int32 i = 0; i < Texts.Num(); ++i)
        {
            float PreSilence = (i < PreSilences.Num()) ? PreSilences[i] : 0.0f;
            float PostSilence = (i < PostSilences.Num()) ? PostSilences[i] : 0.0f;
            UE_LOG(LogTemp, Log, TEXT("  [%d] 文本: %s, 段前静音: %.2f秒, 段后静音: %.2f秒"), 
                   i, *Texts[i], PreSilence, PostSilence);
        }
        return true;
    }
    
    UE_LOG(LogTemp, Error, TEXT("无法加载合并后的音频资产"));
    return false;
}

// TTS音频处理：从内存数据添加静音并保存单个音频
bool UAudioAssetOperation::ProcessSingleAudioWithSilenceFromMemory(const TArray<uint8>& AudioData, const FString& Text, float PreSilence, float PostSilence, USoundWave*& OutProcessedAudio, bool bForceStereo)
{
    if (AudioData.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("音频数据为空"));
        return false;
    }

    // 从内存数据创建临时SoundWave获取音频信息
    USoundWave* TempWave = UGenTools::CreateSoundWaveFromAudioData(AudioData);
    if (!TempWave)
    {
        UE_LOG(LogTemp, Error, TEXT("无法从内存数据创建SoundWave"));
        return false;
    }

    // 获取音频数据
    TArray<uint8> PCMData;
    uint16 NumChannels = 0;
    uint32 SampleRate = 0;
    
    if (!TempWave->GetImportedSoundWaveData(PCMData, SampleRate, NumChannels))
    {
        UE_LOG(LogTemp, Error, TEXT("无法获取音频数据"));
        TempWave->MarkAsGarbage();
        TempWave->ConditionalBeginDestroy();
        return false;
    }

    // 清理临时对象
    TempWave->MarkAsGarbage();
    TempWave->ConditionalBeginDestroy();

    // 计算静音样本数
    int32 PreSilenceSamples = FMath::RoundToInt(PreSilence * SampleRate * NumChannels);
    int32 PostSilenceSamples = FMath::RoundToInt(PostSilence * SampleRate * NumChannels);
    
    // 创建新的音频数据（包含静音）
    TArray<uint8> ProcessedAudioData;
    int32 TotalSamples = PCMData.Num() / sizeof(int16) + PreSilenceSamples + PostSilenceSamples;
    ProcessedAudioData.SetNum(TotalSamples * sizeof(int16));
    
    int16* ProcessedPCM = reinterpret_cast<int16*>(ProcessedAudioData.GetData());
    int16* OriginalPCM = reinterpret_cast<int16*>(PCMData.GetData());
    int32 OriginalSamples = PCMData.Num() / sizeof(int16);
    
    // 填充段前静音（0值）
    for (int32 i = 0; i < PreSilenceSamples; ++i)
    {
        ProcessedPCM[i] = 0;
    }
    
    // 复制原始音频数据
    FMemory::Memcpy(ProcessedPCM + PreSilenceSamples, OriginalPCM, OriginalSamples * sizeof(int16));
    
    // 填充段后静音（0值）
    for (int32 i = 0; i < PostSilenceSamples; ++i)
    {
        ProcessedPCM[PreSilenceSamples + OriginalSamples + i] = 0;
    }

    // 保存为WAV格式
    TArray<uint8> WavData;
    if (!UGenTools::ExportPCMToWavData(ProcessedAudioData, SampleRate, NumChannels, WavData, bForceStereo))
    {
        UE_LOG(LogTemp, Error, TEXT("导出WAV数据失败"));
        return false;
    }

    // 保存为音频资产
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    FString BasePath = Settings->TTSStoragePath;
    FString AssetPathDir, WavDir;
    if (!UGenTools::SetupTTSStoragePaths(BasePath, AssetPathDir, WavDir))
    {
        UE_LOG(LogTemp, Error, TEXT("设置TTS存储路径失败"));
        return false;
    }
    
    FString AssetName = TEXT("TTS_Single_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    
    // 直接保存为音频资产，不保存wav文件
    bool bSaved = UGenTools::SaveAudioAsSoundWaveAsset(WavData, AssetPathDir, AssetName);
    if (!bSaved)
    {
        UE_LOG(LogTemp, Error, TEXT("保存音频资产失败"));
        return false;
    }
    
    // 加载保存的资产
    FString FullPackageName = AssetPathDir + AssetName;
    FString AssetPathForLoad = FString::Printf(TEXT("%s.%s"), *FullPackageName, *AssetName);
    
    FPlatformProcess::Sleep(0.1f);
    
    USoundWave* LoadedSoundWave = nullptr;
    for (int32 RetryCount = 0; RetryCount < 5; ++RetryCount)
    {
        LoadedSoundWave = LoadObject<USoundWave>(nullptr, *AssetPathForLoad);
        if (LoadedSoundWave)
        {
            break;
        }
        FPlatformProcess::Sleep(0.2f);
    }
    
    if (LoadedSoundWave)
    {
        OutProcessedAudio = LoadedSoundWave;
        UE_LOG(LogTemp, Log, TEXT("从内存数据处理单个音频完成 - 文本: %s, 段前静音: %.2f秒, 段后静音: %.2f秒"), 
               *Text, PreSilence, PostSilence);
        return true;
    }
    
    UE_LOG(LogTemp, Error, TEXT("无法加载处理后的音频资产"));
    return false;
}

// TTS音频处理：从内存数据添加静音并合并多个音频
bool UAudioAssetOperation::ProcessMultipleAudioWithSilenceFromMemory(const TArray<FAudioDataWrapper>& AudioDataArray, const TArray<FString>& Texts, const TArray<float>& PreSilences, const TArray<float>& PostSilences, USoundWave*& OutMergedAudio, const FString& CustomFileName, bool bForceStereo)
{
    if (AudioDataArray.Num() == 0 || AudioDataArray.Num() != Texts.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("音频数据数组为空或与文本数量不匹配"));
        return false;
    }

    // 处理每个音频片段
    TArray<TArray<uint8>> ProcessedAudioSegments;
    TArray<int32> SegmentSampleCounts;
    uint32 MaxSampleRate = 0;
    uint16 MaxChannels = 0;
    
    for (int32 i = 0; i < AudioDataArray.Num(); ++i)
    {
        const TArray<uint8>& AudioData = AudioDataArray[i].AudioData;
        if (AudioData.Num() == 0)
        {
            UE_LOG(LogTemp, Error, TEXT("第 %d 个音频数据为空"), i);
            continue;
        }

        // 从内存数据创建临时SoundWave获取音频信息
        USoundWave* TempWave = UGenTools::CreateSoundWaveFromAudioData(AudioData);
        if (!TempWave)
        {
            UE_LOG(LogTemp, Error, TEXT("无法从内存数据创建SoundWave，索引: %d"), i);
            continue;
        }

        // 获取音频数据
        TArray<uint8> PCMData;
        uint16 NumChannels = 0;
        uint32 SampleRate = 0;
        
        if (!TempWave->GetImportedSoundWaveData(PCMData, SampleRate, NumChannels))
        {
            UE_LOG(LogTemp, Error, TEXT("无法获取音频数据，索引: %d"), i);
            TempWave->MarkAsGarbage();
            TempWave->ConditionalBeginDestroy();
            continue;
        }

        // 清理临时对象
        TempWave->MarkAsGarbage();
        TempWave->ConditionalBeginDestroy();

        // 更新最大采样率和声道数
        MaxSampleRate = FMath::Max(MaxSampleRate, SampleRate);
        MaxChannels = FMath::Max(MaxChannels, NumChannels);

        // 计算静音样本数
        float PreSilence = (i < PreSilences.Num()) ? PreSilences[i] : 0.0f;
        float PostSilence = (i < PostSilences.Num()) ? PostSilences[i] : 0.0f;
        
        int32 PreSilenceSamples = FMath::RoundToInt(PreSilence * SampleRate * NumChannels);
        int32 PostSilenceSamples = FMath::RoundToInt(PostSilence * SampleRate * NumChannels);
        
        // 创建新的音频数据（包含静音）
        TArray<uint8> ProcessedSegment;
        int32 TotalSamples = PCMData.Num() / sizeof(int16) + PreSilenceSamples + PostSilenceSamples;
        ProcessedSegment.SetNum(TotalSamples * sizeof(int16));
        
        int16* ProcessedPCM = reinterpret_cast<int16*>(ProcessedSegment.GetData());
        int16* OriginalPCM = reinterpret_cast<int16*>(PCMData.GetData());
        int32 OriginalSamples = PCMData.Num() / sizeof(int16);
        
        // 填充段前静音（0值）
        for (int32 j = 0; j < PreSilenceSamples; ++j)
        {
            ProcessedPCM[j] = 0;
        }
        
        // 复制原始音频数据
        FMemory::Memcpy(ProcessedPCM + PreSilenceSamples, OriginalPCM, OriginalSamples * sizeof(int16));
        
        // 填充段后静音（0值）
        for (int32 j = 0; j < PostSilenceSamples; ++j)
        {
            ProcessedPCM[PreSilenceSamples + OriginalSamples + j] = 0;
        }
        
        ProcessedAudioSegments.Add(ProcessedSegment);
        SegmentSampleCounts.Add(TotalSamples);
    }

    if (ProcessedAudioSegments.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("没有有效的音频片段可以处理"));
        return false;
    }

    // 合并所有音频片段
    int32 TotalSamples = 0;
    for (int32 SampleCount : SegmentSampleCounts)
    {
        TotalSamples += SampleCount;
    }
    
    TArray<uint8> MergedAudioData;
    MergedAudioData.SetNum(TotalSamples * sizeof(int16));
    int16* MergedPCM = reinterpret_cast<int16*>(MergedAudioData.GetData());
    
    int32 CurrentSampleOffset = 0;
    for (int32 i = 0; i < ProcessedAudioSegments.Num(); ++i)
    {
        const TArray<uint8>& Segment = ProcessedAudioSegments[i];
        int32 SegmentSamples = SegmentSampleCounts[i];
        
        FMemory::Memcpy(MergedPCM + CurrentSampleOffset, Segment.GetData(), SegmentSamples * sizeof(int16));
        CurrentSampleOffset += SegmentSamples;
    }

    // 保存为WAV格式
    TArray<uint8> WavData;
    if (!UGenTools::ExportPCMToWavData(MergedAudioData, MaxSampleRate, MaxChannels, WavData, bForceStereo))
    {
        UE_LOG(LogTemp, Error, TEXT("导出WAV数据失败"));
        return false;
    }

    // 保存为音频资产
    const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
    FString BasePath = Settings->TTSStoragePath;
    FString AssetPathDir, WavDir;
    if (!UGenTools::SetupTTSStoragePaths(BasePath, AssetPathDir, WavDir))
    {
        UE_LOG(LogTemp, Error, TEXT("设置TTS存储路径失败"));
        return false;
    }
    
    FString AssetName;
    if (!CustomFileName.IsEmpty())
    {
        AssetName = CustomFileName;
    }
    else
    {
        AssetName = TEXT("TTS_Merged_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    }
    
    // 直接保存为音频资产，不保存wav文件
    bool bSaved = UGenTools::SaveAudioAsSoundWaveAsset(WavData, AssetPathDir, AssetName);
    if (!bSaved)
    {
        UE_LOG(LogTemp, Error, TEXT("保存音频资产失败"));
        return false;
    }
    
    // 加载保存的资产
    FString FullPackageName = AssetPathDir + AssetName;
    FString AssetPathForLoad = FString::Printf(TEXT("%s.%s"), *FullPackageName, *AssetName);
    
    FPlatformProcess::Sleep(0.1f);
    
    USoundWave* LoadedSoundWave = nullptr;
    for (int32 RetryCount = 0; RetryCount < 5; ++RetryCount)
    {
        LoadedSoundWave = LoadObject<USoundWave>(nullptr, *AssetPathForLoad);
        if (LoadedSoundWave)
        {
            break;
        }
        FPlatformProcess::Sleep(0.2f);
    }
    
    if (LoadedSoundWave)
    {
        OutMergedAudio = LoadedSoundWave;
        UE_LOG(LogTemp, Log, TEXT("从内存数据合并多个音频完成 - 合并了 %d 个音频片段"), AudioDataArray.Num());
        for (int32 i = 0; i < Texts.Num(); ++i)
        {
            float PreSilence = (i < PreSilences.Num()) ? PreSilences[i] : 0.0f;
            float PostSilence = (i < PostSilences.Num()) ? PostSilences[i] : 0.0f;
            UE_LOG(LogTemp, Log, TEXT("  [%d] 文本: %s, 段前静音: %.2f秒, 段后静音: %.2f秒"), 
                   i, *Texts[i], PreSilence, PostSilence);
        }
        return true;
    }
    
    UE_LOG(LogTemp, Error, TEXT("无法加载合并后的音频资产"));
    return false;
}

// TTS音频处理：从文件添加静音并保存单个音频
bool UAudioAssetOperation::ProcessSingleAudioWithSilenceFromFile(const FString& FilePath, const FString& Text, float PreSilence, float PostSilence, USoundWave*& OutProcessedAudio, bool bForceStereo)
{
	OutProcessedAudio = nullptr;

	if (FilePath.IsEmpty() || !FPaths::FileExists(FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("无效的文件路径: %s"), *FilePath);
		return false;
	}

	// 从WAV文件创建临时SoundWave
	USoundWave* TempWave = UGenTools::CreateSoundWaveFromWavFile(FilePath);
	if (!TempWave)
	{
		UE_LOG(LogTemp, Error, TEXT("无法从文件创建SoundWave: %s"), *FilePath);
		return false;
	}

	// 提取PCM数据
	TArray<uint8> PCMData;
	uint16 NumChannels = 0;
	uint32 SampleRate = 0;
	if (!TempWave->GetImportedSoundWaveData(PCMData, SampleRate, NumChannels))
	{
		UE_LOG(LogTemp, Error, TEXT("无法获取音频数据: %s"), *FilePath);
		TempWave->MarkAsGarbage();
		TempWave->ConditionalBeginDestroy();
		return false;
	}

	// 释放临时对象
	TempWave->MarkAsGarbage();
	TempWave->ConditionalBeginDestroy();

	// 计算静音样本数
	int32 PreSilenceSamples = FMath::RoundToInt(PreSilence * SampleRate * NumChannels);
	int32 PostSilenceSamples = FMath::RoundToInt(PostSilence * SampleRate * NumChannels);

	// 生成处理后的PCM（前后静音 + 原音频）
	TArray<uint8> ProcessedAudioData;
	int32 OriginalSamples = PCMData.Num() / sizeof(int16);
	int32 TotalSamples = OriginalSamples + PreSilenceSamples + PostSilenceSamples;
	ProcessedAudioData.SetNum(TotalSamples * sizeof(int16));

	int16* ProcessedPCM = reinterpret_cast<int16*>(ProcessedAudioData.GetData());
	int16* OriginalPCM = reinterpret_cast<int16*>(PCMData.GetData());

	// 前静音
	for (int32 i = 0; i < PreSilenceSamples; ++i)
	{
		ProcessedPCM[i] = 0;
	}

	// 原音频
	FMemory::Memcpy(ProcessedPCM + PreSilenceSamples, OriginalPCM, OriginalSamples * sizeof(int16));

	// 后静音
	for (int32 i = 0; i < PostSilenceSamples; ++i)
	{
		ProcessedPCM[PreSilenceSamples + OriginalSamples + i] = 0;
	}

	// 导出为WAV数据
	TArray<uint8> WavData;
	if (!UGenTools::ExportPCMToWavData(ProcessedAudioData, SampleRate, NumChannels, WavData, bForceStereo))
	{
		UE_LOG(LogTemp, Error, TEXT("导出WAV数据失败"));
		return false;
	}

	// 保存为资产
	const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
	FString BasePath = Settings->TTSStoragePath;
	FString AssetPathDir, WavDir;
	if (!UGenTools::SetupTTSStoragePaths(BasePath, AssetPathDir, WavDir))
	{
		UE_LOG(LogTemp, Error, TEXT("设置TTS存储路径失败"));
		return false;
	}

	FString AssetName = TEXT("TTS_Single_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));

	// 直接保存为音频资产，不保存wav文件
	if (!UGenTools::SaveAudioAsSoundWaveAsset(WavData, AssetPathDir, AssetName))
	{
		UE_LOG(LogTemp, Error, TEXT("保存音频资产失败"));
		return false;
	}

	// 加载资产
	const FString FullPackageName = AssetPathDir + AssetName;
	const FString AssetPathForLoad = FString::Printf(TEXT("%s.%s"), *FullPackageName, *AssetName);

	FPlatformProcess::Sleep(0.1f);
	USoundWave* LoadedSoundWave = nullptr;
	for (int32 RetryCount = 0; RetryCount < 5; ++RetryCount)
	{
		LoadedSoundWave = LoadObject<USoundWave>(nullptr, *AssetPathForLoad);
		if (LoadedSoundWave)
		{
			break;
		}
		FPlatformProcess::Sleep(0.2f);
	}

	if (!LoadedSoundWave)
	{
		UE_LOG(LogTemp, Error, TEXT("无法加载处理后的音频资产: %s"), *AssetPathForLoad);
		return false;
	}

	OutProcessedAudio = LoadedSoundWave;
	UE_LOG(LogTemp, Log, TEXT("从文件处理单个音频完成 - 文本: %s, 段前静音: %.2f秒, 段后静音: %.2f秒"), *Text, PreSilence, PostSilence);
	return true;
}

// TTS音频处理：从文件添加静音并合并多个音频
bool UAudioAssetOperation::ProcessMultipleAudioWithSilenceFromFiles(const TArray<FString>& FilePaths, const TArray<FString>& Texts, const TArray<float>& PreSilences, const TArray<float>& PostSilences, USoundWave*& OutMergedAudio, const FString& CustomFileName, bool bForceStereo)
{
	OutMergedAudio = nullptr;

	if (FilePaths.Num() == 0 || FilePaths.Num() != Texts.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("文件路径数量为0或与文本数量不匹配"));
		return false;
	}

	// 读取所有文件，统一到相同采样率/声道，并添加静音
	uint32 MaxSampleRate = 0;
	uint16 MaxChannels = 0;

	TArray<TArray<uint8>> ProcessedAudioSegments;
	TArray<int32> SegmentSampleCounts;

	for (int32 i = 0; i < FilePaths.Num(); ++i)
	{
		const FString& FilePath = FilePaths[i];
		if (FilePath.IsEmpty() || !FPaths::FileExists(FilePath))
		{
			UE_LOG(LogTemp, Error, TEXT("无效的文件路径: %s"), *FilePath);
			return false;
		}

		USoundWave* TempWave = UGenTools::CreateSoundWaveFromWavFile(FilePath);
		if (!TempWave)
		{
			UE_LOG(LogTemp, Error, TEXT("无法从文件创建SoundWave: %s"), *FilePath);
			return false;
		}

		TArray<uint8> AudioData;
		uint16 Channels = 0;
		uint32 SampleRate = 0;
		if (!TempWave->GetImportedSoundWaveData(AudioData, SampleRate, Channels))
		{
			UE_LOG(LogTemp, Error, TEXT("无法获取音频数据: %s"), *FilePath);
			TempWave->MarkAsGarbage();
			TempWave->ConditionalBeginDestroy();
			return false;
		}

		TempWave->MarkAsGarbage();
		TempWave->ConditionalBeginDestroy();

		MaxSampleRate = FMath::Max(MaxSampleRate, SampleRate);
		MaxChannels = FMath::Max(MaxChannels, Channels);

		// 如需，转换到后续合并所需的最高采样率/声道数
		if (SampleRate != MaxSampleRate || Channels != MaxChannels)
		{
			TArray<uint8> ConvertedData;
			if (!ConvertAudioFormat(AudioData, SampleRate, Channels, MaxSampleRate, MaxChannels, ConvertedData))
			{
				UE_LOG(LogTemp, Error, TEXT("音频格式转换失败: %s"), *FilePath);
				return false;
			}
			AudioData = ConvertedData;
			SampleRate = MaxSampleRate;
			Channels = MaxChannels;
		}

		// 添加静音
		float PreSilence = (i < PreSilences.Num()) ? PreSilences[i] : 0.0f;
		float PostSilence = (i < PostSilences.Num()) ? PostSilences[i] : 0.0f;

		int32 PreSilenceSamples = FMath::RoundToInt(PreSilence * SampleRate * Channels);
		int32 PostSilenceSamples = FMath::RoundToInt(PostSilence * SampleRate * Channels);

		TArray<uint8> ProcessedSegment;
		int32 OriginalSamples = AudioData.Num() / sizeof(int16);
		int32 TotalSamples = OriginalSamples + PreSilenceSamples + PostSilenceSamples;
		ProcessedSegment.SetNum(TotalSamples * sizeof(int16));

		int16* ProcessedPCM = reinterpret_cast<int16*>(ProcessedSegment.GetData());
		int16* OriginalPCM = reinterpret_cast<int16*>(AudioData.GetData());

		for (int32 j = 0; j < PreSilenceSamples; ++j)
		{
			ProcessedPCM[j] = 0;
		}
		FMemory::Memcpy(ProcessedPCM + PreSilenceSamples, OriginalPCM, OriginalSamples * sizeof(int16));
		for (int32 j = 0; j < PostSilenceSamples; ++j)
		{
			ProcessedPCM[PreSilenceSamples + OriginalSamples + j] = 0;
		}

		ProcessedAudioSegments.Add(ProcessedSegment);
		SegmentSampleCounts.Add(TotalSamples);
	}

	// 合并
	int32 TotalSamples = 0;
	for (int32 Count : SegmentSampleCounts) { TotalSamples += Count; }

	TArray<uint8> MergedAudioData;
	MergedAudioData.SetNum(TotalSamples * sizeof(int16));
	int16* MergedPCM = reinterpret_cast<int16*>(MergedAudioData.GetData());

	int32 Offset = 0;
	for (int32 i = 0; i < ProcessedAudioSegments.Num(); ++i)
	{
		const TArray<uint8>& Segment = ProcessedAudioSegments[i];
		int32 SegmentSamples = SegmentSampleCounts[i];
		FMemory::Memcpy(MergedPCM + Offset, Segment.GetData(), SegmentSamples * sizeof(int16));
		Offset += SegmentSamples;
	}

	// 导出并保存
	TArray<uint8> WavData;
	if (!UGenTools::ExportPCMToWavData(MergedAudioData, MaxSampleRate, MaxChannels, WavData, bForceStereo))
	{
		UE_LOG(LogTemp, Error, TEXT("导出WAV数据失败"));
		return false;
	}

	const UXVCPluginSettings* Settings = GetDefault<UXVCPluginSettings>();
	FString AssetPathDir, WavDir;
	if (!UGenTools::SetupTTSStoragePaths(Settings->TTSStoragePath, AssetPathDir, WavDir))
	{
		UE_LOG(LogTemp, Error, TEXT("设置TTS存储路径失败"));
		return false;
	}

	FString AssetName;
	if (!CustomFileName.IsEmpty())
	{
		AssetName = CustomFileName;
	}
	else
	{
		AssetName = TEXT("TTS_Merged_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	}

	// 直接保存为音频资产，不保存wav文件
	if (!UGenTools::SaveAudioAsSoundWaveAsset(WavData, AssetPathDir, AssetName))
	{
		UE_LOG(LogTemp, Error, TEXT("保存音频资产失败"));
		return false;
	}

	const FString FullPackageName = AssetPathDir + AssetName;
	const FString AssetPathForLoad = FString::Printf(TEXT("%s.%s"), *FullPackageName, *AssetName);

	FPlatformProcess::Sleep(0.1f);
	USoundWave* LoadedSoundWave = nullptr;
	for (int32 RetryCount = 0; RetryCount < 5; ++RetryCount)
	{
		LoadedSoundWave = LoadObject<USoundWave>(nullptr, *AssetPathForLoad);
		if (LoadedSoundWave) { break; }
		FPlatformProcess::Sleep(0.2f);
	}

	if (!LoadedSoundWave)
	{
		UE_LOG(LogTemp, Error, TEXT("无法加载合并后的音频资产: %s"), *AssetPathForLoad);
		return false;
	}

	OutMergedAudio = LoadedSoundWave;
	UE_LOG(LogTemp, Log, TEXT("从文件合并多个音频完成 - 合并了 %d 个音频片段"), FilePaths.Num());
	for (int32 i = 0; i < Texts.Num(); ++i)
	{
		float PreS = (i < PreSilences.Num()) ? PreSilences[i] : 0.0f;
		float PostS = (i < PostSilences.Num()) ? PostSilences[i] : 0.0f;
		UE_LOG(LogTemp, Log, TEXT("  [%d] 文本: %s, 段前静音: %.2f秒, 段后静音: %.2f秒"), i, *Texts[i], PreS, PostS);
	}
	return true;
}

// TTS音频处理：从内存数据添加静音并合并多个音频（不保存资产，只返回音频数据）
bool UAudioAssetOperation::ProcessMultipleAudioWithSilenceFromMemoryNoSave(const TArray<FAudioDataWrapper>& AudioDataArray, const TArray<FString>& Texts, const TArray<float>& PreSilences, const TArray<float>& PostSilences, TArray<uint8>& OutMergedAudioData, int32& OutSampleRate, int32& OutNumChannels)
{
    if (AudioDataArray.Num() == 0 || AudioDataArray.Num() != Texts.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("音频数据数组为空或与文本数量不匹配"));
        return false;
    }

    // 处理每个音频片段
    TArray<TArray<uint8>> ProcessedAudioSegments;
    TArray<int32> SegmentSampleCounts;
    uint32 MaxSampleRate = 0;
    uint16 MaxChannels = 0;
    
    for (int32 i = 0; i < AudioDataArray.Num(); ++i)
    {
        const TArray<uint8>& AudioData = AudioDataArray[i].AudioData;
        if (AudioData.Num() == 0)
        {
            UE_LOG(LogTemp, Error, TEXT("第 %d 个音频数据为空"), i);
            continue;
        }

        // 从内存数据创建临时SoundWave获取音频信息
        USoundWave* TempWave = UGenTools::CreateSoundWaveFromAudioData(AudioData);
        if (!TempWave)
        {
            UE_LOG(LogTemp, Error, TEXT("无法从内存数据创建SoundWave，索引: %d"), i);
            continue;
        }

        // 获取音频数据
        TArray<uint8> PCMData;
        uint16 NumChannels = 0;
        uint32 SampleRate = 0;
        
        if (!TempWave->GetImportedSoundWaveData(PCMData, SampleRate, NumChannels))
        {
            UE_LOG(LogTemp, Error, TEXT("无法获取音频数据，索引: %d"), i);
            TempWave->MarkAsGarbage();
            TempWave->ConditionalBeginDestroy();
            continue;
        }

        // 清理临时对象
        TempWave->MarkAsGarbage();
        TempWave->ConditionalBeginDestroy();

        // 更新最大采样率和声道数
        MaxSampleRate = FMath::Max(MaxSampleRate, SampleRate);
        MaxChannels = FMath::Max(MaxChannels, NumChannels);

        // 计算静音样本数
        float PreSilence = (i < PreSilences.Num()) ? PreSilences[i] : 0.0f;
        float PostSilence = (i < PostSilences.Num()) ? PostSilences[i] : 0.0f;
        
        int32 PreSilenceSamples = FMath::RoundToInt(PreSilence * SampleRate * NumChannels);
        int32 PostSilenceSamples = FMath::RoundToInt(PostSilence * SampleRate * NumChannels);
        
        // 创建新的音频数据（包含静音）
        TArray<uint8> ProcessedAudioData;
        int32 TotalSamples = PCMData.Num() / sizeof(int16) + PreSilenceSamples + PostSilenceSamples;
        ProcessedAudioData.SetNum(TotalSamples * sizeof(int16));
        
        int16* ProcessedPCM = reinterpret_cast<int16*>(ProcessedAudioData.GetData());
        int16* OriginalPCM = reinterpret_cast<int16*>(PCMData.GetData());
        int32 OriginalSamples = PCMData.Num() / sizeof(int16);
        
        // 填充段前静音（0值）
        for (int32 j = 0; j < PreSilenceSamples; ++j)
        {
            ProcessedPCM[j] = 0;
        }
        
        // 复制原始音频数据
        FMemory::Memcpy(ProcessedPCM + PreSilenceSamples, OriginalPCM, OriginalSamples * sizeof(int16));
        
        // 填充段后静音（0值）
        for (int32 j = 0; j < PostSilenceSamples; ++j)
        {
            ProcessedPCM[PreSilenceSamples + OriginalSamples + j] = 0;
        }

        // 如果当前音频的采样率或声道数小于最大值，需要上采样
        if (SampleRate < MaxSampleRate || NumChannels < MaxChannels)
        {
            // 简化的上采样处理：创建新的音频数据并填充
            TArray<uint8> UpsampledData;
            int32 UpsampledSamples = FMath::RoundToInt(TotalSamples * (float)MaxSampleRate / SampleRate * (float)MaxChannels / NumChannels);
            UpsampledData.SetNum(UpsampledSamples * sizeof(int16));
            
            int16* UpsampledPCM = reinterpret_cast<int16*>(UpsampledData.GetData());
            int16* ProcessedPCMForUpsample = reinterpret_cast<int16*>(ProcessedAudioData.GetData());
            
            // 简单的线性插值上采样
            float SampleRatio = (float)MaxSampleRate / SampleRate;
            float ChannelRatio = (float)MaxChannels / NumChannels;
            
            for (int32 j = 0; j < UpsampledSamples; ++j)
            {
                int32 OriginalIndex = FMath::RoundToInt(j / SampleRatio / ChannelRatio);
                if (OriginalIndex < TotalSamples)
                {
                    UpsampledPCM[j] = ProcessedPCMForUpsample[OriginalIndex];
                }
                else
                {
                    UpsampledPCM[j] = 0;
                }
            }
            
            ProcessedAudioSegments.Add(UpsampledData);
            SegmentSampleCounts.Add(UpsampledSamples);
            UE_LOG(LogTemp, Log, TEXT("音频上采样完成，索引: %d, 原始: %d样本, 上采样后: %d样本"), i, TotalSamples, UpsampledSamples);
        }
        else
        {
            ProcessedAudioSegments.Add(ProcessedAudioData);
            SegmentSampleCounts.Add(TotalSamples);
        }
    }

    if (ProcessedAudioSegments.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("没有成功处理任何音频片段"));
        return false;
    }

    // 合并所有处理后的音频片段
    int32 TotalSamples = 0;
    for (int32 SampleCount : SegmentSampleCounts)
    {
        TotalSamples += SampleCount;
    }

    TArray<uint8> MergedAudioData;
    MergedAudioData.SetNum(TotalSamples * sizeof(int16));
    
    int16* MergedPCM = reinterpret_cast<int16*>(MergedAudioData.GetData());
    
    int32 CurrentSampleOffset = 0;
    for (int32 i = 0; i < ProcessedAudioSegments.Num(); ++i)
    {
        const TArray<uint8>& Segment = ProcessedAudioSegments[i];
        int32 SegmentSamples = SegmentSampleCounts[i];
        
        FMemory::Memcpy(MergedPCM + CurrentSampleOffset, Segment.GetData(), SegmentSamples * sizeof(int16));
        CurrentSampleOffset += SegmentSamples;
    }

    // 转换为WAV格式
    TArray<uint8> WavData;
    if (!UGenTools::ExportPCMToWavData(MergedAudioData, MaxSampleRate, MaxChannels, WavData))
    {
        UE_LOG(LogTemp, Error, TEXT("导出WAV数据失败"));
        return false;
    }

    // 返回结果
    OutMergedAudioData = WavData;
    OutSampleRate = static_cast<int32>(MaxSampleRate);
    OutNumChannels = static_cast<int32>(MaxChannels);

    UE_LOG(LogTemp, Log, TEXT("从内存数据合并多个音频完成（不保存资产） - 合并了 %d 个音频片段"), ProcessedAudioSegments.Num());
    for (int32 i = 0; i < Texts.Num(); ++i)
    {
        float PreS = (i < PreSilences.Num()) ? PreSilences[i] : 0.0f;
        float PostS = (i < PostSilences.Num()) ? PostSilences[i] : 0.0f;
        UE_LOG(LogTemp, Log, TEXT("  [%d] 文本: %s, 段前静音: %.2f秒, 段后静音: %.2f秒"), i, *Texts[i], PreS, PostS);
    }
    
    return true;
}



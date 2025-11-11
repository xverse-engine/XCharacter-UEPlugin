import unreal
import argparse
import sys
import numpy
import os
import shutil
from mhc_to_arkit import MHCToARKitConverter
import gc

def save_asset(package_path: str, asset_name: str):
    unreal.EditorAssetLibrary.save_asset(f"{package_path}/{asset_name}")

def create_performance_asset(
    path_to_sound_wave: str, save_performance_location: str, asset_name: str = None
) -> unreal.MetaHumanPerformance:
    """
    Returns a newly created MetaHuman Performance asset based on the input soundwave.

    Args
        path_to_sound_wave: content path to a USoundWave asset that is going to be used by the performance
        save_performance_location: content path to store the new performance
    """
    sound_wave_asset = unreal.load_asset(path_to_sound_wave)
    performance_asset_name = (
        "{0}_Performance".format(sound_wave_asset.get_name())
        if not asset_name
        else asset_name
    )

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    performance_asset = asset_tools.create_asset(
        asset_name=performance_asset_name,
        package_path=save_performance_location,
        asset_class=unreal.MetaHumanPerformance,
        factory=unreal.MetaHumanPerformanceFactoryNew(),
    )

    # Use this style as set_editor_property doesn't trigger the PostEditChangeProperty required to setup the Performance asset
    performance_asset.set_editor_property("input_type", unreal.DataInputType.AUDIO)
    performance_asset.set_editor_property("audio", sound_wave_asset)

    return performance_asset


def process_shot(
    performance_asset: unreal.MetaHumanPerformance,
    start_frame: int = None,
    end_frame: int = None,
):
    """
    Process the input performance and optionally export the processed range of frames as an AnimSequence asset.

    Args
        performance_asset: the performance to process
        start_frame, end_frame: set start/end frame property to change the processing range, the default range is set to entire shot
                                when modifying the range make sure start and end frames are vaild and not overlaping
                                upper limit frame will not be processed, so for limit of 10, frames 1-9 will be processed
    """
    if start_frame is not None:
        performance_asset.set_editor_property("start_frame_to_process", start_frame)

    if end_frame is not None:
        performance_asset.set_editor_property("end_frame_to_process", end_frame)

    # Setting process to blocking will make sure the action is executed on the main thread, blocking it until processing is finished
    process_blocking = True
    performance_asset.set_blocking_processing(process_blocking)

    unreal.log("Starting MH pipeline for '{0}'".format(performance_asset.get_name()))
    startPipelineError = performance_asset.start_pipeline()
    if startPipelineError is unreal.StartPipelineErrorType.NONE:
        unreal.log(
            "Finished MH pipeline for '{0}'".format(performance_asset.get_name())
        )
    else:
        unreal.log(
            "Unknown error starting MH pipeline for '{0}'".format(
                performance_asset.get_name()
            )
        )


def process_audio_performance(soundwave_path, storage_path, audio_pure_name):
    performance_asset = create_performance_asset(
        path_to_sound_wave=soundwave_path, save_performance_location=storage_path
    )

    process_shot(performance_asset=performance_asset)

    # save the performance here
    save_asset(f"/Game/AudioToFace/{audio_pure_name}", f"{audio_pure_name}_Performance")

    return performance_asset


def run_meta_human_level_sequence_export(
    performance_asset: unreal.MetaHumanPerformance,
    target_meta_human_path: str,
    export_sequence_location: str,
    export_sequence_name: str,
):
    """
    Creates a level Sequence asset from the processed range of the input performance.
    The level sequence contains the metahuman and cine camera.
    Returns the newly created asset.

    Args
        performance_asset: performance with a processed range of frames to generate the sequence from
        target_meta_human_path: content path to the MetaHuman BP asset to target during the level sequence export
        export_sequence_location: content path for the new sequence asset
        export_sequence_name: asset name for the new sequence asset
    """
    performance_asset_name = performance_asset.get_name()

    unreal.log(
        "Exporting level sequence for performance '{0}'".format(performance_asset_name)
    )
    level_sequence_export_settings = (
        unreal.MetaHumanPerformanceExportLevelSequenceSettings()
    )
    level_sequence_export_settings.show_export_dialog = False

    # if the path and name are not set, the export will use the performance as a base name
    level_sequence_export_settings.package_path = export_sequence_location
    level_sequence_export_settings.asset_name = export_sequence_name

    # load the target metahuman blueprint asset
    if not target_meta_human_path:
        unreal.log_error(
            "Unable to export level sequence with MetaHuman in. No target_MetaHuman_path set."
        )
        return None

    target_meta_human_bp_asset: unreal.Blueprint = unreal.load_asset(
        target_meta_human_path
    )
    target_meta_human_bp_generated_class = target_meta_human_bp_asset.generated_class()

    print(target_meta_human_bp_generated_class.get_name())

    # customize various export settings
    level_sequence_export_settings.export_video_track = True
    level_sequence_export_settings.export_depth_track = False
    level_sequence_export_settings.export_audio_track = True
    level_sequence_export_settings.export_image_plane = False
    level_sequence_export_settings.export_identity = False
    level_sequence_export_settings.export_camera = False
    level_sequence_export_settings.apply_lens_distortion = True
    level_sequence_export_settings.export_depth_mesh = False
    level_sequence_export_settings.export_control_rig_track = True
    level_sequence_export_settings.export_transform_track = False
    level_sequence_export_settings.keep_frame_range = True
    level_sequence_export_settings.target_meta_human_class = target_meta_human_bp_asset
    level_sequence_export_settings.enable_meta_human_head_movement = True
    level_sequence_export_settings.export_range = (
        unreal.PerformanceExportRange.WHOLE_SEQUENCE
    )
    level_sequence_export_settings.curve_interpolation = (
        unreal.RichCurveInterpMode.RCIM_LINEAR
    )

    # export the level sequence
    exported_level_sequence: unreal.LevelSequence = (
        unreal.MetaHumanPerformanceExportUtils.export_level_sequence(
            performance=performance_asset,
            export_settings=level_sequence_export_settings,
        )
    )
    if exported_level_sequence:
        unreal.log(
            "Exported Level Sequence {0}".format(exported_level_sequence.get_name())
        )
        return exported_level_sequence
    else:
        unreal.log("Failed to export Level Sequence")
        return None

if __name__ == "__main__":
    # 删除已有的输出文件夹
    Audio_dataset_name= "Audio_emotion"
    # unreal.EditorAssetLibrary.delete_directory("/Game/AudioToFace")
    audios = unreal.EditorAssetLibrary.list_assets(f"/Game/{Audio_dataset_name}")
    converter = MHCToARKitConverter()

    # 新建保存JSON的文件夹
    ue_save_dir = "D:/MH55_0613/Saved"
    mhc_json_dir = "D:/MH55_0613/Saved/Audio2Face/MhcJSON"
    arkit_json_dir = "D:/MH55_0613/Saved/Audio2Face/ARKitJSON"
    os.makedirs(mhc_json_dir, exist_ok=True)
    os.makedirs(arkit_json_dir, exist_ok=True)

    for audio in audios:
        audio_pure_name = audio.split(".")[1]
        if os.path.exists(f"{arkit_json_dir}/{audio_pure_name}.json"):
            print(f"ARKit JSON File {audio_pure_name} already exists")
            continue
        print(f"Processing Audio File: {audio_pure_name}")

        ##########################################################
        performance = process_audio_performance(
            f"/Game/{Audio_dataset_name}/{audio_pure_name}", f"/Game/AudioToFace/{audio_pure_name}", audio_pure_name
        )

        level_sequence = run_meta_human_level_sequence_export(
            performance,
            "/Game/MetaHumans/Ada/BP_Ada",
            f"/Game/AudioToFace/{audio_pure_name}",
            f"{audio_pure_name}",
        )
        if level_sequence is None:
            print(f"Level Sequence {audio_pure_name} export failed")
            break

        # save the level sequence after export
        save_asset(f"/Game/AudioToFace/{audio_pure_name}", audio_pure_name)

        unreal.ExportLevelSeqToJson.read_sequence_file(
            f"/Game/AudioToFace/{audio_pure_name}/{audio_pure_name}"
        )

        # 判断json文件是否存在
        if os.path.exists(f"{ue_save_dir}/{audio_pure_name}.json"):
            print(f"Level Sequence {audio_pure_name} exported successfully")
            # 将json文件移动到MhcJSON文件夹
            shutil.move(f"{ue_save_dir}/{audio_pure_name}.json", f"{mhc_json_dir}/{audio_pure_name}.json")
            # MHC json 转换为 ARKit json
            converter.convert(f"{mhc_json_dir}/{audio_pure_name}.json", f"{arkit_json_dir}/{audio_pure_name}.json")
        else:
            print(f"Level Sequence {audio_pure_name} export failed")
            break

        unreal.SystemLibrary.collect_garbage()

        # unreal.EditorAssetLibrary.delete_asset(f"/Game/AudioToFace/{audio_pure_name}/{audio_pure_name}")
        unreal.EditorAssetLibrary.delete_asset(f"/Game/AudioToFace/{audio_pure_name}/{audio_pure_name}_Performance")
        
        level_sequence = None
        performance = None
        # break;
        gc.collect()
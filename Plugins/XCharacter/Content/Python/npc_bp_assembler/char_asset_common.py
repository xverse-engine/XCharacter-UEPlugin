import unreal

def filter_out_skeletal_mesh() -> [unreal.SkeletalMesh]:
    skeletal_meshes = []

    for asset in unreal.EditorUtilityLibrary.get_selected_assets():
        if isinstance(asset, unreal.SkeletalMesh):
            skeletal_meshes.append(asset)
        else:
            unreal.EditorDialog.show_message(
                title="错误", 
                message=f"选中的资产中只能包含骨骼网格体",
                message_type=unreal.AppMsgType.OK
            )
            exit()

    return skeletal_meshes
import unreal
from .char_asset_common import *

HEAD_BONE = "Bip001-Head"
ROOT_BONE = "Root"
BONES_TO_CHECK = [HEAD_BONE]
LOOK_AT_AXIS_BS = unreal.Vector(0, -1, 0) # the vector in bone space 
ORIENTATION_COS_THRESHOLD = 0.8

skeletal_meshes = []
ref_pose : unreal.AnimPose = None
check_res = True
msg = ""
bone_names : [unreal.Name] = []

# check root bone
def check_root_bone(
        asset : unreal.SkeletalMesh,
        ref_pose : unreal.AnimPose):
    global ROOT_BONE
    global check_res, msg, bone_names
    
    if bone_names[0] != ROOT_BONE:
        msg += f"[SKM Asset Checker] 骨骼网格 '{asset.get_name()}' 的根骨骼命名必须为 '{ROOT_BONE}'\n"
        check_res = False

    return 

# check head bone and root bone
def check_bones_existence(
        asset : unreal.SkeletalMesh, 
        ref_pose : unreal.AnimPose):
    global BONES_TO_CHECK
    global check_res, msg, bone_names

    for bone in BONES_TO_CHECK:
        if bone not in bone_names:
            msg += f"[SKM Asset Checker] 骨骼 '{bone}' 在骨骼网格 '{asset.get_name()}' 中不存在。\n"
            check_res = False
    
    return

# Head bone's look at axis need to 
# align with the forward vector in object space
def check_head_bone_orientation(
        asset : unreal.SkeletalMesh, 
        ref_pose : unreal.AnimPose):
    global HEAD_BONE, LOOK_AT_AXIS_BS
    global check_res, msg, bone_names

    head_bone_transform : unreal.Transform = ref_pose.get_bone_pose(HEAD_BONE, space=unreal.AnimPoseSpaces.WORLD)
    look_at_axis_ws : unreal.Vector = (head_bone_transform.transform_direction(LOOK_AT_AXIS_BS))
    look_at_axis_ws.normalize()
    fwd_vector_ws : unreal.Vector = unreal.Vector(1, 0, 0)
    cos = fwd_vector_ws.dot(look_at_axis_ws)
    unreal.log(f'[SKM Asset Checker] look_at_axis_ws: {look_at_axis_ws}')
    unreal.log(f'[SKM Asset Checker] dot product: {cos}')

    if cos < ORIENTATION_COS_THRESHOLD:
        msg += f"[SKM Asset Checker] 骨骼网格 '{asset.get_name()}' 的骨骼 '{HEAD_BONE}' 的注视轴向({LOOK_AT_AXIS_BS.x}, {LOOK_AT_AXIS_BS.y}, {LOOK_AT_AXIS_BS.z}) 与角色模型空间的前向量相差过大，不满足规范。\n"
        check_res = False

    return

def check_skm_asset(asset : unreal.SkeletalMesh):
    global bone_names

    skeleton : unreal.Skeleton = asset.get_editor_property('skeleton')
    ref_pose : unreal.AnimPose = skeleton.get_reference_pose()
    bone_names = ref_pose.get_bone_names()

    check_bones_existence(asset, ref_pose)
    check_root_bone(asset, ref_pose)
    check_head_bone_orientation(asset, ref_pose)

def check_skm_asset_cpp(asset_path : str, jump_out_pass_window : bool = True) -> bool:
    global msg 
    global bone_names
    global check_res
    
    ### CHECK SKM ASSET ###
    with unreal.ScopedEditorTransaction("SKM Check") as trans:
        try:
            # loading asset
            asset = unreal.EditorAssetLibrary.load_asset(asset_path)
            if not asset:
                return False

            check_skm_asset(asset)
            msg += "\n"

            if check_res:
                if jump_out_pass_window:
                    unreal.EditorDialog.show_message(
                        title="Success",
                        message=f"[SKM Asset Checker] 骨骼网格 '{asset.get_name()}' 检查通过。",
                        message_type=unreal.AppMsgType.OK
                    )
                return True
            else: 
                raise Exception(f"骨骼检查失败：\n{msg}")
                trans.cancel()
                return False

        except Exception as e:
            unreal.EditorDialog.show_message(
                title="Error",
                message=str(e),
                message_type=unreal.AppMsgType.OK
            )
            trans.cancel()
            return False

def main(jump_out_pass_window : bool = False) -> bool:
    global skeletal_meshes
    global msg 
    global bone_names
    global check_res

    ### CHECK IF SKELETAL MESH ### 
    skeletal_meshes = filter_out_skeletal_mesh()
    
    ### CHECK SKM ASSET ###
    with unreal.ScopedEditorTransaction("SKM Check") as trans:
        try:
            for asset in skeletal_meshes:
                check_skm_asset(asset)
                msg += "\n"

            if check_res:
                if jump_out_pass_window:
                    unreal.EditorDialog.show_message(
                        title="Success",
                        message=f"[SKM Asset Checker] 骨骼网格 '{asset.get_name()}' 检查通过。",
                        message_type=unreal.AppMsgType.OK
                    )
                return True
            else: 
                raise Exception(f"骨骼检查失败：\n{msg}")
                trans.cancel()
                return False

        except Exception as e:
            unreal.EditorDialog.show_message(
                title="Error",
                message=str(e),
                message_type=unreal.AppMsgType.OK
            )
            trans.cancel()
            return False

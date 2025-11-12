import unreal
from .char_asset_common import *

WIDGET_BP_PATH = '/XCharacter/BP/NpcBpAssembler/EUW_NpcBpAssembler.EUW_NpcBpAssembler'

skeletal_meshes = []
tab_id = None
eus = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
widget_inst = None
msg = ""

### WIDGET ### 

def check_if_bp_exists(
        folder_path : str, 
        asset_name : str
        ) -> bool:
    asset_path = f'{folder_path}/{asset_name}'
    return unreal.EditorAssetLibrary.does_asset_exist(asset_path)

# increase the index at the end if asset exists
def increment_asset_name(
        folder_path : str, 
        asset_name : str
        ) -> str:

    base_name = asset_name
    index = 1

    while check_if_bp_exists(folder_path, base_name):
        base_name = f"{asset_name}_V{index}"
        index += 1

    return base_name 

def assemble_npc_bp(
        asset : unreal.SkeletalMesh,
        bp_base : unreal.Blueprint,
        abp_base : unreal.AnimBlueprint):
    global msg

    ### DETERMINE ASSET PATHS ###
    asset_path = unreal.EditorAssetLibrary.get_path_name(asset)
    asset_folder_path = asset_path.rsplit('/', 1)[0]

    char_name = asset.get_name()
    if char_name.startswith("SK_"):
        char_name = char_name[3:]

    # rename if asset exists
    bp_name = increment_asset_name(asset_folder_path, f"BP_NPC_{char_name}")
    abp_name = increment_asset_name(asset_folder_path, f"ABP_NPC_{char_name}")

    ### CREATE BLUEPRINT ASSETS ###
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    bp_factory = unreal.BlueprintFactory()
    bp_factory.set_editor_property('parent_class', bp_base)
    new_bp = asset_tools.create_asset(
        asset_name=bp_name,
        package_path=asset_folder_path,
        asset_class=unreal.Blueprint,
        factory=bp_factory
    )
    unreal.BlueprintEditorLibrary.compile_blueprint(new_bp)
    msg += f"蓝图 '{bp_name}' 已创建于 '{asset_folder_path}' \n"

    abp_factory = unreal.AnimBlueprintFactory()
    abp_factory.set_editor_property('parent_class', abp_base)
    abp_factory.set_editor_property('target_skeleton', asset.get_editor_property('skeleton'))
    abp_factory.set_editor_property('preview_skeletal_mesh', asset)
    new_abp = asset_tools.create_asset(
        asset_name=abp_name,
        package_path=asset_folder_path,
        asset_class=unreal.AnimBlueprint,
        factory=abp_factory
    )
    unreal.BlueprintEditorLibrary.compile_blueprint(new_abp)
    msg += f"动画蓝图 '{abp_name}' 已创建于 '{asset_folder_path}' \n"

    ### CONFIGURE CHARACTER BP ###
    subsystem : unreal.SubobjectDataSubsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    root_data_handle: unreal.SubobjectDataHandle = subsystem.k2_gather_subobject_data_for_blueprint(context=new_bp)
    
    objects = []
    
    for handle in root_data_handle:
        subobject = subsystem.k2_find_subobject_data_from_handle(handle)
        objects.append( unreal.SubobjectDataBlueprintFunctionLibrary.get_object(subobject) )

    # get components
    for obj in objects:
        print(f"########### Object: {obj.get_name()} ###########")
        if obj.get_name() == 'SkeletalMesh':
            print('Found SkeletalMesh Component')
            obj.set_editor_property('skeletal_mesh_asset', asset)
            obj.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_BLUEPRINT)
            obj.set_editor_property('anim_class', new_abp.generated_class())

    unreal.BlueprintEditorLibrary.compile_blueprint(new_bp)

    msg += "\n"

def assemble_npc_bp_cpp(    
        asset_path : str, 
        bp_base_path : str, 
        abp_base_path : str):
    global msg

    asset_library : unreal.EditorAssetLibrary = unreal.EditorAssetLibrary
    try:
        asset = asset_library.load_asset(asset_path)
        bp_base : unreal.Class = unreal.load_class(None, bp_base_path)
        abp_base : unreal.Class = unreal.load_class(None, abp_base_path)
        assemble_npc_bp(asset, bp_base, abp_base)
    except Exception as e:
        unreal.EditorDialog.show_message(
            title="Error",
            message=str(e),
            message_type=unreal.AppMsgType.OK
        )

    unreal.EditorDialog.show_message(
        title="角色蓝图创建成功",
        message=msg,
        message_type=unreal.AppMsgType.OK
    )

def button_on_confirmed():
    global eus, widget_inst, tab_id
    global skeletal_meshes

    char_bp = widget_inst.get_editor_property('CharacterBaseClass')
    print(f"Character Base Class: {char_bp}")
    anim_bp = widget_inst.get_editor_property('AnimBaseClass')
    print(f"Anim Base Class: {anim_bp}")

    for asset in skeletal_meshes:
        assemble_npc_bp(asset, char_bp, anim_bp)

    eus.close_tab_by_id(tab_id)
    unreal.EditorDialog.show_message(
        title="角色蓝图创建成功",
        message=msg,
        message_type=unreal.AppMsgType.OK
    )

def invoke_widget(widget_bp: unreal.EditorUtilityWidget):
    global eus, widget_inst, tab_id

    widget_inst, tab_id = eus.spawn_and_register_tab_and_get_id(widget_bp)
    
    # bind events 
    button_confirm : unreal.Button = widget_inst.get_editor_property('ButtonConfirm')
    button_cancel : unreal.Button = widget_inst.get_editor_property('ButtonCancel')

    button_confirm.on_clicked.add_callable(lambda: button_on_confirmed())
    button_cancel.on_clicked.add_callable(lambda: eus.close_tab_by_id(tab_id))

### ASSEMBLE NPC BLUEPRINTS ### 

def main():
    global skeletal_meshes
    # filter out non-skeletal mesh assets
    skeletal_meshes = filter_out_skeletal_mesh()
        
    with unreal.ScopedEditorTransaction("NPC BP Assembler") as trans:
        try: 
            # load widget 
            widget_bp : unreal.EditorUtilityWidget = unreal.EditorAssetLibrary.load_asset(WIDGET_BP_PATH)
            invoke_widget(widget_bp)
        
        except Exception as e:
            unreal.EditorDialog.show_message(
                title="Error", 
                message=str(e),
                message_type=unreal.AppMsgType.OK
            )
            trans.cancel()
            return

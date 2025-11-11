import os
import json
import numpy


def load_json(file_path):
    assert file_path.split('.')[-1] == 'json'
    if not os.path.exists(file_path):
        print("File not found: " + file_path)
        return None
    f = open(file_path, "r")
    data = json.loads(f.read())
    f.close()
    return data


def save_json(save_path, data):
    assert save_path.split('.')[-1] == 'json'
    json_str = json.dumps(data)
    with open(save_path, 'w', encoding='utf-8') as file:
        file.write(json_str)


def matrix_rank(matrix, name=""):
    # 使用 numpy.linalg.matrix_rank() 函数计算矩阵的秩
    mat_rank = numpy.linalg.matrix_rank(matrix)

    # 检查矩阵是否满秩
    is_full_rank = mat_rank == min(
        matrix.shape
    )  # 如果秩等于行数或列数中较小的一个，则为满秩

    print(f"\tmatrix_rank  --  Rank of the matrix {name} is: {mat_rank}")
    print(f"\tmatrix_rank  --  Is the matrix full rank?: {is_full_rank}")


mhc_non_relative_blendshape_list = [
    "CTRL_convergenceSwitch",
    "CTRL_C_jaw_openExtreme",
    "CTRL_R_jaw_clench",
    "CTRL_L_jaw_clench",
    "CTRL_L_neck_mastoidContract",
    "CTRL_R_neck_mastoidContract",
    "CTRL_L_neck_stretch",
    "CTRL_R_neck_stretch",
    "CTRL_neck_digastricUpDown",
    "CTRL_neck_throatExhaleInhale",
    "CTRL_neck_throatUpDown",
    "CTRL_L_jaw_chinCompress",
    "CTRL_R_jaw_chinCompress",
    "CTRL_L_mouth_lipsPressU",
    "CTRL_R_mouth_lipsPressU",
    "CTRL_L_mouth_lipsTogetherU",
    "CTRL_R_mouth_lipsTogetherU",
    "CTRL_L_mouth_lipsTogetherD",
    "CTRL_R_mouth_lipsTogetherD",
    "CTRL_L_mouth_lipSticky",
    "CTRL_R_mouth_lipSticky",
    "CTRL_L_mouth_stretchLipsClose",
    "CTRL_R_mouth_stretchLipsClose",
    "CTRL_C_tongue_press",
    "CTRL_C_tongue_bendTwist",
    "CTRL_C_tongue_tipMove",
    "CTRL_C_tongue_roll",
    "CTRL_L_mouth_sharpCornerPull",
    "CTRL_R_mouth_stickyInnerU",
    "CTRL_C_neck_swallow",
    "CTRL_R_mouth_stickyOuterD",
    "CTRL_C_mouth_stickyD",
    "CTRL_R_mouth_stickyInnerD",
    "CTRL_R_mouth_lipBiteD",
    "CTRL_R_mouth_stickyOuterU",
    "CTRL_C_mouth_stickyU",
    "CTRL_L_mouth_tightenD",
    "CTRL_R_mouth_tightenD",
    "CTRL_L_mouth_stickyOuterU",
    "CTRL_L_mouth_tightenU",
    "CTRL_R_mouth_tightenU",
]


metahuman_blendshape_list = [
    "Neutral",
    "eyeBlinkLeft",
    "eyeLookDownLeft",
    "eyeLookInLeft",
    "eyeLookOutLeft",
    "eyeLookUpLeft",
    "eyeSquintLeft",
    "eyeWideLeft",
    "eyeBlinkRight",
    "eyeLookDownRight",
    "eyeLookInRight",
    "eyeLookOutRight",
    "eyeLookUpRight",
    "eyeSquintRight",
    "eyeWideRight",
    "jawForward",
    "jawLeft",
    "jawRight",
    "jawOpen",
    "mouthFunnel",
    "mouthPucker",
    "mouthLeft",
    "mouthRight",
    "mouthSmileLeft",
    "mouthSmileRight",
    "mouthFrownLeft",
    "mouthFrownRight",
    "mouthDimpleLeft",
    "mouthDimpleRight",
    "mouthStretchLeft",
    "mouthStretchRight",
    "mouthRollLower",
    "mouthRollUpper",
    "mouthShrugLower",
    "mouthShrugUpper",
    "mouthPressLeft",
    "mouthPressRight",
    "mouthLowerDownLeft",
    "mouthLowerDownRight",
    "mouthUpperUpLeft",
    "mouthUpperUpRight",
    "browDownLeft",
    "browDownRight",
    "browInnerUp",
    "browOuterUpLeft",
    "browOuterUpRight",
    "cheekPuff",
    "cheekSquintLeft",
    "cheekSquintRight",
    "noseSneerLeft",
    "noseSneerRight",
    "tongueOut",
]

# MHC to ARKit Converter

这个项目提供了一个规范化的Python脚本来将MetaHuman Creator (MHC) 控制器数据转换为ARKit blendshape数据。

## 功能特性

- 🔄 **完整的转换流程**: 从MHC控制器数据到ARKit blendshapes的端到端转换
- 🏗️ **面向对象设计**: 使用类封装，便于扩展和维护
- 📝 **类型提示**: 完整的类型注解，提高代码可读性
- 🛡️ **错误处理**: 完善的异常处理机制
- 📊 **日志记录**: 详细的处理过程日志
- ⚙️ **可配置参数**: 支持自定义帧率和映射文件路径

## 重构改进

### 原始脚本问题
- 代码结构混乱，所有逻辑都在全局作用域
- 缺乏错误处理
- 没有类型提示
- 难以复用和测试
- 硬编码的参数

### 重构后改进
- ✅ 封装成 `MHCToARKitConverter` 类
- ✅ 分离关注点，每个方法职责单一
- ✅ 完整的类型提示和文档字符串
- ✅ 完善的错误处理和日志记录
- ✅ 可配置的参数和常量
- ✅ 支持命令行和编程接口

## 安装依赖

```bash
pip install numpy
```

## 使用方法

### 1. 命令行使用

```bash
python mhc_to_arkit.py input_mhc_data.json output_arkit_data.json
```

### 2. 编程接口使用

#### 基本用法

```python
from mhc_to_arkit import MHCToARKitConverter

# 创建转换器实例
converter = MHCToARKitConverter()

# 执行转换
converter.convert(
    input_json_path="input_mhc_data.json",
    output_json_path="output_arkit_data.json"
)
```

#### 高级用法

```python
from mhc_to_arkit import MHCToARKitConverter

# 使用自定义映射文件
converter = MHCToARKitConverter(mapping_json_path="custom_mapping.json")

# 分步执行以获得更多控制
converter.load_mapping_data()
converter.build_mapping_matrix()

# 加载输入数据
input_frames = converter.load_input_data("input_mhc_data.json")

# 转换帧数据
result_data = converter.convert_frames(input_frames)

# 格式化输出（自定义帧率）
result_json = converter.format_output_json(result_data, frame_rate=60.0)

# 保存结果
from utils import save_json
save_json("output_arkit_data.json", result_json)
```

#### 批量处理

```python
from mhc_to_arkit import MHCToARKitConverter

input_files = ["input1.json", "input2.json", "input3.json"]
converter = MHCToARKitConverter()

for i, input_file in enumerate(input_files):
    output_file = f"output_{i+1}.json"
    try:
        converter.convert(input_file, output_file)
        print(f"Successfully converted {input_file}")
    except Exception as e:
        print(f"Failed to convert {input_file}: {e}")
```

## API 文档

### MHCToARKitConverter 类

#### 构造函数

```python
MHCToARKitConverter(mapping_json_path: str = "python_scripts/arkit_to_mhc.json")
```

#### 主要方法

- `load_mapping_data()`: 加载ARKit到MHC的映射数据
- `build_mapping_matrix()`: 构建转换矩阵
- `load_input_data(input_json_path: str)`: 加载输入MHC数据
- `convert_frames(input_frames: List[List[float]])`: 转换帧数据
- `format_output_json(result_data: List[List[float]], frame_rate: float = 30.0)`: 格式化输出JSON
- `convert(input_json_path: str, output_json_path: str, frame_rate: float = 30.0)`: 完整转换流程

## 配置参数

| 参数 | 默认值 | 描述 |
|------|--------|------|
| `DEFAULT_FRAME_RATE` | 30.0 | 默认帧率 |
| `DEFAULT_MAPPING_PATH` | "python_scripts/arkit_to_mhc.json" | 默认映射文件路径 |

## 错误处理

脚本包含完善的错误处理机制：

- **文件不存在**: 自动检测并报告文件路径错误
- **数据格式错误**: 验证输入数据格式和长度
- **矩阵计算错误**: 处理矩阵运算中的数值问题
- **内存错误**: 处理大文件的内存限制

## 日志记录

脚本使用Python标准logging模块记录处理过程：

```python
import logging

# 配置日志级别
logging.basicConfig(level=logging.INFO)
```

日志包含：
- 数据加载状态
- 矩阵构建过程
- 转换进度
- 错误信息

## 示例文件

查看 `example_usage.py` 文件获取更多使用示例。

## 依赖文件

- `utils.py`: 包含 `load_json`, `save_json`, `matrix_rank`, `metahuman_blendshape_list` 等工具函数
- `python_scripts/arkit_to_mhc.json`: ARKit到MHC的映射数据文件

## 注意事项

1. 确保输入JSON文件格式正确
2. 映射文件路径必须存在且可读
3. 输出目录必须可写
4. 大文件处理可能需要较长时间

## 许可证

请参考项目根目录的许可证文件。 
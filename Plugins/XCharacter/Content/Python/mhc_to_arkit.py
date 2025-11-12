"""
MHC to ARKit blendshape conversion script.

This module provides functionality to convert MetaHuman Creator (MHC) controller data
to ARKit blendshape data using matrix transformations.
"""

import sys
import logging
from typing import Dict, List, Tuple, Optional
import numpy as np
import numpy.typing as npt

from utils import load_json, save_json, matrix_rank, metahuman_blendshape_list

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Constants
DEFAULT_FRAME_RATE = 30.0
DEFAULT_MAPPING_PATH = "D:/MH55_0613/Plugins/XCharacter_UEPlugin/Content/Python/arkit_to_mhc.json"


class MHCToARKitConverter:
    """Converter class for transforming MHC controller data to ARKit blendshapes."""
    
    def __init__(self, mapping_json_path: str = DEFAULT_MAPPING_PATH):
        """
        Initialize the converter with mapping data.
        
        Args:
            mapping_json_path: Path to the ARKit to MHC mapping JSON file
        """
        self.mapping_json_path = mapping_json_path
        self.arkit_to_mhc_mapping_data: Optional[Dict] = None
        self.arkit_to_mhc_matrix: Optional[npt.NDArray] = None
        self.arkit_to_mhc_matrix_pinv: Optional[npt.NDArray] = None
        self.mhc_controllers_names: Optional[List[str]] = None
        self.mhc_controllers_names_with_xy: Optional[List[str]] = None
        
    def load_mapping_data(self) -> None:
        """Load and process the ARKit to MHC mapping data."""
        try:
            self.arkit_to_mhc_mapping_data = load_json(self.mapping_json_path)
            self.mhc_controllers_names = list(self.arkit_to_mhc_mapping_data.keys())
            
            logger.info(
                f"Loaded ARKit to MHC mapping data. "
                f"ARKit Blendshape count: {len(self.arkit_to_mhc_mapping_data['CTRL_convergenceSwitch'])}, "
                f"MHC controller count: {len(self.mhc_controllers_names)}"
            )
        except Exception as e:
            logger.error(f"Failed to load mapping data: {e}")
            raise
    
    def build_mapping_matrix(self) -> None:
        """Build the transformation matrix from ARKit blendshapes to MHC controllers."""
        if not self.arkit_to_mhc_mapping_data:
            raise ValueError("Mapping data not loaded. Call load_mapping_data() first.")
        
        # Create ARKit to MHC mapping dictionary
        arkit_to_mhc_mapping: Dict[str, Dict[str, float]] = {}
        
        for index, blendshape_name in enumerate(metahuman_blendshape_list):
            blendshape_values: Dict[str, float] = {}
            
            for controller_name, controller_values in self.arkit_to_mhc_mapping_data.items():
                controller_name_x = f"{controller_name}_X"
                controller_name_y = f"{controller_name}_Y"
                
                blendshape_values[controller_name_x] = controller_values[index]["xvalue"]
                blendshape_values[controller_name_y] = controller_values[index]["yvalue"]
            
            arkit_to_mhc_mapping[blendshape_name] = blendshape_values
        
        # Get MHC controllers names with XY axes
        self.mhc_controllers_names_with_xy = list(arkit_to_mhc_mapping["Neutral"].keys())
        
        # Build numpy matrix
        matrix_shape = (len(metahuman_blendshape_list), len(self.mhc_controllers_names_with_xy))
        self.arkit_to_mhc_matrix = np.zeros(matrix_shape)
        
        for index, blendshape_name in enumerate(metahuman_blendshape_list):
            blendshape_values = arkit_to_mhc_mapping[blendshape_name]
            
            for controller_name, controller_value in blendshape_values.items():
                controller_index = list(blendshape_values.keys()).index(controller_name)
                self.arkit_to_mhc_matrix[index][controller_index] = controller_value
        
        # Transpose matrix and calculate pseudo-inverse
        self.arkit_to_mhc_matrix = self.arkit_to_mhc_matrix.T
        self.arkit_to_mhc_matrix_pinv = np.linalg.pinv(self.arkit_to_mhc_matrix)
        
        logger.info(f"Generated ARKit to MHC matrix of shape: {self.arkit_to_mhc_matrix.shape}")
        logger.info(f"Generated ARKit to MHC pinv matrix of shape: {self.arkit_to_mhc_matrix_pinv.shape}")
        
        # Log matrix ranks for debugging
        matrix_rank(self.arkit_to_mhc_matrix, "arkit_to_mhc_matrix")
        matrix_rank(self.arkit_to_mhc_matrix_pinv, "arkit_to_mhc_matrix_pinv")
    
    def load_input_data(self, input_json_path: str) -> List[List[float]]:
        """
        Load and process input MHC data.
        
        Args:
            input_json_path: Path to the input MHC JSON file
            
        Returns:
            List of frame data, where each frame contains controller values
            
        Raises:
            ValueError: If frame data length doesn't match expected controller count
        """
        try:
            input_mhc_data = load_json(input_json_path)
            input_mhc_data_length = len(input_mhc_data["CTRL_convergenceSwitch"])
            
            # Convert input data to frame list
            input_mhc_data_frame_list: List[List[float]] = []
            
            for index in range(input_mhc_data_length):
                frame_values = []
                
                for controller_name, controller_values in input_mhc_data.items():
                    if controller_name not in self.mhc_controllers_names:
                        continue
                    
                    if len(controller_values) < input_mhc_data_length:
                        frame_values.extend([0.0, 0.0])
                        continue
                    
                    frame_values.extend([
                        controller_values[index]["xvalue"],
                        controller_values[index]["yvalue"]
                    ])
                
                input_mhc_data_frame_list.append(frame_values)
            
            logger.info(f"Loaded input data, total frame length: {len(input_mhc_data_frame_list)}")
            logger.info(f"Each frame has number of controller data: {len(input_mhc_data_frame_list[0]) // 2}")
            
            # Validate frame data length
            for i, frame_data in enumerate(input_mhc_data_frame_list):
                if len(frame_data) != len(self.mhc_controllers_names_with_xy):
                    raise ValueError(
                        f"Frame {i} data length ({len(frame_data)}) is not equal to "
                        f"MHC controllers length ({len(self.mhc_controllers_names_with_xy)})"
                    )
            
            return input_mhc_data_frame_list
            
        except Exception as e:
            logger.error(f"Failed to load input data: {e}")
            raise
    
    def convert_frames(self, input_frames: List[List[float]]) -> List[List[float]]:
        """
        Convert MHC frame data to ARKit blendshape data.
        
        Args:
            input_frames: List of frame data from MHC
            
        Returns:
            List of converted ARKit blendshape frame data
        """
        if self.arkit_to_mhc_matrix_pinv is None:
            raise ValueError("Transformation matrix not built. Call build_mapping_matrix() first.")
        
        result_data: List[List[float]] = []
        
        for frame in input_frames:
            result = np.dot(self.arkit_to_mhc_matrix_pinv, frame).tolist()
            result_data.append(result)
        
        logger.info(f"Generated result, total frame length: {len(result_data)}")
        logger.info(f"Each frame has number of blendshape data: {len(result_data[0])}")
        
        return result_data
    
    def format_output_json(self, result_data: List[List[float]], frame_rate: float = DEFAULT_FRAME_RATE) -> Dict[str, List[Dict[str, float]]]:
        """
        Format the result data into the output JSON structure.
        
        Args:
            result_data: Converted ARKit blendshape data
            frame_rate: Frame rate for time calculations
            
        Returns:
            Formatted JSON dictionary
        """
        result_json_dict: Dict[str, List[Dict[str, float]]] = {}
        
        for i, blendshape_name in enumerate(metahuman_blendshape_list):
            if blendshape_name == "Neutral":
                continue
            
            result_json_dict[blendshape_name] = []
            
            for index, frame_data in enumerate(result_data):
                result_json_dict[blendshape_name].append({
                    'time': float(index) / frame_rate,
                    "value": frame_data[i],
                })
        
        logger.info(
            f"Formatted result JSON, blendshape number: {len(result_json_dict)}, "
            f"frame number: {len(result_json_dict['jawOpen'])}"
        )
        logger.info("Removed the Neutral blendshape from the result JSON")
        
        return result_json_dict
    
    def convert(self, input_json_path: str, output_json_path: str, frame_rate: float = DEFAULT_FRAME_RATE) -> None:
        """
        Complete conversion process from MHC to ARKit.
        
        Args:
            input_json_path: Path to input MHC JSON file
            output_json_path: Path to output ARKit JSON file
            frame_rate: Frame rate for time calculations
        """
        logger.info(f"Starting MHC to ARKit conversion")
        logger.info(f"Input: {input_json_path}")
        logger.info(f"Output: {output_json_path}")
        
        try:
            # Load mapping data
            self.load_mapping_data()
            
            # Build transformation matrix
            self.build_mapping_matrix()
            
            # Load input data
            input_frames = self.load_input_data(input_json_path)
            
            # Convert frames
            result_data = self.convert_frames(input_frames)
            
            # Format output
            result_json = self.format_output_json(result_data, frame_rate)
            
            # Save output
            save_json(output_json_path, result_json)
            logger.info("Conversion completed successfully")
            
        except Exception as e:
            logger.error(f"Conversion failed: {e}")
            raise


def main():
    """Main function for command-line usage."""
    if len(sys.argv) != 3:
        print("Usage: python mhc_to_arkit.py <input_json_path> <output_json_path>")
        sys.exit(1)
    
    input_json_path = sys.argv[1]
    output_json_path = sys.argv[2]
    
    converter = MHCToARKitConverter()
    converter.convert(input_json_path, output_json_path)


if __name__ == "__main__":
    main()

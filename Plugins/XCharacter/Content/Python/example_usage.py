"""
Example usage of the MHCToARKitConverter class.

This file demonstrates how to use the refactored MHC to ARKit conversion functionality.
"""

from mhc_to_arkit import MHCToARKitConverter
import logging

# Configure logging to see detailed output
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

def example_basic_usage():
    """Basic usage example."""
    print("=== Basic Usage Example ===")
    
    # Create converter instance
    converter = MHCToARKitConverter()
    
    # Perform conversion
    try:
        converter.convert(
            input_json_path="path/to/input_mhc_data.json",
            output_json_path="path/to/output_arkit_data.json"
        )
        print("Conversion completed successfully!")
    except Exception as e:
        print(f"Conversion failed: {e}")

def example_advanced_usage():
    """Advanced usage example with custom parameters."""
    print("\n=== Advanced Usage Example ===")
    
    # Create converter with custom mapping path
    converter = MHCToARKitConverter(mapping_json_path="custom/path/to/mapping.json")
    
    try:
        # Step-by-step conversion for more control
        converter.load_mapping_data()
        converter.build_mapping_matrix()
        
        # Load and process input data
        input_frames = converter.load_input_data("path/to/input_mhc_data.json")
        
        # Convert frames
        result_data = converter.convert_frames(input_frames)
        
        # Format output with custom frame rate
        result_json = converter.format_output_json(result_data, frame_rate=60.0)
        
        # Save output
        from utils import save_json
        save_json("path/to/output_arkit_data.json", result_json)
        
        print("Advanced conversion completed successfully!")
        
    except Exception as e:
        print(f"Advanced conversion failed: {e}")

def example_batch_processing():
    """Example of batch processing multiple files."""
    print("\n=== Batch Processing Example ===")
    
    input_files = [
        "path/to/input1.json",
        "path/to/input2.json",
        "path/to/input3.json"
    ]
    
    converter = MHCToARKitConverter()
    
    for i, input_file in enumerate(input_files):
        output_file = f"path/to/output_{i+1}.json"
        
        try:
            print(f"Processing file {i+1}/{len(input_files)}: {input_file}")
            converter.convert(input_file, output_file)
            print(f"Successfully converted to: {output_file}")
        except Exception as e:
            print(f"Failed to convert {input_file}: {e}")

def example_error_handling():
    """Example of proper error handling."""
    print("\n=== Error Handling Example ===")
    
    converter = MHCToARKitConverter()
    
    try:
        # This will fail if the file doesn't exist
        converter.convert("nonexistent_file.json", "output.json")
    except FileNotFoundError as e:
        print(f"File not found error: {e}")
    except ValueError as e:
        print(f"Value error: {e}")
    except Exception as e:
        print(f"Unexpected error: {e}")

if __name__ == "__main__":
    # Run examples (commented out to avoid actual file operations)
    print("MHC to ARKit Converter Usage Examples")
    print("Note: These examples are for demonstration purposes.")
    print("Uncomment the function calls below to run actual examples.\n")
    
    # example_basic_usage()
    # example_advanced_usage()
    # example_batch_processing()
    # example_error_handling()
    
    print("To run actual examples, uncomment the function calls above and provide valid file paths.") 
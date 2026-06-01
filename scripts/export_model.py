#!/usr/bin/env python3
"""
Export Demi neural network model from C header to .tflite file
Usage: python export_model.py
"""

import re
import sys
import os

HEADER_PATH = "../src/demi_mood_model.h"
OUTPUT_PATH = "demi_v3.tflite"

def extract_model_from_header(header_path):
    if not os.path.exists(header_path):
        print(f"Header not found: {header_path}")
        sys.exit(1)
    
    with open(header_path, 'r') as f:
        content = f.read()
    
    array_name_match = re.search(r'const unsigned char\s+(\w+)\s*\[\]', content)
    if not array_name_match:
        print("Could not find model array in header")
        sys.exit(1)
    
    array_name = array_name_match.group(1)
    print(f"Found array: {array_name}")
    
    len_match = re.search(rf'{array_name}_len\s*=\s*(\d+)', content)
    if not len_match:
        print("Could not find model length")
        sys.exit(1)
    
    model_len = int(len_match.group(1))
    print(f"Model length: {model_len} bytes")
    
    hex_pattern = re.search(
        r'\{([^}]+)\}',
        content,
        re.DOTALL
    )
    if not hex_pattern:
        print("Could not find model data")
        sys.exit(1)
    
    hex_data = hex_pattern.group(1)
    hex_values = re.findall(r'0x[0-9a-fA-F]{2}', hex_data)
    
    if len(hex_values) != model_len:
        print(f"Warning: Found {len(hex_values)} bytes, expected {model_len}")
    
    bytes_data = bytes([int(x, 16) for x in hex_values])
    
    return bytes_data

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    header_path = os.path.join(script_dir, HEADER_PATH)
    
    print(f"Reading: {header_path}")
    model_data = extract_model_from_header(header_path)
    
    output_path = os.path.join(script_dir, OUTPUT_PATH)
    with open(output_path, 'wb') as f:
        f.write(model_data)
    
    print(f"Exported: {output_path} ({len(model_data)} bytes)")
    print("\nNow run: python nn_sandbox.py demi_v3.tflite")

if __name__ == "__main__":
    main()
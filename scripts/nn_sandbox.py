#!/usr/bin/env python3
"""
NN Sandbox - Test Demi mood neural network on PC (no dependencies)
Usage: python nn_sandbox.py [model.tflite]
"""

import sys
import os
import struct

MOOD_NAMES = ["HAPPY", "CONTENT", "NEUTRAL", "CONCERNED", "SAD", "CRITICAL", 
            "SPOOKED", "EXCITED", "TIRED", "HUNGRY", "DIRTY", "SLEEPY"]

PRESETS = {
    "happy":    (100, 100, 100, 100, 100),
    "hungry":   (10, 80, 80, 80, 80),
    "tired":   (80, 80, 10, 80, 80),
    "sad":     (80, 10, 80, 80, 80),
    "dirty":   (80, 80, 80, 80, 10),
    "critical": (15, 15, 15, 15, 15),
}

def load_model(path):
    with open(path, 'rb') as f:
        data = f.read()
    
    if data[:4] != b'\x20\x00\x00\x00':
        print(f"Invalid header: {data[:4]}")
        return None
    
    flatbuf_size = struct.unpack('<I', data[:4])[0]
    print(f"Flatbuffer size: {flatbuf_size}")
    
    model = parse_flatbuffer(data, flatbuf_size)
    return model

def parse_flatbuffer(data, size):
    weights = []
    
    weight_offsets = [397, 660, 683, 729, 742, 814, 823, 867, 890, 900, 937, 948, 1055, 1112, 1153]
    for off in weight_offsets:
        if off + 4 <= len(data):
            val = struct.unpack('<f', data[off:off+4])[0]
            weights.append(val)
    
    print(f"Extracted {len(weights)} weights: {[round(w, 3) for w in weights]}")
    
    return {
        'weights': weights,
    }

class Interpreter:
    def __init__(self, model):
        self.model = model
        self.weights = model['weights']
        self.input_data = [0.0] * 5
    
    def set_input(self, idx, data):
        self.input_data = data
    
    def invoke(self):
        w = self.weights
        
        h, hp, e, he, c = self.input_data
        
        if len(w) >= 20:
            happy = w[0]*h + w[1]*hp + w[2]*e + w[3]*he + w[4]*c + w[15]
            hungry = w[5]*h + w[6]*hp + w[7]*e + w[8]*he + w[9]*c + w[16]
            sad = w[10]*h + w[11]*hp + w[12]*e + w[13]*he + w[14]*c + w[17]
        else:
            happy = 0.1 * h - 0.05 * (1-hp)
            hungry = -0.15 * h + 0.1
            sad = -0.1 * (1-hp)
        
        output = [
            max(-1, min(1, happy)),
            max(-1, min(1, hungry)),
            max(-1, min(1, sad)),
            0.05,
            0.02,
        ]
        
        return output

def main():
    model_path = "scripts/demi_v3.tflite"
    
    test_mode = "--test" in sys.argv
    
    if len(sys.argv) > 1 and sys.argv[1] != "--test":
        model_path = sys.argv[1]
    elif not os.path.exists(model_path):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        model_path = os.path.join(script_dir, "demi_v3.tflite")
        if not os.path.exists(model_path):
            print(f"Usage: {sys.argv[0]} [model.tflite]")
            print(f"\n  --test  Run preset tests")
            print(f"\nPresets: {', '.join(PRESETS.keys())}")
            print("Export model first: python scripts/export_model.py")
            sys.exit(1)
    
    print(f"Loading model: {model_path}")
    model = load_model(model_path)
    
    if model is None:
        sys.exit(1)
    
    interpreter = Interpreter(model)
    print(f"Weights loaded: {len(interpreter.weights)} floats")
    
    if test_mode:
        print("Running preset tests...\n")
        for name, stats in PRESETS.items():
            print(f"Test: {name}")
            interpreter.set_input(0, [x/100.0 for x in stats])
            output = interpreter.invoke()
            print(f"  happy={output[0]:+.3f} hungry={output[1]:+.3f} sad={output[2]:+.3f}\n")
        sys.exit(0)
    
    print("Model loaded!\n")
    
    while True:
        print(f"{'='*50}")
        print("NN SANDBOX - Enter 5 values (0-100)")
        print(f"Presets: {', '.join(PRESETS.keys())}")
        print("Enter 'q' to quit")
        
        user_input = input("\n> ").strip()
        
        if user_input.lower() == 'q':
            break
        elif user_input.lower() in PRESETS:
            stats = PRESETS[user_input.lower()]
            print(f"preset: {user_input}")
        else:
            try:
                parts = user_input.replace(',', ' ').split()
                stats = [int(x) for x in parts]
                if len(stats) != 5:
                    print("Need exactly 5 values: hunger happiness energy health cleanliness")
                    continue
                stats = [max(0, min(100, x)) for x in stats]
            except ValueError:
                print("Invalid input. Enter 5 numbers or preset name")
                continue
        
        print(f"\nInputs: H={stats[0]} Hp={stats[1]} E={stats[2]} He={stats[3]} C={stats[4]}")
        
        interpreter.set_input(0, [x/100.0 for x in stats])
        output = interpreter.invoke()
        
        print("\n--- NN Output (biases) ---")
        print(f"  happy_bias: {output[0]:+.3f}")
        print(f"  hungry_bias: {output[1]:+.3f}")
        print(f"  sad_bias: {output[2]:+.3f}")
        print(f"  urgency: {output[3]:+.3f}")
        print(f"  noise: {output[4]:+.3f}")
        
        h, hp, e, he, c = stats
        base_happy = hp / 100.0
        base_hungry = (100 - h) / 100.0
        base_sad = (100 - hp) / 100.0
        
        final_happy = base_happy + output[0] * 0.3
        final_hungry = base_hungry + output[1] * 0.3
        final_sad = base_sad + output[2] * 0.3
        
        print("\n--- With Bias Applied ---")
        print(f"  final_happy: {final_happy:.1f}")
        print(f"  final_hungry: {final_hungry:.1f}")
        print(f"  final_sad: {final_sad:.1f}")

if __name__ == "__main__":
    main()
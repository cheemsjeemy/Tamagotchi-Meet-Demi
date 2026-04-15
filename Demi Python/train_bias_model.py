#!/usr/bin/env python3
"""
Demi AI Bias Model Trainer
================================
TRAINING FOR THE NEW ARCHITECTURE:
AI ONLY OUTPUTS BIAS VALUES - NEVER FULL MOODS

Model Input:  5 stats (hunger, happiness, energy, health, cleanliness)
Model Output: 5 bias values [happy_bias, hungry_bias, sad_bias, urgency_boost, emotional_noise]
"""

import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import numpy as np
import matplotlib.pyplot as plt
import random

# =============================================================================
# MODEL ARCHITECTURE - TINY, EFFICIENT, ONLY FOR BIAS
# =============================================================================
def create_bias_model():
    """
    Ultra small model designed ONLY to output bias modifiers
    Total params: ~1200 - fits perfectly in ESP32-S3
    """
    model = keras.Sequential([
        layers.Input(shape=(5,)),
        
        # Hidden layers are intentionally small
        layers.Dense(16, activation='tanh', kernel_initializer='glorot_normal'),
        layers.Dense(12, activation='tanh', kernel_initializer='glorot_normal'),
        layers.Dense(8, activation='tanh', kernel_initializer='glorot_normal'),
        
        # ✅ OUTPUT IS ONLY BIAS VALUES
        # Range for each output:
        # 0: happy_bias      [-1.0 .. 1.0]
        # 1: hungry_bias     [-1.0 .. 1.0]
        # 2: sad_bias        [-1.0 .. 1.0]
        # 3: urgency_boost   [-0.2 .. 0.2]
        # 4: emotional_noise [-0.1 .. 0.1]
        layers.Dense(5, activation='tanh', kernel_initializer='glorot_normal')
    ])
    
    model.compile(
        optimizer=keras.optimizers.Adam(learning_rate=0.001),
        loss='mse',
        metrics=['mae']
    )
    
    return model

# =============================================================================
# GENERATE TRAINING DATA
# =============================================================================
def generate_training_data(samples=50000):
    """
    Generate training data that teaches the model to ONLY nudge, NOT decide
    We intentionally train it to output SMALL values
    """
    X = []
    y = []
    
    for _ in range(samples):
        # Random stats 0-100
        hunger = random.randint(0, 100)
        happiness = random.randint(0, 100)
        energy = random.randint(0, 100)
        health = random.randint(0, 100)
        cleanliness = random.randint(0, 100)
        
        # Normalize inputs 0.0 - 1.0
        inputs = [
            hunger / 100.0,
            happiness / 100.0,
            energy / 100.0,
            health / 100.0,
            cleanliness / 100.0
        ]
        
        # Calculate what the bias SHOULD be
        # THESE ARE ALL SMALL VALUES - TEACHES MODEL TO ONLY NUDGE
        happy_bias = ((happiness - 50) / 50.0) * 0.3
        hungry_bias = ((50 - hunger) / 50.0) * 0.4
        sad_bias = ((50 - happiness) / 50.0) * 0.3
        
        # Urgency only when stats are low
        avg_low = 1.0 - ((hunger + energy + cleanliness) / 300.0)
        urgency_boost = avg_low * 0.2
        
        # Small random noise
        emotional_noise = (random.random() - 0.5) * 0.1
        
        outputs = [
            happy_bias,
            hungry_bias,
            sad_bias,
            urgency_boost,
            emotional_noise
        ]
        
        X.append(inputs)
        y.append(outputs)
    
    return np.array(X, dtype=np.float32), np.array(y, dtype=np.float32)

# =============================================================================
# TRAIN
# =============================================================================
if __name__ == "__main__":
    print("=" * 60)
    print("DEMI AI BIAS MODEL TRAINER")
    print("Training model that ONLY outputs bias values - MAX 30% influence")
    print("=" * 60)
    
    # Create model
    model = create_bias_model()
    model.summary()
    
    # Generate data
    print("\nGenerating training data...")
    X_train, y_train = generate_training_data(60000)
    X_val, y_val = generate_training_data(10000)
    
    # Train
    print("\nTraining...")
    history = model.fit(
        X_train, y_train,
        validation_data=(X_val, y_val),
        epochs=50,
        batch_size=32,
        verbose=1
    )
    
    # Test
    print("\nTesting model outputs:")
    test_cases = [
        [100, 100, 100, 100, 100],  # Perfect stats
        [20,  60,  50,  70,  80],   # Hungry
        [100, 20,  100, 100, 100],  # Sad
        [100, 100, 15,  100, 100],  # Tired
        [50,  50,  50,  50,  50],   # Neutral
    ]
    
    for test in test_cases:
        pred = model.predict(np.array([[x/100.0 for x in test]]), verbose=0)[0]
        print(f"\nStats: {test}")
        print(f"  Happy bias:   {pred[0]:+.3f}")
        print(f"  Hungry bias:  {pred[1]:+.3f}")
        print(f"  Sad bias:     {pred[2]:+.3f}")
        print(f"  Urgency:      {pred[3]:+.3f}")
        print(f"  Noise:        {pred[4]:+.3f}")
    
    # Save model for TFLite conversion
    print("\nConverting to TFLite...")
    
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    
    def representative_dataset():
        for i in range(100):
            yield [X_val[i:i+1]]
    
    converter.representative_dataset = representative_dataset
    tflite_model = converter.convert()
    
    with open('demi_bias_model.tflite', 'wb') as f:
        f.write(tflite_model)
    
    # Generate C header file
    print("Generating C header file...")
    
    byte_array = ', '.join([f'0x{b:02x}' for b in tflite_model])
    
    header_content = f"""// Demi AI Bias Model
// AUTOGENERATED - DO NOT EDIT
// Outputs 5 bias values: happy, hungry, sad, urgency, noise
// MAX INFLUENCE: 20-30%

const unsigned char demi_bias_tflite[] __attribute__((aligned(16))) = {{
  {byte_array}
}};

const unsigned int demi_bias_tflite_len = {len(tflite_model)};
"""
    
    with open('demi_bias_model.h', 'w') as f:
        f.write(header_content)
    
    print("\n✅ Done!")
    print(f"Model size: {len(tflite_model)} bytes")
    print("\nCopy demi_bias_model.h to your src/ folder")

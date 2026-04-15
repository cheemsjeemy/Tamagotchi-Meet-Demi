import numpy as np
import tensorflow as tf
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense, Dropout, BatchNormalization, Activation
import matplotlib.pyplot as plt
from google.colab import files

# 1. THE DYNAMIC LOGIC (The "Brain" Rules)
def get_label_from_stats(h, hp, e, c):
    # Calculate Base Need Scores
    scores = {
        11: (100 - e) * 1.5, # Energy need (Sleep)
        9:  (100 - h) * 1.3, # Hunger need 
        10: (100 - c) * 1.1, # Cleanliness need
        5:  (100 - hp) * 1.0  # Emotional need (Sad)
    }

    # Identify the highest need and the Gap
    winning_mood = max(scores, key=scores.get)
    max_score = scores[winning_mood]
    avg_stats = (h + hp + e + c) / 4
    
    # --- RULE 1: THE ANCHOR (Fixes Scenario 1) ---
    # If stats are all high (Need Score < 20), stay Happy/Content
    if max_score < 20:
        return 0 if hp >= 85 else 1

    # --- RULE 2: THE AMBIGUITY ZONE (Hits your <50% target) ---
    # If stats are in the mid-range and no need is "screaming"
    if 25 < max_score < 65:
        roll = np.random.random()
        if roll < 0.40: return winning_mood # 40% Confidence
        if roll < 0.70: return 3            # 30% Concerned (Mixed)
        return 2                            # 30% Neutral

    # --- RULE 3: CRITICAL PRIORITY ---
    # If a need is desperate (Score > 85), stay confident but allow 10% leak
    if max_score > 85:
        if np.random.random() < 0.10: return 3
        return winning_mood

    return winning_mood

# 2. GENERATE SAMPLES (80,000 for S3 N16R8 Precision)
NUM_SAMPLES = 80000 
X, y = [], []

print(f"Generating {NUM_SAMPLES} samples...")
for _ in range(NUM_SAMPLES):
    h, hp, e, c = [np.random.randint(0, 101) for _ in range(4)]
    health = (h + hp + e + c) // 4
    X.append([h, hp, e, health, c])
    y.append(get_label_from_stats(h, hp, e, c))

X = np.array(X, dtype=np.float32) / 100.0
y_cat = tf.keras.utils.to_categorical(y, num_classes=12)

# 3. S3 N16R8 ARCHITECTURE
model = Sequential([
    Dense(128, input_shape=(5,)), 
    BatchNormalization(),
    Activation('swish'),
    Dense(64),
    Activation('swish'),
    Dense(32),
    Activation('swish'),
    Dropout(0.1),
    Dense(12, activation='softmax')
])

model.compile(optimizer='adam', loss='categorical_crossentropy', metrics=['accuracy'])

# 4. TRAIN
print("\nTraining V3 Dynamic Ambiguity Model...")
history = model.fit(X, y_cat, epochs=100, batch_size=64, validation_split=0.1, verbose=1)

# 5. TEST SCENARIOS
print("\n--- FINAL BEHAVIORAL VERIFICATION ---")
scenarios = {
    "Perfect Stats": [1.0, 1.0, 1.0, 1.0, 1.0],
    "Hungry Socialite (16 vs 76)": [0.16, 0.76, 0.60, 0.60, 0.80],
    "Mid-Range Ambiguity": [0.50, 0.50, 0.50, 0.50, 0.50],
    "Dying Battery": [0.80, 0.80, 0.05, 0.50, 0.80]
}

mood_names = ["Happy", "Content", "Neutral", "Concerned", "Sad", "Critical", 
              "Spooked", "Excited", "Tired", "Hungry", "Dirty", "Sleeping"]

for name, stats in scenarios.items():
    pred = model.predict(np.array([stats]), verbose=0)[0]
    print(f"\n>> {name}:")
    top_indices = np.argsort(pred)[::-1]
    for i in top_indices:
        if pred[i] > 0.02:
            print(f"   |-- {mood_names[i]:<10}: {pred[i]*100:>5.1f}%")

# 6. CONVERT & DOWNLOAD
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.target_spec.supported_types = [tf.float16]
tflite_model = converter.convert()

with open('/tmp/demi_v3_final.h', 'w') as f:
    f.write('// V3 Final Dynamic Model for ESP32-S3\n')
    f.write('const unsigned char demi_v3_tflite[] __attribute__((aligned(16))) = {\n')
    for i in range(0, len(tflite_model), 12):
        chunk = tflite_model[i:i+12]
        f.write('  ' + ', '.join(f'0x{b:02x}' for b in chunk) + ',\n')
    f.write('};\nunsigned int demi_v3_tflite_len = ' + str(len(tflite_model)) + ';')

files.download('/tmp/demi_v3_final.h')

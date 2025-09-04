import tensorflow as tf
import numpy as np
import sys
from pathlib import Path

def convert_tflite_to_c_arrays(tflite_path: Path, output_h: Path):
    interp = tf.lite.Interpreter(model_path=str(tflite_path))
    interp.allocate_tensors()
    tensors = interp.get_tensor_details()
    input_details = interp.get_input_details()[0]
    output_details = interp.get_output_details()[0]

    print("Tensor Details:")
    for tensor in tensors:
        print(f"Name: {tensor['name']}, Shape: {tensor['shape']}, Type: {tensor['dtype']}")

    with output_h.open('w') as f:
        f.write('#pragma once\n#include <cstdint>\n\n')
        scale, zp = input_details['quantization']
        f.write(f'const float input_scale = {scale}f;\n')
        f.write(f'const int32_t input_zero_point = {zp};\n\n')
        scale, zp = output_details['quantization']
        f.write(f'const float output_scale = {scale}f;\n')
        f.write(f'const int32_t output_zero_point = {zp};\n\n')

        seen_names = set()
        for tensor in tensors:
            if 'serving_default' in tensor['name'] or 'reduction_indices' in tensor['name'] or \
                any(x in tensor['name'] for x in ['Relu', 'Mean', 'AvgPool', 'Stateful']):
                print(f"Skipping tensor (non-weight): {tensor['name']}")
                continue
            try:
                data = interp.get_tensor(tensor['index'])
                if data.size == 0:
                    print(f"Skipping empty tensor: {tensor['name']}")
                    continue
                shape = tensor['shape']
                dtype = 'int8_t' if tensor['dtype'] == np.int8 else 'int32_t' if tensor['dtype'] == np.int32 else 'float'
                flat = data.flatten()
                # Split fused names on ';' and use the last part for the name
                parts = tensor['name'].split(';')
                name_part = parts[-1].strip() if len(parts) > 1 else tensor['name']
                name = name_part.replace('/', '_').replace(':', '_').replace(' ', '_')
                # Ensure unique names
                base_name = name
                counter = 1
                while name in seen_names:
                    name = f"{base_name}_{counter}"
                    counter += 1
                seen_names.add(name)
                f.write(f'const {dtype} {name}[] = {{\n')
                for i in range(0, len(flat), 8):
                    chunk = flat[i:i+8]
                    f.write('  ' + ', '.join(f'{int(v)}' for v in chunk) + ',\n')
                f.write('};\n')
                f.write(f'// Shape: {shape}\n\n')
            except ValueError as e:
                print(f"Error processing tensor {tensor['name']}: {e}")
                continue

    print(f"Generated: {output_h}")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python model_converter.py <model_int8.tflite> <output_dir>")
        sys.exit(1)
    tflite_path = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])
    output_h = output_dir / 'model_weights.h'
    convert_tflite_to_c_arrays(tflite_path, output_h)
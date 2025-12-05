#!/usr/bin/env python3
"""
Porkchop ML Training Pipeline
Local training scripts for Edge Impulse model generation

This script processes collected WiFi data and prepares it for Edge Impulse training.
"""

import os
import sys
import json
import argparse
import numpy as np
from pathlib import Path
from datetime import datetime

# Feature configuration - must match ESP32 feature extractor
FEATURE_NAMES = [
    'rssi', 'noise', 'snr', 'channel', 'secondary_channel',
    'beacon_interval', 'capability_low', 'capability_high',
    'has_wps', 'has_wpa', 'has_wpa2', 'has_wpa3', 'is_hidden',
    'response_time', 'beacon_count', 'beacon_jitter',
    'responds_to_probe', 'probe_response_time',
    'vendor_ie_count', 'supported_rates', 'ht_capabilities', 'vht_capabilities',
    'anomaly_score'
]

LABELS = ['normal', 'rogue_ap', 'evil_twin', 'deauth_target', 'vulnerable']


def load_capture_data(filepath):
    """Load captured WiFi data from Porkchop export format."""
    data = []
    
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            
            try:
                entry = json.loads(line)
                data.append(entry)
            except json.JSONDecodeError:
                # Try CSV format
                parts = line.split(',')
                if len(parts) >= 10:
                    entry = {
                        'bssid': parts[0],
                        'ssid': parts[1],
                        'rssi': int(parts[2]),
                        'channel': int(parts[3]),
                        'features': [float(x) for x in parts[4:]]
                    }
                    data.append(entry)
    
    return data


def extract_features(entry):
    """Extract feature vector from capture entry."""
    features = np.zeros(32, dtype=np.float32)
    
    if 'features' in entry and len(entry['features']) >= 23:
        features[:23] = entry['features'][:23]
    else:
        # Basic features from simple capture
        features[0] = entry.get('rssi', -80)
        features[1] = -95  # Default noise
        features[2] = features[0] - features[1]  # SNR
        features[3] = entry.get('channel', 1)
    
    return features


def compute_normalization_params(features):
    """Compute mean and std for feature normalization."""
    features = np.array(features)
    means = np.mean(features, axis=0)
    stds = np.std(features, axis=0)
    stds[stds < 0.001] = 1.0  # Avoid division by zero
    
    return means.tolist(), stds.tolist()


def normalize_features(features, means, stds):
    """Apply z-score normalization."""
    return [(f - m) / s for f, m, s in zip(features, means, stds)]


def prepare_edge_impulse_data(data, output_dir):
    """Prepare data in Edge Impulse format."""
    Path(output_dir).mkdir(parents=True, exist_ok=True)
    
    # Split data by label
    for label in LABELS:
        label_dir = Path(output_dir) / label
        label_dir.mkdir(exist_ok=True)
    
    # Extract all features for normalization
    all_features = []
    for entry in data:
        features = extract_features(entry)
        all_features.append(features)
    
    means, stds = compute_normalization_params(all_features)
    
    # Save normalization params
    norm_params = {
        'means': means,
        'stds': stds,
        'feature_names': FEATURE_NAMES
    }
    
    with open(Path(output_dir) / 'normalization.json', 'w') as f:
        json.dump(norm_params, f, indent=2)
    
    # Export samples
    for i, entry in enumerate(data):
        features = extract_features(entry)
        norm_features = normalize_features(features, means, stds)
        
        label = entry.get('label', 'normal')
        if label not in LABELS:
            label = 'normal'
        
        sample = {
            'values': norm_features,
            'label': label,
            'metadata': {
                'bssid': entry.get('bssid', ''),
                'ssid': entry.get('ssid', ''),
                'timestamp': entry.get('timestamp', '')
            }
        }
        
        filename = f'{label}_{i:05d}.json'
        with open(Path(output_dir) / label / filename, 'w') as f:
            json.dump(sample, f)
    
    print(f"Prepared {len(data)} samples in Edge Impulse format")
    print(f"Normalization params saved to {output_dir}/normalization.json")


def export_c_header(norm_params, output_path):
    """Export normalization parameters as C header for ESP32."""
    
    header = '''// Auto-generated normalization parameters for Porkchop ML
// Generated: {timestamp}
#pragma once

#define FEATURE_VECTOR_SIZE 32

// Feature normalization parameters (z-score)
const float FEATURE_MEANS[FEATURE_VECTOR_SIZE] = {{
    {means}
}};

const float FEATURE_STDS[FEATURE_VECTOR_SIZE] = {{
    {stds}
}};
'''
    
    means_str = ', '.join([f'{m:.6f}f' for m in norm_params['means']])
    stds_str = ', '.join([f'{s:.6f}f' for s in norm_params['stds']])
    
    content = header.format(
        timestamp=datetime.now().isoformat(),
        means=means_str,
        stds=stds_str
    )
    
    with open(output_path, 'w') as f:
        f.write(content)
    
    print(f"C header exported to {output_path}")


def main():
    parser = argparse.ArgumentParser(description='Porkchop ML Training Pipeline')
    parser.add_argument('command', choices=['prepare', 'export-header', 'analyze'])
    parser.add_argument('--input', '-i', help='Input data file')
    parser.add_argument('--output', '-o', help='Output directory or file')
    parser.add_argument('--label', '-l', help='Default label for unlabeled data', default='normal')
    
    args = parser.parse_args()
    
    if args.command == 'prepare':
        if not args.input or not args.output:
            print("Error: --input and --output required for prepare")
            sys.exit(1)
        
        data = load_capture_data(args.input)
        
        # Apply default label
        for entry in data:
            if 'label' not in entry:
                entry['label'] = args.label
        
        prepare_edge_impulse_data(data, args.output)
    
    elif args.command == 'export-header':
        if not args.input or not args.output:
            print("Error: --input and --output required for export-header")
            sys.exit(1)
        
        with open(args.input, 'r') as f:
            norm_params = json.load(f)
        
        export_c_header(norm_params, args.output)
    
    elif args.command == 'analyze':
        if not args.input:
            print("Error: --input required for analyze")
            sys.exit(1)
        
        data = load_capture_data(args.input)
        
        print(f"\n=== Data Analysis ===")
        print(f"Total samples: {len(data)}")
        
        # Label distribution
        label_counts = {}
        for entry in data:
            label = entry.get('label', 'unlabeled')
            label_counts[label] = label_counts.get(label, 0) + 1
        
        print("\nLabel distribution:")
        for label, count in sorted(label_counts.items()):
            print(f"  {label}: {count}")
        
        # Feature statistics
        all_features = [extract_features(e) for e in data]
        if all_features:
            features = np.array(all_features)
            print("\nFeature statistics:")
            for i, name in enumerate(FEATURE_NAMES[:10]):
                print(f"  {name}: mean={features[:,i].mean():.2f}, std={features[:,i].std():.2f}")


if __name__ == '__main__':
    main()

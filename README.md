# Porkchop - ML-Enhanced Piglet Security Companion

```
   ^  ^   
  (o oo)  
 -(____)- 
   |  |   
   ''  '' 
```

A tamagotchi-like security companion for the M5Cardputer, featuring:
- **OINK Mode**: Packet sniffing, network discovery, and handshake capture
- **WARHOG Mode**: GPS-enabled wardriving with export to Wigle/Kismet formats
- **ML-powered detection**: TinyML for rogue AP and anomaly detection

## Features

### 🐷 Piglet Personality
Your digital piglet companion reacts to discoveries:
- Gets excited when capturing handshakes
- Becomes sleepy during quiet periods
- Shows hunting focus when scanning

### 📡 OINK Mode
- Channel hopping WiFi scanner
- Promiscuous mode packet capture
- EAPOL/WPA handshake detection
- Deauth capability (for authorized testing only)
- ML-based network classification

### 🗺️ WARHOG Mode
- GPS-enabled wardriving
- Automatic network logging
- Export to CSV, Wigle, or Kismet formats
- Real-time statistics display

### 🧠 Machine Learning
- Edge Impulse-trained models
- Rogue AP detection
- Evil twin identification
- Vulnerability scoring
- OTA model updates (with user confirmation)

## Hardware Requirements

- M5Cardputer (ESP32-S3)
- AT6668 GPS Module (optional, for WARHOG mode)
- MicroSD card for data storage

## Quick Start

1. Copy `data/config.json` to your SD card
2. Flash the firmware via PlatformIO
3. Press `O` for OINK mode, `W` for WARHOG mode
4. Use `` ` `` to access the menu

## Controls

| Key | Action |
|-----|--------|
| O | Enter OINK mode |
| W | Enter WARHOG mode |
| ` | Toggle menu |
| ; | Navigate up |
| . | Navigate down |
| Enter | Select |
| ESC | Return to idle |

## Building

```bash
# Install PlatformIO
pip install platformio

# Build
pio run

# Upload
pio run -t upload

# Monitor
pio device monitor
```

## ML Training

Local training workflow:

```bash
# Analyze collected data
python scripts/train_model.py analyze -i captured_data.json

# Prepare for Edge Impulse
python scripts/train_model.py prepare -i captured_data.json -o training_data/

# After Edge Impulse training, export normalization params
python scripts/train_model.py export-header -i training_data/normalization.json -o src/ml/norm_params.h
```

## File Structure

```
porkchop/
├── src/
│   ├── main.cpp              # Entry point
│   ├── core/
│   │   ├── porkchop.h/cpp    # Main state machine
│   │   └── config.h/cpp      # Configuration management
│   ├── ui/
│   │   ├── display.h/cpp     # Triple-canvas display
│   │   └── menu.h/cpp        # Menu system
│   ├── piglet/
│   │   ├── avatar.h/cpp      # ASCII piglet art
│   │   └── mood.h/cpp        # Personality system
│   ├── gps/
│   │   └── gps.h/cpp         # AT6668 GPS driver
│   ├── ml/
│   │   ├── features.h/cpp    # Feature extraction
│   │   └── inference.h/cpp   # Edge Impulse inference
│   └── modes/
│       ├── oink.h/cpp        # Packet sniffing mode
│       └── warhog.h/cpp      # Wardriving mode
├── scripts/
│   ├── pre_build.py          # Version generation
│   └── train_model.py        # ML training pipeline
├── data/
│   └── config.json           # Default configuration
└── platformio.ini            # Build configuration
```

## Legal Disclaimer

This tool is intended for **authorized security research and educational purposes only**. 

- Only use on networks you own or have explicit permission to test
- Deauth attacks may be illegal in your jurisdiction
- The authors assume no liability for misuse

## Credits

- Inspired by [pwnagotchi](https://github.com/evilsocket/pwnagotchi)
- Built for [M5Cardputer](https://docs.m5stack.com/en/core/Cardputer)
- ML powered by [Edge Impulse](https://edgeimpulse.com/)

## License

MIT License - See LICENSE file for details

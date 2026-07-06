#!/bin/bash

# Deploy Script for chaos STATION
# This script checks for PlatformIO dependencies and runs the full build/upload process.

echo "==========================================================="
echo "⚡ chaos STATION - Build & Deploy System"
echo "==========================================================="

# Check if PlatformIO is installed
if ! command -v pio &> /dev/null; then
    echo "❌ Error: PlatformIO (pio) is not installed or not in your PATH."
    echo "Please install it by running: pip install platformio"
    echo "Or install the PlatformIO extension in VSCode."
    exit 1
fi

echo "✅ PlatformIO found. Starting deployment..."

# 1. Build and Upload Firmware
echo "🔨 Building and uploading firmware..."
pio run -t upload
if [ $? -ne 0 ]; then
    echo "❌ Firmware upload failed! Please check the output above."
    exit 1
fi
echo "✅ Firmware flashed successfully!"

# 2. Build and Upload Filesystem (LittleFS)
echo "📁 Building and uploading LittleFS filesystem (Web Dashboard)..."
pio run -t uploadfs
if [ $? -ne 0 ]; then
    echo "❌ Filesystem upload failed! Please check the output above."
    exit 1
fi
echo "✅ Filesystem uploaded successfully!"

echo "==========================================================="
echo "🎉 Deployment Complete!"
echo "Your ESP32 is now running chaos STATION."
echo "Wait for it to connect to WiFi and grab the IP address from the OLED display."
echo "==========================================================="

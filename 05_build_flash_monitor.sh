#!/bin/bash

echo ""
echo "====================================="
echo " ESP32 Build → Upload → Monitor"
echo "====================================="
echo ""

docker run -it --rm \
-v $(pwd):/workspace \
--device=/dev/ttyUSB0 \
esp32-dev \
bash -c "
echo '=== BUILD ==='
pio run

echo ''
echo '=== UPLOAD ==='
pio run -t upload

echo ''
echo '=== MONITOR ==='
pio device monitor -b 115200
"
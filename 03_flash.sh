#!/bin/bash
echo "=== ESP32 書き込み ==="

docker run --rm \
-v $(pwd):/workspace \
--device=/dev/ttyUSB0 \
esp32-dev \
pio run -t upload
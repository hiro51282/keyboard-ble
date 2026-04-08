#!/bin/bash

echo ""
echo "==============================="
echo " ESP32 Docker 開発環境"
echo "==============================="
echo ""
echo "使えるコマンド:"
echo ""
echo "ビルド:"
echo "  pio run"
echo ""
echo "ESP32へ書き込み:"
echo "  pio run -t upload"
echo ""
echo "シリアルモニタ:"
echo "  pio device monitor -b 115200"
echo ""
echo "終了:"
echo "  exit"
echo ""
echo "==============================="
echo ""

docker run -it --rm \
-v $(pwd):/workspace \
--device=/dev/ttyUSB0 \
esp32-dev
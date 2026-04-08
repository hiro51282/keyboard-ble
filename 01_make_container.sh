#!/bin/bash
echo "=== ESP32 Docker image build ==="

docker build -t esp32-dev .

echo ""
echo "=== 完了 ==="
echo "次は 02_enter_container.sh を実行"
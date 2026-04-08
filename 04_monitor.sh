#!/bin/bash

docker run --rm \
-v $(pwd):/workspace \
--device=/dev/ttyUSB0 \
esp32-dev \
pio device monitor -b 115200
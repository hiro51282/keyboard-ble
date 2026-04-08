FROM python:3.11-slim

RUN pip install platformio

WORKDIR /workspace

CMD ["bash"]
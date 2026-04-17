bool readRawFrame(RawFrame &f)
{
  static uint8_t buf[64];
  static int idx = 0;

  while (mySerial.available())
  {
    uint8_t d = mySerial.read();

    // ヘッダ開始検出
    if (d == 0x57) idx = 0;

    if (idx < (int)sizeof(buf))
      buf[idx++] = d;

    if (idx < 3) continue;

    // ヘッダ確認
    if (buf[0] != 0x57 || buf[1] != 0xAB)
      continue;

    uint8_t type = buf[2];

    // =========================
    // 状態2（マウス）: 固定7バイト
    // =========================
    if (type == 0x02)
    {
      if (idx < 7) continue;

      f.type = 0x02;
      f.len  = 4;  // ボタン + dx + dy + wheel

      memcpy(f.data, buf, 7);

      idx = 0;
      return true;
    }

    // =========================
    // 状態0（従来）
    // =========================
    if (idx < 4) continue;

    uint8_t len = buf[3];

    // 特殊フレーム
    if (type == 0x82)
    {
      f.type = type;
      f.len  = len;
      memcpy(f.data, buf, idx);
      idx = 0;
      return true;
    }

    if (idx < (len + 4)) continue;

    f.type = type;
    f.len  = len;
    memcpy(f.data, buf, len + 4);

    idx = 0;
    return true;
  }

  return false;
}
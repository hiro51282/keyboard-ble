# Unit Testing Guide

このプロジェクトは PlatformIO + Unity フレームワークでユニットテストを実装しています。

## テスト対象

- **フレームパーサー** (`readRawFrame()`)
  - キーボードフレーム (0x01) のパーサー
  - マウスフレーム (0x02) のパーサー
  - キープアライブフレーム (0x80) の無視
  - ガベージデータ処理
  - 負値の処理

- **マウス累積ロジック**
  - 累積バッファの動作
  - constrain（範囲制限）機能
  - 複数フレームの集約
  - ホイールスクロール処理

- **キーボードレポート**
  - レポート初期化
  - 単一キー/複数キー入力
  - Modifier（Shift/Ctrl/Alt）キー処理
  - 6KRO（同時押し最大6キー）対応
  - 日本語キーボード拡張キー

## テスト実行方法

### ホスト PC 上で実行（推奨）

```bash
pio test -e test
```

出力例：
```
21 test cases: 21 succeeded in 0.654 seconds
```

### 詳細表示

```bash
pio test -e test -v
```

## テストファイル

- `test/test_unified.cpp` — すべてのユニットテスト

## テスト結果

| カテゴリ | テスト数 | 結果 |
|---------|--------|------|
| フレームパーサー | 6 | ✅ PASSED |
| マウス累積 | 8 | ✅ PASSED |
| キーボードレポート | 7 | ✅ PASSED |
| **合計** | **21** | **✅ ALL PASSED** |

## テストなしに実装を変更した場合

リグレッション検出のため、以下を実行：

```bash
# ビルド
pio run -e esp32dev

# テスト実行
pio test -e test

# 両方を一度に
pio test -e test && pio run -e esp32dev
```

## テストの追加方法

新しいテストを追加する場合：

1. `test/test_unified.cpp` を編集
2. テスト関数を追加（例：`void test_XX_something(void) { ... }`)
3. `main()` 内に `RUN_TEST(test_XX_something)` を追加
4. `pio test -e test` で実行

例：
```cpp
void test_27_new_feature(void) {
    // Arrange
    int value = 100;
    
    // Act
    int result = my_function(value);
    
    // Assert
    TEST_ASSERT_EQUAL_INT(200, result);
}
```

## CI/CD への統合

GitHub Actions で自動テスト実行する場合（推奨）：

```yaml
- name: Run unit tests
  run: pio test -e test
```

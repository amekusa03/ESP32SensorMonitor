# ESP32 1.9" LCD Smart Clock & Environment Monitor

[English README](README.md) | 日本語

ESP32 と 1.9インチ IPS液晶（ST7789）を搭載したボード上で動作する、**Wi-Fi NTP同期デジタル時計 兼 室内環境モニター** です。  
ESP-IDF 標準の `esp_lcd` ドライバと内部 DMA フレームバッファを採用し、チラつきのない高速で滑らかな描画を実現しています。

---

## 🌟 主な機能

- **NTP 時刻同期**: Wi-Fi接続時に日本標準時（JST, UTC+9）を取得し、正確な年月日・曜日・時刻を表示
- **SwitchBot+sensor Web API 連携**:
  - ローカルネットワーク上のセンサーデバイス（`http://esp32-switchbot.local/api/sensor`）から mDNS 経由で定期取得
  - **室温（TEMP）** を画面中央に特大フォントで強調表示
  - **明るさ（BRIGHTNESS / LUX）** を下部にリアルタイム表示
- **自動調光（Auto-Dimming）**:
  - 照度センサー値に応じて液晶バックライト輝度を PWM でスムーズに自動調整
- **視認性に優れたUIデザイン**:
  - 統一されたビットマップフォントによるモダンなダークテーマUI
  - 320×170 横画面レイアウト

---

## 🖥 画面レイアウト

```text
+-------------------------------------------------------------+
|  2026/09/06 (SUNDAY)                        12:34           | <- 日付・曜日 ＆ 時刻
|-------------------------------------------------------------|
|  +-------------------------------------------------------+  |
|  | ROOM TEMP                                             |  |
|  |                                                       |  |
|  |                 2 5 . 3   C                           |  | <- メイン: 特大温度表示
|  |                                                       |  |
|  +-------------------------------------------------------+  |
|  +-------------------------------------------------------+  |
|  | BRIGHTNESS                       28.5 lx              |  | <- 照度 (LUX)
|  +-------------------------------------------------------+  |
+-------------------------------------------------------------+
```

---

## 🔧 ハードウェア仕様 & ピンアサイン

- **MCU**: ESP32 (Xtensa Dual-Core 240MHz, 16MB Flash, CH340 USB-Serial)
- **ディスプレイ**: 1.9インチ IPS TFT LCD (解像度: 170×320, ドライバ: ST7789)

### 液晶ピン接続 (SPI)

| LCD ピン | ESP32 GPIO | 機能説明 |
| :--- | :--- | :--- |
| **VCC** | 3.3V | 電源 |
| **GND** | GND | グランド |
| **DC** | **GPIO 2** | データ / コマンド切替 |
| **RST** | **GPIO 4** | リセット |
| **CS** | **GPIO 15** | チップセレクト |
| **SCLK** | **GPIO 18** | SPI クロック |
| **MOSI** | **GPIO 23** | SPI データ送信 |
| **BLK** | **GPIO 32** | バックライト制御 |

---

## 📁 ディレクトリ構成

```text
ESP32SensorMonitor/
├── CMakeLists.txt              # プロジェクトルート CMake 設定
├── sdkconfig.defaults          # 16MB Flash等のデフォルト設定
├── .gitignore                  # Git除外設定 (秘密情報やビルド成果物)
├── README.md                   # 英語版ドキュメント
├── README.JP.md                # 日本語版ドキュメント
└── main/
    ├── CMakeLists.txt          # コンポーネント依存関係
    ├── idf_component.yml       # mDNS などのコンポーネント依存
    ├── main.c                  # メインアプリケーション
    ├── secrets.example.h       # Wi-Fi 設定用テンプレート
    └── secrets.h               # Wi-Fi 認証情報 (※Git管理外)
```

---

## 🚀 開発環境 & ビルド・書き込み手順

### 1. 前提環境
- **ESP-IDF v5.x**（v5.4 推奨）

### 2. 環境変数の読み込み
```bash
source $HOME/esp/esp-idf/export.sh
```

### 3. Wi-Fi 設定
公開リポジトリからクローンした場合は、`secrets.example.h` を `secrets.h` にコピーして Wi-Fi 情報を入力します：

```bash
cp main/secrets.example.h main/secrets.h
```

`main/secrets.h` を編集：
```c
#pragma once
#define WIFI_SSID      "ご自身のWi-FiのSSID"
#define WIFI_PASS      "ご自身のWi-Fiのパスワード"
```

### 4. ターゲット設定 (初回のみ)
```bash
idf.py set-target esp32
```

### 5. ビルド
```bash
idf.py build
```

### 6. フラッシュ書き込み & モニター起動
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

*(※シリアルモニターを終了するには `Ctrl + ]` を押します)*

---

## 📜 ライセンス

MIT License

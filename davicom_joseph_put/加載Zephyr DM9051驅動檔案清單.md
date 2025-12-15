# Zephyr DM9051 網路驅動使用者手冊

## 驅動檔案清單

### 1. 驅動核心檔案

| 檔案路徑 | 說明 |
|----------|------|
| `zephyr/drivers/ethernet/eth_dm9051.c` | DM9051 SPI 乙太網路驅動主要實作 |
| `zephyr/drivers/ethernet/eth_dm9051_priv.h` | DM9051 驅動私有標頭檔，定義暫存器與內部結構 |

### 2. 建置與配置檔案

| 檔案路徑 | 說明 |
|----------|------|
| `zephyr/drivers/ethernet/CMakeLists.txt` | 驅動建置腳本 |
| `zephyr/drivers/ethernet/Kconfig` | 乙太網路驅動主 Kconfig |
| `zephyr/drivers/ethernet/Kconfig.dm9051` | DM9051 專用 Kconfig 配置選項 |

### 3. Device Tree 綁定檔案

| 檔案路徑 | 說明 |
|----------|------|
| `zephyr/dts/bindings/ethernet/davicom,dm9051.yaml` | DM9051 Device Tree 綁定定義 |

---

## 檔案詳細說明

### eth_dm9051.c
DM9051 驅動的主要實作檔案，包含：
- SPI 通訊初始化與資料傳輸
- MAC 位址設定
- 封包收發處理
- 中斷處理邏輯
- Reset 腳位控制邏輯

### eth_dm9051_priv.h
定義 DM9051 晶片的：
- 暫存器位址與位元遮罩
- 驅動內部資料結構
- 私有函式宣告

### CMakeLists.txt
驅動建置腳本，包含條件編譯指令：
```cmake
zephyr_library_sources_ifdef(CONFIG_ETH_DM9051 eth_dm9051.c)
```

**說明：**
- `zephyr_library_sources_ifdef()` 是 Zephyr 建置系統的巨集
- 當 `CONFIG_ETH_DM9051=y` 時，才會將 `eth_dm9051.c` 加入編譯
- 這確保只有在啟用 DM9051 驅動時才編譯相關程式碼

### Kconfig
乙太網路驅動主 Kconfig 檔案，包含引用指令：
```kconfig
source "drivers/ethernet/Kconfig.dm9051"
```

**說明：**
- `source` 指令用於引入子 Kconfig 檔案
- 將 `Kconfig.dm9051` 的配置選項整合至主選單
- 使用者可透過 `menuconfig` 或 `guiconfig` 啟用 DM9051 驅動

### Kconfig.dm9051
DM9051 專用 Kconfig 配置檔案，提供以下配置選項：

**主選項：**
- `CONFIG_ETH_DM9051` - 啟用 DM9051 乙太網路控制器驅動
  - 預設值：`y`
  - 自動選擇：`SPI` 子系統
  - 說明：DM9051 獨立式乙太網路控制器，具備 SPI 介面

**子選項（當 `CONFIG_ETH_DM9051=y` 時可用）：**

| 配置選項 | 預設值 | 說明 |
|:---------|:-------|:-----|
| `CONFIG_ETH_DM9051_RX_THREAD_STACK_SIZE` | 800 | 接收封包處理執行緒的堆疊大小（bytes） |
| `CONFIG_ETH_DM9051_RX_THREAD_PRIO` | 2 | 接收封包處理執行緒的優先權等級 |
| `CONFIG_ETH_DM9051_CLKRDY_INIT_WAIT_MS` | 2 | 驅動初始化時等待 CLKRDY 位元的超時時間（毫秒），若超時則初始化失敗並回傳 `-ETIMEDOUT` |
| `CONFIG_ETH_DM9051_TIMEOUT` | 100 | 等待 IP 堆疊記憶體緩衝區的超時時間（毫秒），若超時則丟棄乙太網路封包 |

### davicom,dm9051.yaml
DM9051 Device Tree 綁定定義檔案，描述硬體節點的屬性規範。

**檔案結構：**
```yaml
# Copyright (c) 2025 - 2026, Davicom, Inc. Limited
# SPDX-License-Identifier: Apache-2.0

description: DM9051 standalone 10/100BASE-T Ethernet controller with SPI interface

compatible: "davicom,dm9051"

include: [spi-device.yaml, ethernet-controller.yaml]

properties:
  int-gpios:
    type: phandle-array
    required: true
    description: Interrupt pin.
      The interrupt pin of DM9051 is active low.
      If connected directly the MCU pin should be configured
      as active low.

  reset-gpios:
    type: phandle-array
    required: true
    description: Reset pin.
      Reset GPIO pin for hardware reset of the DM9051 chip.
      This pin is active low. The driver will toggle this pin
      with a low pulse during initialization to perform a hardware reset.
```

**屬性說明：**

| 屬性 | 類型 | 必要性 | 說明 |
|:-----|:-----|:-------|:-----|
| `compatible` | string | 必要 | 相容性字串，固定為 `"davicom,dm9051"` |
| `reg` | int | 必要 | SPI 裝置位址（CS 編號），通常為 `0` |
| `spi-max-frequency` | int | 必要 | SPI 最大時脈頻率（Hz），建議 `8000000~40000000`（8MHz~40MHz） |
| `int-gpios` | phandle-array | 必要 | 中斷腳位，低電位觸發 |
| `reset-gpios` | phandle-array | 必要 | 硬體重置腳位，低電位觸發 |
| `local-mac-address` | uint8-array | 選用 | 本地 MAC 位址，格式 `[XX XX XX XX XX XX]` |

**繼承的綁定檔案：**
- `spi-device.yaml` - 提供 SPI 裝置通用屬性（如 `reg`、`spi-max-frequency`）
- `ethernet-controller.yaml` - 提供乙太網路控制器通用屬性（如 `local-mac-address`）

---

## 啟用 DM9051 驅動的基本步驟

1. **在 prj.conf 中啟用驅動**
   ```
   CONFIG_ETH_DM9051=y
   CONFIG_SPI=y
   CONFIG_GPIO=y
   ```

2. **在 Device Tree Overlay 中定義硬體連接**
   - 指定 SPI 匯流排
   - 設定中斷腳位 (`int-gpios`)
   - 設定 Reset 腳位 (`reset-gpios`)
   - 配置 MAC 位址（可選）

   **Device Tree Overlay 範例：**
   ```dts
   /* Ensure GPIO0, GPIO1 and their GPIOTE instances are enabled */
   &gpio0 {
      status = "okay";
   };

   &gpio1 {
      status = "okay";
   };

   &gpiote20 {
      status = "okay";
   };

   &pinctrl {
      spi21_default: spi21_default {
         group1 {
            psels = <NRF_PSEL(SPIM_SCK, 1, 3)>,
               <NRF_PSEL(SPIM_MOSI, 1, 1)>,
               <NRF_PSEL(SPIM_MISO, 1, 0)>;
         };
      };

      spi21_sleep: spi21_sleep {
         group1 {
            psels = <NRF_PSEL(SPIM_SCK, 1, 3)>,
               <NRF_PSEL(SPIM_MOSI, 1, 1)>,
               <NRF_PSEL(SPIM_MISO, 1, 0)>;
            low-power-enable;
         };
      };
   };

   &spi21 {
       status = "okay";
       cs-gpios = <&gpio0 10 GPIO_ACTIVE_LOW>;

       dm9051: dm9051@0 {
           compatible = "davicom,dm9051";
           reg = <0>;
           spi-max-frequency = <8000000>;
           int-gpios = <&gpio0 3 GPIO_ACTIVE_LOW>;
           reset-gpios = <&gpio2 0 GPIO_ACTIVE_LOW>;
           local-mac-address = [00 00 00 00 00 00];
           status = "okay";
       };
   };
   ```

   **腳位說明：**

| 屬性 | 說明 |
|:-----|:-----|
| `int-gpios` | 中斷為 CPU 輸入腳位，用於接收 DM9051 的中斷信號 |
| `reset-gpios` | Reset 為 CPU 輸出腳位，用於硬體重置 DM9051 晶片 |

---

## 相關文件參考

- Davicom DM9051 資料手冊
- Zephyr 網路驅動開發指南
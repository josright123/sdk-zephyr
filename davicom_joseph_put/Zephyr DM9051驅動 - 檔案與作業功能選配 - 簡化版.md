# Zephyr DM9051 網路驅動使用者手冊

## 驅動檔案清單

### 1. 驅動核心檔案

| 檔案路徑                                        | 說明                                                                 |
| ------------------------------------------- | ------------------------------------------------------------------ |
| `zephyr/drivers/ethernet/eth_dm9051.c`      | DM9051 SPI 乙太網路驅動主要實作                                              |
| `zephyr/drivers/ethernet/eth_dm9051_priv.h` | DM9051 驅動私有標頭檔，定義暫存器與內部結構 (#define MBNDRY_DEFAULT <br>MBNDRY_WORD) |

### 2. 建置與配置檔案

| 檔案路徑 | 說明 |
|----------|------|
| `zephyr/drivers/ethernet/CMakeLists.txt` | 驅動建置腳本 |
| `zephyr/drivers/ethernet/Kconfig` | 乙太網路驅動主 Kconfig |
| `zephyr/drivers/ethernet/Kconfig.dm9051` | DM9051 專用 Kconfig 配置選項 |

### 3. Device Tree 綁定檔案

| 檔案路徑                                               | 說明                      |
| -------------------------------------------------- | ----------------------- |
| `zephyr/dts/bindings/ethernet/davicom,dm9051.yaml` | DM9051 Device Tree 綁定定義 |

### 4. Overlay 覆蓋檔

| 檔案路徑                                                               | 說明                                    |
| ------------------------------------------------------------------ | ------------------------------------- |
| `zephyr/samples/net/dhcpv4_client/overlay_nrf54l15_dm9051.overlay` | 實作案例一,應用程式通過DM9051 Device Tree 可調屬性定義 |
| `nrf/samples/net/http_server/overlay_nrf54l15_dm9051.overlay`      | 實作案例二,應用程式通過DM9051 Device Tree 可調屬性定義 |

---

## 檔案詳細說明

### eth_dm9051.c
DM9051 驅動的主要實作檔案，包含：
- SPI 通訊初始化與資料傳輸
- MAC 位址設定
- 封包收發處理
- 中斷處理邏輯
- Reset 腳位控制邏輯
- 創建驅動的機制,
1. `#define DT_DRV_COMPAT davicom_dm9051` 指定了此驅動對應的 `compatible` 名稱是 `davicom,dm9051`（逗號會在巨集展開時自動補上）。
2. `DT_DRV_COMPAT` 是「驅動要綁定的 compatible 名稱」，`davicom_dm9051` 則是該名稱的標識符，對應binding 檔案與 devicetree 節點的 `compatible = "davicom,dm9051"`宣告，確保驅動只會匹配並實例化 DM9051 的節點。
3. - 在 binding 檔 `davicom,dm9051.yaml` 中會宣告 `compatible: "davicom,dm9051"`。
   - 在 devicetree/overlay（例如 `&spi21` 下的節點）使用 `compatible = "davicom,dm9051";`。
   - 驅動透過 `DT_DRV_COMPAT` 把驅動與上述二者串起來,`DT_INST_FOREACH_STATUS_OKAY(DM9051_DEFINE)` 就會為所有 `status = "okay"` 且 `compatible = "davicom,dm9051"` 的節點產生裝置實例（`dm9051_runtime_*`、`dm9051_config_*`）並呼叫 `ETH_NET_DEVICE_DT_INST_DEFINE` 建立網路介面。
4. 創建代碼說明

   ```
   #define DM9051_DEFINE(inst) \
    static struct dm9051_runtime dm9051_runtime_##inst = { \
      ... \
    }; \
    static const struct dm9051_config dm9051_config_##inst = { \
      ... \
    }; \
    ETH_NET_DEVICE_DT_INST_DEFINE(inst, eth_dm9051_init, NULL, &dm9051_runtime_##inst, \  
                      &dm9051_config_##inst, CONFIG_ETH_INIT_PRIORITY, &api_funcs, \
                      NET_ETH_MTU);

   DT_INST_FOREACH_STATUS_OKAY(DM9051_DEFINE);
   ```
    
    `inst` 不是字串，而是編譯期的「實例索引」整數。
    `DT_INST_FOREACH_STATUS_OKAY(DM9051_DEFINE)` 會對所有 `compatible = "davicom,dm9051"` 且 `status = "okay"` 的節點，依裝置樹掃描順序依序傳入 0、1、2… 來展開 `DM9051_DEFINE(inst)`。
    這個索引用來：
    - 拼出靜態物件名稱：`dm9051_runtime_0`、`dm9051_config_0` 等。
    - `ETH_NET_DEVICE_DT_INST_DEFINE` 會用該實例建裝置，實際的 `dev->name` 通常會被展開成 `DM9051@0`、`DM9051@1` 這類名稱（依實例索引）。

  5. **如何用 overlay 開/關 `int-gpios` / `reset-gpios`（不改 driver）**

- [eth_dm9051.c](vscode-file://vscode-app/c:/Users/joseph/AppData/Local/Programs/Microsoft%20VS%20Code/resources/app/out/vs/code/electron-browser/workbench/workbench.html) 會用  
    `GPIO_DT_SPEC_INST_GET_OR(..., {0})`，當 overlay **沒寫**  
    `int-gpios` / `reset-gpios` 時，`port == NULL`，  
    驅動就會自動走：
    - 沒 `int-gpios` → Polling mode（`cint(dev)==false`）
    - 沒 `reset-gpios` → 不做硬體 reset（`crst(dev)==false`）


   6. 在 [overlay_nrf54l15_dm9051.overlay](vscode-file://vscode-app/c:/Users/joseph/AppData/Local/Programs/Microsoft%20VS%20Code/resources/app/out/vs/code/electron-browser/workbench/workbench.html) 的 `dm9051@0` 區塊：

- 開中斷：加上例如 `int-gpios = <&gpio0 3 GPIO_ACTIVE_LOW>;`
    - 這會用到 `gpio0 -> gpiote30`，所以 `&gpiote30` 必須 `okay`
- 開 reset：加上 `reset-gpios = <&gpioX PIN GPIO_ACTIVE_LOW>;`
    - 若你要用 `gpio2`，需另外確認 SoC/board 是否有 `gpio2`  
        與其對應的 `gpiote-instance` 也必須 `okay`
- 關中斷：不要寫 `int-gpios`（或註解掉）
- 關 reset：不要寫 `reset-gpios`

| 將開中斷,開reset改成關中斷,關reset              |
| ------------------------------------ |
| ![[Pasted image 20251218101936.png]] |

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

## Device Tree Overlay 配置詳解

### 1. SPI 匯流排配置

#### 1.1 SPI 實例選擇
在 nRF54L15 等 SoC 上，通常有多個 SPI 實例可用（如 `&spi00`, `&spi20`, `&spi21`, `&spi22`）。需要在 overlay 中啟用並配置使用的 SPI：

```dts
&spi21 {
    status = "okay";
    cs-gpios = <&gpio0 10 GPIO_ACTIVE_LOW>;
    pinctrl-0 = <&spi21_default>;
    pinctrl-1 = <&spi21_sleep>;
    pinctrl-names = "default", "sleep";
};
```

**說明：**
- `status = "okay"` - 啟用此 SPI 實例
- `cs-gpios` - 晶片選擇腳位（Chip Select），格式為 `<&gpio控制器 腳位號 旗標>`
- `pinctrl-0/1` - 引用預定義的腳位配置群組

#### 1.2 SPI 腳位配置（pinctrl）
使用 `&pinctrl` 節點定義 SPI 腳位映射：

```dts
&pinctrl {
    spi21_default: spi21_default {
        group1 {
            psels = <NRF_PSEL(SPIM_SCK, 1, 3)>,   /* P1.3 = SCK */
                    <NRF_PSEL(SPIM_MOSI, 1, 1)>,  /* P1.1 = MOSI */
                    <NRF_PSEL(SPIM_MISO, 1, 0)>;  /* P1.0 = MISO */
        };
    };

    spi21_sleep: spi21_sleep {
        group1 {
            psels = <NRF_PSEL(SPIM_SCK, 1, 3)>,
                    <NRF_PSEL(SPIM_MOSI, 1, 1)>,
                    <NRF_PSEL(SPIM_MISO, 1, 0)>;
            low-power-enable;  /* 睡眠模式時進入低功耗狀態 */
        };
    };
};
```

**SPI 腳位說明：**

| 訊號名稱 | 全名 | 方向 | 說明 |
|:---------|:-----|:-----|:-----|
| SCK | Serial Clock | 輸出 | SPI 時脈訊號 |
| MOSI | Master Out Slave In | 輸出 | MCU → DM9051 資料線 |
| MISO | Master In Slave Out | 輸入 | DM9051 → MCU 資料線 |
| CS | Chip Select | 輸出 | 晶片選擇（在 `cs-gpios` 中定義） |

**腳位選擇格式：**
```c
NRF_PSEL(<功能>, <Port號>, <Pin號>)
```
- **Port號**：GPIO 埠編號（0, 1, 2...）
- **Pin號**：該埠的腳位編號（0-31）
- 範例：`NRF_PSEL(SPIM_SCK, 1, 3)` = 使用 GPIO1 的第 3 腳位作為 SPI 時脈

### 2. DM9051 節點配置屬性

#### 2.1 spi-max-frequency（SPI 時脈頻率）

```dts
dm9051@0 {
    spi-max-frequency = <8000000>;  /* 8 MHz */
};
```

**說明：**
- 單位：Hz（赫茲）
- **建議範圍：8MHz ~ 40MHz**
- DM9051 最高支援 40MHz，但實際速度需考慮：
  - MCU 的 SPI 控制器最高頻率
  - PCB 走線品質與長度
  - 訊號完整性要求

**常用設定值：**

| 頻率 | 用途 | 特性 |
|:-----|:-----|:-----|
| `8000000` (8MHz) | 穩定性優先 | 較低速度但訊號品質最佳，適合長距離或多層 PCB |
| `16000000` (16MHz) | 平衡選擇 | 速度與穩定性的折衷方案 |
| `32000000` (32MHz) | 高速應用 | 高吞吐量，需要良好的 PCB 設計 |
| `40000000` (40MHz) | 最高速度 | DM9051 極限速度，需要優質 PCB 與短走線 |

#### 2.2 int-gpios（中斷腳位）

```dts
dm9051@0 {
    int-gpios = <&gpio0 3 GPIO_ACTIVE_LOW>;
};
```

**格式說明：**
```
<&gpio控制器 腳位號 觸發極性>
```

**參數解析：**
- `&gpio0` - GPIO 控制器實例（gpio0, gpio1, gpio2...）
- `3` - 該控制器的第 3 號腳位
- `GPIO_ACTIVE_LOW` - 低電位有效（DM9051 的 INT 腳位為 Active Low）

**重要提醒：**
1. **方向：**INT 是 DM9051 的輸出，MCU 的輸入
2. **極性：**DM9051 的 INT 腳位在有中斷時會拉低，因此必須使用 `GPIO_ACTIVE_LOW`
3. **依賴：**使用的 GPIO 控制器及其對應的 GPIOTE 實例必須啟用

**GPIOTE 依賴範例：**
```dts
/* 若使用 gpio0，需啟用對應的 GPIOTE 實例 */
&gpio0 {
    status = "okay";
};

&gpiote20 {  /* gpio0 對應 gpiote20 (依 SoC 而異) */
    status = "okay";
};
```

**Polling 模式（不使用中斷）：**
若不想使用中斷，可以在 overlay 中完全移除 `int-gpios` 屬性：
```dts
dm9051@0 {
    compatible = "davicom,dm9051";
    reg = <0>;
    spi-max-frequency = <8000000>;
    /* int-gpios 省略 → 驅動自動進入 Polling 模式 */
    reset-gpios = <&gpio2 0 GPIO_ACTIVE_LOW>;
};
```

#### 2.3 reset-gpios（硬體重置腳位）

```dts
dm9051@0 {
    reset-gpios = <&gpio2 0 GPIO_ACTIVE_LOW>;
};
```

**格式說明：**
```
<&gpio控制器 腳位號 觸發極性>
```

**參數解析：**
- `&gpio2` - GPIO 控制器實例
- `0` - 該控制器的第 0 號腳位（即 P2.0）
- `GPIO_ACTIVE_LOW` - 低電位觸發重置（DM9051 的 RST# 腳位為 Active Low）

**重要提醒：**
1. **方向：**RST# 是 MCU 的輸出，DM9051 的輸入
2. **時序：**驅動在初始化時會發送低脈衝（Low Pulse）進行硬體重置
3. **脈衝寬度：**驅動通常會拉低至少 2~10 ms

**重置時序：**
```
     _______________           _______________
RST#                |_________|               （拉低期間 DM9051 重置）
                     ← 2ms+ →
```

**不使用 Reset 腳位：**
若硬體設計中 DM9051 的 RST# 始終接高電位，可省略此屬性：
```dts
dm9051@0 {
    compatible = "davicom,dm9051";
    /* reset-gpios 省略 → 驅動不執行硬體重置 */
};
```

#### 2.4 local-mac-address（本地 MAC 位址）

```dts
dm9051@0 {
    local-mac-address = [00 00 00 00 00 00];
};
```

**格式說明：**
- 使用方括號 `[ ]` 包圍，內部為 6 個十六進位位元組
- 每個位元組可用 1 或 2 位數表示
- 位元組間可用空格分隔（也可不用）

**範例：**
```dts
/* 以下三種寫法等價 */
local-mac-address = [00 00 00 00 00 00];
local-mac-address = [0 0 0 0 0 0];
local-mac-address = [000000000000];
```

**自訂 MAC 位址範例：**
```dts
/* 使用 Davicom OUI (00:60:6E) + 自訂序號 */
local-mac-address = [00 60 6E 12 34 56];

/* 使用本地管理位址（Locally Administered Address） */
local-mac-address = [02 00 00 12 34 56];  /* 第一位元組的第2位元設為1 */
```

**MAC 位址規則：**

| 位元 | 說明 | 值 |
|:-----|:-----|:---|
| Byte[0] bit 0 | I/G (Individual/Group) | 0=單播, 1=多播 |
| Byte[0] bit 1 | U/L (Universal/Local) | 0=全域唯一, 1=本地管理 |

**建議：**
1. **全零位址 `[00 00 00 00 00 00]`** - 驅動會使用 DM9051 內部預設 MAC（通常為隨機或晶片序號）
2. **自訂位址** - 請確保：
   - 第一位元組為偶數（單播位址）
   - 若無購買 OUI，建議使用本地管理位址（`02:XX:XX:XX:XX:XX`）

#### 2.5 reg（SPI 裝置位址）

```dts
dm9051@0 {
    reg = <0>;
};
```

**說明：**
- **值範圍：**0, 1, 2...（對應 SPI 匯流排上的 CS 編號）
- **慣例：**若 SPI 匯流排上只有一個裝置，通常使用 `<0>`
- **注意：**`reg` 的值必須與父節點 `cs-gpios` 陣列的索引對應

**多裝置範例：**
```dts
&spi21 {
    cs-gpios = <&gpio0 10 GPIO_ACTIVE_LOW>,  /* CS0 */
               <&gpio0 11 GPIO_ACTIVE_LOW>;  /* CS1 */

    dm9051_1: dm9051@0 {
        reg = <0>;  /* 使用 CS0 (gpio0.10) */
    };

    dm9051_2: dm9051@1 {
        reg = <1>;  /* 使用 CS1 (gpio0.11) */
    };
};
```

### 3. GPIO 與 GPIOTE 啟用

#### 3.1 GPIO 控制器啟用
使用到的 GPIO 控制器必須啟用：

```dts
&gpio0 {
    status = "okay";
};

&gpio1 {
    status = "okay";
};

&gpio2 {
    status = "okay";
};
```

#### 3.2 GPIOTE 實例啟用
在 nRF 系列 SoC 上，中斷功能需要 GPIOTE（GPIO Task and Event）支援：

```dts
&gpiote20 {  /* gpio0 使用 */
    status = "okay";
};

&gpiote30 {  /* gpio1/gpio2 使用 (依 SoC 而異) */
    status = "okay";
};
```

**對應關係（nRF54L15 範例）：**

| GPIO Port | GPIOTE Instance | 說明 |
|:----------|:----------------|:-----|
| gpio0 | gpiote20 | 安全域 (Secure) GPIO |
| gpio1 | gpiote30 | 非安全域 GPIO |
| gpio2 | gpiote30 | 非安全域 GPIO |

**注意：**不同 nRF SoC 的對應關係可能不同，請查閱特定晶片的 Device Tree 定義。

### 4. 完整配置範例

#### 4.1 中斷模式 + 硬體重置

```dts
/* GPIO 與 GPIOTE 啟用 */
&gpio0 {
    status = "okay";
};

&gpio1 {
    status = "okay";
};

&gpio2 {
    status = "okay";
};

&gpiote20 {
    status = "okay";
};

&gpiote30 {
    status = "okay";
};

/* SPI 腳位配置 */
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

/* SPI 匯流排設定 */
&spi21 {
    status = "okay";
    cs-gpios = <&gpio0 10 GPIO_ACTIVE_LOW>;
    pinctrl-0 = <&spi21_default>;
    pinctrl-1 = <&spi21_sleep>;
    pinctrl-names = "default", "sleep";

    dm9051: dm9051@0 {
        compatible = "davicom,dm9051";
        reg = <0>;
        spi-max-frequency = <16000000>;           /* 16 MHz */
        int-gpios = <&gpio0 3 GPIO_ACTIVE_LOW>;   /* P0.3 中斷 */
        reset-gpios = <&gpio2 0 GPIO_ACTIVE_LOW>; /* P2.0 重置 */
        local-mac-address = [00 60 6E 12 34 56];  /* 自訂 MAC */
        status = "okay";
    };
};
```

#### 4.2 Polling 模式（無中斷、無硬體重置）

```dts
&gpio0 {
    status = "okay";
};

&gpio1 {
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
    pinctrl-0 = <&spi21_default>;
    pinctrl-1 = <&spi21_sleep>;
    pinctrl-names = "default", "sleep";

    dm9051: dm9051@0 {
        compatible = "davicom,dm9051";
        reg = <0>;
        spi-max-frequency = <8000000>;
        /* int-gpios 省略 → Polling 模式 */
        /* reset-gpios 省略 → 無硬體重置 */
        local-mac-address = [00 00 00 00 00 00];
        status = "okay";
    };
};
```

### 5. 配置檢查清單

在建立 Device Tree Overlay 時，請確認以下項目：

- [ ] **SPI 匯流排**
  - [ ] 選擇可用的 SPI 實例（如 `&spi21`）
  - [ ] 啟用 `status = "okay"`
  - [ ] 定義 `cs-gpios`
  - [ ] 引用正確的 `pinctrl` 配置

- [ ] **SPI 腳位**
  - [ ] 在 `&pinctrl` 中定義 `spiXX_default` 和 `spiXX_sleep`
  - [ ] 設定 SCK、MOSI、MISO 腳位
  - [ ] 確認腳位與硬體設計一致

- [ ] **DM9051 節點**
  - [ ] `compatible = "davicom,dm9051"`
  - [ ] `reg` 值對應 CS 編號
  - [ ] `spi-max-frequency` 在合理範圍（8~40MHz）
  - [ ] `status = "okay"`

- [ ] **中斷配置（若使用）**
  - [ ] 定義 `int-gpios`
  - [ ] 啟用對應的 GPIO 控制器
  - [ ] 啟用對應的 GPIOTE 實例

- [ ] **重置配置（若使用）**
  - [ ] 定義 `reset-gpios`
  - [ ] 啟用對應的 GPIO 控制器

- [ ] **MAC 位址（可選）**
  - [ ] 設定 `local-mac-address` 或使用預設值 `[00 00 00 00 00 00]`

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
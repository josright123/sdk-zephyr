# Zephyr DM9051 驅動快速指南

## 系統版本環境：Zephyr

*** Using Zephyr OS v4.1.99-5f4c874a5ee8 ***
# 一 ，核心檔案清單

| 類型              | 檔案路徑                                                                                                                                                    | 說明                           |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------- |
| **驅動**          | `zephyr/drivers/ethernet/eth_dm9051.c`                                                                                                                  | 驅動主程式                        |
|                 | `zephyr/drivers/ethernet/eth_dm9051_priv.h`                                                                                                             | 暫存器定義與內部結構(with MBNDRY_WORD) |
| **建置**          | `zephyr/drivers/ethernet/CMakeLists.txt`                                                                                                                | 建置腳本                         |
| **Kconfig**     | `zephyr/drivers/ethernet/Kconfig`                                                                                                                       | Kconfig 文檔                   |
|                 | `zephyr/drivers/ethernet/Kconfig.dm9051`                                                                                                                | 配置選項                         |
| **Device Tree** | `zephyr/dts/bindings/ethernet/davicom,dm9051.yaml`                                                                                                      | 硬體綁定定義                       |
| **範例**          | `samples/net/dhcpv4_client/prj.conf`<br>`samples/net/dhcpv4_client/overlay_dm9051.conf`<br>`samples/net/dhcpv4_client/boards/overlay_nrf54l15.conf`<br> | 應用配置範例<br>軟件功能選擇             |
| **範例**          | `samples/net/dhcpv4_client/overlay_dm9051.overlay`<br>`samples/net/dhcpv4_client/boards/overlay_nrf54l15.overlay`                                       | 應用配置範例<br>硬件佈局選擇             |

# 二 ，移植Zephyr dm9051驅動
## **在 CMakeLists.txt 引用 dm9051驅動源碼編譯**
```cmake
zephyr_library_sources_ifdef(CONFIG_ETH_DM9051		eth_dm9051.c)
```

## **在 Kconfig 疊加配置 dm9051選項選單**
```cmake
source "drivers/ethernet/Kconfig.dm9051"
```
## Kconfig.dm9051 配置Config腳本軟件預定選項選單

```kconfig
CONFIG_ETH_DM9051=y                             # 啟用驅動(腳本選用選項)
CONFIG_ETH_DM9051_DEBUG_PRINTS=n                # 封包處理超時(預設)
CONFIG_ETH_DM9051_RX_THREAD_STACK_SIZE=800      # RX 執行緒堆疊（預設）
CONFIG_ETH_DM9051_RX_THREAD_PRIO=2              # RX 執行緒優先權（預設）
```

## **Davicom dm9051 源碼**
```cmake
"drivers/ethernet/eth_dm9051.c"
```
## **Davicom dm9051 源碼表頭檔**
```cmake
"drivers/ethernet/eth_dm9051_priv.h"
```
## **Davicom dm9051 設備樹yaml定義**
```cmake
"dts/bindings/ethernet/davicom,dm9051.yaml"
```

# 三 ，系統應用加載dm9051驅動
## Device Tree 硬件屬性

### 裝置版本環境：

Device tree 的描述制定,需依據目標處理器硬件的Device Tree定義施行,本文以Nordic Semiconductor的nRF54L15為標的說明,且使用Nordic的ncs v3.1.0,其版本訊息如下:

*** Booting nRF Connect SDK v3.1.0-a7a6d338252f ***

### GPIO 與 GPIOTE 配置

**GPIO 與 GPIOTE 節點屬性：**

| 節點            | 屬性       | 必要  | 範例       | 說明                                       |
| ------------- | -------- | --- | -------- | ---------------------------------------- |
| **&gpio0**    | `status` | ✗   | `"okay"` | 啟用 GPIO Port 0（需使用int-gpios中斷時必須使用）      |
| **&gpio1**    | `status` | ✓   | `"okay"` | 啟用 GPIO Port 1（用於 SPI CS、CLK、MO、MI等通訊腳位） |
| **&gpio2**    | `status` | ✗   | `"okay"` | 啟用 GPIO Port 2（需使用 reset-gpios 時必須啟用）    |
| **&gpiote20** | `status` | ✓   | `"okay"` | 啟用 GPIOTE 實例 20（gpio1 專用，必須啟用）           |
| **&gpiote30** | `status` | ✗   | `"okay"` | 啟用 GPIOTE 實例 30（gpio0 專用，使用中斷時必須啟用）      |

**nRF54L15 GPIO 與 GPIOTE 對應關係（根據硬體定義）：**

| GPIO Port | GPIOTE Instance | 說明                         |
| --------- | --------------- | -------------------------- |
| **gpio0** | **gpiote30**    | GPIO Port 0 使用 GPIOTE30    |
| **gpio1** | **gpiote20**    | GPIO Port 1 使用 GPIOTE20    |
| **gpio2** | 無對應             | GPIO Port 2 無 GPIOTE 支援（僅輸出） |

**說明：**
- **GPIO**：GPIO Port（如 gpio0，gpio1）負責啟用 GPIO 訊號
- **GPIOTE**：GPIO Task and Event 負責處理 GPIO 的中斷和事件
  - **SPI 使用 gpio1**（P1.0-P1.3）→ 必須啟用 **gpiote20**
  - **中斷使用 gpio0.3**（P0.3）→ 必須啟用 **gpiote30**
  - **重置使用 gpio2.0**（P2.0）→ 無需 GPIOTE（僅輸出控制）
- **Polling 模式**：即使不使用中斷功能，SPI 仍需啟用 gpio1 和 gpiote20

**配置範例：**
```dts
/* SPI 使用 gpio1，必須啟用 gpiote20 */
&gpio1 {
    status = "okay";
};

&gpiote20 {
    status = "okay";    /* gpio1 專用 GPIOTE */
};

/* 中斷使用 gpio0.3，必須啟用 gpiote30 */
&gpio0 {
    status = "okay";
};

&gpiote30 {
    status = "okay";    /* gpio0 專用 GPIOTE */
};

/* 重置使用 gpio2.0，無需 GPIOTE */
&gpio2 {
    status = "okay";    /* 僅需啟用 GPIO Port，無需 GPIOTE */
};
```

### SPI 引腳配置

**SPI引腳配置屬性（在 &pinctrl 節點中定義）：**

| 屬性                 | 必要  | 範例                                                                                                  | 說明                     |
| ------------------ | --- | --------------------------------------------------------------------------------------------------- | ---------------------- |
| `psels`            | ✓   | `<NRF_PSEL(SPIM_SCK, 1, 3)>,`<br>`<NRF_PSEL(SPIM_MOSI, 1, 1)>,`<br>`<NRF_PSEL(SPIM_MISO, 1, 0)>` | SPI 引腳選擇（SCK/MOSI/MISO） |
| `low-power-enable` | ✗   | （屬性存在即啟用）                                                                                           | 睡眠模式低功耗設定（僅用於 sleep 配置）  |

**SPI引腳配置範例（在 &pinctrl 節點中定義）：**

- **`spi21_default`**：正常工作模式的引腳配置，定義 SPI 通訊所需的 SCK、MOSI、MISO 三個引腳
- **`spi21_sleep`**：睡眠模式的引腳配置，使用相同引腳但啟用 `low-power-enable` 以降低功耗

```dts
&pinctrl {
    spi21_default: spi21_default {
        group1 {
            psels = <NRF_PSEL(SPIM_SCK, 1, 3)>,   /* SPI時鐘腳位 */
                    <NRF_PSEL(SPIM_MOSI, 1, 1)>,  /* MOSI 腳位 */
                    <NRF_PSEL(SPIM_MISO, 1, 0)>;  /* MISO 腳位 */
        };
    };
    spi21_sleep: spi21_sleep {
        group1 {
            psels = <NRF_PSEL(SPIM_SCK, 1, 3)>,
                    <NRF_PSEL(SPIM_MOSI, 1, 1)>,
                    <NRF_PSEL(SPIM_MISO, 1, 0)>;
            low-power-enable;                     /* 啟用低功耗模式 */
        };
    };
};
```

### SPI 總線配置與DM9051 設備節點

| 節點          | 屬性                  | 必要  | 範例                           | 說明                                               |
| ----------- | ------------------- | --- | ---------------------------- | ------------------------------------------------ |
| **&spi21**  | `status`            | ✓   | `"okay"`                     | 啟用 SPI 總線                                        |
|             | `clocks`            | ✗   | `<&hfpll>`                   | 時脈源選擇（選用，預設使用系統定義的時脈源）                           |
|             | `cs-gpios`          | ✓   | `<&gpio1 2 GPIO_ACTIVE_LOW>` | 片選腳位                                             |
|             | `pinctrl-0`         | ✓   | `<&spi21_default>`           | 預設引腳配置                                           |
|             | `pinctrl-1`         | ✗   | `<&spi21_sleep>`             | 睡眠引腳配置（省略則無低功耗）                                  |
|             | `pinctrl-names`     | ✓   | `"default", "sleep"`         | 引腳配置名稱列表                                         |
| **dm9051@0** | `compatible`        | ✓   | `"davicom,dm9051"`           | 節點固定值                                            |
|             | `reg`               | ✓   | `<0>`                        | SPI CS 編號                                        |
|             | `spi-max-frequency` | ✓   | `<16000000>`                 | SPI 時脈（8~40MHz）                                  |
|             | `int-gpios`         | ✗   | `<&gpio0 3 GPIO_ACTIVE_LOW>` | 中斷腳位（省略則 Polling）                                |
|             | `reset-gpios`       | ✗   | `<&gpio2 0 GPIO_ACTIVE_LOW>` | 重置腳位（省略則無硬體重置）                                   |
|             | `local-mac-address` | ✗   | `[00 60 6E 12 34 56]`        | MAC 位址（預設全零）                                     |

**SPI 時脈源說明（`clocks` 屬性）：**

- **`clocks = <&hfpll>`**：指定 SPI 使用 HFPLL（High Frequency PLL，128MHz）作為時脈源
- **選用性質**：此屬性為**選用**，可省略
  - **未指定時**：SPI 驅動使用系統預設的時脈源
  - **指定時**：覆寫預設時脈源，使用指定的 HFPLL
- **nRF54L15 可用時脈源**：
  - `<&hfpll>`：128MHz 高頻 PLL
  - `<&hfxo>`：32MHz 高頻振盪器
- **何時需要**：
  - 需要特定時脈源以達成高速 SPI 傳輸
  - 電源管理優化（選擇低功耗時脈源）
  - **一般應用**：可省略此屬性，使用預設值即可

## 快速硬件配置範例

**中斷與硬體重置模式**：
    
    - 配置腳位 → 使能
        
    - 不配置腳位 → 停用
### 中斷模式（含硬體重置）
(透過配置腳位來使能)

```dts
&gpio1 { status = "okay"; };
&gpiote20 { status = "okay"; };

&gpio0 { status = "okay"; };
&gpiote30 { status = "okay"; };

&gpio2 { status = "okay"; };

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

    dm9051@0 {
        compatible = "davicom,dm9051";
        reg = <0>;
        spi-max-frequency = <16000000>;
        int-gpios = <&gpio0 3 GPIO_ACTIVE_LOW>;
        reset-gpios = <&gpio2 0 GPIO_ACTIVE_LOW>;
        local-mac-address = [00 60 6E 12 34 56];
    };
};
```

### Polling 模式（無中斷、無重置）
(透過不配置腳位來停用)

```dts
&gpio1 { status = "okay"; };
&gpiote20 { status = "okay"; };

&pinctrl {
    spi21_default: spi21_default {
        group1 {
            psels = <NRF_PSEL(SPIM_SCK, 1, 3)>,
                    <NRF_PSEL(SPIM_MOSI, 1, 1)>,
                    <NRF_PSEL(SPIM_MISO, 1, 0)>;
        };
    };
};

&spi21 {
    status = "okay";
    cs-gpios = <&gpio0 10 GPIO_ACTIVE_LOW>;
    pinctrl-0 = <&spi21_default>;
    pinctrl-names = "default";

    dm9051@0 {
        compatible = "davicom,dm9051";
        reg = <0>;
        spi-max-frequency = <8000000>;
        local-mac-address = [00 00 00 00 00 00];
    };
};
```

# 四 ，啟用步驟軟件區塊

案例資訊:  samples/net/dhcpv4_client/

**1. 在應用程式添加套用的 overlay-dm9051.conf 直接直觀方式設定啟用驅動選項**
```
CONFIG_ETH_DM9051=y
CONFIG_SPI=y
CONFIG_GPIO=y
```

**2. 創建 Device Tree Overlay**
```dts
/* 參考上方範例配置, 以配置SPI介面通訊 */
```

**3. 編譯並燒錄**
```bash
west build -b nrf54l15dk/nrf54l15/cpuapp
west flash
```

# 五 ，重要提醒

- **SPI 頻率**：建議從 8MHz 開始測試，穩定後可提升至 16~40MHz
- **中斷 vs Polling**：添加 `int-gpios`為中斷，移除即自動切換為 Polling 模式
- **RESET Pluse**：添加 `reset-gpios` 即自動加上開機reset訊號波
- **MAC 位址**：`[00 00 00 00 00 00]` 會使用晶片預設值

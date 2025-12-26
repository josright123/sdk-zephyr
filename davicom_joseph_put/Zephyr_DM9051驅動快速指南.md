# Zephyr DM9051 驅動快速指南

## 系統版本環境：Zephyr

*** Using Zephyr OS v4.1.99-5f4c874a5ee8 ***
## 核心檔案清單

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

## Kconfig.dm9051 配置Config腳本軟件預定選項選單

```kconfig
CONFIG_ETH_DM9051=y                             # 啟用驅動(腳本選用選項)
CONFIG_ETH_DM9051_DEBUG_PRINTS=n                # 封包處理超時(預設)
CONFIG_ETH_DM9051_RX_THREAD_STACK_SIZE=800      # RX 執行緒堆疊（預設）
CONFIG_ETH_DM9051_RX_THREAD_PRIO=2              # RX 執行緒優先權（預設）
```

## Device Tree 硬件屬性

### SPI 總線配置

| 屬性              | 必要  | 範例                            | 說明              |
| --------------- | --- | ----------------------------- | --------------- |
| `status`        | ✓   | `"okay"`                      | 啟用 SPI 總線       |
| `cs-gpios`      | ✓   | `<&gpio0 10 GPIO_ACTIVE_LOW>` | 片選腳位            |
| `pinctrl-0`     | ✓   | `<&spi21_default>`            | 預設引腳配置          |
| `pinctrl-1`     | ✗   | `<&spi21_sleep>`              | 睡眠引腳配置（省略則無低功耗） |
| `pinctrl-names` | ✓   | `"default", "sleep"`          | 引腳配置名稱列表        |

**引腳配置屬性（在 &pinctrl 節點中定義）：**

| 屬性                 | 必要  | 範例                           | 說明                      |
| ------------------ | --- | ---------------------------- | ----------------------- |
| `psels`            | ✓   | `<NRF_PSEL(SPIM_SCK, 1, 3)>` | SPI 引腳選擇（SCK/MOSI/MISO） |
| `low-power-enable` | ✗   | （屬性存在即啟用）                    | 睡眠模式低功耗設定（僅用於 sleep 配置）  |

**引腳配置範例（在 &pinctrl 節點中定義）：**
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

### DM9051 設備節點

| 屬性                  | 必要  | 範例                           | 說明                |
| ------------------- | --- | ---------------------------- | ----------------- |
| `compatible`        | ✓   | `"davicom,dm9051"`           | 節點固定值             |
| `reg`               | ✓   | `<0>`                        | SPI CS 編號         |
| `spi-max-frequency` | ✓   | `<16000000>`                 | SPI 時脈（8~40MHz）   |
| `int-gpios`         | ✗   | `<&gpio0 3 GPIO_ACTIVE_LOW>` | 中斷腳位（省略則 Polling） |
| `reset-gpios`       | ✗   | `<&gpio2 0 GPIO_ACTIVE_LOW>` | 重置腳位（省略則無硬體重置）    |
| `local-mac-address` | ✗   | `[00 60 6E 12 34 56]`        | MAC 位址（預設全零）      |

## 快速硬件配置範例

**中斷與硬體重置模式，皆透過配置腳位（或不配置）來使能（或停用）：**
**中斷與上電硬體重置模式**：
    
    - 配置腳位 → 使能
        
    - 不配置腳位 → 停用
### 中斷模式（含硬體重置）

```dts
&gpio0 { status = "okay"; };
&gpio1 { status = "okay"; };
&gpio2 { status = "okay"; };
&gpiote20 { status = "okay"; };
&gpiote30 { status = "okay"; };

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

```dts
&gpio0 { status = "okay"; };
&gpio1 { status = "okay"; };

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

## 啟用步驟軟件區塊

**1. 在 prj.conf 設定啟用驅動選項**
```
CONFIG_ETH_DM9051=y
CONFIG_SPI=y
CONFIG_GPIO=y
```

**2. 創建 Device Tree Overlay**
```dts
/* 參考上方範例配置 */
```

**3. 在 CMakeLists.txt 引用 dm90驅動源碼**
```cmake
zephyr_library_sources_ifdef(CONFIG_ETH_DM9051		eth_dm9051.c)
```

**4. 編譯並燒錄**
```bash
west build -b nrf54l15dk/nrf54l15/cpuapp
west flash
```

## 重要提醒

- **SPI 頻率**：建議從 8MHz 開始測試，穩定後可提升至 16~40MHz
- **MAC 位址**：`[00 00 00 00 00 00]` 會使用晶片預設值
- **Polling vs 中斷**：移除 `int-gpios` 即自動切換為 Polling 模式
- **RESET Pluse**：添加 `reset-gpios` 即自動加上開機reset訊號波

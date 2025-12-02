# Zephyr DM9051 驅動配置清單

本文件列出 DHCPv4 Client 專案配置 DM9051 Ethernet 驅動並實現網路功能所需的所有配置檔案。

## 📋 目錄

- [核心配置檔案](#核心配置檔案)
- [驅動程式配置](#驅動程式配置)
- [應用程式檔案](#應用程式檔案)
- [關鍵配置參數](#關鍵配置參數)
- [建置指令](#建置指令)

---

## 核心配置檔案

### 1. 專案主配置檔案

#### [prj.conf](file:///c:/ncs/v3.1.0/zephyr/samples/net/dhcpv4_client/prj.conf)
專案的主要配置檔案，定義所有必要的網路和驅動選項。

**關鍵配置項目：**
```conf
# 網路功能
CONFIG_NETWORKING=y
CONFIG_NET_IPV4=y
CONFIG_NET_IPV6=n
CONFIG_NET_ARP=y
CONFIG_NET_UDP=y
CONFIG_NET_DHCPV4=y
CONFIG_NET_DHCPV4_OPTION_CALLBACKS=y

# Ethernet L2 層
CONFIG_NET_L2_ETHERNET=y

# DM9051 驅動
CONFIG_ETH_DM9051=y

# SPI 支援
CONFIG_SPI=y

# 日誌和除錯
CONFIG_LOG=y
CONFIG_NET_LOG=y
CONFIG_PRINTK=y
```

---

### 2. DM9051 專用配置

#### [overlay-dm9051.conf](file:///c:/ncs/v3.1.0/zephyr/samples/net/dhcpv4_client/overlay-dm9051.conf)
DM9051 驅動的額外配置選項。

**內容：**
```conf
# DM9051 Ethernet driver configuration
CONFIG_NET_L2_ETHERNET=y

# SPI support (required for DM9051 which uses SPI)
CONFIG_SPI=y

# Optional: Enable debug logging for Ethernet
#CONFIG_ETHERNET_LOG_LEVEL_DBG=y
```

#### [overlay-dm9051.overlay](file:///c:/ncs/v3.1.0/zephyr/samples/net/dhcpv4_client/overlay-dm9051.overlay)
DM9051 的 Device Tree 覆蓋檔案（通用版本）。

**內容：**
```dts
&spi00 {
	status = "okay";
	
	dm9051: dm9051@0 {
		compatible = "davicom,dm9051";
		reg = <0>;
		spi-max-frequency = <20000000>;
		int-gpios = <&gpio0 5 1>;
	};
};

&gpio0 {
	status = "okay";
};
```

---

### 3. 開發板專用配置 (nRF54L15DK)

#### [boards/nrf54l15dk_nrf54l15_cpuapp.conf](file:///c:/ncs/v3.1.0/zephyr/samples/net/dhcpv4_client/boards/nrf54l15dk_nrf54l15_cpuapp.conf)
nRF54L15DK 開發板的特定配置。

**內容：**
```conf
# Disable the unsupported UART0 driver
CONFIG_NRFX_UARTE0=n

# Enable UART console for logging output
CONFIG_UART_CONSOLE=y
CONFIG_LOG_BACKEND_UART=y

# ENC28J60 Ethernet driver configuration
CONFIG_NET_L2_ETHERNET=y

# SPI support (required for ENC28J60 which uses SPI)
CONFIG_SPI=y
```

#### [boards/nrf54l15dk_nrf54l15_cpuapp.overlay](file:///c:/ncs/v3.1.0/zephyr/samples/net/dhcpv4_client/boards/nrf54l15dk_nrf54l15_cpuapp.overlay)
nRF54L15DK 開發板的完整 Device Tree 配置。

**硬體連接：**
- **SCK**: P1.12 - SPI 時鐘
- **MOSI**: P1.1 - SPI 主出從入
- **MISO**: P1.0 - SPI 主入從出
- **CS**: P1.2 - 晶片選擇
- **INT**: P0.3 - 中斷腳位

**配置內容：**
```dts
/* Ensure GPIO1 and its GPIOTE instance are enabled */
&gpio1 {
	status = "okay";
};

&gpiote20 {
	status = "okay";
};

&pinctrl {
	spi21_default: spi21_default {
		group1 {
			psels = <NRF_PSEL(SPIM_SCK, 1, 12)>,
				<NRF_PSEL(SPIM_MOSI, 1, 1)>,
				<NRF_PSEL(SPIM_MISO, 1, 0)>;
		};
	};

	spi21_sleep: spi21_sleep {
		group1 {
			psels = <NRF_PSEL(SPIM_SCK, 1, 12)>,
				<NRF_PSEL(SPIM_MOSI, 1, 1)>,
				<NRF_PSEL(SPIM_MISO, 1, 0)>;
			low-power-enable;
		};
	};
};

&spi21 {
	status = "okay";
	clocks = <&hfpll>;
	cs-gpios = <&gpio1 2 GPIO_ACTIVE_LOW>;
	pinctrl-0 = <&spi21_default>;
	pinctrl-1 = <&spi21_sleep>;
	pinctrl-names = "default", "sleep";
	
	dm9051: dm9051@0 {
		compatible = "davicom,dm9051";
		reg = <0>;
		spi-max-frequency = <1000000>;  /* 1MHz - minimum for debugging */
		int-gpios = <&gpio0 3 GPIO_ACTIVE_LOW>;
		
		/* Local MAC address */
		local-mac-address = [00 11 22 33 44 55];
	};
};
```

> [!NOTE]
> 使用 SPI21 而非 SPI20，因為 SPI20 與 UART20 共用硬體實例。SPI00 已被外部快閃記憶體使用。

---

## 驅動程式配置

### 4. DM9051 驅動 Kconfig

#### [drivers/ethernet/Kconfig.dm9051](file:///c:/ncs/v3.1.0/zephyr/drivers/ethernet/Kconfig.dm9051)
DM9051 驅動的 Kconfig 配置選項。

**可配置參數：**

| 配置選項 | 預設值 | 說明 |
|---------|--------|------|
| `CONFIG_ETH_DM9051_RX_THREAD_STACK_SIZE` | 800 | RX 執行緒堆疊大小（bytes） |
| `CONFIG_ETH_DM9051_RX_THREAD_PRIO` | 2 | RX 執行緒優先級 |
| `CONFIG_ETH_DM9051_CLKRDY_INIT_WAIT_MS` | 2 | 時鐘就緒等待時間（ms） |
| `CONFIG_ETH_DM9051_TIMEOUT` | 100 | IP 緩衝區超時時間（ms） |

---

### 5. DM9051 驅動程式碼

#### [drivers/ethernet/eth_dm9051.c](file:///c:/ncs/v3.1.0/zephyr/drivers/ethernet/eth_dm9051.c)
DM9051 驅動的主要實作檔案。

**主要功能：**
- SPI 通訊介面
- 裝置初始化和重置
- MAC 位址配置
- PHY 設定
- 封包傳送和接收
- 中斷處理

#### [drivers/ethernet/eth_dm9051_priv.h](file:///c:/ncs/v3.1.0/zephyr/drivers/ethernet/eth_dm9051_priv.h)
DM9051 驅動的私有標頭檔。

**定義內容：**
- DM9051 暫存器位址定義
- 控制位元定義
- 配置結構 `dm9051_config`
- 執行時結構 `dm9051_runtime`
- PHY 暫存器定義
- SPI 操作碼定義

---

## 應用程式檔案

### 6. DHCPv4 客戶端應用程式

#### [samples/net/dhcpv4_client/src/main.c](file:///c:/ncs/v3.1.0/zephyr/samples/net/dhcpv4_client/src/main.c)
DHCPv4 客戶端的主程式。

**主要功能：**
- 初始化網路管理事件回呼
- 啟動 DHCPv4 客戶端
- 處理 IP 位址獲取事件
- 顯示網路配置資訊（IP、子網路遮罩、閘道）

#### [samples/net/dhcpv4_client/CMakeLists.txt](file:///c:/ncs/v3.1.0/zephyr/samples/net/dhcpv4_client/CMakeLists.txt)
CMake 建置腳本。

---

## 關鍵配置參數

### SPI 配置

| 參數 | 值 | 說明 |
|------|-----|------|
| SPI 介面 | SPI21 | 避免與 UART20 衝突 |
| SPI 頻率 | 1MHz | 除錯用最小頻率，穩定後可提升至 4-8MHz |
| 時鐘源 | HFPLL | 高頻 PLL |

### GPIO 腳位配置

| 訊號 | 腳位 | 方向 | 說明 |
|------|------|------|------|
| SCK | P1.12 | 輸出 | SPI 時鐘 |
| MOSI | P1.1 | 輸出 | 主出從入 |
| MISO | P1.0 | 輸入 | 主入從出 |
| CS | P1.2 | 輸出 | 晶片選擇（低電位有效） |
| INT | P0.3 | 輸入 | 中斷（低電位有效） |

### 網路配置

| 參數 | 值 | 說明 |
|------|-----|------|
| MAC 位址 | 00:11:22:33:44:55 | 本地 MAC 位址 |
| IP 配置 | DHCP | 動態 IP 位址獲取 |
| 協定 | IPv4 | 僅支援 IPv4 |

### 執行緒配置

| 參數 | 值 | 說明 |
|------|-----|------|
| RX 執行緒堆疊 | 800 bytes | 接收封包處理執行緒 |
| RX 執行緒優先級 | 2 | 較高優先級確保即時處理 |

---

## 建置指令

### 標準建置（使用 west）

```bash
# 使用 nRF54L15DK 開發板建置
west build -b nrf54l15dk/nrf54l15/cpuapp samples/net/dhcpv4_client

# 清除並重新建置
west build -b nrf54l15dk/nrf54l15/cpuapp samples/net/dhcpv4_client --pristine
```

### 燒錄到開發板

```bash
west flash
```

### 查看日誌輸出

```bash
# 使用 minicom 或其他串列埠工具
minicom -D /dev/ttyACM0 -b 115200

# 或使用 screen
screen /dev/ttyACM0 115200
```

---

## 檔案結構總覽

```
zephyr/
├── samples/net/dhcpv4_client/
│   ├── CMakeLists.txt                          # 建置腳本
│   ├── prj.conf                                # 主配置檔案
│   ├── overlay-dm9051.conf                     # DM9051 配置
│   ├── overlay-dm9051.overlay                  # DM9051 Device Tree
│   ├── boards/
│   │   ├── nrf54l15dk_nrf54l15_cpuapp.conf    # 開發板配置
│   │   └── nrf54l15dk_nrf54l15_cpuapp.overlay # 開發板 Device Tree
│   └── src/
│       └── main.c                              # 應用程式主程式
│
└── drivers/ethernet/
    ├── Kconfig.dm9051                          # DM9051 Kconfig
    ├── eth_dm9051.c                            # DM9051 驅動實作
    └── eth_dm9051_priv.h                       # DM9051 私有標頭檔
```

---

## 除錯建議

### 啟用除錯日誌

在 `prj.conf` 或 `overlay-dm9051.conf` 中新增：

```conf
# Ethernet 驅動除錯
CONFIG_ETHERNET_LOG_LEVEL_DBG=y

# SPI 驅動除錯
CONFIG_SPI_LOG_LEVEL_DBG=y

# 網路除錯
CONFIG_NET_LOG_LEVEL_DBG=y
```

### 常見問題排查

1. **SPI 無訊號**
   - 檢查 GPIO 和 GPIOTE 是否啟用
   - 確認 pinctrl 配置正確
   - 驗證 SPI 頻率設定

2. **無法讀取晶片 ID**
   - 降低 SPI 頻率至 1MHz
   - 檢查 CS 腳位配置
   - 確認電源供應穩定

3. **DHCP 無法獲取 IP**
   - 確認網路線連接
   - 檢查 PHY 連線狀態
   - 驗證 MAC 位址配置

---

## 參考資源

- [DM9051 Datasheet](https://www.davicom.com.tw/userfile/24106/DM9051-DS-F01-102415.pdf)
- [Zephyr Networking Documentation](https://docs.zephyrproject.org/latest/connectivity/networking/index.html)
- [nRF54L15DK User Guide](https://docs.nordicsemi.com/bundle/ug_nrf54l15_dk/page/UG/nrf54L15_DK/intro.html)

---

**文件版本：** 1.0  
**最後更新：** 2025-12-02  
**適用平台：** nRF54L15DK + DM9051 Ethernet Controller

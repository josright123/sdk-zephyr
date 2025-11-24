# Zephyr ENC28J60 SPI 乙太網配置指南

## 專案概述

本專案實現了使用 Zephyr RTOS 通過 SPI 介面連接 ENC28J60 乙太網控制器，建立網路連接並支援 DHCPv4 客戶端功能。專案包含兩個主要元件：

1. **main.c** - DHCPv4 客戶端應用程式，負責初始化網路並獲取 IP 地址
2. **eth_enc28j60.c** - ENC28J60 SPI 乙太網驅動程式，負責硬體層面的通訊

---

## 檔案結構說明

```
zephyr/
├── samples/net/dhcpv4_client/
│   ├── src/
│   │   └── main.c                    # 主應用程式
│   ├── prj.conf                      # 專案配置檔案
│   ├── CMakeLists.txt               # CMake 構建檔案
│   └── overlay-enc28j60.overlay     # 設備樹覆蓋檔案（需創建）
└── drivers/ethernet/
    ├── eth_enc28j60.c               # ENC28J60 驅動程式實作
    └── eth_enc28j60_priv.h          # 驅動程式私有標頭檔案
```

---

## 一、主應用程式 (main.c) 說明

### 檔案位置
`samples/net/dhcpv4_client/src/main.c`

### 程式碼分析 (第86-105行)

```c
int main(void)
{
    LOG_INF("Run dhcpv4 client");
    
    // 初始化網路管理事件回調函數
    // 監聽 IPv4 地址添加事件（NET_EVENT_IPV4_ADDR_ADD）
    net_mgmt_init_event_callback(&mgmt_cb, handler,
                                 NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&mgmt_cb);
    
    // 初始化 DHCP 選項回調函數
    // 處理 DHCP 選項 42（NTP 伺服器地址）
    net_dhcpv4_init_option_callback(&dhcp_cb, option_handler,
                                    DHCP_OPTION_NTP, ntp_server,
                                    sizeof(ntp_server));
    net_dhcpv4_add_option_callback(&dhcp_cb);
    
    // 遍歷所有網路介面並啟動 DHCPv4 客戶端
    net_if_foreach(start_dhcpv4_client, NULL);
    return 0;
}
```

### 主要功能

1. **事件處理** (`handler` 函數，第40-73行)
   - 監聽 DHCP 獲得的 IP 地址事件
   - 輸出獲得的 IP 地址、子網掩碼、路由器地址和租約時間

2. **DHCP 選項處理** (`option_handler` 函數，第75-84行)
   - 處理自定義 DHCP 選項（如 NTP 伺服器地址）

3. **啟動 DHCP 客戶端** (`start_dhcpv4_client` 函數，第31-38行)
   - 在每個可用的網路介面上啟動 DHCPv4 客戶端

---

## 二、ENC28J60 驅動程式 (eth_enc28j60.c) 說明

### 檔案位置
`drivers/ethernet/eth_enc28j60.c`

### 核心功能模組

#### 1. SPI 通訊函數
- `eth_enc28j60_soft_reset()` - 軟體重置 ENC28J60
- `eth_enc28j60_write_reg()` - 寫入暫存器
- `eth_enc28j60_read_reg()` - 讀取暫存器
- `eth_enc28j60_write_mem()` - 寫入緩衝區記憶體
- `eth_enc28j60_read_mem()` - 讀取緩衝區記憶體

#### 2. PHY 層配置
- `eth_enc28j60_write_phy()` - 寫入 PHY 暫存器
- `eth_enc28j60_read_phy()` - 讀取 PHY 暫存器
- `eth_enc28j60_init_phy()` - 初始化 PHY（設定全雙工/半雙工）

#### 3. MAC 層配置
- `eth_enc28j60_init_mac()` - 初始化 MAC 地址和 MAC 暫存器
- `eth_enc28j60_init_buffers()` - 初始化接收和發送緩衝區

#### 4. 數據收發
- `eth_enc28j60_tx()` - 發送以太網幀
- `eth_enc28j60_rx()` - 接收以太網幀
- `eth_enc28j60_rx_thread()` - 接收執行緒（中斷驅動）

#### 5. 初始化流程 (`eth_enc28j60_init`)
1. 檢查 SPI 和 GPIO 是否就緒
2. 配置中斷 GPIO
3. 執行軟體重置
4. 初始化緩衝區
5. 初始化 MAC 和 PHY
6. 啟用中斷和接收功能
7. 創建接收執行緒

---

## 三、配置檔案

### 1. prj.conf - 專案配置檔案

**檔案位置**: `samples/net/dhcpv4_client/prj.conf`

```conf
# 基本網路配置
CONFIG_NETWORKING=y              # 啟用網路功能
CONFIG_NET_IPV6=n                # 禁用 IPv6
CONFIG_NET_IPV4=y                # 啟用 IPv4
CONFIG_NET_ARP=y                 # 啟用 ARP 協議
CONFIG_NET_UDP=y                 # 啟用 UDP 協議

# DHCPv4 客戶端配置
CONFIG_NET_DHCPV4=y              # 啟用 DHCPv4 客戶端
CONFIG_NET_DHCPV4_OPTION_CALLBACKS=y  # 啟用 DHCP 選項回調

# DNS 解析
CONFIG_DNS_RESOLVER=y            # 啟用 DNS 解析器

# 隨機數生成器（用於 MAC 地址生成）
CONFIG_TEST_RANDOM_GENERATOR=y

# 堆疊監控
CONFIG_INIT_STACKS=y             # 啟用堆疊初始化監控

# 網路管理
CONFIG_NET_MGMT=y                # 啟用網路管理
CONFIG_NET_MGMT_EVENT=y          # 啟用網路管理事件

# 日誌配置
CONFIG_NET_LOG=y                 # 啟用網路日誌
CONFIG_LOG=y                     # 啟用日誌系統

# 網路 Shell
CONFIG_NET_SHELL=y               # 啟用網路 Shell（可選，用於調試）

# ENC28J60 驅動程式配置（自動啟用，當設備樹中定義了 ENC28J60 時）
# 如需自定義，可在 prj.conf 中覆蓋以下默認值：
# CONFIG_ETH_ENC28J60_RX_THREAD_STACK_SIZE=800      # 接收執行緒堆疊大小（字節）
# CONFIG_ETH_ENC28J60_RX_THREAD_PRIO=2              # 接收執行緒優先級
# CONFIG_ETH_ENC28J60_CLKRDY_INIT_WAIT_MS=2         # 時鐘就緒等待時間（毫秒）
# CONFIG_ETH_ENC28J60_TIMEOUT=100                   # IP 緩衝區超時時間（毫秒）

# SPI 配置（如果未在設備樹中配置）
CONFIG_SPI=y                     # 啟用 SPI 驅動程式

# GPIO 配置
CONFIG_GPIO=y                    # 啟用 GPIO 驅動程式

# 時鐘配置（Nordic 平台示例）
CONFIG_CLOCK_CONTROL_NRF_K32SRC_RC=y
```

### 2. overlay-enc28j60.overlay - 設備樹覆蓋檔案

**檔案位置**: `samples/net/dhcpv4_client/overlay-enc28j60.overlay`（需創建）

```dts
/dts-v1/;
/plugin/;

#include <dt-bindings/gpio/gpio.h>

/ {
    compatible = "your,board-compatible-string";
};

&spi0 {
    /* 根據您的硬體配置選擇正確的 SPI 控制器 */
    status = "okay";
    
    enc28j60: enc28j60@0 {
        compatible = "microchip,enc28j60";
        reg = <0>;  /* SPI CS 線編號 */
        spi-max-frequency = <10000000>;  /* SPI 頻率，最高 20MHz */
        
        /* 中斷 GPIO 配置 */
        int-gpios = <&gpio0 5 GPIO_ACTIVE_LOW>;  /* 根據硬體連接修改 */
        
        /* 可選配置 */
        full-duplex;  /* 啟用全雙工模式（可選） */
        
        /* MAC 地址（可選，不設定則使用隨機地址） */
        local-mac-address = [00 04 A3 12 34 56];  /* 根據需要修改 */
        
        /* 硬體接收過濾器（可選，默認 0xA3） */
        /* hw-rx-filter = <0xA3>; */
    };
};

/* 如果使用 SPI CS 控制，可能需要配置 GPIO */
&gpio0 {
    status = "okay";
};
```

#### 設備樹配置說明

1. **SPI 控制器**
   - `&spi0` - 根據您的開發板選擇正確的 SPI 控制器（可能是 spi1, spi2 等）
   - `status = "okay"` - 啟用 SPI 控制器

2. **ENC28J60 設備節點**
   - `compatible = "microchip,enc28j60"` - 必須使用此字串以匹配驅動程式
   - `reg = <0>` - SPI 片選（CS）線編號
   - `spi-max-frequency` - SPI 通訊頻率，ENC28J60 最高支援 20MHz

3. **中斷 GPIO**
   - `int-gpios` - ENC28J60 的中斷引腳連接
   - 格式: `<&gpio控制器 GPIO引腳編號 GPIO電平>`
   - ENC28J60 中斷為低電平有效，使用 `GPIO_ACTIVE_LOW`

4. **可選配置**
   - `full-duplex` - 啟用全雙工模式（省略則為半雙工）
   - `local-mac-address` - 設定 MAC 地址（省略則使用隨機生成）
   - `hw-rx-filter` - 硬體接收過濾器配置（默認 0xA3，接受單播、多播、廣播）

### 3. CMakeLists.txt

**檔案位置**: `samples/net/dhcpv4_client/CMakeLists.txt`

```cmake
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dhcpv4_client)

# 編譯所有源檔案
FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
```

此檔案已存在，無需修改。

---

## 四、硬體連接

### ENC28J60 與 MCU 連接示意

```
ENC28J60                MCU
--------                --------
VCC      ------------   3.3V
GND      ------------   GND
SCK      ------------   SPI_SCK (SPI 時鐘)
SO       ------------   SPI_MISO (SPI 主輸入從輸出)
SI       ------------   SPI_MOSI (SPI 主輸出從輸入)
CS       ------------   SPI_CS (SPI 片選)
INT      ------------   GPIO_INT (中斷引腳)
RESET    ------------   (可選，可接 VCC 或通過軟體重置)
```

### 重要說明

1. **電源**: ENC28J60 需要 3.3V 電源
2. **SPI 通訊**: 使用標準 SPI 介面，最高頻率 20MHz
3. **中斷**: INT 腳為低電平有效，需配置為 `GPIO_ACTIVE_LOW`
4. **CS 片選**: 根據硬體連接選擇正確的 CS 線（通常是 SPI 控制器的第幾個 CS）

---

## 五、編譯和燒錄

### 編譯命令

```bash
# 進入專案目錄
cd samples/net/dhcpv4_client

# 編譯（替換 <board> 為您的開發板名稱，如 nrf52840dk_nrf52840）
west build -b <board> -- -DOVERLAY_CONFIG=overlay-enc28j60.overlay

# 或者使用 CMake 直接編譯
mkdir build && cd build
cmake -GNinja -DBOARD=<board> -DOVERLAY_CONFIG=../overlay-enc28j60.overlay ..
ninja
```

### 燒錄命令

```bash
# 使用 west 燒錄
west flash

# 或使用 openocd/jlink 等工具
```

---

## 六、執行和測試

### 1. 啟動應用程式

燒錄完成後，重啟開發板。應用程式會自動：
1. 初始化 ENC28J60 驅動程式
2. 初始化網路介面
3. 啟動 DHCPv4 客戶端
4. 獲取 IP 地址、子網掩碼、閘道地址

### 2. 查看日誌輸出

使用串口工具（如 minicom、PuTTY）連接開發板，設定波特率通常為 115200，您應該看到類似以下的輸出：

```
[00:00:00.000,000] <inf> eth_enc28j60: Initialized
[00:00:00.100,000] <inf> net_dhcpv4_client_sample: Run dhcpv4 client
[00:00:01.000,000] <inf> eth_enc28j60: Link up
[00:00:02.500,000] <inf> net_dhcpv4_client_sample:    Address[0]: 192.168.1.100
[00:00:02.500,000] <inf> net_dhcpv4_client_sample:     Subnet[0]: 255.255.255.0
[00:00:02.500,000] <inf> net_dhcpv4_client_sample:     Router[0]: 192.168.1.1
[00:00:02.500,000] <inf> net_dhcpv4_client_sample: Lease time[0]: 86400 seconds
```

### 3. 網路測試

#### 使用網路 Shell（如果啟用）

```bash
# 查看網路介面狀態
net iface

# 查看 IP 配置
net ipv4

# Ping 測試
net ping 192.168.1.1
```

#### 從外部電腦測試

```bash
# Ping 開發板 IP 地址（從 DHCP 獲得的地址）
ping 192.168.1.100

# 或者使用獲得的 MAC 地址進行 ARP 查詢
arp -a
```

---

## 七、常見問題排除

### 1. 驅動程式初始化失敗

**問題**: 日誌顯示 "SPI master port not ready" 或 "GPIO port not ready"

**解決方法**:
- 檢查設備樹中的 SPI 控制器和 GPIO 配置
- 確認 SPI 控制器在設備樹中已啟用（`status = "okay"`）
- 檢查 SPI 頻率是否超過 20MHz

### 2. 無法獲取 DHCP IP 地址

**問題**: 長時間無法獲得 IP 地址

**解決方法**:
- 檢查網路線是否正確連接
- 確認路由器或 DHCP 伺服器正常運作
- 檢查 MAC 地址是否合法（前三個字節應為廠商 OUI）
- 增加日誌級別以查看詳細資訊：在 `prj.conf` 中添加 `CONFIG_LOG_LEVEL_DBG=y`

### 3. 中斷無法觸發

**問題**: 無法接收網路封包

**解決方法**:
- 確認 INT GPIO 配置正確（應為 `GPIO_ACTIVE_LOW`）
- 檢查 INT 引腳是否正確連接到開發板
- 使用示波器或邏輯分析儀檢查中斷信號

### 4. SPI 通訊錯誤

**問題**: SPI 讀寫失敗

**解決方法**:
- 降低 SPI 頻率（例如改為 5MHz）
- 檢查 SPI 接線是否正確（SCK, MOSI, MISO, CS）
- 確認 CS 片選線配置正確
- 檢查電源供應是否穩定

### 5. 鏈路狀態異常

**問題**: 顯示 "Link down" 或無法建立鏈路

**解決方法**:
- 檢查網路線連接
- 確認網路交換機或路由器支援 10BASE-T（ENC28J60 僅支援 10Mbps）
- 檢查 PHY 初始化配置（全雙工/半雙工設定）

---

## 八、進階配置

### 1. 自定義 MAC 地址

在設備樹中明確指定 MAC 地址：

```dts
enc28j60@0 {
    compatible = "microchip,enc28j60";
    local-mac-address = [00 04 A3 12 34 56];
    /* ... */
};
```

### 2. 使用隨機 MAC 地址

在設備樹中添加：

```dts
enc28j60@0 {
    compatible = "microchip,enc28j60";
    zephyr,random-mac-address;
    /* ... */
};
```

### 3. 調整接收執行緒優先級

在 `prj.conf` 中：

```conf
CONFIG_ETH_ENC28J60_RX_THREAD_PRIO=5  # 提高優先級（數字越小優先級越高）
CONFIG_ETH_ENC28J60_RX_THREAD_STACK_SIZE=1024  # 增加堆疊大小
```

### 4. 啟用 VLAN 支援

在 `prj.conf` 中：

```conf
CONFIG_NET_VLAN=y
```

### 5. 調整超時時間

在 `prj.conf` 中：

```conf
CONFIG_ETH_ENC28J60_TIMEOUT=200  # 增加到 200ms
```

---

## 九、參考資料

### 相關檔案位置

1. **驅動程式實作**: `drivers/ethernet/eth_enc28j60.c`
2. **驅動程式標頭**: `drivers/ethernet/eth_enc28j60_priv.h`
3. **驅動程式配置**: `drivers/ethernet/Kconfig.enc28j60`
4. **設備樹綁定**: `dts/bindings/ethernet/microchip,enc28j60.yaml`
5. **主應用程式**: `samples/net/dhcpv4_client/src/main.c`

### ENC28J60 晶片資料

- ENC28J60 資料手冊: [Microchip ENC28J60 Datasheet](https://www.microchip.com/en-us/product/ENC28J60)

### Zephyr 文檔

- Zephyr 網路文檔: [Zephyr Networking Documentation](https://docs.zephyrproject.org/latest/connectivity/networking/index.html)
- Zephyr 設備樹文檔: [Zephyr Device Tree Documentation](https://docs.zephyrproject.org/latest/build/dts/index.html)

---

## 十、總結

本專案實現了完整的 SPI 介面乙太網功能，主要包括：

1. ✅ ENC28J60 驅動程式初始化和配置
2. ✅ SPI 通訊介面實現
3. ✅ 以太網數據收發
4. ✅ DHCPv4 客戶端功能
5. ✅ 網路事件監聽和處理

通過正確配置設備樹和專案配置檔案，您可以在 Zephyr RTOS 上建立穩定的網路連接。

---

**最後更新**: 2024年
**適用版本**: Zephyr RTOS 3.1.0+
**維護者**: Zephyr Project Contributors


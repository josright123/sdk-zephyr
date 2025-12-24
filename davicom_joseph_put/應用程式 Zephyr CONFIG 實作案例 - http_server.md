# Zephyr 應用程式 CONFIG 實作案例 DM9051 Networking Application -- http_server

## 前言：http_server應用的 .conf 套用關係
- Zephyr 構建時會依序合併：板級 `*_defconfig` → 應用層 `prj.conf` → 額外覆寫的 overlay 檔。
- 本範例的核心設定在 `prj.conf`，提供共用的網路/協定能力；若需硬體或情境差異，則以 overlay 疊加。
- 主要 overlay：`overlay_nrf54l15_dm9051.conf` 負責 nRF54L15 + DM9051 的網路驅動與底層緩衝配置。
- 建議在指令加入 `-DOVERLAY_CONFIG="overlay_nrf54l15_dm9051.conf"` 這類串列，確保 base (prj.conf) 先生效，再讓 overlay 逐一覆寫細節。
- 指令加入`-DOVERLAY_CONFIG="overlay_nrf54l15_dm9051.conf;overlay_furthermore.conf"`，可配置多個overlay配置，以分號分隔。

## 建置命令範例

```bash
# 使用基礎配置
west build -b nrf54l15dk/nrf54l15/cpuapp

# 使用 overlay 配置
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DOVERLAY_CONFIG="overlay_nrf54l15_dm9051.conf"

# 使用多個 overlay 配置（用分號分隔）
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DOVERLAY_CONFIG="overlay_nrf54l15_dm9051.conf;overlay_furthermore.conf";
```

### prj.conf

CONFIG_NET_TCP=y

CONFIG_NET_DHCPV4=y
CONFIG_NET_DHCPV4_OPTION_CALLBACKS=y
CONFIG_DNS_RESOLVER=y

CONFIG_HTTP_PARSER=y
CONFIG_HTTP_PARSER_URL=y

CONFIG_NET_CONNECTION_MANAGER=y
CONFIG_NET_SOCKETS=y

CONFIG_INIT_STACKS=y

CONFIG_NET_MGMT=y
CONFIG_NET_MGMT_EVENT=y

CONFIG_POSIX_API=y

#CONFIG_NET_LOG=y
CONFIG_LOG=y

CONFIG_NET_SHELL=y

#CONFIG_USE_SEGGER_RTT=n
#CONFIG_LOG_BACKEND_RTT=n

CONFIG_NET_MAX_CONN=16
CONFIG_HTTP_SERVER_SAMPLE_CLIENTS_MAX=2
CONFIG_HTTP_SERVER_SAMPLE_STACK_SIZE=8192

### overlay_nrf54l15_dm9051.conf

CONFIG_NETWORKING=y
CONFIG_NET_IPV6=n
CONFIG_NET_IPV4=y
CONFIG_NET_ARP=y
CONFIG_NET_UDP=y

CONFIG_NET_L2_ETHERNET=y
CONFIG_NET_IPV4_FRAGMENT=y
CONFIG_NET_BUF_DATA_SIZE=2048

CONFIG_ETH_DM9051=y
CONFIG_SPI=y
CONFIG_GPIO=y

CONFIG_LOG_BACKEND_UART=y
CONFIG_SHELL_LOG_BACKEND=n
CONFIG_CLOCK_CONTROL_NRF_K32SRC_RC=y

## 專案目錄結構圖

```
nrf/
├── samples/net/http_server/
│   ├── CMakeLists.txt              # 建置腳本
│   ├── prj.conf                    # 主配置檔案
│   ├── boards/
│   │   ├── nrf54l15dk_nrf54l15_cpuapp.conf      # 板級配置
│   │   └── nrf54l15dk_nrf54l15_cpuapp.overlay   # 板級 Device Tree
│   └── src/
│       └── main.c                  # 應用程式主程式

zephyr/
├── dts/bindings/ethernet/
│   └── davicom,dm9051.yaml
│
├── samples/net/dhcpv4_client/
│   ├── CMakeLists.txt              # 建置腳本
│   ├── prj.conf                    # 主配置檔案
│   ├── boards/
│   │   ├── nrf54l15dk_nrf54l15_cpuapp.conf      # 板級配置
│   │   └── nrf54l15dk_nrf54l15_cpuapp.overlay   # 板級 Device Tree
│   └── src/
│       └── main.c                  # 應用程式主程式
│
└── drivers/ethernet/
    ├── CMakeLists.txt              # 建置腳本
    ├── Kconfig                     # Kconfig
    ├── Kconfig.dm9051              # DM9051 Kconfig
    ├── eth_dm9051.c                # DM9051 驅動實作
    └── eth_dm9051_priv.h           # DM9051 私有標頭檔
```

# Zephyr 應用程式 CONFIG 實作案例 DM9051 Networking Application -- dhcpv4_client

## 系統版本環境：nRF and Zephyr

*** Booting nRF Connect SDK v3.1.0-a7a6d338252f ***
*** Using Zephyr OS v4.1.99-5f4c874a5ee8 ***

## 前言：dhcpv4_client應用的 .conf 套用關係

- Zephyr 構建時會依序合併：板級 `*_defconfig` → 應用層 `prj.conf` → 額外覆寫的 overlay 檔。
- 本範例的核心設定在 `prj.conf`，提供共用的網路/協定能力；若需硬體或情境差異，則以 overlay 疊加。
- 主要 overlay：`overlay_nrf54l15.conf` 負責 nRF54L15專有的配置，overlay_dm9051.conf負責 DM9051 的網路驅動與底層緩衝配置。
- 建議在指令加入 `-DOVERLAY_CONFIG="boards/overlay_nrf54l15.conf"` 這類串列，確保 base (prj.conf) 先生效，再讓 overlay 逐一覆寫細節。
- 指令加入`-DOVERLAY_CONFIG="boards/overlay_nrf54l15.conf;overlay_dm9051.conf"`，可配置多個overlay配置，以分號分隔。

## 建置命令範例

```bash
# 使用基礎配置
west build -b nrf54l15dk/nrf54l15/cpuapp

# 使用 overlay 配置
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DOVERLAY_CONFIG="boards/overlay_nrf54l15.conf"

# 使用多個 overlay 配置（用分號分隔）
west build -b nrf54l15dk/nrf54l15/cpuapp -- -DOVERLAY_CONFIG="boards/overlay_nrf54l15.conf;overlay_dm9051.conf";
```
### prj.conf

```
CONFIG_NET_DHCPV4=y
CONFIG_NET_DHCPV4_OPTION_CALLBACKS=y
CONFIG_DNS_RESOLVER=y

CONFIG_INIT_STACKS=y

CONFIG_NET_MGMT=y
CONFIG_NET_MGMT_EVENT=y

CONFIG_LOG=y
CONFIG_NET_SHELL=y
```

### board/overlay_nrf54l15.conf

```
CONFIG_CLOCK_CONTROL_NRF_K32SRC_RC=y

CONFIG_LOG_BACKEND_UART=y
CONFIG_SHELL_LOG_BACKEND=n
```

### overlay_dm9051.conf

```
CONFIG_NETWORKING=y
CONFIG_NET_IPV6=n
CONFIG_NET_IPV4=y
CONFIG_NET_ARP=y
CONFIG_NET_UDP=y

CONFIG_NET_L2_ETHERNET=y

CONFIG_NET_BUF_DATA_SIZE=2048
CONFIG_NET_IPV4_FRAGMENT=y

CONFIG_ETH_DM9051=y
CONFIG_SPI=y
CONFIG_GPIO=y
```

## 專案目錄結構圖

```
zephyr/
├── dts/bindings/ethernet/
│   └── davicom,dm9051.yaml
│
├── samples/net/dhcpv4_client/
│   ├── CMakeLists.txt                      # 建置腳本
│   ├── prj.conf                            # 主配置檔案
│   ├── boards/
│   │   ├── overlay_nrf54l15.conf           # 板級配置
│   │   └── overlay_nrf54l15.overlay        # 板級 Device Tree
│   ├── overlay_dm9051.conf                 # DM9051配置檔案
│   ├── overlay_dm9051.overlay              # DM9051 Device Tree
│   └── src/
│       └── main.c                          # 應用程式主程式
│
└── drivers/ethernet/
    ├── CMakeLists.txt                      # 建置腳本
    ├── Kconfig                             # Kconfig
    ├── Kconfig.dm9051                      # DM9051 Kconfig(配置選項)
    ├── eth_dm9051.c                        # DM9051 驅動實作
    └── eth_dm9051_priv.h                   # DM9051 私有標頭檔
```

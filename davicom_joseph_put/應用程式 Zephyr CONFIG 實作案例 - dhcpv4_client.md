# Zephyr 應用程式 CONFIG 實作案例 DM9051 Networking Application -- dhcpv4_client

## 前言：dhcpv4_client應用的 .conf 套用關係
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

### overlay_nrf54l15_dm9051.conf

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
CONFIG_CLOCK_CONTROL_NRF_K32SRC_RC=y

CONFIG_LOG_BACKEND_UART=y
CONFIG_SHELL_LOG_BACKEND=n
```

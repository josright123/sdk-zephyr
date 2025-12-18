# 應用程式 Zephyr DM9051 Networking 實作案例

## 範例與檔案位置
- 範例目錄：`zephyr/samples/net/dhcpv4_client`
- 主要檔案：`prj.conf`、`CMakeLists.txt`、`src/main.c`
- DM9051 專用覆蓋檔：`overlay_nrf54l15_dm9051.conf`、`overlay_nrf54l15_dm9051.overlay`
- 裝置樹綁定：`dts/bindings/ethernet/davicom,dm9051.yaml`
- 驅動設定：`drivers/ethernet/Kconfig`、`drivers/ethernet/Kconfig.dm9051`
- 驅動程式：`drivers/ethernet/CMakelists.txt`、`drivers/ethernet/eth_dm9051.c`、`drivers/ethernet/eth_dm9051_priv.h`
## 硬體連接重點（`overlay_nrf54l15_dm9051.overlay`）
- SPI 匯流排：`spi21`，使用 `hfpll` 時脈，預設 `spi-max-frequency = 1 MHz`（可依實際板子調高）。
- 腳位：
  - SCK/MOSI/MISO：P1.3 / P1.1 / P1.0
  - CS：P1.2（`cs-gpios = <&gpio1 2 GPIO_ACTIVE_LOW>`）
  - INT：P0.3（`int-gpios`，低電位為有效）
  - RESET：P2.0（`reset-gpios`，低脈衝重置）
- MAC 位址：`local-mac-address = [00 00 00 00 00 00]`，請改為唯一的實際 MAC。
- 其他：釋放 NFCT 腳為 GPIO、調整 SRAM/RRAM 區塊，確保 GPIO0/1、GPIOTE20/30 可用。

### 裝置樹可調屬性（dm9051 節點）
- `spi-max-frequency`：SPI 最大時脈，依硬體/線長調整。
- `int-gpios`：中斷腳，需配合實際連接並設為 active-low。
- `reset-gpios`：硬體重置腳，driver 初始化會拉低觸發。
- `local-mac-address`：裝置 MAC 位址，6 bytes。
- `reg`：SPI CS 編號，預設 `<0>`。
- `cs-gpios`：晶片選擇腳，與 SPI 控制器節點同時設定。
- `pinctrl-0/1`：預設/睡眠 pin 配置，可依實際佈線修改。

## 組態重點（`prj.conf`）
- 必要功能：`CONFIG_NETWORKING=y`、`CONFIG_NET_L2_ETHERNET=y`、`CONFIG_ETH_DM9051=y`、`CONFIG_SPI=y`。
- DHCP/IPv4：`CONFIG_NET_IPV4=y`、`CONFIG_NET_DHCPV4=y`、`CONFIG_NET_DHCPV4_OPTION_CALLBACKS=y`、`CONFIG_NET_ARP=y`。
- 緩衝與分片：`CONFIG_NET_BUF_DATA_SIZE=2048`、`CONFIG_NET_IPV4_FRAGMENT=y` 以避免封包線性化錯誤。
- 日誌/除錯：`CONFIG_LOG=y`、`CONFIG_NET_LOG=y`、`CONFIG_NET_SHELL=y`。
- 串列輸出：`CONFIG_SERIAL=y`、`CONFIG_UART_CONSOLE=y`、`CONFIG_PRINTK=y`。

### DM9051 driver Kconfig（`drivers/ethernet/Kconfig.dm9051`）
- `CONFIG_ETH_DM9051_RX_THREAD_STACK_SIZE`：接收處理執行緒堆疊，預設 800。
- `CONFIG_ETH_DM9051_RX_THREAD_PRIO`：接收處理優先序，預設 2。
- `CONFIG_ETH_DM9051_CLKRDY_INIT_WAIT_MS`：初始化等待 CLKRDY 時間，預設 2 ms。
- `CONFIG_ETH_DM9051_TIMEOUT`：IP 緩衝逾時（等待堆疊緩衝），預設 100 ms。

## 應用程式行為（`src/main.c`）
- 範例目前僅輸出啟動/結束訊息，DHCP 啟動與事件回呼已包在 `#if 0` 中，需解除註解才會：
  - 使用 `net_dhcpv4_start()` 主動啟動 DHCP。
  - 透過 `net_mgmt_event_callback` 取得 IPv4 位址事件。
  - 透過 `net_dhcpv4_option_callback` 解析 DHCP 選項（如 NTP）。
- 若僅依 `CONFIG_NET_DHCPV4`，可配合 Connection Manager 或手動呼叫 `net_dhcpv4_start()` 讓介面自動取得位址。

## 建置與燒錄範例
以 nRF54L15 DK CPUAPP 為例（請依實際板名替換 `nrf54l15dk_nrf54l15_cpuapp`）：
```powershell
west build -b nrf54l15dk_nrf54l15_cpuapp zephyr/samples/net/dhcpv4_client `
  -- -DDTC_OVERLAY_FILE=overlay_nrf54l15_dm9051.overlay `
  -DEXTRA_CONF_FILE=overlay_nrf54l15_dm9051.conf
west flash
```
- 若需同時使用原始 `prj.conf` 與覆蓋檔，可用 `-DCONF_FILE="prj.conf;overlay_nrf54l15_dm9051.conf"`。
- 提高 SPI 速率時，請同步檢查走線與 INT/RESET 穩定性。

## 驗證與除錯建議
- 在 shell 中啟用網路命令：`net iface` 查看介面狀態，`net arp`、`net dns` 檢查解析狀況。
- 若收不到 DHCP 位址，確認 `int-gpios` 腳位與中斷極性、`reset-gpios` 是否正確拉低，並觀察 log 中 CLKRDY 逾時訊息。
- 遇到封包線性化錯誤或 MTU 超大封包，適度增加 `CONFIG_NET_BUF_DATA_SIZE` 或降低傳輸速率。
- 需要固定 MAC 時直接在裝置樹填入 `local-mac-address`；若要改為動態產生，可在驅動前覆寫 net_if 的 MAC 介面。

## 快速檢查清單
- [ ] SPI 腳位、CS/INT/RESET 配線與極性符合 overlay
- [ ] `CONFIG_ETH_DM9051=y`、`CONFIG_NET_L2_ETHERNET=y` 已啟用
- [ ] MAC 位址已改為唯一值
- [ ] DHCP 啟動流程（程式呼叫或 Connection Manager）已確認
- [ ] 目標板與覆蓋檔路徑在 build 參數中正確帶入

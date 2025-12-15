
#### DM9051A/普羅通信

## 檔案結構總覽

```
nrf/
├── samples/net/http_server/
│   ├── CMakeLists.txt                          # 建置腳本
│   ├── prj.conf                                # 主配置檔案
│   ├── overlay_nrf54l15_dm9051.conf            # 開發板配置
│   ├── overlay_nrf54l15_dm9051.overlay         # 開發板 Device Tree
│   └── credentials/
│       ├── client.crt    
│       ├── client.key    
│       ├── server_certificate.pem
│       └── server_private_key.pem
│   └── src/
│       ├── credentials_provision.c    
│       ├── credentials_provision.h   
│       └── main.c                              # 應用程式主程式
zephyr/
├── dts/bindings/ethernet/
│   └── davicom,dm9051.yaml
├── samples/net/dhcpv4_client/
│   ├── CMakeLists.txt                          # 建置腳本
│   ├── prj.conf                                # 主配置檔案
│   ├── overlay_nrf54l15_dm9051.conf            # 開發板配置
│   ├── overlay_nrf54l15_dm9051.overlay         # 開發板 Device Tree
│   └── src/
│       └── main.c                              # 應用程式主程式
│
└── drivers/ethernet/
    ├── CMakeLists.txt                          # 建置腳本
    ├── Kconfig                                 # Kconfig
    ├── Kconfig.dm9051                          # DM9051 Kconfig
    ├── eth_dm9051.c                            # DM9051 驅動實作
    └── eth_dm9051_priv.h                       # DM9051 私有標頭檔
```

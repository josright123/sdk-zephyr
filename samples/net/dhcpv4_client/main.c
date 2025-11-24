/* Networking DHCPv4 client */

/*
 * Copyright (c) 2017 ARM Ltd.
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_dhcpv4_client_sample, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/linker/sections.h>
#include <zephyr/devicetree.h>
#include <errno.h>
#include <stdio.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>

#define DHCP_OPTION_NTP (42)

static uint8_t ntp_server[4];

static struct net_mgmt_event_callback mgmt_cb;

static struct net_dhcpv4_option_callback dhcp_cb;

/* 网络接口计数器 */
static int interface_count = 0;

/* 网络接口遍历回调函数 */
static bool count_interfaces(struct net_if *iface, void *user_data)
{
	int *count = (int *)user_data;
	(*count)++;
	LOG_INF("Found network interface %d: %s (index=%d)", 
		*count,
		net_if_get_device(iface)->name,
		net_if_get_by_iface(iface));
	return false;
}

static void start_dhcpv4_client(struct net_if *iface, void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_INF("Start DHCP on %s: index=%d", net_if_get_device(iface)->name,
		net_if_get_by_iface(iface));

	/* 根据 SoC 选择不同的网络接口 */
	#ifdef CONFIG_SOC_NRF54L15
		/* nRF54L15 特定配置 */
		LOG_INF("nRF54L15: Starting DHCP on %s", 
			net_if_get_device(iface)->name);
	#elif defined(CONFIG_SOC_NRF52840)
		/* nRF52840 特定配置 */
		LOG_INF("nRF52840: Starting DHCP on %s", 
			net_if_get_device(iface)->name);
	#endif

	net_dhcpv4_start(iface);
}

static void handler(struct net_mgmt_event_callback *cb,
		    uint32_t mgmt_event,
		    struct net_if *iface)
{
	int i = 0;

	if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
		return;
	}

	for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		char buf[NET_IPV4_ADDR_LEN];

		if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type !=
							NET_ADDR_DHCP) {
			continue;
		}

		LOG_INF("   Address[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(AF_INET,
			    &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr,
						  buf, sizeof(buf)));
		LOG_INF("    Subnet[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(AF_INET,
				       &iface->config.ip.ipv4->unicast[i].netmask,
				       buf, sizeof(buf)));
		LOG_INF("    Router[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(AF_INET,
						 &iface->config.ip.ipv4->gw,
						 buf, sizeof(buf)));
		LOG_INF("Lease time[%d]: %u seconds", net_if_get_by_iface(iface),
			iface->config.dhcpv4.lease_time);
	}
}

static void option_handler(struct net_dhcpv4_option_callback *cb,
			   size_t length,
			   enum net_dhcpv4_msg_type msg_type,
			   struct net_if *iface)
{
	char buf[NET_IPV4_ADDR_LEN];

	LOG_INF("DHCP Option %d: %s", cb->option,
		net_addr_ntop(AF_INET, cb->data, buf, sizeof(buf)));
}

int main(void) ..................t6n56 ...................
{
	LOG_INF("=== DHCPv4 Client Sample ===");
	
	/* SoC 识别 - 方法 1: 使用 Kconfig 宏 */
	#ifdef CONFIG_SOC_NRF54L15
		LOG_INF("SoC: nRF54L15 (via Kconfig)");
	#elif defined(CONFIG_SOC_NRF52840)
		LOG_INF("SoC: nRF52840 (via Kconfig)");
	#elif defined(CONFIG_SOC_NRF52832)
		LOG_INF("SoC: nRF52832 (via Kconfig)");
	#else
		LOG_INF("SoC: Unknown (check Kconfig)");
	#endif

	/* SoC 识别 - 方法 2: 使用设备树宏 */
	#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf54l15)
		LOG_INF("SoC: nRF54L15 (via Device Tree)");
	#elif DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf52840)
		LOG_INF("SoC: nRF52840 (via Device Tree)");
	#elif DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf52832)
		LOG_INF("SoC: nRF52832 (via Device Tree)");
	#endif

	/* SoC 识别 - 方法 3: 检查 SoC 系列 */
	#ifdef CONFIG_SOC_SERIES_NRF52X
		LOG_INF("SoC Series: nRF52x");
	#elif defined(CONFIG_SOC_SERIES_NRF54HX)
		LOG_INF("SoC Series: nRF54Hx");
	#endif

	/* 检查网络接口 */
	LOG_INF("Checking network interfaces...");
	
	struct net_if *default_iface = net_if_get_default();
	if (default_iface == NULL) {
		LOG_WRN("No default network interface found");
	} else {
		LOG_INF("Default network interface: %s (index=%d)", 
			net_if_get_device(default_iface)->name,
			net_if_get_by_iface(default_iface));
	}

	/* 统计所有可用的网络接口 */
	interface_count = 0;
	net_if_foreach(count_interfaces, &interface_count);
	
	if (interface_count == 0) {
		LOG_ERR("No network interfaces available!");
		LOG_ERR("Please check:");
		LOG_ERR("  1. Network driver (DM9051) is enabled in prj.conf");
		LOG_ERR("  2. Device tree overlay (overlay-dm9051.overlay) is configured");
		LOG_ERR("  3. Hardware connections (SPI, GPIO) are correct");
		LOG_ERR("  4. Build with: west build -b <board> -- -DOVERLAY_CONFIG=overlay-dm9051.overlay");
		return -ENODEV;
	}
	
	LOG_INF("Found %d network interface(s)", interface_count);

	/* 初始化网络管理回调 */
	net_mgmt_init_event_callback(&mgmt_cb, handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&mgmt_cb);

	/* 初始化 DHCPv4 选项回调 */
	net_dhcpv4_init_option_callback(&dhcp_cb, option_handler,
					DHCP_OPTION_NTP, ntp_server,
					sizeof(ntp_server));

	net_dhcpv4_add_option_callback(&dhcp_cb);

	/* 在所有网络接口上启动 DHCPv4 客户端 */
	LOG_INF("Starting DHCPv4 client on all interfaces...");
	net_if_foreach(start_dhcpv4_client, NULL);
	
	LOG_INF("DHCPv4 client started. Waiting for IP address...");
	
	return 0;
}
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
static void count_interfaces_proc(struct net_if *iface, void *user_data)
{
	int *count = (int *)user_data;
	(*count)++;
#ifdef CONFIG_SOC_NRF54L15
//	printk("NRF54L15, Found network interface %d: %s (index=%d)", *count,
//		net_if_get_device(iface)->name, net_if_get_by_iface(iface));
#endif
}

static void start_dhcpv4_client(struct net_if *iface, void *user_data)
{
	ARG_UNUSED(user_data);

//	printk("Start DHCP on %s: index=%d", net_if_get_device(iface)->name,
//	       net_if_get_by_iface(iface));

/* 根据 SoC 选择不同的网络接口 */
#ifdef CONFIG_SOC_NRF54L15
	/* nRF54L15 特定配置 */
//	LOG_INF("nRF54L15: Starting DHCP on %s", net_if_get_device(iface)->name);
#endif

	net_dhcpv4_start(iface);
}

static void handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface)
{
	int i = 0;

	if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
		return;
	}

	for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		char buf[NET_IPV4_ADDR_LEN];

		if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type != NET_ADDR_DHCP) {
			continue;
		}

		LOG_INF("   Address[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(AF_INET,
				      &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr, buf,
				      sizeof(buf)));
		LOG_INF("    Subnet[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(AF_INET, &iface->config.ip.ipv4->unicast[i].netmask, buf,
				      sizeof(buf)));
		LOG_INF("    Router[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(AF_INET, &iface->config.ip.ipv4->gw, buf, sizeof(buf)));
		LOG_INF("Lease time[%d]: %u seconds", net_if_get_by_iface(iface),
			iface->config.dhcpv4.lease_time);
	}
}

static void option_handler(struct net_dhcpv4_option_callback *cb, size_t length,
			   enum net_dhcpv4_msg_type msg_type, struct net_if *iface)
{
	char buf[NET_IPV4_ADDR_LEN];

	LOG_INF("DHCP Option %d: %s", cb->option,
		net_addr_ntop(AF_INET, cb->data, buf, sizeof(buf)));
}

#define MAIN_BSACIC_COUNT 1000
int endc = 0;

int main(void)
{
	int through_c = endc + MAIN_BSACIC_COUNT;
	endc++;
	printk("\n(end.s=%d) %s\n", through_c, STRINGIFY(BUILD_VERSION));
	printk("Booting %s / %s\n", STRINGIFY(BUILD_VERSION), "=== DHCPv4 Client Sample ===");

/* SoC 识别 - 方法 1: 使用 Kconfig 宏 */
#ifdef CONFIG_SOC_NRF54L15
	//		LOG_INF("SoC: nRF54L15 (via Kconfig)");
#endif

	/* 检查网络接口 */
	//	LOG_INF("Checking network interfaces...");

	struct net_if *default_iface = net_if_get_default();
	if (default_iface == NULL) {
		LOG_WRN("No default network interface found");
	}
	//	else {
	//		printk("Default network interface: %s (index=%d)",
	//		       net_if_get_device(default_iface)->name,
	//net_if_get_by_iface(default_iface));
	//	}

	/* 统计所有可用的网络接口 */
	interface_count = 0;
	net_if_foreach(count_interfaces_proc, &interface_count);

	if (interface_count == 0) {
		return -ENODEV;
	}

	printk("Booting_Find: %d eth interfaces\n", interface_count);
	//	LOG_INF("Found %d network interface(s)", interface_count);

	/* 初始化网络管理回调 */
	net_mgmt_init_event_callback(&mgmt_cb, handler, NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&mgmt_cb);

	/* 初始化 DHCPv4 选项回调 */
	net_dhcpv4_init_option_callback(&dhcp_cb, option_handler, DHCP_OPTION_NTP, ntp_server,
					sizeof(ntp_server));

	net_dhcpv4_add_option_callback(&dhcp_cb);

	/* 在所有网络接口上启动 DHCPv4 客户端 */
	net_if_foreach(start_dhcpv4_client, NULL);
	//	LOG_INF("DHCPv4 client started. Waiting for IP address...");
//	printk("BootStart_DHCP: %d eth ifaces\n", interface_count);

#if 1

#ifndef BANNER_VERSION
#if defined(BUILD_VERSION) && !IS_EMPTY(BUILD_VERSION)
// #define BANNER_VERSION STRINGIFY(BUILD_VERSION)
#else
// #define BANNER_VERSION KERNEL_VERSION_STRING
#endif /* BUILD_VERSION */
#endif /* !BANNER_VERSION */

	printk("\n(end.e=%d) %s\n", through_c, STRINGIFY(BUILD_VERSION));
	printk("BootStart_DHCP: %d eth ifaces\n", interface_count);
	printk("*** END *** main.c\n"); // boot_banner();
#endif

	return 0;
}

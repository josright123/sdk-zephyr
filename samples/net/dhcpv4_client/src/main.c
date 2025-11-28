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
#include <zephyr/net/dhcpv4.h>

#define DHCP_OPTION_NTP (42)

static uint8_t ntp_server[4];

static struct net_mgmt_event_callback mgmt_cb;

static struct net_dhcpv4_option_callback dhcp_cb;

/* Debug callback for DHCP state changes */
static struct net_mgmt_event_callback dhcp_mgmt_cb;

/* Callback for interface up event */
static struct net_mgmt_event_callback iface_up_cb;

/* 网络接口计数器 */
static int interface_count = 0;

/* Flag to track if interface is ready */
static bool iface_ready = false;

/* 网络接口遍历回调函数 */
static void count_interfaces_proc(struct net_if *iface, void *user_data)
{
	int *count = (int *)user_data;
	(*count)++;
#ifdef CONFIG_SOC_NRF54L15
	// printk("NRF54L15, Found network interface %d: %s (index=%d)", *count,
	//        net_if_get_device(iface)->name, net_if_get_by_iface(iface));
#endif
}

static void start_dhcpv4_client(struct net_if *iface, void *user_data)
{
	ARG_UNUSED(user_data);

	printk("[DEBUG] start_dhcpv4_client: Starting DHCP on %s (index=%d)\n",
	       net_if_get_device(iface)->name, net_if_get_by_iface(iface));
	
	/* Check interface state before starting DHCP */
	printk("[DEBUG] start_dhcpv4_client: Interface flags: LOWER_UP=%d, UP=%d, IS_UP=%d\n",
	       net_if_flag_is_set(iface, NET_IF_LOWER_UP),
	       net_if_flag_is_set(iface, NET_IF_UP),
	       net_if_is_up(iface));
	
	/* Wait for interface to be ready (LOWER_UP must be set) */
	if (!net_if_flag_is_set(iface, NET_IF_LOWER_UP)) {
		printk("[DEBUG] start_dhcpv4_client: Interface not ready (LOWER_UP=0), "
		       "waiting for carrier...\n");
		/* Don't start DHCP yet, wait for interface up event */
		return;
	}
	
	printk("[DEBUG] start_dhcpv4_client: DHCP state before start: %d\n",
	       iface->config.dhcpv4.state);
	
	/* 根据 SoC 选择不同的网络接口 */
#ifdef CONFIG_SOC_NRF54L15
	// LOG_INF("nRF54L15: Starting DHCP on %s", net_if_get_device(iface)->name);
#endif
	
	printk("[DEBUG] start_dhcpv4_client: Calling net_dhcpv4_start()\n");
	net_dhcpv4_start(iface);
	
	printk("[DEBUG] start_dhcpv4_client: DHCP state after start: %d\n",
	       iface->config.dhcpv4.state);
	printk("[DEBUG] start_dhcpv4_client: DHCP timeout: %u seconds\n",
	       iface->config.dhcpv4.request_time);
}

static void handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface)
{
	int i = 0;
	if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
		return;
	}
	printk("[DEBUG] handler: NET_EVENT_IPV4_ADDR_ADD received for iface %d\n",
	       net_if_get_by_iface(iface));
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

/* Debug handler for DHCP management events */
static void dhcp_debug_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
			       struct net_if *iface)
{
	const char *event_name = "UNKNOWN";
	
	switch (mgmt_event) {
	case NET_EVENT_IPV4_DHCP_START:
		event_name = "NET_EVENT_IPV4_DHCP_START";
		break;
	case NET_EVENT_IPV4_DHCP_BOUND:
		event_name = "NET_EVENT_IPV4_DHCP_BOUND";
		break;
	case NET_EVENT_IPV4_DHCP_STOP:
		event_name = "NET_EVENT_IPV4_DHCP_STOP";
		break;
	default:
		break;
	}
	
	printk("[DEBUG] dhcp_debug_handler: Event=%s (0x%08x) iface=%d state=%d\n",
	       event_name, mgmt_event, net_if_get_by_iface(iface),
	       iface->config.dhcpv4.state);
}

/* Handler for interface up event - start DHCP when interface is ready */
static void iface_up_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
			     struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_IF_UP) {
		printk("[DEBUG] iface_up_handler: Interface %d is UP, LOWER_UP=%d\n",
		       net_if_get_by_iface(iface),
		       net_if_flag_is_set(iface, NET_IF_LOWER_UP));
		
		/* Check if interface is fully ready */
		if (net_if_flag_is_set(iface, NET_IF_LOWER_UP) &&
		    net_if_flag_is_set(iface, NET_IF_UP)) {
			printk("[DEBUG] iface_up_handler: Interface ready, starting DHCP\n");
			if (!iface_ready) {
				iface_ready = true;
				net_dhcpv4_start(iface);
				printk("[DEBUG] iface_up_handler: DHCP started on iface %d\n",
				       net_if_get_by_iface(iface));
			}
		}
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

#ifdef CONFIG_SOC_NRF54L15
	// LOG_INF("SoC: nRF54L15 (via Kconfig)");
#endif

	struct net_if *default_iface = net_if_get_default();
	if (default_iface == NULL) {
		LOG_WRN("No default network interface found");
	}

	interface_count = 0;
	net_if_foreach(count_interfaces_proc, &interface_count);

	if (interface_count == 0) {
		return -ENODEV;
	}

	printk("Booting_Find: %d eth interfaces\n", interface_count);

	net_mgmt_init_event_callback(&mgmt_cb, handler, NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&mgmt_cb);

	/* Register DHCP debug callback */
	net_mgmt_init_event_callback(&dhcp_mgmt_cb, dhcp_debug_handler,
				    NET_EVENT_IPV4_DHCP_START |
				    NET_EVENT_IPV4_DHCP_BOUND |
				    NET_EVENT_IPV4_DHCP_STOP);
	net_mgmt_add_event_callback(&dhcp_mgmt_cb);

	/* Register interface up callback to start DHCP when interface is ready */
	net_mgmt_init_event_callback(&iface_up_cb, iface_up_handler, NET_EVENT_IF_UP);
	net_mgmt_add_event_callback(&iface_up_cb);

	net_dhcpv4_init_option_callback(&dhcp_cb, option_handler, DHCP_OPTION_NTP, ntp_server,
					sizeof(ntp_server));
	net_dhcpv4_add_option_callback(&dhcp_cb);

	printk("[DEBUG] main: About to check interfaces and start DHCP if ready\n");
	net_if_foreach(start_dhcpv4_client, NULL);
	printk("[DEBUG] main: Interface check completed, iface_ready=%d\n", iface_ready);
	// LOG_DBG("DHCP client started on all interfaces");

#if 1
#ifndef BANNER_VERSION
#if defined(BUILD_VERSION) && !IS_EMPTY(BUILD_VERSION)
// #define BANNER_VERSION STRINGIFY(BUILD_VERSION)
#else
// #define BANNER_VERSION KERNEL_VERSION_STRING
#endif
#endif
	printk("\n(end.e=%d) %s\n", through_c, STRINGIFY(BUILD_VERSION));
	printk("BootStart_DHCP: %d eth ifaces\n", interface_count);
	printk("*** END *** main.c\n"); // boot_banner();
#endif

	return 0;
}

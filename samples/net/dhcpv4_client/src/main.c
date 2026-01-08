/* Networking DHCPv4 client */

/*
 * Copyright (c) 2017 ARM Ltd.
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dhcpv4_client_sample, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/linker/sections.h>
#include <errno.h>
#include <stdio.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/ethernet_mgmt.h>

#define DHCP_OPTION_NTP (42)

static uint8_t ntp_server[4];

static struct net_mgmt_event_callback mgmt_cb_ipv4;
static struct net_mgmt_event_callback mgmt_cb_eth;

static struct net_dhcpv4_option_callback dhcp_cb;

#define MAIN_BSACIC_COUNT 1000
int endc = 0;

static void start_dhcpv4_client(struct net_if *iface, void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_INF("Start on %s (index=%d)", net_if_get_device(iface)->name,
		net_if_get_by_iface(iface));
	net_dhcpv4_start(iface);
}

static void handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface)
{
	int i = 0;
	const struct device *dev = net_if_get_device(iface);
	const char *ifname = dev ? dev->name : "?";
	int ifindex = net_if_get_by_iface(iface);
	char lladdr_buf[3 * 16];
	const struct net_linkaddr *lladdr = net_if_get_link_addr(iface);
	int llpos = 0;

	lladdr_buf[0] = '\0';
	if (lladdr && lladdr->addr && lladdr->len > 0) {
		for (size_t j = 0; j < lladdr->len && j < 16; j++) {
			llpos += snprintk(lladdr_buf + llpos, sizeof(lladdr_buf) - llpos, "%s%02x",
					  (j == 0) ? "" : ":", lladdr->addr[j]);
			if (llpos >= sizeof(lladdr_buf)) {
				break;
			}
		}
	}

	/* Decode event layer and command */
	// uint32_t event_layer = mgmt_event & 0xFFFF0000;
	// uint32_t event_cmd = mgmt_event & 0x0000FFFF;

	// printk("[TRACE] hdlr event=0x%08x (layer=0x%08x cmd=0x%04x) on %s (index=%d) mac=%s\n",
	//        mgmt_event, event_layer, event_cmd, ifname, ifindex, lladdr_buf);

	if (mgmt_event == NET_EVENT_ETHERNET_CARRIER_ON) {
		printk("[TRACE] *** CARRIER ON *** on %s (index=%d) ll=%s\n", ifname, ifindex,
		       lladdr_buf);
		return;
	}

	if (mgmt_event == NET_EVENT_ETHERNET_CARRIER_OFF) {
		printk("[TRACE] *** CARRIER OFF *** on %s (index=%d) ll=%s\n", ifname, ifindex,
		       lladdr_buf);
		return;
	}

	/* if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD ||) */
	if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
		// const char *event_name =
		//	(mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) ? "NET_EVENT_IPV4_DHCP_BOUND"
		//						 : "NET_EVENT_IPV4_ADDR_ADD";

		// printk("[TRACE] hdlr %s on %s (index=%d) ll=%s\n",
		//        event_name, ifname, ifindex, lladdr_buf);

		for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
			char buf[NET_IPV4_ADDR_LEN];

			if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type != NET_ADDR_DHCP) {
				continue;
			}

			LOG_INF("   Address[%d]: %s", net_if_get_by_iface(iface),
				net_addr_ntop(
					AF_INET,
					&iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr,
					buf, sizeof(buf)));
			LOG_INF("    Subnet[%d]: %s", net_if_get_by_iface(iface),
				net_addr_ntop(AF_INET, &iface->config.ip.ipv4->unicast[i].netmask,
					      buf, sizeof(buf)));
			LOG_INF("    Router[%d]: %s", net_if_get_by_iface(iface),
				net_addr_ntop(AF_INET, &iface->config.ip.ipv4->gw, buf,
					      sizeof(buf)));
			// LOG_INF("Lease time[%d]: %u seconds", net_if_get_by_iface(iface),
			//	iface->config.dhcpv4.lease_time);
		}
		return;
	}

	/* Fallback: unknown/unexpected event */
	// printk("[TRACE] hdlr UNKNOWN mgmt_event=0x%08x on %s (index=%d) ll=%s\n",
	//        mgmt_event, ifname, ifindex, lladdr_buf);
}

static void option_handler(struct net_dhcpv4_option_callback *cb, size_t length,
			   enum net_dhcpv4_msg_type msg_type, struct net_if *iface)
{
	char buf[NET_IPV4_ADDR_LEN];

	LOG_INF("DHCP Option %d: %s", cb->option,
		net_addr_ntop(AF_INET, cb->data, buf, sizeof(buf)));
}

/* nRF54L15DK LEDs (gpio-leds) */
#define LED0_NODE DT_NODELABEL(led0)
#define LED1_NODE DT_NODELABEL(led1)
#define LED2_NODE DT_NODELABEL(led2)
#define LED3_NODE DT_NODELABEL(led3)

static const struct gpio_dt_spec status_leds[] = {
	GPIO_DT_SPEC_GET(LED0_NODE, gpios),
	// GPIO_DT_SPEC_GET(LED1_NODE, gpios),
	// GPIO_DT_SPEC_GET(LED2_NODE, gpios),
	// GPIO_DT_SPEC_GET(LED3_NODE, gpios),
};

static const char *const status_led_names[] = {
	"led0",
	//"led1",
	//"led2",
	//"led3",
};

static uint8_t leds_ready_mask;

static int leds_init(void)
{
	int last_err = 0;

	leds_ready_mask = 0;

	for (size_t i = 0; i < ARRAY_SIZE(status_leds); i++) {
		const struct gpio_dt_spec *led = &status_leds[i];
		int ret;

		LOG_INF("Init %s: port %s, pin %d", status_led_names[i], led->port->name,
			led->pin); //", dt_flags=0x%x", led->dt_flags

		if (!device_is_ready(led->port)) {
			LOG_ERR("%s device %s not ready", status_led_names[i], led->port->name);
			last_err = -ENODEV;
			continue;
		}

		ret = gpio_pin_configure_dt(led, GPIO_OUTPUT);
		if (ret != 0) {
			LOG_ERR("%s configure failed: %d", status_led_names[i], ret);
			last_err = ret;
			continue;
		}

		(void)gpio_pin_set_dt(led, 0);
		leds_ready_mask |= BIT(i);
	}

	if (leds_ready_mask == 0) {
		return last_err != 0 ? last_err : -ENODEV;
	}

	// LOG_INF("LEDs ready mask=0x%x", leds_ready_mask);
	return 0;
}

int main(void)
{
	if (leds_init() != 0) {
		LOG_ERR("LED initialization failed - continuing without LED");
	}

	LOG_INF("Run dhcpv4 client.s (main.s=%d)", endc++);

	/* Note: net_mgmt event masks are layer-specific. Use one callback per layer. */
	net_mgmt_init_event_callback(&mgmt_cb_ipv4, handler,
				     /*NET_EVENT_IPV4_ADDR_ADD | */
				     NET_EVENT_IPV4_DHCP_BOUND);
	net_mgmt_add_event_callback(&mgmt_cb_ipv4);

	net_mgmt_init_event_callback(&mgmt_cb_eth, handler,
				     NET_EVENT_ETHERNET_CARRIER_ON |
					     NET_EVENT_ETHERNET_CARRIER_OFF);
	net_mgmt_add_event_callback(&mgmt_cb_eth);

	net_dhcpv4_init_option_callback(&dhcp_cb, option_handler, DHCP_OPTION_NTP, ntp_server,
					sizeof(ntp_server));

	net_dhcpv4_add_option_callback(&dhcp_cb);

	net_if_foreach(start_dhcpv4_client, NULL);

	LOG_INF("Run dhcpv4 client.e (main.e=%d)", endc++);

	printk("[TRACE] Entering main LED blink loop\n");
	int count = 0;
	int led_state = 0;
	while (1) {
		led_state ^= 1;
		for (size_t i = 0; i < ARRAY_SIZE(status_leds); i++) {
			if ((leds_ready_mask & BIT(i)) == 0) {
				continue;
			}

			const struct gpio_dt_spec *led = &status_leds[i];
			int ret = gpio_pin_set_dt(led, led_state);
			if (ret != 0) {
				LOG_ERR("%s gpio_pin_set_dt failed: %d", status_led_names[i], ret);
			}
		}

		if (count % 10 == 0) {
			for (size_t i = 0; i < ARRAY_SIZE(status_leds); i++) {
				if ((leds_ready_mask & BIT(i)) == 0) {
					continue;
				}

				int pin = gpio_pin_get_dt(&status_leds[i]);
				// OG_INF("%s set=%d read=%d count=%d",
				//	status_led_names[i], led_state, pin, count);
			}
		}
		count++;
		k_msleep(1000);
	}
	return 0;
}

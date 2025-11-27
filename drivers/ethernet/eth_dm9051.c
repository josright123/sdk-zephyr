/* DM9051 Stand-alone Ethernet Controller with SPI
 *
 * Copyright (c) 2025~2026 Davicom Semiconductor Incorporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT davicom_dm9051

#define LOG_MODULE_NAME eth_dm9051
#define LOG_LEVEL       CONFIG_ETHERNET_LOG_LEVEL

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <string.h>
#include <errno.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/ethernet.h>
#include <ethernet/eth_stats.h>

#include "eth_dm9051_priv.h"

/* DM9051 Constants */
#define DM9051_PHY     (0x40)
#define DM9051_PKT_RDY (0x01)
#define PHY_ADV_REG    (0x04)

/*******************************************************************************
 * Hardware Abstraction Layer - SPI Operations
 ******************************************************************************/

/**
 * @brief SPI transfer single byte
 * @param dev Device structure
 * @param byte Byte to transmit
 * @return Received byte
 */
static uint8_t dm9051_spi_xfer(const struct device *dev, uint8_t byte)
{
	const struct dm9051_config *config = dev->config;
	uint8_t rx_data = 0;
	struct spi_buf tx_buf = {.buf = &byte, .len = 1};
	struct spi_buf rx_buf = {.buf = &rx_data, .len = 1};
	const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
	const struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};
	int ret;

	ret = spi_transceive_dt(&config->spi, &tx, &rx);
	if (ret < 0) {
		printk("ERROR: SPI transfer failed: %d\n", ret);
		return 0xFF;
	}
	return rx_data;
}

/**
 * @brief Read single register from DM9051
 * @param dev Device structure
 * @param reg Register address
 * @return Register value
 */
static uint8_t dm9051_read_reg(const struct device *dev, uint8_t reg)
{
	const struct dm9051_config *config = dev->config;
	uint8_t result;

	/* CS low */
	const struct device *gpio1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));

	//	int err = gpio_pin_configure(gpio1, pin, GPIO_OUTPUT_INACTIVE);
	//	if (err) {
	//		printk("ERROR: P1.%d - Failed to configure: %d\n", pin, err);
	//		continue;
	//	}
	gpio_pin_set(gpio1, 8, 0);
	//	gpio_pin_set_dt(&config->spi.config.cs.gpio, 0);

	/* Send register address with read opcode */
	dm9051_spi_xfer(dev, reg | OPC_REG_R);
	/* Read data */
	result = dm9051_spi_xfer(dev, 0);

	/* CS high */
	gpio_pin_set(gpio1, 8, 1);
	//	gpio_pin_set_dt(&config->spi.config.cs.gpio, 1);

	return result;
}

/**
 * @brief Write single register to DM9051
 * @param dev Device structure
 * @param reg Register address
 * @param val Value to write
 */
static void dm9051_write_reg(const struct device *dev, uint8_t reg, uint8_t val)
{
	const struct dm9051_config *config = dev->config;

	/* CS low */
	const struct device *gpio1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));
	gpio_pin_set(gpio1, 8, 0);
	//	gpio_pin_set_dt(&config->spi.config.cs.gpio, 0);

	/* Send register address with write opcode */
	dm9051_spi_xfer(dev, reg | OPC_REG_W);
	/* Write data */
	dm9051_spi_xfer(dev, val);

	/* CS high */
	gpio_pin_set(gpio1, 8, 1);
	//	gpio_pin_set_dt(&config->spi.config.cs.gpio, 1);
}

/**
 * @brief Read multiple bytes from DM9051 memory
 * @param dev Device structure
 * @param buf Buffer to store read data
 * @param len Number of bytes to read
 */
static void dm9051_read_mem(const struct device *dev, uint8_t *buf, uint16_t len)
{
	const struct dm9051_config *config = dev->config;

	/* CS low */
	const struct device *gpio1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));
	gpio_pin_set(gpio1, 8, 0);
	//	gpio_pin_set_dt(&config->spi.config.cs.gpio, 0);

	/* Send memory read command */
	dm9051_spi_xfer(dev, DM9051_MRCMD | OPC_REG_R);

	/* Read data */
	for (uint16_t i = 0; i < len; i++) {
		buf[i] = dm9051_spi_xfer(dev, 0);
	}

	/* CS high */
	gpio_pin_set(gpio1, 8, 1);
	//	gpio_pin_set_dt(&config->spi.config.cs.gpio, 1);
}

/**
 * @brief Write multiple bytes to DM9051 memory
 * @param dev Device structure
 * @param buf Buffer containing data to write
 * @param len Number of bytes to write
 */
static void dm9051_write_mem(const struct device *dev, const uint8_t *buf, uint16_t len)
{
	const struct dm9051_config *config = dev->config;

	/* CS low */
	const struct device *gpio1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));
	gpio_pin_set(gpio1, 8, 0);
	//	gpio_pin_set_dt(&config->spi.config.cs.gpio, 0);

	/* Send memory write command */
	dm9051_spi_xfer(dev, DM9051_MWCMD | OPC_REG_W);

	/* Write data */
	for (uint16_t i = 0; i < len; i++) {
		dm9051_spi_xfer(dev, buf[i]);
	}

	/* CS high */
	gpio_pin_set(gpio1, 8, 1);
	//	gpio_pin_set_dt(&config->spi.config.cs.gpio, 1);
}

/*******************************************************************************
 * PHY Operations
 ******************************************************************************/

/**
 * @brief Read PHY register
 * @param dev Device structure
 * @param reg PHY register address
 * @return PHY register value
 */
static uint16_t dm9051_phy_read(const struct device *dev, uint16_t reg)
{
	uint16_t value;
	int timeout = 500;

	dm9051_write_reg(dev, DM9051_EPAR, DM9051_PHY | reg);
	dm9051_write_reg(dev, DM9051_EPCR, 0x0c);
	k_busy_wait(1);

	while ((dm9051_read_reg(dev, DM9051_EPCR) & 0x01) && timeout--) {
		k_busy_wait(1);
	}

	dm9051_write_reg(dev, DM9051_EPCR, 0x00);
	value = (dm9051_read_reg(dev, DM9051_EPDRH) << 8) | dm9051_read_reg(dev, DM9051_EPDRL);

	return value;
}

/**
 * @brief Write PHY register
 * @param dev Device structure
 * @param reg PHY register address
 * @param value Value to write
 */
static void dm9051_phy_write(const struct device *dev, uint16_t reg, uint16_t value)
{
	int timeout = 500;

	dm9051_write_reg(dev, DM9051_EPAR, DM9051_PHY | reg);
	dm9051_write_reg(dev, DM9051_EPDRL, value & 0xff);
	dm9051_write_reg(dev, DM9051_EPDRH, (value >> 8) & 0xff);
	dm9051_write_reg(dev, DM9051_EPCR, 0x0a);
	k_busy_wait(1);

	while ((dm9051_read_reg(dev, DM9051_EPCR) & 0x01) && timeout--) {
		k_busy_wait(1);
	}

	dm9051_write_reg(dev, DM9051_EPCR, 0x00);
}

/*******************************************************************************
 * Core Driver Functions
 ******************************************************************************/

/**
 * @brief Perform core reset of DM9051
 * @param dev Device structure
 */
static void dm9051_core_reset(const struct device *dev)
{
	/* Power on PHY */
	dm9051_write_reg(dev, DM9051_GPR, 0x00);
	k_msleep(25);

	/* NCR reset */
	dm9051_write_reg(dev, DM9051_NCR, DM9051_NCR_RESET);
	k_msleep(5);

	/* Wait for reset completion */
	int timeout = 100;
	while ((dm9051_read_reg(dev, DM9051_NCR) & DM9051_NCR_RESET) && timeout--) {
		k_msleep(1);
	}

	/* Software defaults */
	dm9051_write_reg(dev, DM9051_MBNDRY, MBNDRY_BYTE);
	dm9051_write_reg(dev, DM9051_PPCR, PPCR_PAUSE_COUNT);
	dm9051_write_reg(dev, DM9051_LMCR, LMCR_MODE1);
	dm9051_write_reg(dev, DM9051_INTR, INTR_ACTIVE_LOW);

#ifdef CONFIG_ETH_DM9051_TX_CHECKSUM_OFFLOAD
	/* Enable TX checksum offload */
	dm9051_write_reg(dev, DM9051_CSCR,
			 TCSCR_IPCS_ENABLE | TCSCR_UDPCS_ENABLE | TCSCR_TCPCS_ENABLE);
#endif

#ifdef CONFIG_ETH_DM9051_RX_CHECKSUM_OFFLOAD
	/* Enable RX checksum offload */
	dm9051_write_reg(dev, DM9051_RCSSR, RCSSR_RCSEN | RCSSR_DCSE);
#endif

	LOG_DBG("%s: Core reset complete", dev->name);
}

/**
 * @brief Get chip ID
 * @param dev Device structure
 * @return Chip ID (0x9051 or 0x9058 for DM9051A)
 */
static uint16_t dm9051_get_chipid(const struct device *dev)
{
	uint16_t id;
	uint8_t pidh, pidl;

	pidh = dm9051_read_reg(dev, DM9051_PIDH);
	pidl = dm9051_read_reg(dev, DM9051_PIDL);
	id = (pidh << 8) | pidl;

	/* Print raw register values for debugging */
	// LOG_INF("DEBUG: Chip ID registers - PIDH: 0x%02x, PIDL: 0x%02x, Combined: 0x%04x", pidh,
	//	pidl, id);

	/* DM9051 returns 0x9000, normalize to 0x9051 */
	if (id == 0x9000) {
		id = 0x9051;
		LOG_INF("DEBUG: Normalized chip ID from 0x9000 to 0x9051");
	}

	return id;
}

/**
 * @brief Set MAC address
 * @param dev Device structure
 * @param mac MAC address array (6 bytes)
 */
static void dm9051_set_mac_address(const struct device *dev, const uint8_t *mac)
{
	for (int i = 0; i < 6; i++) {
		dm9051_write_reg(dev, DM9051_PAR + i, mac[i]);
	}

	// LOG_INF("INFO%s: MAC %02x:%02x:%02x:%02x:%02x:%02x", dev->name, mac[0], mac[1], mac[2],
	//	mac[3], mac[4], mac[5]);
	//	printk("INFO%s: MAC %02x:%02x:%02x:%02x:%02x:%02x\n", dev->name, mac[0], mac[1],
	// mac[2], 		mac[3], mac[4], mac[5]);
}

/**
 * @brief Configure multicast address registers
 * @param dev Device structure
 */
static void dm9051_set_multicast(const struct device *dev)
{
	for (int i = 0; i < 8; i++) {
		dm9051_write_reg(dev, DM9051_MAR + i, (i == 7) ? 0x80 : 0x00);
	}
}

/**
 * @brief Configure receive settings
 * @param dev Device structure
 */
static void dm9051_set_receive(const struct device *dev)
{
	/* Configure multicast addresses */
	dm9051_set_multicast(dev);

	/* Configure flow control */
	dm9051_write_reg(dev, DM9051_FCR, FCR_DEFAULT);
	dm9051_phy_write(dev, PHY_ADV_REG, 0x0400 | 0x01e1);

	/* Configure interrupts */
#ifdef DMPLUG_INT
	dm9051_write_reg(dev, DM9051_IMR, IMR_INT_DEFAULT);
#else
	dm9051_write_reg(dev, DM9051_IMR, IMR_POL_DEFAULT);
#endif

	/* Enable receiver */
	dm9051_write_reg(dev, DM9051_RCR, RCR_DEFAULT | RCR_RXEN);

	LOG_DBG("%s: Receive configured", dev->name);
}

/*******************************************************************************
 * Packet Transmission
 ******************************************************************************/

/**
 * @brief Transmit packet
 * @param dev Device structure
 * @param pkt Network packet
 * @return 0 on success, negative errno on failure
 */
static int eth_dm9051_tx(const struct device *dev, struct net_pkt *pkt)
{
	struct dm9051_runtime *context = dev->data;
	uint16_t len = net_pkt_get_len(pkt);
	struct net_buf *frag;
	int timeout = 500;

	LOG_DBG("%s: TX packet len=%u", dev->name, len);

	k_sem_take(&context->tx_rx_sem, K_FOREVER);

	/* Set packet length */
	dm9051_write_reg(dev, DM9051_TXPLL, len & 0xff);
	dm9051_write_reg(dev, DM9051_TXPLH, (len >> 8) & 0xff);

	/* Write packet data */
	for (frag = pkt->frags; frag; frag = frag->frags) {
		dm9051_write_mem(dev, frag->data, frag->len);
	}

	/* Trigger transmission */
	dm9051_write_reg(dev, DM9051_TCR, TCR_TXREQ);

	/* Wait for completion with timeout */
	while (timeout--) {
		if (!(dm9051_read_reg(dev, DM9051_TCR) & TCR_TXREQ)) {
			break;
		}
		k_busy_wait(1);
	}

	k_sem_give(&context->tx_rx_sem);

	if (timeout == 0) {
		LOG_ERR("%s: TX timeout", dev->name);
		return -ETIMEDOUT;
	}

	LOG_DBG("%s: TX successful", dev->name);
	return 0;
}

/*******************************************************************************
 * Packet Reception
 ******************************************************************************/

/**
 * @brief Check if RX packet is ready
 * @param dev Device structure
 * @return true if packet ready, false otherwise
 */
static bool dm9051_rx_ready(const struct device *dev)
{
	uint8_t rxbyte;

	/* Read RX byte twice (dummy read first) */
	rxbyte = dm9051_read_reg(dev, DM9051_MRCMDX);
	rxbyte = dm9051_read_reg(dev, DM9051_MRCMDX);

	return (rxbyte & 0x01) == DM9051_PKT_RDY;
}

/**
 * @brief Receive packet
 * @param dev Device structure
 * @return 0 on success, negative errno on failure
 */
static int dm9051_rx_packet(const struct device *dev)
{
	const struct dm9051_config *config = dev->config;
	struct dm9051_runtime *context = dev->data;
	uint8_t header[4];
	uint16_t rx_len;
	uint8_t rx_status;
	struct net_pkt *pkt;

	if (!dm9051_rx_ready(dev)) {
		return 0;
	}

	/* Read packet header */
	dm9051_read_mem(dev, header, 4);
	dm9051_write_reg(dev, DM9051_ISR, 0x80);

	rx_status = header[1];
	rx_len = header[2] | (header[3] << 8);

	/* Validate packet */
	if (rx_status & RSR_ERR_BITS) {
		LOG_ERR("%s: RX error status=0x%02x", dev->name, rx_status);
		return -EIO;
	}

	if (rx_len > NET_ETH_MTU + 4) {
		LOG_ERR("%s: RX length error len=%u", dev->name, rx_len);
		return -EINVAL;
	}

	/* Allocate packet buffer */
	pkt = net_pkt_rx_alloc_with_buffer(context->iface, rx_len, AF_UNSPEC, 0,
					   K_MSEC(config->timeout));
	if (!pkt) {
		LOG_ERR("%s: Failed to allocate RX buffer", dev->name);
		eth_stats_update_errors_rx(context->iface);
		return -ENOMEM;
	}

	/* Read packet data */
	dm9051_read_mem(dev, net_pkt_data(pkt), rx_len);
	dm9051_write_reg(dev, DM9051_ISR, 0x80);

	net_pkt_set_iface(pkt, context->iface);

	/* Feed to network stack */
	if (net_recv_data(context->iface, pkt) < 0) {
		net_pkt_unref(pkt);
		return -EIO;
	}

	LOG_DBG("%s: RX packet len=%u", dev->name, rx_len);
	return 0;
}

/*******************************************************************************
 * Ethernet API Functions
 ******************************************************************************/

static enum ethernet_hw_caps eth_dm9051_get_capabilities(const struct device *dev)
{
	enum ethernet_hw_caps dm9051_caps;

	ARG_UNUSED(dev);

	dm9051_caps = ETHERNET_LINK_10BASE | ETHERNET_LINK_100BASE;

#ifdef CONFIG_NET_PROMISCUOUS_MODE
	dm9051_caps |= ETHERNET_PROMISC_MODE;
#endif

#ifdef CONFIG_ETH_DM9051_MULTICAST_FILTER
	dm9051_caps |= ETHERNET_HW_FILTERING;
#endif

	return dm9051_caps;
}

static int eth_dm9051_set_config(const struct device *dev, enum ethernet_config_type type,
				 const struct ethernet_config *config)
{
	struct dm9051_runtime *context = dev->data;

	if (type == ETHERNET_CONFIG_TYPE_MAC_ADDRESS) {
		memcpy(context->mac_address, config->mac_address.addr,
		       sizeof(context->mac_address));

#if 1
		/* Set MAC address */
		printk("\n\n");
		dm9051_set_mac_address(dev, context->mac_address);
		printk("_dm9051_set_config: MAC, %02x:%02x:%02x:%02x:%02x:%02x\n",
		       context->mac_address[0], context->mac_address[1], context->mac_address[2],
		       context->mac_address[3], context->mac_address[4], context->mac_address[5]);

		/* Configure receive */
		dm9051_set_receive(dev);
		printk("_dm9051_set_config: DM9051_RCR configured, RCR_DEFAULT | RCR_RXEN\n");
#endif

		if (context->iface != NULL) {
			net_if_set_link_addr(context->iface, context->mac_address,
					     sizeof(context->mac_address), NET_LINK_ETHERNET);
		}

		printk("%s: _dm9051_set_config: Interface configured.e [(set mac address, and set receive)]\n",
		       dev->name);
		return 0;
	}

	printk("%s: _dm9051_set_config: Interface configured.e [(nothing)]\n", dev->name);
	return -ENOTSUP;
}

static void eth_dm9051_iface_init(struct net_if *iface)
{
	const struct device *dev = net_if_get_device(iface);
	struct dm9051_runtime *context = dev->data;

	net_if_set_link_addr(iface, context->mac_address, sizeof(context->mac_address),
			     NET_LINK_ETHERNET);

	if (context->iface == NULL) {
		context->iface = iface;
	}

	ethernet_init(iface);

	/* Set carrier status */
	if (context->iface_carrier_on_init) {
		net_if_carrier_on(iface);
	} else {
		net_if_carrier_off(iface);
	}

	context->iface_initialized = true;

	// LOG_INF("%s: Interface initialized", dev->name);
	printk("_eth_dm9051_iface_init: Interface initialized.e\n");
}

static const struct ethernet_api api_funcs = {
	.iface_api.init = eth_dm9051_iface_init,
	.set_config = eth_dm9051_set_config,
	.get_capabilities = eth_dm9051_get_capabilities,
	.send = eth_dm9051_tx,
};

/*******************************************************************************
 * Device Initialization
 ******************************************************************************/

static int eth_dm9051_init(const struct device *dev)
{
	const struct dm9051_config *config = dev->config;
	struct dm9051_runtime *context = dev->data;
	uint16_t chip_id;

	printk("\n\n");
	printk("_eth_dm9051_init: eth_dm9051_init.s\n");
	printk("_eth_dm9051_init: Initializing DM9051\n");

	/* Check SPI is ready */
	if (!spi_is_ready_dt(&config->spi)) {
		printk("_eth_dm9051_init: SPI not ready\n");
		return -ENODEV;
	}

	/* Print SPI configuration */
	// LOG_INF("%s: SPI frequency: %u Hz (%u MHz)", dev->name, config->spi.config.frequency,
	// 	config->spi.config.frequency / 1000000);

	/* Verify CS GPIO is ready */
	// if (!gpio_is_ready_dt(&config->spi.config.cs.gpio)) {
	// 	LOG_ERR("%s: CS GPIO not ready", dev->name);
	// 	return -ENODEV;
	// }

	/* Initialize CS pin */
	gpio_pin_configure_dt(&config->spi.config.cs.gpio, GPIO_OUTPUT_INACTIVE);

	/* now manual by hard code Pin: 8: Test GPIO1 multiple pins to find working alternatives */
	const struct device *gpio1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));
	if (!device_is_ready(gpio1)) {
		printk("_eth_dm9051_init: ERROR: GPIO1 device not ready!\n");
		return -ENODEV;
	}
	int err = gpio_pin_configure(gpio1, 8, GPIO_OUTPUT_INACTIVE);
	if (err) {
		printk("_eth_dm9051_init: ERROR: P1.8 - Failed to configure: %d\n", err);
		return -ENODEV;
	}

	/* Print detailed GPIO information */
	printk("_eth_dm9051_init: INFO: ========================================\n");
	printk("_eth_dm9051_init: INFO: dev->name = %s\n", dev->name);
	printk("_eth_dm9051_init: INFO: SPI Bus: %s\n", config->spi.bus->name);
	printk("_eth_dm9051_init: INFO: CS GPIO ready - Port: %s, Pin: %d  (but now manual by hard code Pin: %d)\n",
		config->spi.config.cs.gpio.port->name, config->spi.config.cs.gpio.pin, 8);
	// LOG_INF("INFO: CS GPIO Flags: 0x%x", config->spi.config.cs.gpio.dt_flags);
	printk("_eth_dm9051_init: INFO: ========================================\n");

	/* Try reading chip ID multiple times */
	for (int attempt = 0; attempt < 3; attempt++) {
		k_msleep(50);
		chip_id = dm9051_get_chipid(dev);
		if (chip_id == 0x9051 || chip_id == 0x9058) {
			break;
		}
	}

	/* Verify chip ID before reset */
	if (chip_id != 0x9051 && chip_id != 0x9058) {
		printk("_eth_dm9051_init: ERROR: Invalid chip ID: 0x%04x (expected 0x9051 or 0x9058)\n",
			chip_id);

		while (1) {
			chip_id = dm9051_get_chipid(dev);
			if (chip_id == 0x9051 || chip_id == 0x9058) {
				LOG_INF("INFO: DM9051 chip ID verified: 0x%04x", chip_id);
				break;
			}
			LOG_INF("LOOP-TEST: DM9051 chip ID verification failed: 0x%04x", chip_id);
			k_msleep(1000);
		}
		return -ENODEV;
	}

//	printk("_eth_dm9051_init: INFO: Chip ID verified: 0x%04x\n", chip_id);

	/* Perform core reset */
	dm9051_core_reset(dev);

#if 1
	/* Set MAC address */
	
	#if 0
	//boot_banner();
	//printk("*** " CONFIG_BOOT_BANNER_STRING " " BANNER_VERSION BANNER_POSTFIX " *** main.c\n");
	#endif

	#ifndef BANNER_VERSION
	#if defined(BUILD_VERSION) && !IS_EMPTY(BUILD_VERSION)
	//#define BANNER_VERSION STRINGIFY(BUILD_VERSION)
	#else
	//#define BANNER_VERSION KERNEL_VERSION_STRING
	#endif /* BUILD_VERSION */
	#endif /* !BANNER_VERSION */

	int endc = 0;
	printk("\n(end.e=%d) %s\n", endc, STRINGIFY(BUILD_VERSION));
	dm9051_set_mac_address(dev, context->mac_address);
	printk("_eth_dm9051_init: end.e (set mac address, %02x:%02x:%02x:%02x:%02x:%02x) Chip ID: 0x%04x\n",
		   context->mac_address[0], context->mac_address[1], context->mac_address[2],
		   context->mac_address[3], context->mac_address[4], context->mac_address[5], chip_id);

	/* Configure receive */
	dm9051_set_receive(dev);
	printk("_eth_dm9051_init: end.e (set receive, RCR_DEFAULT | RCR_RXEN) Chip ID: 0x%04x\n", chip_id);

	// LOG_INF("%s: eth_dm9051_init.e", dev->name);
	// printk("%s: eth_dm9051_init.e\n", dev->name);
#endif
	return 0;
}

/*******************************************************************************
 * Device Instantiation
 ******************************************************************************/

#define DM9051_DEFINE(inst)                                                                        \
	static struct dm9051_runtime dm9051_runtime_##inst = {                                     \
		.mac_address = DT_INST_PROP(inst, local_mac_address),                              \
		.tx_rx_sem = Z_SEM_INITIALIZER((dm9051_runtime_##inst).tx_rx_sem, 1, UINT_MAX),    \
		.int_sem = Z_SEM_INITIALIZER((dm9051_runtime_##inst).int_sem, 0, UINT_MAX),        \
	};                                                                                         \
                                                                                                   \
	static const struct dm9051_config dm9051_config_##inst = {                                 \
		.spi = SPI_DT_SPEC_INST_GET(inst, SPI_WORD_SET(8), 0),                             \
		.interrupt = GPIO_DT_SPEC_INST_GET(inst, int_gpios),                               \
		.timeout = 100,                                                                    \
	};                                                                                         \
                                                                                                   \
	ETH_NET_DEVICE_DT_INST_DEFINE(inst, eth_dm9051_init, NULL, &dm9051_runtime_##inst,         \
				      &dm9051_config_##inst, CONFIG_ETH_INIT_PRIORITY, &api_funcs, \
				      NET_ETH_MTU);

DT_INST_FOREACH_STATUS_OKAY(DM9051_DEFINE);

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
#include <zephyr/random/random.h>
#include <string.h>
#include <errno.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/toolchain.h>
#include <ethernet/eth_stats.h>

#include "eth_dm9051_priv.h"

static uint8_t dm9051_read_reg(const struct device *dev, uint8_t reg);
static void dm9051_read_mem(const struct device *dev, uint8_t *buf, uint16_t len);

extern int endc;

static bool dm9051_mac_is_valid(const uint8_t mac[6])
{
 bool all_zero = true;

 for (int i = 0; i < 6; i++) {
  if (mac[i] != 0x00) {
   all_zero = false;
   break;
  }
 }

 if (all_zero) {
  return false;
 }

 /* Reject multicast/broadcast */
 if ((mac[0] & 0x01) != 0) {
  return false;
 }

 return true;
}

static void dm9051_generate_random_mac(uint8_t mac[6])
{
	uint32_t r = sys_rand32_get();

	/* OUI: Davicom vendor prefix */
	mac[0] = 0x00;
	mac[1] = 0x60;
	mac[2] = 0x6e;

	/* NIC: random bytes */
	mac[3] = (uint8_t)(r >> 16);
	mac[4] = (uint8_t)(r >> 8);
	mac[5] = (uint8_t)(r & 0xFF);
}

int dm9051_load_mac_from_current_fit(const struct device *dev, uint8_t mac[6])
{
 for (int i = 0; i < 6; i++) {
  mac[i] = dm9051_read_reg(dev, DM9051_PAR + i);
 }
 return 0;
}

static void dm9051_init_mac(const struct device *dev)
{
 struct dm9051_runtime *context = dev->data;


	printk("\n(start.s=%d)\n", endc);
 /* Priority 1: devicetree local-mac-address (already copied into context) */
 if (dm9051_mac_is_valid(context->mac_address)) {
  printk("dm9051_init_mac: Using DT MAC address %02x:%02x:%02x:%02x:%02x:%02x\n",
         context->mac_address[0], context->mac_address[1], context->mac_address[2],
         context->mac_address[3], context->mac_address[4], context->mac_address[5]);
  return;
 }

 /* Priority 2: try NVS */
 if (dm9051_load_mac_from_current_fit(dev, context->mac_address) == 0 &&
	 dm9051_mac_is_valid(context->mac_address)) {
  printk("dm9051_init_mac: Using CHIP MAC address %02x:%02x:%02x:%02x:%02x:%02x\n",
         context->mac_address[0], context->mac_address[1], context->mac_address[2],
         context->mac_address[3], context->mac_address[4], context->mac_address[5]);
  return;
 }

 /* Priority 3: fallback random locally administered unicast */
 dm9051_generate_random_mac(context->mac_address);
 printk("dm9051_init_mac: Using random MAC address %02x:%02x:%02x:%02x:%02x:%02x\n",
        context->mac_address[0], context->mac_address[1], context->mac_address[2],
        context->mac_address[3], context->mac_address[4], context->mac_address[5]);
}

int dm9051_read_mem_cb(void *ctx, uint8_t *buf, int len)
{
	const struct device *dev = ctx;

	dm9051_read_mem(dev, buf, len);
	return 0;
}

/*
 * The following function and type definition are local implementations of
 * APIs that may not be available in the user's SDK version. This ensures
 * compatibility while using modern Zephyr patterns.
 */
typedef int (*net_pkt_read_from_cb_t)(void *ctx, uint8_t *buf, int len);

static inline int local_net_pkt_write_from(struct net_pkt *pkt, net_pkt_read_from_cb_t cb,
					   void *ctx, size_t len)
{
	size_t remaining = len;
	struct net_buf *frag;

	if (!pkt->buffer) {
		return -ENOMEM;
	}

	frag = pkt->buffer;

	while (frag && remaining > 0) {
		size_t copy_len = MIN(remaining, net_buf_tailroom(frag));
		int ret;

		ret = cb(ctx, net_buf_add(frag, copy_len), copy_len);
		if (ret < 0) {
			return ret;
		}

		remaining -= copy_len;

		if (remaining > 0) {
			frag = frag->frags;
		}
	}

	if (remaining > 0) {
		return -ENOMEM;
	}

	return 0;
}

/* Driver configuration structure */
struct driver_config {
	const char *release_version;
};

/* Default driver configuration */
const struct driver_config confdata = {
	.release_version = "zephyr_dm9051_v3.1.0_v1.0",
};

/* Helper macro to check if interrupt mode is enabled based on device tree configuration */
#define cint(dev) (((const struct dm9051_config *)(dev)->config)->interrupt.port != NULL)

/* DM9051 Constants */
#define DM9051_PHY     (0x40)
#define DM9051_PKT_RDY (0x01)
#define PHY_ADV_REG    (0x04)

/*******************************************************************************
 * Hardware Abstraction Layer - SPI Operations
 ******************************************************************************/

/* Note: All SPI operations use spi_transceive_dt() or spi_write_dt() directly.
 * CS (Chip Select) is automatically controlled by the SPI driver layer.
 * The cs-gpios property in device tree specifies which GPIO pin to use for CS.
 * We use static buffers on stack (like W5500) instead of k_malloc.
 */

/**
 * @brief Read single register from DM9051
 * @param dev Device structure
 * @param reg Register address
 * @return Register value
 */
static uint8_t dm9051_read_reg(const struct device *dev, uint8_t reg)
{
	const struct dm9051_config *config = dev->config;
	uint8_t tx_data[2] = {reg | OPC_REG_R, 0x00};
	uint8_t rx_data[2] = {0};

	struct spi_buf tx_buf = {.buf = tx_data, .len = 2};
	struct spi_buf rx_buf = {.buf = rx_data, .len = 2};
	const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
	const struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};

	int ret = spi_transceive_dt(&config->spi, &tx, &rx);
	if (ret < 0) {
		LOG_ERR("SPI read register failed: %d", ret);
		return 0xFF;
	}

	return rx_data[1];
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
	uint8_t tx_data[2] = {reg | OPC_REG_W, val};

	struct spi_buf tx_buf = {.buf = tx_data, .len = 2};
	const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};

	int ret = spi_write_dt(&config->spi, &tx);
	if (ret < 0) {
		LOG_ERR("SPI write register failed: %d", ret);
	}
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
	uint8_t cmd = DM9051_MRCMD | OPC_REG_R;

	const struct spi_buf tx_buf = {.buf = &cmd, .len = 1};
	const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};

	const struct spi_buf rx_buf[2] = {
		{.buf = NULL, .len = 1}, /* Discard command echo */
		{.buf = buf, .len = len} /* Actual data */
	};
	const struct spi_buf_set rx = {.buffers = rx_buf, .count = 2};

	int ret = spi_transceive_dt(&config->spi, &tx, &rx);
	if (ret < 0) {
		LOG_ERR("SPI read memory failed: %d", ret);
	}
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
	uint8_t cmd = DM9051_MWCMD | OPC_REG_W;

	const struct spi_buf tx_buf[2] = {
		{.buf = &cmd, .len = 1},         /* Command byte */
		{.buf = (void *)buf, .len = len} /* Data bytes */
	};
	const struct spi_buf_set tx = {.buffers = tx_buf, .count = 2};

	int ret = spi_write_dt(&config->spi, &tx);
	if (ret < 0) {
		LOG_ERR("SPI write memory failed: %d", ret);
	}
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
#if 0
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

	if (dm9051_read_reg(dev, DM9051_EPCR) & 0x01) {
		return 0xffff;
	}

	dm9051_write_reg(dev, DM9051_EPCR, 0x00);
	value = (dm9051_read_reg(dev, DM9051_EPDRH) << 8) | dm9051_read_reg(dev, DM9051_EPDRL);

	return value;
}
#endif

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

void dm9051_interrupt_disble_irq(const struct device *dev)
{
	dm9051_write_reg(dev, DM9051_IMR, IMR_PAR);
}

void dm9051_isr_enab(const struct device *dev)
{
	uint8_t isrs = dm9051_read_reg(dev, DM9051_ISR);
	dm9051_write_reg(dev, DM9051_ISR, isrs);
}
void dm9051_imr_enab(const struct device *dev)
{
	dm9051_write_reg(dev, DM9051_IMR, IMR_INT_DEFAULT);
}

static void dm9051_interrupt_reset_for_cb_sem(const struct device *dev)
{
	dm9051_isr_enab(dev);
	dm9051_imr_enab(dev);
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

	/* Configure interrupts based on device tree configuration */
	if (cint(dev)) {
		/* Interrupt mode enabled via int-gpios in device tree */
		dm9051_write_reg(dev, DM9051_IMR, IMR_INT_DEFAULT);
	} else {
		/* Polling mode (no int-gpios defined) */
		dm9051_write_reg(dev, DM9051_IMR, IMR_POL_DEFAULT);
	}

	/* Enable receiver */
	dm9051_write_reg(dev, DM9051_RCR, RCR_DEFAULT | RCR_RXEN);

	LOG_DBG("%s: Receive configured (%s mode)", dev->name, cint(dev) ? "INTERRUPT" : "POLLING");
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
		return 1; // 0;
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

	if (rx_len > NET_ETH_MTU + sizeof(struct net_eth_hdr) + 4 || rx_len < 4) {
		LOG_ERR("%s: RX length error len=%u", dev->name, rx_len);
		return -EINVAL;
	}

	/* rx_len from chip includes 4-byte CRC, but net_pkt is for frame data only */
	uint16_t frame_len = rx_len - 4;

	/* Allocate packet buffer for the frame with timeout retry strategy
	 * First attempt: use configured timeout
	 * If that fails, try with K_NO_WAIT in case buffers become available
	 */
	pkt = net_pkt_rx_alloc_with_buffer(context->iface, frame_len, AF_UNSPEC, 0,
					   K_MSEC(config->timeout));
	if (!pkt) {
		/* Retry once without blocking - buffers may have been freed by RX thread */
		pkt = net_pkt_rx_alloc_with_buffer(context->iface, frame_len, AF_UNSPEC, 0,
						   K_NO_WAIT);
		if (!pkt) {
			LOG_WRN("%s: RX buffer allocation failed (size=%u, available pools: "
				"RX_PKT=%d, RX_BUF=%d) - discarding packet",
				dev->name, frame_len,
				CONFIG_NET_PKT_RX_COUNT, CONFIG_NET_BUF_RX_COUNT);
			/* Discard the packet from DM9051's memory to prevent blocking */
			uint8_t dummy[rx_len];
			dm9051_read_mem(dev, dummy, rx_len);
			eth_stats_update_errors_rx(context->iface);
			return -ENOMEM;
		}
	}

	/* Read frame data into buffer fragments using the local implementation */
	if (local_net_pkt_write_from(pkt, dm9051_read_mem_cb, (void *)dev, frame_len)) {
		LOG_ERR("%s: Failed to write packet into fragments", dev->name);
		net_pkt_unref(pkt);
		/* Attempt to discard the rest of the packet to prevent being stuck */
		uint8_t dummy[rx_len];
		dm9051_read_mem(dev, dummy, rx_len);
		return -EIO;
	}

	/* Read and discard the 4-byte CRC to clear the RX buffer */
	uint8_t crc_buf[4];
	dm9051_read_mem(dev, crc_buf, 4);

	dm9051_write_reg(dev, DM9051_ISR, 0x80);

	net_pkt_set_iface(pkt, context->iface);

	/* Feed to network stack */
	if (net_recv_data(context->iface, pkt) < 0) {
		net_pkt_unref(pkt);
		return -EIO;
	}

	// LOG_DBG("%s: RX packet len=%u", dev->name, rx_len);
	return 0;
}

/*******************************************************************************
 * RX Thread
 ******************************************************************************/

static uint8_t dm9051_link_status(const struct device *dev)
{
	// uint16_t bmsr;
	uint8_t nsr;
	struct dm9051_runtime *context = dev->data;

	// bmsr = dm9051_phy_read(dev, PHY_STATUS_REG);
	nsr = dm9051_read_reg(dev, DM9051_NSR);
	// if (bmsr == 0xffff) {
	//	LOG_ERR("%s: PHY read failed", dev->name);
	//	return;
	// }
	if (nsr == 0xff) {
		LOG_ERR("%s: NSR read failed", dev->name);
		return 0xff;
	}

	// if (bmsr & 0x01) --- PHY_STATUS_LINK = 0x0004
	if (nsr & NSR_LINKST) {
		if (context->link_up != true) {
			printk("\n(link_status.o=%d)\n", endc++);
			LOG_INF("%s: Link up", dev->name);
			context->link_up = true;
			net_eth_carrier_on(context->iface);
		}
	} else {
		if (context->link_up != false) {
			printk("\n(link_status.x=%d)\n", endc++);
			LOG_INF("%s: Link down", dev->name);
			context->link_up = false;
			net_eth_carrier_off(context->iface);
		}
	}
	return nsr;
}

/**
 * @brief GPIO interrupt callback for DM9051
 * @param dev GPIO device (unused)
 * @param cb Callback structure
 * @param pins Pins that triggered the interrupt
 */
static void dm9051_gpio_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);

	struct dm9051_runtime *context = CONTAINER_OF(cb, struct dm9051_runtime, gpio_cb);

	// dm9051_interrupt_disble_irq(dev);
	// printk("---------DM9051 INT.s pins=0x%x sem_count=%u--------\n", 
	//       pins, k_sem_count_get(&context->int_sem));
	k_sem_give(&context->int_sem);
}

/**
 * @brief RX thread for polling and processing incoming packets
 * @param arg1 Device structure pointer
 * @param arg2 Unused
 * @param arg3 Unused
 */
static void dm9051_rx_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	const struct device *dev = arg1;
	struct dm9051_runtime *context = dev->data;
	uint32_t int_count = 0;
	uint8_t flg_print_rx_status = 0;

	while (1) {
		int loop_count = 0;
		if (cint(dev)) {
			/* Interrupt mode: wait for GPIO interrupt signal */
			int res = k_sem_take(&context->int_sem, K_MSEC(100));
			if (res == 0) {
				if (int_count % 100 == 0) {
					flg_print_rx_status = 1;
					printk("--------- %5d DM9051 INT.s sem_count=%u --------\n", 
					       int_count, k_sem_count_get(&context->int_sem)+1);
				}
			} else {
				/* Interrupt mode, Semaphore timeout - when no interrupt received
				 * could support update link status */
				dm9051_link_status(dev);
				continue;
			}
			dm9051_interrupt_disble_irq(dev);
		} else {
			/* Polling mode: periodic check every 10ms */
			k_sem_take(&context->int_sem, K_MSEC(10));
		}

		/* Take semaphore to protect SPI access */
		k_sem_take(&context->tx_rx_sem, K_FOREVER);

		/* Process all available packets */
		while (dm9051_rx_packet(dev) == 0) {
			loop_count++;
		}

		/* Release semaphore */
		k_sem_give(&context->tx_rx_sem);
		if (flg_print_rx_status) {
			flg_print_rx_status = 0;
			printk("---------%5d DM9051 INT.e sem_count=%u nRX=%d--------\n", 
			       int_count, k_sem_count_get(&context->int_sem), loop_count);
		}
		int_count++;
		dm9051_interrupt_reset_for_cb_sem(dev);
	}
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
	uint8_t rcr_value;

	switch (type) {
	case ETHERNET_CONFIG_TYPE_MAC_ADDRESS:
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

		printk("%s: _dm9051_set_config: Interface configured [(set mac address)]\n",
		       dev->name);
		return 0;

#ifdef CONFIG_NET_PROMISCUOUS_MODE
	case ETHERNET_CONFIG_TYPE_PROMISC_MODE:
		k_sem_take(&context->tx_rx_sem, K_FOREVER);

		/* Read current RCR value */
		rcr_value = dm9051_read_reg(dev, DM9051_RCR);

		if (config->promisc_mode) {
			/* Enable promiscuous mode */
			rcr_value |= RCR_PRMSC;
			LOG_INF("%s: Promiscuous mode enabled", dev->name);
		} else {
			/* Disable promiscuous mode */
			rcr_value &= ~RCR_PRMSC;
			LOG_INF("%s: Promiscuous mode disabled", dev->name);
		}

		/* Write updated RCR value */
		dm9051_write_reg(dev, DM9051_RCR, rcr_value);

		k_sem_give(&context->tx_rx_sem);
		return 0;
#endif

#ifdef CONFIG_ETH_DM9051_MULTICAST_FILTER
	case ETHERNET_CONFIG_TYPE_FILTER:
		/* Configure MAC Address Register (MAR) for multicast filtering */
		if (config->filter.type == ETHERNET_FILTER_TYPE_SET_MULTICAST) {
			const struct ethernet_filter_multicast *filter = &config->filter.multicast;
			uint32_t hash;
			uint8_t mar[8] = {0};

			k_sem_take(&context->tx_rx_sem, K_FOREVER);

			/* Calculate hash for the multicast address */
			hash = ((uint32_t)filter->mac_address.addr[0] << 24) |
			       ((uint32_t)filter->mac_address.addr[1] << 16) |
			       ((uint32_t)filter->mac_address.addr[2] << 8) |
			       ((uint32_t)filter->mac_address.addr[3]);

			/* Use CRC to determine hash table position */
			uint8_t hash_bit = (hash >> 26) & 0x3f;
			uint8_t mar_index = hash_bit / 8;
			uint8_t bit_index = hash_bit % 8;

			/* Read current MAR values */
			for (int i = 0; i < 8; i++) {
				mar[i] = dm9051_read_reg(dev, DM9051_MAR + i);
			}

			if (filter->enable) {
				/* Set bit in hash table */
				mar[mar_index] |= BIT(bit_index);
				LOG_DBG("%s: Added multicast filter for "
					"%02x:%02x:%02x:%02x:%02x:%02x",
					dev->name, filter->mac_address.addr[0],
					filter->mac_address.addr[1],
					filter->mac_address.addr[2],
					filter->mac_address.addr[3],
					filter->mac_address.addr[4],
					filter->mac_address.addr[5]);
			} else {
				/* Clear bit in hash table */
				mar[mar_index] &= ~BIT(bit_index);
				LOG_DBG("%s: Removed multicast filter for "
					"%02x:%02x:%02x:%02x:%02x:%02x",
					dev->name, filter->mac_address.addr[0],
					filter->mac_address.addr[1],
					filter->mac_address.addr[2],
					filter->mac_address.addr[3],
					filter->mac_address.addr[4],
					filter->mac_address.addr[5]);
			}

			/* Write updated MAR values */
			for (int i = 0; i < 8; i++) {
				dm9051_write_reg(dev, DM9051_MAR + i, mar[i]);
			}

			k_sem_give(&context->tx_rx_sem);
			return 0;
		}

		return -ENOTSUP;
#endif

	default:
		break;
	}

	LOG_DBG("%s: Unsupported configuration type %d", dev->name, type);
	return -ENOTSUP;
}

static void eth_dm9051_iface_init(struct net_if *iface)
{
	const struct device *dev = net_if_get_device(iface);
	struct dm9051_runtime *context = dev->data;
	int through_c = endc++;

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

	/* Create RX thread for packet reception */
	k_thread_create(&context->thread, context->thread_stack,
			CONFIG_ETH_DM9051_RX_THREAD_STACK_SIZE, dm9051_rx_thread, (void *)dev, NULL,
			NULL, K_PRIO_COOP(2), /* High priority for network RX */
			0, K_NO_WAIT);
	k_thread_name_set(&context->thread, "dm9051_rx");

	printk("(end.e=%d)\n", through_c);
	printk("iface_init.e\n");
}

static const struct ethernet_api api_funcs = {
	.iface_api.init = eth_dm9051_iface_init,
	.set_config = eth_dm9051_set_config,
	.get_capabilities = eth_dm9051_get_capabilities,
	.send = eth_dm9051_tx,
};

void dm9051_init_log(const struct device *dev)
{
	// const struct dm9051_config *config = dev->config;
	printk("_eth_dm9051_init: INFO: ========================================\n");
	printk("_eth_dm9051_init: INFO: dev->name = %s\n", dev->name);
	printk("_eth_dm9051_init: INFO: dev->config->spi.bus->name: %s\n",
	       ((struct dm9051_config *)dev->config)->spi.bus->name);
	printk("_eth_dm9051_init: INFO: dev->config->spi.config.frequency: %u MHz\n",
	       ((struct dm9051_config *)dev->config)->spi.config.frequency / 1000000);
	// printk("_eth_dm9051_init: INFO: dev->config->spi.config.cs.gpio.port->name: %s, Pin: %d "
	//" (but now hard code Pin: %d)\n",
	// dev->config->spi.config.cs.gpio.port->name, dev->config->spi.config.cs.gpio.pin, 2);
	printk("_eth_dm9051_init: INFO: ========================================\n");
}

/*******************************************************************************
 * Device Initialization
 ******************************************************************************/

/**
 * @brief Detect and verify DM9051 chip ID
 * @param dev Device structure
 * @return Chip ID on success, 0 on failure
 */
static uint16_t dm9051_detect_id(const struct device *dev)
{
	uint16_t chip_id;

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
		printk("_eth_dm9051_init: ERROR: Invalid chip ID: 0x%04x (expected 0x9051 or "
		       "0x9058)\n",
		       chip_id);

		while (1) {
			chip_id = dm9051_get_chipid(dev);
			if (chip_id == 0x9051 || chip_id == 0x9058) {
				printk("\nINFO: DM9051 chip ID verified succeed: 0x%04x", chip_id);
				break;
			}
			printk(" INFO: DM9051 chip ID verified failed: 0x%04x", chip_id);
			printk(" (LOOP-TEST: delay)");
			k_msleep(1000);
		}
		return 0;
	}

	return chip_id;
}

static int eth_dm9051_init(const struct device *dev)
{
	const struct dm9051_config *config = dev->config;
	struct dm9051_runtime *context = dev->data;
	uint16_t chip_id;

	/* Check SPI is ready */
	if (!spi_is_ready_dt(&config->spi)) {
		printk("_eth_dm9051_init: SPI not ready\n");
		return -ENODEV;
	}

	/* Print SPI configuration */
	printk("\n\n");
	printk("_eth_dm9051_init: eth_dm9051_init.s8.7\n");
	dm9051_init_log(dev); /* Print detailed GPIO information */
	printk("_eth_dm9051_init: CS automatically controlled by SPI driver (P1.2)\n");

	/* CS GPIO is automatically configured and controlled by SPI driver layer.
	 * No manual GPIO configuration needed when cs-gpios is set in device tree.
	 */

	/* Configure interrupt GPIO if int-gpios is defined in device tree */
	if (cint(dev)) {
		printk("_eth_dm9051_init: Configuring INTERRUPT mode\n");

		if (!gpio_is_ready_dt(&config->interrupt)) {
			LOG_ERR("GPIO port %s not ready", config->interrupt.port->name);
			return -EINVAL;
		}

		if (gpio_pin_configure_dt(&config->interrupt, GPIO_INPUT)) {
			LOG_ERR("Unable to configure GPIO pin %u", config->interrupt.pin);
			return -EINVAL;
		}

		gpio_init_callback(&context->gpio_cb, dm9051_gpio_callback,
				   BIT(config->interrupt.pin));

		if (gpio_add_callback(config->interrupt.port, &(context->gpio_cb))) {
			return -EINVAL;
		}

		gpio_pin_interrupt_configure_dt(&config->interrupt, GPIO_INT_EDGE_FALLING);
		printk("_eth_dm9051_init: Interrupt GPIO configured - Port: %s, Pin: %d\n",
		       config->interrupt.port->name, config->interrupt.pin);
	} else {
		printk("_eth_dm9051_init: Configuring POLLING mode (no int-gpios defined)\n");
	}

	/* Detect and verify chip ID */
	chip_id = dm9051_detect_id(dev);
	if (chip_id == 0) {
		return -ENODEV;
	}

	/* Perform core reset */
	dm9051_core_reset(dev);

	/* Decide MAC address: DT local-mac-address > NVS > random */
	dm9051_init_mac(dev);

	/* Set MAC address */
	dm9051_set_mac_address(dev, context->mac_address); // to be checked! more!

	/* Configure receive */
	dm9051_set_receive(dev);

	/* Set carrier on after successful initialization */
	context->iface_carrier_on_init = true;

	printk("(end.e=%d) Configuring %s\n", endc++,
	       cint(dev) ? "INTERRUPT mode" : "POLL mode");
	printk("dm9051_init.e: (set mac address, %02x:%02x:%02x:%02x:%02x:%02x) Chip ID: "
	       "0x%04x\n",
	       context->mac_address[0], context->mac_address[1], context->mac_address[2],
	       context->mac_address[3], context->mac_address[4], context->mac_address[5], chip_id);
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
		.link_up = false,                                                                  \
	};                                                                                         \
                                                                                                   \
	static const struct dm9051_config dm9051_config_##inst = {                                 \
		.spi = SPI_DT_SPEC_INST_GET(inst, SPI_WORD_SET(8), 0),                             \
		.interrupt = GPIO_DT_SPEC_INST_GET(inst, int_gpios),                               \
		.timeout = 500,                                                                    \
	};                                                                                         \
                                                                                                   \
	ETH_NET_DEVICE_DT_INST_DEFINE(inst, eth_dm9051_init, NULL, &dm9051_runtime_##inst,         \
				      &dm9051_config_##inst, CONFIG_ETH_INIT_PRIORITY, &api_funcs, \
				      NET_ETH_MTU);

DT_INST_FOREACH_STATUS_OKAY(DM9051_DEFINE);

// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *     Jean-Jacques Hiblot <jjhiblot@ti.com>
 */

#include <common.h>
#include <dm.h>
#include <board.h>
#include <i2c.h>
#include <asm/gpio.h>
#include <asm/arch/hardware.h>
#include <../board/ti/common/board_detect.h>
#include "ti-board.h"

/* Daughter card presence detection signals */
enum {
	AM65X_EVM_APP_BRD_DET,
	AM65X_EVM_LCD_BRD_DET,
	AM65X_EVM_SERDES_BRD_DET,
	AM65X_EVM_HDMI_GPMC_BRD_DET,
	AM65X_EVM_BRD_DET_COUNT,
};
#define DAUGHTER_CARD_NO_OF_MAC_ADDR       8

/* Declaration of daughtercards to probe */
struct ext_card {
	u8 slot_index;		/* Slot the card is installed */
	char *card_name;	/* EEPROM-programmed card name */
	char *dtbo_name;	/* Device tree overlay to apply */
	u8 eth_offset;		/* ethXaddr MAC address index offset */
};

/*
* Daughter card presence detection signal name to GPIO (via I2C I/O
* expander @ address 0x38) name and EEPROM I2C address mapping.
*/
static const struct {
	char *gpio_name;
	u8 i2c_addr;
} slot_map[AM65X_EVM_BRD_DET_COUNT] = {
	{ "gpio@38_0", 0x52, },	/* AM65X_EVM_APP_BRD_DET */
	{ "gpio@38_1", 0x55, },	/* AM65X_EVM_LCD_BRD_DET */
	{ "gpio@38_2", 0x54, },	/* AM65X_EVM_SERDES_BRD_DET */
	{ "gpio@38_3", 0x53, },	/* AM65X_EVM_HDMI_GPMC_BRD_DET */
};

static const struct ext_card ext_cards[] = {
	{
		AM65X_EVM_APP_BRD_DET,
		"AM6-GPAPPEVM",
		"k3-am654-gp.dtbo",
		0,
	},
	{
		AM65X_EVM_APP_BRD_DET,
		"AM6-IDKAPPEVM",
		"k3-am654-idk.dtbo",
		3,
	},
	{
		AM65X_EVM_SERDES_BRD_DET,
		"SER-PCIE2LEVM",
		"k3-am654-pcie-usb2.dtbo",
		0,
	},
	{
		AM65X_EVM_SERDES_BRD_DET,
		"SER-PCIEUSBEVM",
		"k3-am654-pcie-usb3.dtbo",
		0,
	},
	{
		AM65X_EVM_LCD_BRD_DET,
		"OLDI-LCD1EVM",
		"k3-am654-evm-oldi-lcd1evm.dtbo",
		0,
	},
};

typedef char macaddr[ETH_ALEN];

struct daughter_card_info {
	const struct ext_card * extcard_info;
	macaddr *macs;
	u8 nb_macs;
};

struct board_am65x_evm_priv {
	bool detection_done;

	char *name;
	char *version;
	char *serial;
	char *software_revision;
	macaddr *macs;
	u8 nb_macs;

	int nb_daughter_cards;
	struct daughter_card_info *cards[ARRAY_SIZE(ext_cards)];
};


static const struct daughter_card_info *detected_card(struct board_am65x_evm_priv *priv,
					    int index)
{
	if (index < priv->nb_daughter_cards)
		return priv->cards[index];
	return NULL;
}

static int init_daughtercard_det_gpio(char *gpio_name, struct gpio_desc *desc)
{
	int ret;

	memset(desc, 0, sizeof(*desc));

	ret = dm_gpio_lookup_name(gpio_name, desc);
	if (ret < 0)
		return ret;

	/* Request GPIO, simply re-using the name as label */
	ret = dm_gpio_request(desc, gpio_name);
	if (ret < 0)
		return ret;

	return dm_gpio_set_dir_flags(desc, GPIOD_IS_IN);
}

static int board_am65x_evm_detect(struct udevice *dev)
{
	struct board_am65x_evm_priv *priv = dev_get_priv(dev);
	struct ti_am6_eeprom ep;
	struct gpio_desc board_det_gpios[AM65X_EVM_BRD_DET_COUNT];
	u8 mac_addr_cnt = 0;
	char mac_addr[DAUGHTER_CARD_NO_OF_MAC_ADDR][ETH_ALEN];
	int i;
	int ret;

	/* detection is done only once */
	if (priv->detection_done)
		return 0;

	ret = ti_i2c_eeprom_am6_get(CONFIG_EEPROM_BUS_ADDRESS,
				    CONFIG_EEPROM_CHIP_ADDRESS,
				    &ep, (char **)mac_addr,
				    AM6_EEPROM_HDR_NO_OF_MAC_ADDR,
				    &mac_addr_cnt);

	if (ret) {
		pr_err("Reading on-board EEPROM at 0x%02x failed %d\n",
		       CONFIG_EEPROM_CHIP_ADDRESS, ret);
		return ret;
	}
	if (ep.header == TI_DEAD_EEPROM_MAGIC) {
		pr_err("Invalid EEPROM MAGIC at 0x%02x\n",
		       CONFIG_EEPROM_CHIP_ADDRESS);
		return -EIO;
	}

	priv->name = strdup(ep.name);
	priv->serial = strdup(ep.serial);
	priv->version = strdup(ep.version);
	priv->software_revision = strdup(ep.software_revision);
	if (mac_addr_cnt) {
		priv->macs = malloc(mac_addr_cnt * ETH_ALEN);
		memcpy(priv->macs, mac_addr, mac_addr_cnt * ETH_ALEN);
		priv->nb_macs = mac_addr_cnt;
	}

	/*
	 * Initialize GPIO used for daughtercard slot presence detection and
	 * keep the resulting handles in local array for easier access.
	 */
	for (i = 0; i < AM65X_EVM_BRD_DET_COUNT; i++) {
		ret = init_daughtercard_det_gpio(slot_map[i].gpio_name,
						 &board_det_gpios[i]);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < ARRAY_SIZE(ext_cards); i++) {
		/* Obtain card-specific slot index and associated I2C address */
		u8 slot_index = ext_cards[i].slot_index;
		u8 i2c_addr = slot_map[slot_index].i2c_addr;
		struct daughter_card_info *dci;

		mac_addr_cnt = 0;

		/*
		 * The presence detection signal is active-low, hence skip
		 * over this card slot if anything other than 0 is returned.
		 */
		ret = dm_gpio_get_value(&board_det_gpios[slot_index]);
		if (ret < 0)
			goto err;
		else if (ret)
			continue;

		/* Get and parse the daughter card EEPROM record */
		ret = ti_i2c_eeprom_am6_get(CONFIG_EEPROM_BUS_ADDRESS, i2c_addr,
					    &ep, (char **)mac_addr,
					    DAUGHTER_CARD_NO_OF_MAC_ADDR,
					    &mac_addr_cnt);
		if (ret) {
			pr_err("Reading daughtercard EEPROM at 0x%02x failed %d\n",
			       i2c_addr, ret);
			/*
			 * Even this is pretty serious let's just skip over
			 * this particular daughtercard, rather than ending
			 * the probing process altogether.
			 */
			continue;
		}

		/* Only process the parsed data if we found a match */
		if (strncmp(ep.name, ext_cards[i].card_name, sizeof(ep.name)))
			continue;

		dci = calloc(1, sizeof(*dci));
		if (!dci) {
			pr_debug("unable to allocate daughter card info\n");
			ret = -ENOMEM;
			goto err;
		}

		if (mac_addr_cnt) {
			dci->macs = malloc(mac_addr_cnt * ETH_ALEN);
			memcpy(dci->macs, mac_addr, mac_addr_cnt * ETH_ALEN);
			dci->nb_macs = mac_addr_cnt;
		}
		dci->extcard_info = &ext_cards[i];
		priv->cards[priv->nb_daughter_cards] = dci;
		priv->nb_daughter_cards++;

	}

	priv->detection_done = true;
	gpio_free_list(dev, board_det_gpios, AM65X_EVM_BRD_DET_COUNT);

	return 0;

err:
	gpio_free_list(dev, board_det_gpios, AM65X_EVM_BRD_DET_COUNT);
	return ret;
}

static int board_am65x_evm_get_fit_loadable(struct udevice *dev, int index,
					    const char *type, const char **strp)
{
	struct board_am65x_evm_priv *priv = dev_get_priv(dev);
	const struct daughter_card_info *card;


	if (!strcmp(type, FIT_FDT_PROP)) {
		card = detected_card(priv, index);
		if (card) {
			*strp = card->extcard_info->dtbo_name;
			return 0;
		}
	}

	return -ENOENT;
}


static int board_am65x_evm_get_mac(struct udevice *dev, int id, u8 mac[ETH_ALEN])
{
	struct board_am65x_evm_priv *priv = dev_get_priv(dev);
	const struct daughter_card_info *card;
	macaddr *macs;
	int nb_macs;
	int card_idx = TI_BOARD_CARD(id);
	int mac_idx =  TI_BOARD_INDEX(id);

	if (card_idx == 0) {
		macs = priv->macs;
		nb_macs = priv->nb_macs;
	} else {
		card = detected_card(priv, card_idx -1);
		if (!card)
			return -ENODEV;
		macs = card->macs;
		nb_macs = card->nb_macs;
	}

	if (mac_idx >= nb_macs)
		return -ERANGE;

	memcpy(mac, macs[mac_idx], ETH_ALEN);

	return 0;
}

static int board_am65x_evm_get_int(struct udevice *dev, int id, int *val)
{
	struct board_am65x_evm_priv *priv = dev_get_priv(dev);
	const struct daughter_card_info *card;
	int card_idx = TI_BOARD_CARD(id);

	switch (TI_BOARD_INDEX(id) & 0xFF)
	{
		case INT_NB_DAUGHTER_BOARDS:
			*val = priv->nb_daughter_cards;
			break;
		case INT_ETH_OFFSET:
			if (card_idx == 0) {
				/*
				* The first MAC address for ethernet a.k.a.
				* ethernet0 comes from efuse populated via the
				* am654 gigabit eth switch subsystem driver.
				*/
				*val = 1;
				break;
			}
			card = detected_card(priv, card_idx - 1);
			if (!card)
				return -ENODEV;
			*val = card->extcard_info->eth_offset;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static int board_am65x_evm_get_str(struct udevice *dev, int id,
				   size_t size, char *val)
{
	struct board_am65x_evm_priv *priv = dev_get_priv(dev);
	const char *s;

	switch (TI_BOARD_INDEX(id))
	{
		case STR_NAME:
			s = priv->name;
			break;
		case STR_VERSION:
			s = priv->version;
			break;
		case STR_SERIAL:
			s = priv->serial;
			break;
		case STR_SW_REV:
			s = priv->software_revision;
			break;
		default:
			return -EINVAL;
	}
	if (!s)
		return -ENODATA;

	strlcpy(val, s, size);
	return 0;
}

static const struct udevice_id board_am65x_evm_ids[] = {
	{ .compatible = "ti,am654-evm" },
	{ /* sentinel */ }
};

static const struct board_ops board_am65x_evm_ops = {
	.detect = board_am65x_evm_detect,
	.get_fit_loadable = board_am65x_evm_get_fit_loadable,
	.get_mac_addr = board_am65x_evm_get_mac,
	.get_int = board_am65x_evm_get_int,
	.get_str = board_am65x_evm_get_str,
};

static int board_am65x_evm_probe(struct udevice *dev)
{
	return 0;
}

U_BOOT_DRIVER(board_am65x_evm) = {
	.name           = "board_am65x_evm",
	.id             = UCLASS_BOARD,
	.of_match       = board_am65x_evm_ids,
	.ops		= &board_am65x_evm_ops,
	.priv_auto_alloc_size = sizeof(struct board_am65x_evm_priv),
	.probe          = board_am65x_evm_probe,
};

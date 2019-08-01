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
#include "am6_board_detect.h"
#include "ti-board.h"

#define _CARD(id) ((id >> 8) & 0xFF)
#define _INDEX(id)((id >> 0) & 0xFF)

struct k3_ext_card {
	u8 slot_index;		/* Slot the card is installed */
	char *card_name;	/* EEPROM-programmed card name */
	char *dtbo_name;	/* Device tree overlay to apply */
	int overlay_level;	/* Order in which the overlays are applied */
	u8 eth_offset;		/* ethXaddr MAC address index offset */
};

struct k3_boards_slot {
	char *gpio_name;
	u8 i2c_addr;
};

struct k3_boards_drv_data {
	int i2c_bus;
	int eeprom_addr;
	const struct k3_boards_slot *slots;
	const struct k3_ext_card *ext_cards;
};

struct card_info {
	char name[AM6_EEPROM_HDR_NAME_LEN + 1];
	char version[AM6_EEPROM_HDR_VERSION_LEN + 1];
	char software_revision[AM6_EEPROM_HDR_SW_REV_LEN + 1];
	char serial[AM6_EEPROM_HDR_SERIAL_LEN + 1];
	int nb_macs;
	char (*macs)[ETH_ALEN];
	const struct k3_ext_card *ext;
	struct card_info *next;
};

struct k3_boards_priv {
	struct card_info *head;
};

/* Daughter card presence detection signals */
enum am65x_slots {
	AM65X_EVM_APP_BRD_DET,
	AM65X_EVM_LCD_BRD_DET,
	AM65X_EVM_SERDES_BRD_DET,
	AM65X_EVM_HDMI_GPMC_BRD_DET,
};

/*
 * Daughter card presence detection signal name to GPIO (via I2C I/O
 * expander @ address 0x38) name and EEPROM I2C address mapping.
 */
static const struct k3_boards_slot am65x_evm_slots[] = {
	{ "gpio@38_0", 0x52},	/* AM65X_EVM_APP_BRD_DET */
	{ "gpio@38_1", 0x55},	/* AM65X_EVM_LCD_BRD_DET */
	{ "gpio@38_2", 0x54},	/* AM65X_EVM_SERDES_BRD_DET */
	{ "gpio@38_3", 0x53},	/* AM65X_EVM_HDMI_GPMC_BRD_DET */
	{ NULL, 0xFF}
};

static const struct k3_ext_card am65x_evm_ext_cards[] = {
	{
		AM65X_EVM_APP_BRD_DET,
		"AM6-GPAPPEVM",
		"k3-am654-gp", 0,
		0,
	},
	{
		AM65X_EVM_APP_BRD_DET,
		"AM6-IDKAPPEVM",
		"k3-am654-idk", 2,
		3,
	},
	{
		AM65X_EVM_SERDES_BRD_DET,
		"SER-PCIE2LEVM",
		"k3-am654-pcie-usb2", 1,
		0,
	},
	{
		AM65X_EVM_SERDES_BRD_DET,
		"SER-PCIEUSBEVM",
		"k3-am654-pcie-usb3", 1,
		0,
	},
	{
		AM65X_EVM_LCD_BRD_DET,
		"OLDI-LCD1EVM",
		"k3-am654-evm-oldi-lcd1evm", 1,
		0,
	},
	{
		.card_name = NULL
	}
};

struct k3_boards_drv_data am65x_evm_drv_data = {
	.i2c_bus = 0,
	.eeprom_addr = 0x50,
	.slots = am65x_evm_slots,
	.ext_cards = am65x_evm_ext_cards,
};

static const struct k3_boards_slot j721e_evm_slots[] = {
	{ NULL, 0x51},
	{ NULL, 0x52},
	{ NULL, 0x54},
	{ NULL, 0xFF}
};

static const struct k3_ext_card j721e_evm_ext_cards[] = {
	{
		0,
		"J7X-BASE-CPB",
		"", 0,		/* No dtbo for this board */
		0,
	},
	{
		1,
		"J7X-INFOTAN-EXP",
		"k3-j721e-common-proc-board-infotainment", 1,
		0,
	},
	{
		1,
		"J7X-GESI-EXP",
		"", 0,		/* No dtbo for this board */
		5,		/* Start populating from eth5addr */
	},
	{
		2,
		"J7X-VSC8514-ETH",
		"", 0,		/* No dtbo for this board */
		1,		/* Start populating from eth1addr */
	},
	{
		.card_name = NULL
	}
};

struct k3_boards_drv_data j721e_evm_drv_data = {
	.i2c_bus = 0,
	.eeprom_addr = 0x50,
	.slots = j721e_evm_slots,
	.ext_cards = j721e_evm_ext_cards,
};

/**
 * eeprom_string_cleanup() - Handle eeprom programming errors
 * @s:	eeprom string (should be NULL terminated)
 * @n:	max len of eeprom string
 *
 * Some Board manufacturers do not add a NULL termination at the
 * end of string, instead some binary information is kludged in, hence
 * convert the string to just printable characters of ASCII chart.
 */
static void eeprom_string_cleanup(char *s, int n)
{
	int i;

	for (i = 0; *s && i < n; i++, s++)
		if (*s < ' ' || *s > '~') {
			*s = 0;
			break;
		}
}

static int parse_board_info(struct udevice *dev, struct card_info *ci,
			    struct ti_am6_eeprom_record *rec)
{
	if (rec->header.len != sizeof(rec->data.board_info)) {
		debug("invalid record length\n");
		return -EINVAL;
	}

	/* Populate (and clean, if needed) the board name */
	strlcpy(ci->name, rec->data.board_info.name, sizeof(ci->name));
	eeprom_string_cleanup(ci->name, sizeof(ci->name));

	/* Populate selected other fields from the board info record */
	strlcpy(ci->version, rec->data.board_info.version, sizeof(ci->version));
	strlcpy(ci->software_revision, rec->data.board_info.software_revision,
		sizeof(ci->software_revision));
	strlcpy(ci->serial, rec->data.board_info.serial, sizeof(ci->serial));
	return 0;
}

static int parse_mac_info(struct udevice *dev, struct card_info *ci,
			  struct ti_am6_eeprom_record *rec)
{
	int ret;
	int cnt;
	char (*macs)[ETH_ALEN];

	if (rec->header.len != sizeof(rec->data.mac_info)) {
		debug("invalid record length\n");
		ret = -EINVAL;
		goto no_mac;
	}

	cnt = ((rec->data.mac_info.mac_control & TI_AM6_EEPROM_MAC_ADDR_COUNT_MASK)
		>> TI_AM6_EEPROM_MAC_ADDR_COUNT_SHIFT) + 1;
	if (cnt > TI_AM6_EEPROM_MAC_ADDR_MAX_COUNT) {
		debug("invalid MAC address count\n");
		ret = -EINVAL;
		goto no_mac;
	}

	if (!cnt) {
		ret = 0;
		goto no_mac;
	}

	macs = devm_kzalloc(dev, cnt * ETH_ALEN, 0);
	if (!macs) {
		ret = -ENOMEM;
		goto no_mac;
	}

	memcpy(macs, rec->data.mac_info.mac_addr, cnt * ETH_ALEN);
	ci->macs = macs;
	ci->nb_macs = cnt;

#ifdef DEBUG
	while (cnt--)
		debug(" MAC %d : %pM\n", cnt, macs[cnt]);

#endif
	return 0;

no_mac:
	ci->nb_macs = 0;
	ci->macs = NULL;
	return ret;
}

void dump_card_info(struct card_info *ci)
{
	int i;

	printf("name    %s\n", ci->name);
	printf("version %s\n", ci->version);
	printf("sw rev  %s\n", ci->software_revision);
	printf("serial  %s\n", ci->serial);
	for (i = 0; i < ci->nb_macs; i++)
		printf(" MAC %d : %pM\n", i, ci->macs[i]);
}

void dump_cards_info(struct card_info *head)
{
	while (head) {
		dump_card_info(head);
		head = head->next;
		if (head)
			printf("------------------------\n");
	};
}

struct card_info *probe_one_card(struct udevice *dev, int bus, int chip_addr)
{
	int ret;
	struct udevice *i2c;
	struct card_info *ci = NULL;
	struct ti_am6_eeprom_record_board_id board_id;
	int offset = 0;

	ret = i2c_get_chip_for_busnum(bus, chip_addr, 1, &i2c);
	if (ret) {
		debug("No chip detected on I2C%d @0x%x\n", bus, chip_addr);
		ret = -ENODEV;
		goto fail;
	}
	ret = i2c_set_chip_offset_len(i2c, 2);
	if (ret)
		goto fail;

	ci = devm_kzalloc(dev, sizeof(*ci), 0);
	if (!ci) {
		ret = -ENOMEM;
		goto fail;
	}

	ret = dm_i2c_read(i2c, offset, (u8 *)&board_id, sizeof(board_id));
	if (ret)
		goto fail;

	offset += sizeof(board_id);

	if (board_id.magic_number != TI_EEPROM_HEADER_MAGIC) {
		dev_err(dev, "Wrong magic number 0x%x (sould be 0x%x)\n",
			board_id.magic_number, TI_EEPROM_HEADER_MAGIC);
		ret = -EINVAL;
		goto fail;
	}

	while (offset < board_id.header.len + sizeof(board_id)) {
		struct ti_am6_eeprom_record record;

		ret = dm_i2c_read(i2c, offset, (u8 *)&record.header,
				  sizeof(record.header));
		if (ret)
			goto fail;
		offset += sizeof(record.header);

		if (record.header.id == TI_AM6_EEPROM_RECORD_END_LIST)
			break;

		ret = dm_i2c_read(i2c, offset, (u8 *)&record.data,
				  record.header.len);
		if (ret)
			goto fail;
		offset += record.header.len;

		switch (record.header.id) {
		case TI_AM6_EEPROM_RECORD_BOARD_INFO:
			ret = parse_board_info(dev, ci, &record);
			break;
		case TI_AM6_EEPROM_RECORD_MAC_INFO:
			ret = parse_mac_info(dev, ci, &record);
			break;
		case 0x00:
			/* Illegal value... Fall through... */
		case 0xFF:
			/* Illegal value... Something went horribly wrong... */
			ret = -EINVAL;
			break;
		default:
			dev_warn(dev, "%s: Ignoring record id %u\n", __func__,
				 record.header.id);
			break;
		}
		if (ret)
			goto fail;
	}

	return ci;
fail:
	if (ci)
		devm_kfree(dev, ci);

	return ERR_PTR(ret);
}

static const struct card_info *detected_card(struct k3_boards_priv *priv,
					     int index)
{
	int i = 0;
	struct card_info *ci = priv->head;

	while (ci) {
		if (i++ == index)
			return ci;
		ci = ci->next;
	}
	return NULL;
}

const struct k3_ext_card *find_matching_extcard(const struct k3_ext_card *cards,
						int slot, const char *name)
{
	int i;

	for (i = 0; cards[i].card_name; i++)
		if (slot == cards[i].slot_index &&
		    !strcmp(name, cards[i].card_name))
			return &cards[i];
	return NULL;
}

void insert_in_list(struct card_info *head, struct card_info *ci)
{
	int level = ci->ext->overlay_level;
	struct card_info *prev = head;

	while (head) {
		if (head->ext && head->ext->overlay_level > level)
			break;
		prev = head;
		head = head->next;
	}

	ci->next = prev->next;
	prev->next = ci;
}

#if CONFIG_IS_ENABLED(DM_GPIO)
int presence_gpio_value(const char *name)
{
	int ret;
	struct gpio_desc desc;

	if (!name)
		return -EINVAL;

	memset(&desc, 0, sizeof(desc));

	ret = dm_gpio_lookup_name(name, &desc);
	if (ret < 0)
		return ret;

	ret = dm_gpio_request(&desc, name);
	if (ret < 0)
		return ret;

	ret = dm_gpio_set_dir_flags(&desc, GPIOD_IS_IN);
	if (ret < 0)
		goto end;

	ret = dm_gpio_get_value(&desc);
	if (ret < 0)
		goto end;

	dm_gpio_free(NULL, &desc);
	return ret ? 1 : 0;

end:
	dm_gpio_free(NULL, &desc);
	return ret;
}
#else
static inline int presence_gpio_value(const char *name)
{
	return 0;
}

#endif

static int k3_board_detect(struct udevice *dev)
{
	struct k3_boards_priv *priv = dev_get_priv(dev);
	struct k3_boards_drv_data *data;
	struct card_info *head;
	int i, ret;

	/* detection already done ? */
	if (priv->head)
		return 0;

	data = (struct k3_boards_drv_data *)dev_get_driver_data(dev);

	head = probe_one_card(dev, data->i2c_bus, data->eeprom_addr);
	if (IS_ERR(head))
		return PTR_ERR(head);

	priv->head = head;

	for (i = 0; data->slots[i].i2c_addr != 0xFF; i++) {
		struct card_info *ci;
		const struct k3_ext_card *ext;

		if (data->slots[i].gpio_name) {
			ret = presence_gpio_value(data->slots[i].gpio_name);
			if (ret < 0 || ret == 1)
				continue;
		}

		ci = probe_one_card(dev, data->i2c_bus,
				    data->slots[i].i2c_addr);
		if (IS_ERR(ci))
			continue;

		ext = find_matching_extcard(data->ext_cards, i, ci->name);
		if (!ext) {
			devm_kfree(dev, ci);
			continue;
		}
		ci->ext = ext;
		insert_in_list(head, ci);
	}

#ifdef DEBUG
	dump_cards_info(head);
#endif
	return 0;
}

static int k3_board_get_fit_loadable(struct udevice *dev, int index,
				     const char *type, const char **strp)
{
	struct k3_boards_priv *priv = dev_get_priv(dev);

	if (!strcmp(type, FIT_FDT_PROP)) {
		/* Add 1 to index, since the first card never has an overlay */
		const struct card_info *ci = detected_card(priv, index + 1);

		if (ci) {
			*strp = ci->ext->dtbo_name;
			return 0;
		}
	}

	return -ENOENT;
}

static int k3_board_get_mac(struct udevice *dev, int id, u8 mac[ETH_ALEN])
{
	struct k3_boards_priv *priv = dev_get_priv(dev);
	int card_idx = _CARD(id);
	int mac_idx =  _INDEX(id);
	const struct card_info *ci = detected_card(priv, card_idx);

	if (!ci)
		return -ENODEV;

	if (mac_idx >= ci->nb_macs)
		return -ERANGE;

	memcpy(mac, ci->macs[mac_idx], ETH_ALEN);

	return 0;
}

static int count_cards(struct card_info *head)
{
	int i = 0;

	while (head) {
		i++;
		head = head->next;
	}
	return i;
}

static int k3_board_get_int(struct udevice *dev, int id, int *val)
{
	struct k3_boards_priv *priv = dev_get_priv(dev);
	int card_idx = _CARD(id);
	const struct card_info *ci;

	switch (_INDEX(id)) {
	case INT_NB_DAUGHTER_BOARDS:
		*val = count_cards(priv->head);
		break;
	case INT_ETH_OFFSET:
		ci = detected_card(priv, card_idx);
		if (!ci)
			return -ENODEV;

		if (!ci->ext)
			return -ENODATA;

		*val = ci->ext->eth_offset;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int k3_board_get_str(struct udevice *dev, int id,
			    size_t size, char *val)
{
	int card_idx = _CARD(id);
	struct k3_boards_priv *priv = dev_get_priv(dev);
	const struct card_info *ci = detected_card(priv, card_idx);
	const char *s;

	if (!ci)
		return -ENODEV;

	switch (_INDEX(id)) {
	case STR_NAME:
		s = ci->name;
		break;
	case STR_VERSION:
		s = ci->version;
		break;
	case STR_SERIAL:
		s = ci->serial;
		break;
	case STR_SW_REV:
		s = ci->software_revision;
		break;
	default:
		return -EINVAL;
	}
	if (!s)
		return -ENODATA;

	strlcpy(val, s, size);

	return 0;
}

static const struct udevice_id k3_board_ids[] = {
	{ .compatible = "ti,am654-evm", .data = (ulong)&am65x_evm_drv_data },
	{ .compatible = "ti,j721e-evm", .data = (ulong)&j721e_evm_drv_data },
	{ /* sentinel */ }
};

static const struct board_ops k3_board_ops = {
	.detect = k3_board_detect,
	.get_fit_loadable = k3_board_get_fit_loadable,
	.get_mac_addr = k3_board_get_mac,
	.get_int = k3_board_get_int,
	.get_str = k3_board_get_str,
};

U_BOOT_DRIVER(board_k3_evms) = {
	.name           = "board_k3_evms",
	.id             = UCLASS_BOARD,
	.of_match       = k3_board_ids,
	.ops		= &k3_board_ops,
	.priv_auto_alloc_size = sizeof(struct k3_boards_priv),
};

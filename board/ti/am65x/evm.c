// SPDX-License-Identifier: GPL-2.0+
/*
 * Board specific initialization for AM654 EVM
 *
 * Copyright (C) 2017-2018 Texas Instruments Incorporated - http://www.ti.com/
 *	Lokesh Vutla <lokeshvutla@ti.com>
 *
 */

#include <common.h>
#include <board.h>
#include <dm.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/hardware.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/omap_common.h>
#include <env.h>
#include <spl.h>
#include <asm/arch/sys_proto.h>
#include "../../../drivers/board/ti/ti-board.h"


DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	return 0;
}

int dram_init(void)
{
#ifdef CONFIG_PHYS_64BIT
	gd->ram_size = 0x100000000;
#else
	gd->ram_size = 0x80000000;
#endif

	return 0;
}

ulong board_get_usable_ram_top(ulong total_size)
{
#ifdef CONFIG_PHYS_64BIT
	/* Limit RAM used by U-Boot to the DDR low region */
	if (gd->ram_top > 0x100000000)
		return 0x100000000;
#endif

	return gd->ram_top;
}

int dram_init_banksize(void)
{
	/* Bank 0 declares the memory available in the DDR low region */
	gd->bd->bi_dram[0].start = CONFIG_SYS_SDRAM_BASE;
	gd->bd->bi_dram[0].size = 0x80000000;

#ifdef CONFIG_PHYS_64BIT
	/* Bank 1 declares the memory available in the DDR high region */
	gd->bd->bi_dram[1].start = CONFIG_SYS_SDRAM_BASE1;
	gd->bd->bi_dram[1].size = 0x80000000;
#endif

	return 0;
}

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
#ifdef CONFIG_TARGET_AM654_A53_EVM
	if (!strcmp(name, "k3-am654-base-board"))
		return 0;
#endif

	return -1;
}
#endif

#if defined(CONFIG_OF_LIBFDT) && defined(CONFIG_OF_BOARD_SETUP)
int ft_board_setup(void *blob, bd_t *bd)
{
	int ret;

	ret = fdt_fixup_msmc_ram(blob, "/interconnect@100000", "sram@70000000");
	if (ret) {
		printf("%s: fixing up msmc ram failed %d\n", __func__, ret);
		return ret;
	}

#if defined(CONFIG_TI_SECURE_DEVICE)
	/* Make HW RNG reserved for secure world use */
	ret = fdt_disable_node(blob, "/interconnect@100000/trng@4e10000");
	if (ret) {
		printf("%s: disabling TRGN failed %d\n", __func__, ret);
		return ret;
	}
#endif

	return 0;
}
#endif

int checkboard(void)
{
	/* default value when EEPROM not populated */
	char name[20] =  "AM6-COMPROCEVM";
	char version[20] = "E3";
	struct udevice *board;

	board_get(&board);
	if (board_detect(board)) {
		board_get_str(board, STR_NAME, sizeof(name), name);
		board_get_str(board, STR_VERSION, sizeof(version), version);
	}

	printf("Board: %s rev %s\n", name, version);

	return 0;
}

#ifdef CONFIG_SPL_BUILD
void spl_board_init(void)
{
	struct udevice *board;

	/*
	 * We ned to detect the daughter cards before the SPL loads
	 * the dtb overlays from the FIT image
	 */
	if (!board_get(&board))
		board_detect(board);
}
#else
static void set_single_env_str(struct udevice *board, const char *env, int id)
{
	char s[20];

	if (!board_get_str(board, id, sizeof(s), s))
		env_set(env, s);
	else
		env_set(env, "unknown");
}

void setup_board_eeprom_env(struct udevice *board, const char *name)
{
	if (name)
		env_set("board_name", name);
	else
		set_single_env_str(board, "board_name", STR_NAME);

	set_single_env_str(board, "board_rev", STR_VERSION);
	set_single_env_str(board, "board_software_revision", STR_SW_REV);
	set_single_env_str(board, "board_serial", STR_SERIAL);
}

static void setup_mac_addresses(struct udevice *b)
{
	int card, j, rc;
	u8 mac[ETH_ALEN];
	int macbase = 0;

	for (card = 0; ; card++) {
		rc = board_get_int(b, CARD_ID(card, INT_ETH_OFFSET),
				   &macbase);
		if (rc == -ENODATA && card == 0) {
			/*
			* The first MAC address for ethernet a.k.a.
			* ethernet0 comes from efuse populated via the
			* am654 gigabit eth switch subsystem driver.
			*/
			macbase = 1;
		} else if (rc < 0) {
			break;
		}

		for (j = 0; !board_get_mac_addr(b, CARD_ID(card, j), mac); j++)
			if (is_valid_ethaddr((u8 *)mac))
				eth_env_set_enetaddr_by_index("eth",
							      macbase + j,
							      mac);
	}
}

static int setup_overlays_env(struct udevice *board)
{
	int i;
	const char suffix[] = ".dtbo ";
	char name_overlays[1024] = { 0 };
	char *p = name_overlays;
	int remaining_sz = sizeof(name_overlays) - 1;
	const char *s = NULL;

	for (i = 0; !board_get_fit_loadable(board, i, FIT_FDT_PROP, &s); i++) {
		int l = strlen(s) + sizeof(suffix);

		if (l < remaining_sz) {
			strcpy(p, s);
			strcat(p, suffix);
			remaining_sz -= l;
			p += l;
		} else {
			pr_err("no room to add %s to name_overlays\n", s);
			return -ENOMEM;
		}
	}
	if (s)
		env_set("name_overlays", name_overlays);

	return 0;
}

int board_late_init(void)
{
	struct udevice *board;
	int ret;

	ret = board_get(&board);
	if (ret) {
		pr_warn("Cannot find board device\n");
		goto end;
	}

	ret = board_detect(board);
	if (ret) {
		pr_warn("Board detection failed\n");
		goto end;
	}

	setup_board_eeprom_env(board, "am65x");
	setup_mac_addresses(board);
	setup_overlays_env(board);

end:
	return 0;

}
#endif

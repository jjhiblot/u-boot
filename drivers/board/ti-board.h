/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *     Jean-Jacques Hiblot <jjhiblot@ti.com>
 */
#define TI_BOARD_ID(card, idx) (((card & 0xFF) << 8) | (idx & 0xFF))
#define TI_BOARD_CARD(id) ((id >> 8) & 0xFF)
#define TI_BOARD_INDEX(id)((id >> 0) & 0xFF)

enum {
	INT_NB_DAUGHTER_BOARDS,
	INT_ETH_OFFSET
};

enum {
	STR_NAME,
	STR_VERSION,
	STR_SERIAL,
	STR_SW_REV,
};

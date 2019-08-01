// SPDX-License-Identifier: GPL-2.0+

#define TI_EEPROM_HEADER_MAGIC	0xEE3355AA

/* AM6x TI EVM EEPROM Definitions */
#define TI_AM6_EEPROM_RECORD_BOARD_ID		0x01
#define TI_AM6_EEPROM_RECORD_BOARD_INFO		0x10
#define TI_AM6_EEPROM_RECORD_DDR_INFO		0x11
#define TI_AM6_EEPROM_RECORD_DDR_SPD		0x12
#define TI_AM6_EEPROM_RECORD_MAC_INFO		0x13
#define TI_AM6_EEPROM_RECORD_END_LIST		0xFE

/*
 * Common header for AM6x TI EVM EEPROM records. Used to encapsulate the config
 * EEPROM in its entirety as well as for individual records contained within.
 */
struct ti_am6_eeprom_record_header {
	u8 id;
	u16 len;
} __attribute__ ((__packed__));

/* AM6x TI EVM EEPROM board ID structure */
struct ti_am6_eeprom_record_board_id {
	u32 magic_number;
	struct ti_am6_eeprom_record_header header;
} __attribute__ ((__packed__));

/* AM6x TI EVM EEPROM board info structure */
#define AM6_EEPROM_HDR_NAME_LEN			16
#define AM6_EEPROM_HDR_VERSION_LEN		2
#define AM6_EEPROM_HDR_PROC_NR_LEN		4
#define AM6_EEPROM_HDR_VARIANT_LEN		2
#define AM6_EEPROM_HDR_PCB_REV_LEN		2
#define AM6_EEPROM_HDR_SCH_BOM_REV_LEN		2
#define AM6_EEPROM_HDR_SW_REV_LEN		2
#define AM6_EEPROM_HDR_VID_LEN			2
#define AM6_EEPROM_HDR_BLD_WK_LEN		2
#define AM6_EEPROM_HDR_BLD_YR_LEN		2
#define AM6_EEPROM_HDR_4P_NR_LEN		6
#define AM6_EEPROM_HDR_SERIAL_LEN		4

struct ti_am6_eeprom_record_board_info {
	char name[AM6_EEPROM_HDR_NAME_LEN];
	char version[AM6_EEPROM_HDR_VERSION_LEN];
	char proc_number[AM6_EEPROM_HDR_PROC_NR_LEN];
	char variant[AM6_EEPROM_HDR_VARIANT_LEN];
	char pcb_revision[AM6_EEPROM_HDR_PCB_REV_LEN];
	char schematic_bom_revision[AM6_EEPROM_HDR_SCH_BOM_REV_LEN];
	char software_revision[AM6_EEPROM_HDR_SW_REV_LEN];
	char vendor_id[AM6_EEPROM_HDR_VID_LEN];
	char build_week[AM6_EEPROM_HDR_BLD_WK_LEN];
	char build_year[AM6_EEPROM_HDR_BLD_YR_LEN];
	char board_4p_number[AM6_EEPROM_HDR_4P_NR_LEN];
	char serial[AM6_EEPROM_HDR_SERIAL_LEN];
} __attribute__ ((__packed__));

/* Memory location to keep a copy of the AM6 board info record */
#define TI_AM6_EEPROM_BD_INFO_DATA ((struct ti_am6_eeprom_record_board_info *) \
					     TI_SRAM_SCRATCH_BOARD_EEPROM_START)

/* AM6x TI EVM EEPROM DDR info structure */
#define TI_AM6_EEPROM_DDR_CTRL_INSTANCE_MASK		GENMASK(1, 0)
#define TI_AM6_EEPROM_DDR_CTRL_INSTANCE_SHIFT		0
#define TI_AM6_EEPROM_DDR_CTRL_SPD_DATA_LOC_MASK	GENMASK(3, 2)
#define TI_AM6_EEPROM_DDR_CTRL_SPD_DATA_LOC_NA		(0 << 2)
#define TI_AM6_EEPROM_DDR_CTRL_SPD_DATA_LOC_BOARDID	(2 << 2)
#define TI_AM6_EEPROM_DDR_CTRL_SPD_DATA_LOC_I2C51	(3 << 2)
#define TI_AM6_EEPROM_DDR_CTRL_MEM_TYPE_MASK		GENMASK(5, 4)
#define TI_AM6_EEPROM_DDR_CTRL_MEM_TYPE_DDR3		(0 << 4)
#define TI_AM6_EEPROM_DDR_CTRL_MEM_TYPE_DDR4		(1 << 4)
#define TI_AM6_EEPROM_DDR_CTRL_MEM_TYPE_LPDDR4		(2 << 4)
#define TI_AM6_EEPROM_DDR_CTRL_IF_DATA_WIDTH_MASK	GENMASK(7, 6)
#define TI_AM6_EEPROM_DDR_CTRL_IF_DATA_WIDTH_16		(0 << 6)
#define TI_AM6_EEPROM_DDR_CTRL_IF_DATA_WIDTH_32		(1 << 6)
#define TI_AM6_EEPROM_DDR_CTRL_IF_DATA_WIDTH_64		(2 << 6)
#define TI_AM6_EEPROM_DDR_CTRL_DEV_DATA_WIDTH_MASK	GENMASK(9, 8)
#define TI_AM6_EEPROM_DDR_CTRL_DEV_DATA_WIDTH_8		(0 << 8)
#define TI_AM6_EEPROM_DDR_CTRL_DEV_DATA_WIDTH_16	(1 << 8)
#define TI_AM6_EEPROM_DDR_CTRL_DEV_DATA_WIDTH_32	(2 << 8)
#define TI_AM6_EEPROM_DDR_CTRL_RANKS_2			BIT(10)
#define TI_AM6_EEPROM_DDR_CTRL_DENS_MASK		GENMASK(13, 11)
#define TI_AM6_EEPROM_DDR_CTRL_DENS_1GB			(0 << 11)
#define TI_AM6_EEPROM_DDR_CTRL_DENS_2GB			(1 << 11)
#define TI_AM6_EEPROM_DDR_CTRL_DENS_4GB			(2 << 11)
#define TI_AM6_EEPROM_DDR_CTRL_DENS_8GB			(3 << 11)
#define TI_AM6_EEPROM_DDR_CTRL_DENS_12GB		(4 << 11)
#define TI_AM6_EEPROM_DDR_CTRL_DENS_16GB		(5 << 11)
#define TI_AM6_EEPROM_DDR_CTRL_DENS_24GB		(6 << 11)
#define TI_AM6_EEPROM_DDR_CTRL_DENS_32GB		(7 << 11)
#define TI_AM6_EEPROM_DDR_CTRL_ECC			BIT(14)

struct ti_am6_eeprom_record_ddr_info {
	u16 ddr_control;
} __attribute__ ((__packed__));

/* AM6x TI EVM EEPROM DDR SPD structure */
#define TI_AM6_EEPROM_DDR_SPD_INSTANCE_MASK		GENMASK(1, 0)
#define TI_AM6_EEPROM_DDR_SPD_INSTANCE_SHIFT		0
#define TI_AM6_EEPROM_DDR_SPD_MEM_TYPE_MASK		GENMASK(4, 3)
#define TI_AM6_EEPROM_DDR_SPD_MEM_TYPE_DDR3		(0 << 3)
#define TI_AM6_EEPROM_DDR_SPD_MEM_TYPE_DDR4		(1 << 3)
#define TI_AM6_EEPROM_DDR_SPD_MEM_TYPE_LPDDR4		(2 << 3)
#define TI_AM6_EEPROM_DDR_SPD_DATA_LEN			512

struct ti_am6_eeprom_record_ddr_spd {
	u16 spd_control;
	u8 data[TI_AM6_EEPROM_DDR_SPD_DATA_LEN];
} __attribute__ ((__packed__));

/* AM6x TI EVM EEPROM MAC info structure */
#define TI_AM6_EEPROM_MAC_INFO_INSTANCE_MASK		GENMASK(2, 0)
#define TI_AM6_EEPROM_MAC_INFO_INSTANCE_SHIFT		0
#define TI_AM6_EEPROM_MAC_ADDR_COUNT_MASK		GENMASK(7, 3)
#define TI_AM6_EEPROM_MAC_ADDR_COUNT_SHIFT		3
#define TI_AM6_EEPROM_MAC_ADDR_MAX_COUNT		32

struct ti_am6_eeprom_record_mac_info {
	u16 mac_control;
	u8 mac_addr[TI_AM6_EEPROM_MAC_ADDR_MAX_COUNT][ETH_ALEN];
} __attribute__ ((__packed__));

struct ti_am6_eeprom_record {
	struct ti_am6_eeprom_record_header header;
	union {
		struct ti_am6_eeprom_record_board_info board_info;
		struct ti_am6_eeprom_record_ddr_info ddr_info;
		struct ti_am6_eeprom_record_ddr_spd ddr_spd;
		struct ti_am6_eeprom_record_mac_info mac_info;
	} data;
} __attribute__ ((__packed__));

// SPDX-License-Identifier: GPL-2.0+
/*
 * J721E: SoC specific initialization
 *
 * Copyright (C) 2018-2019 Texas Instruments Incorporated - http://www.ti.com/
 *	Lokesh Vutla <lokeshvutla@ti.com>
 */

#include <common.h>
#include <spl.h>
#include <asm/io.h>
#include <asm/armv7_mpu.h>
#include <asm/arch/hardware.h>
#include <asm/arch/sysfw-loader.h>
#include "common.h"
#include <asm/arch/sys_proto.h>
#include <linux/soc/ti/ti_sci_protocol.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <dm/pinctrl.h>
#include <clk.h>
#include <remoteproc.h>

extern void reinit_mmc_device(int dev);

#ifdef CONFIG_SPL_BUILD
static void mmr_unlock(u32 base, u32 partition)
{
	/* Translate the base address */
	phys_addr_t part_base = base + partition * CTRL_MMR0_PARTITION_SIZE;

	/* Unlock the requested partition if locked using two-step sequence */
	writel(CTRLMMR_LOCK_KICK0_UNLOCK_VAL, part_base + CTRLMMR_LOCK_KICK0);
	writel(CTRLMMR_LOCK_KICK1_UNLOCK_VAL, part_base + CTRLMMR_LOCK_KICK1);
}

void setup_initiator_credentials(void)
{
	u32 i;

	/* Initiators for virtid=2 */
	/* MMC1*/
	writel(DMSC_QOS_PVU_CTX(2), DMSC_QOS_MMC1_RD_MAP);
	writel(DMSC_QOS_PVU_CTX(2), DMSC_QOS_MMC1_WR_MAP);

	/* DSS.VID1 */
	writel(DMSC_QOS_PVU_CTX(2), DMSC_QOS_DSS_DMA_MAP + 0 * 4);
	writel(DMSC_QOS_PVU_CTX(2), DMSC_QOS_DSS_DMA_MAP + 1 * 4);

	/* DSS.VID2 */
	writel(DMSC_QOS_PVU_CTX(2), DMSC_QOS_DSS_DMA_MAP + 4 * 4);
	writel(DMSC_QOS_PVU_CTX(2), DMSC_QOS_DSS_DMA_MAP + 5 * 4);

	/* DSS.VIDL2 */
	writel(DMSC_QOS_PVU_CTX(2), DMSC_QOS_DSS_DMA_MAP + 6 * 4);
	writel(DMSC_QOS_PVU_CTX(2), DMSC_QOS_DSS_DMA_MAP + 7 * 4);

	/* Initiators for virtid=3 */
	/* MMC0 */
	writel(DMSC_QOS_PVU_CTX(3), DMSC_QOS_MMC0_RD_MAP);
	writel(DMSC_QOS_PVU_CTX(3), DMSC_QOS_MMC0_WR_MAP);

	/* DSS.VIDL1 */
	writel(DMSC_QOS_PVU_CTX(3), DMSC_QOS_DSS_DMA_MAP + 2 * 4);
	writel(DMSC_QOS_PVU_CTX(3), DMSC_QOS_DSS_DMA_MAP + 3 * 4);

	/* GPU OS_id=0, chanid=[0-3] */
	for (i = 0; i < 4; i++) {
		writel(DMSC_QOS_PVU_CTX(3),
			(volatile unsigned int *)DMSC_QOS_GPU_M0_RD_MAP + i);
		writel(DMSC_QOS_PVU_CTX(3),
			(volatile unsigned int *)DMSC_QOS_GPU_M0_WR_MAP + i);
		writel(DMSC_QOS_PVU_CTX(3),
			(volatile unsigned int *)DMSC_QOS_GPU_M1_RD_MAP + i);
		writel(DMSC_QOS_PVU_CTX(3),
			(volatile unsigned int *)DMSC_QOS_GPU_M1_WR_MAP + i);
	}

	/* D5520 chanid=[0-1] */
	for (i = 0; i < 2; i++) {
		writel(DMSC_QOS_PVU_CTX(3),
			(volatile unsigned int *)DMSC_QOS_D5520_RD_MAP + i);
		writel(DMSC_QOS_PVU_CTX(3),
			(volatile unsigned int *)DMSC_QOS_D5520_WR_MAP + i);
	}
}

static void ctrl_mmr_unlock(void)
{
	/* Unlock all WKUP_CTRL_MMR0 module registers */
	mmr_unlock(WKUP_CTRL_MMR0_BASE, 0);
	mmr_unlock(WKUP_CTRL_MMR0_BASE, 1);
	mmr_unlock(WKUP_CTRL_MMR0_BASE, 2);
	mmr_unlock(WKUP_CTRL_MMR0_BASE, 3);
	mmr_unlock(WKUP_CTRL_MMR0_BASE, 4);
	mmr_unlock(WKUP_CTRL_MMR0_BASE, 6);
	mmr_unlock(WKUP_CTRL_MMR0_BASE, 7);

	/* Unlock all MCU_CTRL_MMR0 module registers */
	mmr_unlock(MCU_CTRL_MMR0_BASE, 0);
	mmr_unlock(MCU_CTRL_MMR0_BASE, 1);
	mmr_unlock(MCU_CTRL_MMR0_BASE, 2);
	mmr_unlock(MCU_CTRL_MMR0_BASE, 3);
	mmr_unlock(MCU_CTRL_MMR0_BASE, 4);

	/* Unlock all CTRL_MMR0 module registers */
	mmr_unlock(CTRL_MMR0_BASE, 0);
	mmr_unlock(CTRL_MMR0_BASE, 1);
	mmr_unlock(CTRL_MMR0_BASE, 2);
	mmr_unlock(CTRL_MMR0_BASE, 3);
	mmr_unlock(CTRL_MMR0_BASE, 4);
	mmr_unlock(CTRL_MMR0_BASE, 5);
	mmr_unlock(CTRL_MMR0_BASE, 6);
	mmr_unlock(CTRL_MMR0_BASE, 7);
}

/*
 * This uninitialized global variable would normal end up in the .bss section,
 * but the .bss is cleared between writing and reading this variable, so move
 * it to the .data section.
 */
u32 bootindex __attribute__((section(".data")));

static void store_boot_index_from_rom(void)
{
	bootindex = *(u32 *)(CONFIG_SYS_K3_BOOT_PARAM_TABLE_INDEX);
}

#ifdef CONFIG_K3_LOAD_SYSFW
static void j721e_config_pm_done_callback(void)
{
	struct udevice *dev;
	int ret;

	if (spl_boot_device() == BOOT_DEVICE_MMC1)
		reinit_mmc_device(0);
	else if (spl_boot_device() == BOOT_DEVICE_MMC2)
		reinit_mmc_device(1);

	if (spl_boot_device() == BOOT_DEVICE_HYPERFLASH) {
		ret = uclass_find_first_device(UCLASS_MTD, &dev);
		if (ret)
			panic("%s: Can't find HyperFlash device! (%d)\n",
			      __func__, ret);

		/* Reconfigure HBMC clk */
		clk_set_defaults(dev);
	}
}
#endif

#ifdef CONFIG_CPU_V7R
void setup_dss_credentials(void)
{
	unsigned int ch, group;
	phys_addr_t *pMapReg, *pMap1Reg, *pMap2Reg;

	/* set order ID for DSS masters, there are 10 masters in DSS */
	for (ch = 0; ch < 10; ch++) {
		pMapReg = (phys_addr_t *)((uintptr_t)DSS_DMA_QOS_BASE + 0x100 + (4 * ch));
		pMapReg[ch] &= ~(GENMASK(7, 4));
		pMapReg[ch] |= (0x9 << 4);  /* Set orderid=9 */
	}

	for (group = 0; group < 2; group++) {
		pMap1Reg = (phys_addr_t *)((uintptr_t)DSS_DMA_QOS_BASE + 0x0 + (8 * group));
		pMap2Reg = (phys_addr_t *)((uintptr_t)DSS_DMA_QOS_BASE + 0x4 + (8 * group));
		*pMap1Reg = 0x76543210;
		*pMap2Reg = 0xfedcba98;
	}

	/* Setup NB configuration */
	*((unsigned int *)(CSL_NAVSS0_NBSS_NB0_CFG_MMRS_BASE + 0x10)) = 2;
	*((unsigned int *)(CSL_NAVSS0_NBSS_NB1_CFG_MMRS_BASE + 0x10)) = 2;
	*((unsigned int *)(CSL_DSS0_VIDL1_BASE + 0x3C)) = 0xFFF0800; /* 20000 */
	*((unsigned int *)(CSL_DSS0_VIDL2_BASE + 0x3C)) = 0xFFF0800; /* 30000 */
	*((unsigned int *)(CSL_DSS0_VID1_BASE + 0x3C)) = 0xFFF0800; /* 50000 */
	*((unsigned int *)(CSL_DSS0_VID2_BASE + 0x3C)) = 0xFFF0800; /* 60000 */
}

static void j721e_setup_drive_strength(void)
{
	u8 i, do_wa = 0;

	/*
	 * 0x0 in LVCMOS drive strength in any of the registers imply a bad
	 * sample, So, mark such a sample to be reprogrammed.
	 */
	for (i = 0; i < CTRLMMR_LVCMOS_DRIVE_REG_COUNT; i++) {
		if (!readl(CTRLMMR_LVCMOS_DRIVE_H_BASE + (i * 0x4)) ||
		    !readl(CTRLMMR_LVCMOS_DRIVE_V_BASE + (i * 0x4))) {
			do_wa = 1;
			break;
		}
	}

	/* Apply Workaround for ALL registers assume bad strength programming */
	if (do_wa == 1) {
		for (i = 0; i < CTRLMMR_LVCMOS_DRIVE_REG_COUNT; i++) {
			writel(CTRLMMR_LVCMOS_DRIVE_DEFAULT_VAL,
			       CTRLMMR_LVCMOS_DRIVE_H_BASE + (i * 0x4));
			writel(CTRLMMR_LVCMOS_DRIVE_DEFAULT_VAL,
			       CTRLMMR_LVCMOS_DRIVE_V_BASE + (i * 0x4));
		}
	}
}

#endif

void board_init_f(ulong dummy)
{
#if defined(CONFIG_K3_J721E_DDRSS) || defined(CONFIG_K3_LOAD_SYSFW)
	struct udevice *dev;
	int ret;
#endif
	/*
	 * Cannot delay this further as there is a chance that
	 * K3_BOOT_PARAM_TABLE_INDEX can be over written by SPL MALLOC section.
	 */
	store_boot_index_from_rom();

	/* Make all control module registers accessible */
	ctrl_mmr_unlock();

#ifdef CONFIG_CPU_V7R
	setup_k3_mpu_regions();
	setup_initiator_credentials();

	setup_dss_credentials();

	j721e_setup_drive_strength();

	/*
	 * When running SPL on R5 we are using SRAM for BSS to have global
	 * data etc. working prior to relocation. Since this means we need
	 * to self-manage BSS, clear that section now.
	 */
	memset(__bss_start, 0, __bss_end - __bss_start);
#endif

	/* Init DM early */
	spl_early_init();

#ifdef CONFIG_K3_EARLY_CONS
	/*
	 * Allow establishing an early console as required for example when
	 * doing a UART-based boot. Note that this console may not "survive"
	 * through a SYSFW PM-init step and will need a re-init in some way
	 * due to changing module clock frequencies.
	 */
	switch (spl_boot_device()) {
	case BOOT_DEVICE_UART:
		early_console_init();
		break;
	}
#endif

#ifdef CONFIG_K3_LOAD_SYSFW
	/*
	 * Process pinctrl for the serial0 a.k.a. MCU_UART0 module and continue
	 * regardless of the result of pinctrl. Do this without probing the
	 * device, but instead by searching the device that would request the
	 * given sequence number if probed. The UART will be used by the system
	 * firmware (SYSFW) image for various purposes and SYSFW depends on us
	 * to initialize its pin settings.
	 */
	ret = uclass_find_device_by_seq(UCLASS_SERIAL, 0, true, &dev);
	if (!ret)
		pinctrl_select_state(dev, "default");

	/*
	 * Load, start up, and configure system controller firmware while
	 * also populating the SYSFW post-PM configuration callback hook.
	 */
	k3_sysfw_loader(j721e_config_pm_done_callback);
#endif

	/* Prepare console output */
	preloader_console_init();

#ifdef CONFIG_K3_LOAD_SYSFW
	/* Output System Firmware version info */
	k3_sysfw_loader_print_ver();
#endif

	/* Perform EEPROM-based board detection */
	do_board_detect();

#if defined(CONFIG_CPU_V7R) && defined(CONFIG_K3_AVS0)
	ret = uclass_get_device(UCLASS_AVS, 0, &dev);
	if (ret)
		printf("AVS init failed: %d\n", ret);
#endif

#if defined(CONFIG_K3_J721E_DDRSS)
	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (ret)
		panic("DRAM init failed: %d\n", ret);
#endif
}

u32 spl_boot_mode(const u32 boot_device)
{
	u32 wkup_devstat = readl(CTRLMMR_WKUP_DEVSTAT);
	u32 main_devstat;
	u32 bootmode;

	if (wkup_devstat & WKUP_DEVSTAT_MCU_OMLY_MASK) {
		printf("ERROR: MCU only boot is not yet supported\n");
		return MMCSD_MODE_RAW;
	}

	main_devstat = readl(CTRLMMR_MAIN_DEVSTAT);

	bootmode = (wkup_devstat & WKUP_DEVSTAT_PRIMARY_BOOTMODE_MASK) >>
			WKUP_DEVSTAT_PRIMARY_BOOTMODE_SHIFT;

	bootmode |= (main_devstat & MAIN_DEVSTAT_BOOT_MODE_B_MASK) <<
			BOOT_MODE_B_SHIFT;

	if (bootmode == BOOT_DEVICE_MMC1)
		return MMCSD_MODE_EMMCBOOT;
	else
		return MMCSD_MODE_FS;
}

static u32 __get_backup_bootmedia(u32 main_devstat)
{
	u32 bkup_boot = (main_devstat & MAIN_DEVSTAT_BKUP_BOOTMODE_MASK) >>
			MAIN_DEVSTAT_BKUP_BOOTMODE_SHIFT;

	switch (bkup_boot) {
	case BACKUP_BOOT_DEVICE_DFU:
		return BOOT_DEVICE_DFU;
	case BACKUP_BOOT_DEVICE_UART:
		return BOOT_DEVICE_UART;
	case BACKUP_BOOT_DEVICE_ETHERNET:
		return BOOT_DEVICE_ETHERNET;
	case BACKUP_BOOT_DEVICE_MMC2:
	{
		u32 port = (main_devstat & MAIN_DEVSTAT_BKUP_MMC_PORT_MASK) >>
			    MAIN_DEVSTAT_BKUP_MMC_PORT_SHIFT;
		if (port == 0x0)
			return BOOT_DEVICE_MMC1;
		return BOOT_DEVICE_MMC2;
	}
	case BACKUP_BOOT_DEVICE_SPI:
		return BOOT_DEVICE_SPI;
	case BACKUP_BOOT_DEVICE_I2C:
		return BOOT_DEVICE_I2C;
	}

	return BOOT_DEVICE_RAM;
}

static u32 __get_primary_bootmedia(u32 main_devstat, u32 wkup_devstat)
{

	u32 bootmode = (wkup_devstat & WKUP_DEVSTAT_PRIMARY_BOOTMODE_MASK) >>
			WKUP_DEVSTAT_PRIMARY_BOOTMODE_SHIFT;

	bootmode |= (main_devstat & MAIN_DEVSTAT_BOOT_MODE_B_MASK) <<
			BOOT_MODE_B_SHIFT;

	if (bootmode == BOOT_DEVICE_OSPI || bootmode ==	BOOT_DEVICE_QSPI)
		bootmode = BOOT_DEVICE_SPI;

	if (bootmode == BOOT_DEVICE_MMC2) {
		u32 port = (main_devstat &
			    MAIN_DEVSTAT_PRIM_BOOTMODE_MMC_PORT_MASK) >>
			   MAIN_DEVSTAT_PRIM_BOOTMODE_PORT_SHIFT;
		if (port == 0x0)
			bootmode = BOOT_DEVICE_MMC1;
	}

	return bootmode;
}

u32 spl_boot_device(void)
{
	u32 wkup_devstat = readl(CTRLMMR_WKUP_DEVSTAT);
	u32 main_devstat;

	if (wkup_devstat & WKUP_DEVSTAT_MCU_OMLY_MASK) {
		printf("ERROR: MCU only boot is not yet supported\n");
		return BOOT_DEVICE_RAM;
	}

	/* MAIN CTRL MMR can only be read if MCU ONLY is 0 */
	main_devstat = readl(CTRLMMR_MAIN_DEVSTAT);

	if (bootindex == K3_PRIMARY_BOOTMODE)
		return __get_primary_bootmedia(main_devstat, wkup_devstat);
	else
		return __get_backup_bootmedia(main_devstat);
}
#endif

#ifdef CONFIG_SYS_K3_SPL_ATF

#define J721E_DEV_MCU_RTI0			262
#define J721E_DEV_MCU_RTI1			263
#define J721E_DEV_MCU_ARMSS0_CPU0		250
#define J721E_DEV_MCU_ARMSS0_CPU1		251

void release_resources_for_core_shutdown(void)
{
	struct ti_sci_handle *ti_sci;
	struct ti_sci_dev_ops *dev_ops;
	struct ti_sci_proc_ops *proc_ops;
	int ret;
	u32 i;

	const u32 put_device_ids[] = {
		J721E_DEV_MCU_RTI0,
		J721E_DEV_MCU_RTI1,
	};

	ti_sci = get_ti_sci_handle();
	dev_ops = &ti_sci->ops.dev_ops;
	proc_ops = &ti_sci->ops.proc_ops;

	/* Iterate through list of devices to put (shutdown) */
	for (i = 0; i < ARRAY_SIZE(put_device_ids); i++) {
		u32 id = put_device_ids[i];

		ret = dev_ops->put_device(ti_sci, id);
		if (ret)
			panic("Failed to put device %u (%d)\n", id, ret);
	}

	const u32 put_core_ids[] = {
		J721E_DEV_MCU_ARMSS0_CPU1,
		J721E_DEV_MCU_ARMSS0_CPU0,	/* Handle CPU0 after CPU1 */
	};

	/* Iterate through list of cores to put (shutdown) */
	for (i = 0; i < ARRAY_SIZE(put_core_ids); i++) {
		u32 id = put_core_ids[i];

		/*
		 * Queue up the core shutdown request. Note that this call
		 * needs to be followed up by an actual invocation of an WFE
		 * or WFI CPU instruction.
		 */
		ret = proc_ops->proc_shutdown_no_wait(ti_sci, id);
		if (ret)
			panic("Failed sending core %u shutdown message (%d)\n",
			      id, ret);
	}
}
#endif

#ifdef CONFIG_SYS_K3_SPL_ATF
void start_non_linux_remote_cores(void)
{
	int size = 0, ret;
	u32 loadaddr = 0;

	size = load_firmware("mainr5f0_0fwname", "mainr5f0_0loadaddr",
			     &loadaddr);
	if (size <= 0)
		goto err_load;

	/* assuming remoteproc 2 is aliased for the needed remotecore */
	ret = rproc_load(2, loadaddr, size);
	if (ret) {
		printf("Firmware failed to start on rproc (%d)\n", ret);
		goto err_load;
	}

	ret = rproc_start(2);
	if (ret) {
		printf("Firmware init failed on rproc (%d)\n", ret);
		goto err_load;
	}

	printf("Remoteproc 2 started successfully\n");

	return;

err_load:
	rproc_reset(2);
}
#endif

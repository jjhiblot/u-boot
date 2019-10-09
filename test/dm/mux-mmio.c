// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2017-2018 Texas Instruments Incorporated - http://www.ti.com/
 * Jean-Jacques Hiblot <jjhiblot@ti.com>
 */

#include <common.h>
#include <fdtdec.h>
#include <dm.h>
#include <mux.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/test.h>
#include <dm/root.h>
#include <dm/test.h>
#include <dm/util.h>
#include <test/ut.h>

/* Test that mmio mux work correctly */
static int dm_test_mux_mmio(struct unit_test_state *uts)
{
	struct udevice *dev, *dev_b;
	struct regmap *map;
	struct mux_control *ctl0_a, *ctl0_b;
	struct mux_control *ctl1;
	struct mux_control *ctl_err;
	u32 val;
	int i;

	ut_assertok(uclass_get_device(UCLASS_TEST_FDT, 0, &dev));
	ut_assertok(uclass_get_device(UCLASS_TEST_FDT, 1, &dev_b));
	ut_asserteq_str("a-test", dev->name);
	ut_asserteq_str("b-test", dev_b->name);
	map = syscon_regmap_lookup_by_phandle(dev, "mux-syscon");
	ut_assert(!IS_ERR(map));
	ut_assert(map);

	/* check default states */
	ut_assertok(regmap_read(map, 3, &val));
	ut_asserteq(0x02, (val & 0x1E) >> 1);
	ut_assertok(regmap_read(map, 1, &val));
	ut_asserteq(0x73, (val & 0xFF) >> 0);

	ut_assertok(mux_control_get(dev, "mux0", &ctl0_a));
	ut_assertok(mux_control_get(dev, "mux1", &ctl1));
	ut_asserteq(-ERANGE, mux_control_get(dev, "mux3", &ctl_err));
	ut_asserteq(-ENODATA, mux_control_get(dev, "dummy", &ctl_err));
	ut_assertok(mux_control_get(dev_b, "mux0", &ctl0_b));

	for (i = 0; i < mux_control_states(ctl0_a); i++) {
		/* select a new state and verify the value in the regmap */
		ut_assertok(mux_control_select(ctl0_a, i));
		ut_assertok(regmap_read(map, 0, &val));
		ut_asserteq(i, (val & 0x30) >> 4);
		/*
		 * deselect the mux and verify that the value in the regmap
		 * reflects the idle state (fixed to MUX_IDLE_AS_IS)
		 */
		ut_assertok(mux_control_deselect(ctl0_a));
		ut_assertok(regmap_read(map, 0, &val));
		ut_asserteq(i, (val & 0x30) >> 4);
	}

	for (i = 0; i < mux_control_states(ctl1); i++) {
		/* select a new state and verify the value in the regmap */
		ut_assertok(mux_control_select(ctl1, i));
		ut_assertok(regmap_read(map, 3, &val));
		ut_asserteq(i, (val & 0x1E) >> 1);
		/*
		 * deselect the mux and verify that the value in the regmap
		 * reflects the idle state (fixed to 2)
		 */
		ut_assertok(mux_control_deselect(ctl1));
		ut_assertok(regmap_read(map, 3, &val));
		ut_asserteq(2, (val & 0x1E) >> 1);
	}

	// try unbalanced selection/deselection
	ut_assertok(mux_control_select(ctl0_a, 0));
	ut_asserteq(-EBUSY, mux_control_select(ctl0_a, 1));
	ut_asserteq(-EBUSY, mux_control_select(ctl0_a, 0));
	ut_assertok(mux_control_deselect(ctl0_a));

	// try concurent selection
	ut_assertok(mux_control_select(ctl0_a, 0));
	ut_assert(mux_control_select(ctl0_b, 0));
	ut_assertok(mux_control_deselect(ctl0_a));
	ut_assertok(mux_control_select(ctl0_b, 0));
	ut_assert(mux_control_select(ctl0_a, 0));
	ut_assertok(mux_control_deselect(ctl0_b));
	ut_assertok(mux_control_select(ctl0_a, 0));
	ut_assertok(mux_control_deselect(ctl0_a));

	return 0;
}
DM_TEST(dm_test_mux_mmio, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);

/* Test that managed API for mux work correctly */
static int dm_test_devm_mux_mmio(struct unit_test_state *uts)
{
	struct udevice *dev, *dev_b;
	struct mux_control *ctl0_a, *ctl0_b;
	struct mux_control *ctl1;
	struct mux_control *ctl_err;

	ut_assertok(uclass_get_device(UCLASS_TEST_FDT, 0, &dev));
	ut_assertok(uclass_get_device(UCLASS_TEST_FDT, 1, &dev_b));
	ut_asserteq_str("a-test", dev->name);
	ut_asserteq_str("b-test", dev_b->name);

	ctl0_a = devm_mux_control_get(dev, "mux0");
	ut_assertok(IS_ERR(ctl0_a));
	ut_assert(ctl0_a);
	ctl1 = devm_mux_control_get(dev, "mux1");
	ut_assertok(IS_ERR(ctl1));
	ut_assert(ctl1);
	ctl_err = devm_mux_control_get(dev, "mux3");
	ut_asserteq(-ERANGE, PTR_ERR(ctl_err));
	ctl_err = devm_mux_control_get(dev, "dummy");
	ut_asserteq(-ENODATA, PTR_ERR(ctl_err));

	ctl0_b = devm_mux_control_get(dev_b, "mux0");
	ut_assertok(IS_ERR(ctl0_b));
	ut_assert(ctl0_b);

	/* try concurent selection */
	ut_assertok(mux_control_select(ctl0_a, 0));
	ut_assert(mux_control_select(ctl0_b, 0));
	ut_assertok(mux_control_deselect(ctl0_a));
	ut_assertok(mux_control_select(ctl0_b, 0));
	ut_assert(mux_control_select(ctl0_a, 0));
	ut_assertok(mux_control_deselect(ctl0_b));

	/* removed one device and check that the mux is released */
	ut_assertok(mux_control_select(ctl0_a, 0));
	ut_assert(mux_control_select(ctl0_b, 0));
	device_remove(dev, DM_REMOVE_NORMAL);
	ut_assertok(mux_control_select(ctl0_b, 0));

	device_remove(dev_b, DM_REMOVE_NORMAL);
	return 0;
}
DM_TEST(dm_test_devm_mux_mmio, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);

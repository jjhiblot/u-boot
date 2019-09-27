// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2015 Google, Inc
 */

#include <common.h>
#include <dm.h>
#include <mapmem.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/test.h>
#include <dm/test.h>
#include <test/ut.h>

/* Base test of register maps */
static int dm_test_regmap_base(struct unit_test_state *uts)
{
	struct udevice *dev;
	struct regmap *map;
	ofnode node;
	int i;

	ut_assertok(uclass_get_device(UCLASS_SYSCON, 0, &dev));
	map = syscon_get_regmap(dev);
	ut_assertok_ptr(map);
	ut_asserteq(1, map->range_count);
	ut_asserteq(0x10, map->ranges[0].start);
	ut_asserteq(16, map->ranges[0].size);
	ut_asserteq(0x10, map_to_sysmem(regmap_get_range(map, 0)));

	ut_assertok(uclass_get_device(UCLASS_SYSCON, 1, &dev));
	map = syscon_get_regmap(dev);
	ut_assertok_ptr(map);
	ut_asserteq(4, map->range_count);
	ut_asserteq(0x20, map->ranges[0].start);
	for (i = 0; i < 4; i++) {
		const unsigned long addr = 0x20 + 8 * i;

		ut_asserteq(addr, map->ranges[i].start);
		ut_asserteq(5 + i, map->ranges[i].size);
		ut_asserteq(addr, map_to_sysmem(regmap_get_range(map, i)));
	}

	/* Check that we can't pretend a different device is a syscon */
	ut_assertok(uclass_get_device(UCLASS_I2C, 0, &dev));
	map = syscon_get_regmap(dev);
	ut_asserteq_ptr(ERR_PTR(-ENOEXEC), map);

	/* A different device can be a syscon by using Linux-compat API */
	node = ofnode_path("/syscon@2");
	ut_assert(ofnode_valid(node));

	map = syscon_node_to_regmap(node);
	ut_assertok_ptr(map);
	ut_asserteq(4, map->range_count);
	ut_asserteq(0x40, map->ranges[0].start);
	for (i = 0; i < 4; i++) {
		const unsigned long addr = 0x40 + 8 * i;

		ut_asserteq(addr, map->ranges[i].start);
		ut_asserteq(5 + i, map->ranges[i].size);
		ut_asserteq(addr, map_to_sysmem(regmap_get_range(map, i)));
	}

	return 0;
}
DM_TEST(dm_test_regmap_base, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);

/* Test we can access a regmap through syscon */
static int dm_test_regmap_syscon(struct unit_test_state *uts)
{
	struct regmap *map;

	map = syscon_get_regmap_by_driver_data(SYSCON0);
	ut_assertok_ptr(map);
	ut_asserteq(1, map->range_count);

	map = syscon_get_regmap_by_driver_data(SYSCON1);
	ut_assertok_ptr(map);
	ut_asserteq(4, map->range_count);

	map = syscon_get_regmap_by_driver_data(SYSCON_COUNT);
	ut_asserteq_ptr(ERR_PTR(-ENODEV), map);

	ut_asserteq(0x10, map_to_sysmem(syscon_get_first_range(SYSCON0)));
	ut_asserteq(0x20, map_to_sysmem(syscon_get_first_range(SYSCON1)));
	ut_asserteq_ptr(ERR_PTR(-ENODEV),
			syscon_get_first_range(SYSCON_COUNT));

	return 0;
}

DM_TEST(dm_test_regmap_syscon, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);

/* Read/Write/Modify test */
static int dm_test_regmap_rw(struct unit_test_state *uts)
{
	struct udevice *dev;
	struct regmap *map;
	uint reg;

	ut_assertok(uclass_get_device(UCLASS_SYSCON, 0, &dev));
	map = syscon_get_regmap(dev);
	ut_assertok_ptr(map);

	ut_assertok(regmap_write(map, 0, 0xcacafafa));
	ut_assertok(regmap_write(map, 3, 0x55aa2211));

	ut_assertok(regmap_read(map, 0, &reg));
	ut_assertok(regmap_read(map, 3, &reg));

	ut_assertok(regmap_update_bits(map, 0, 0xff00ff00, 0x55aa2211));
	ut_assertok(regmap_update_bits(map, 3, 0x00ff00ff, 0xcacafada));

	return 0;
}

DM_TEST(dm_test_regmap_rw, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);

/* Get/Set test */
static int dm_test_regmap_getset(struct unit_test_state *uts)
{
	struct udevice *dev;
	struct regmap *map;
	uint reg;
	struct layout {
		u32 val0;
		u32 val1;
		u32 val2;
		u32 val3;
	};

	ut_assertok(uclass_get_device(UCLASS_SYSCON, 0, &dev));
	map = syscon_get_regmap(dev);
	ut_assertok_ptr(map);

	regmap_set(map, struct layout, val0, 0xcacafafa);
	regmap_set(map, struct layout, val3, 0x55aa2211);

	ut_assertok(regmap_get(map, struct layout, val0, &reg));
	ut_assertok(regmap_get(map, struct layout, val3, &reg));

	return 0;
}

DM_TEST(dm_test_regmap_getset, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);

/* Read polling test */
static int dm_test_regmap_poll(struct unit_test_state *uts)
{
	struct udevice *dev;
	struct regmap *map;
	uint reg;
	unsigned long start;

	ut_assertok(uclass_get_device(UCLASS_SYSCON, 0, &dev));
	map = syscon_get_regmap(dev);
	ut_assertok_ptr(map);

	start = get_timer(0);

	ut_asserteq(-ETIMEDOUT,
		    regmap_read_poll_timeout_test(map, 0, reg,
						  (reg == 0xcacafafa),
						  1, 5 * CONFIG_SYS_HZ,
						  5 * CONFIG_SYS_HZ));

	ut_assert(get_timer(start) > (5 * CONFIG_SYS_HZ));

	return 0;
}

DM_TEST(dm_test_regmap_poll, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);

struct regmaptest_priv {
	struct regmap *regmap;
	struct regmap_field **fields;
};

#define REGMAP_TEST_BUF_SZ 12
struct regmaptest_context {
	unsigned short buffer[REGMAP_TEST_BUF_SZ];
} ctx;

static int regmaptest_write(void *context, unsigned int reg, unsigned int val)
{
	struct regmaptest_context *ctx = context;

	if (reg < ARRAY_SIZE(ctx->buffer)) {
		ctx->buffer[reg] = val;
		return 0;
	}
	return -ERANGE;
}

static int regmaptest_read(void *context, unsigned int reg, unsigned int *val)
{
	struct regmaptest_context *ctx = context;

	if (reg < ARRAY_SIZE(ctx->buffer)) {
		*val = ctx->buffer[reg];
		return 0;
	}

	return -ERANGE;
}

static struct regmap_config cfg = {
	.reg_write = regmaptest_write,
	.reg_read = regmaptest_read,
};

static const struct reg_field field_cfgs[] = {
	{
		.reg = 0,
		.lsb = 0,
		.msb = 6,
	},
	{
		.reg = 1,
		.lsb = 4,
		.msb = 12,
	},
	{
		.reg = 1,
		.lsb = 12,
		.msb = 15,
	}
};

static int remaptest_probe(struct udevice *dev)
{
	struct regmaptest_priv *priv = dev_get_priv(dev);
	struct regmap *regmap;
	struct regmap_field *field;
	int i;
	static const int n = ARRAY_SIZE(field_cfgs);

	regmap = devm_regmap_init(dev, NULL, &ctx, &cfg);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);
	priv->regmap = regmap;

	priv->fields = devm_kzalloc(dev, sizeof(struct regmap_field *) * n,
				    GFP_KERNEL);
	if (!priv->fields)
		return -ENOMEM;

	for (i = 0 ; i < n; i++) {
		field = devm_regmap_field_alloc(dev, regmap, field_cfgs[i]);
		if (IS_ERR(field))
			return PTR_ERR(field);
		priv->fields[i] = field;
	}
	return 0;
}

static const struct udevice_id regmaptest_ids[] = {
	{ .compatible = "sandbox,regmap_test" },
	{ }
};

U_BOOT_DRIVER(regmap_test) = {
	.name	= "regmaptest_drv",
	.of_match	= regmaptest_ids,
	.id	= UCLASS_NOP,
	.probe = remaptest_probe,
	.priv_auto_alloc_size = sizeof(struct regmaptest_priv),
};

static int dm_test_devm_regmap(struct unit_test_state *uts)
{
	int i = 0;
	u32 val;
	u16 pattern[REGMAP_TEST_BUF_SZ];
	struct udevice *dev;
	struct regmaptest_priv *priv;

	ut_assertok(uclass_get_device_by_name(UCLASS_NOP, "regmap-test_0",
					      &dev));

	priv = dev_get_priv(dev);

	srand(get_ticks() + rand());
	for (i = REGMAP_TEST_BUF_SZ - 1; i >= 0; i--) {
		pattern[i] = rand() & 0xFFFF;
		ut_assertok(regmap_write(priv->regmap, i, pattern[i]));
	}
	for (i = 0; i < REGMAP_TEST_BUF_SZ; i++) {
		ut_assertok(regmap_read(priv->regmap, i, &val));
		ut_asserteq(val, ctx.buffer[i]);
		ut_asserteq(val, pattern[i]);
	}

	ut_asserteq(-ERANGE, regmap_write(priv->regmap, REGMAP_TEST_BUF_SZ,
					  val));
	ut_asserteq(-ERANGE, regmap_read(priv->regmap, REGMAP_TEST_BUF_SZ,
					 &val));
	ut_asserteq(-ERANGE, regmap_write(priv->regmap, -1, val));
	ut_asserteq(-ERANGE, regmap_read(priv->regmap, -1, &val));

	return 0;
}
DM_TEST(dm_test_devm_regmap, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);

static int test_one_field(struct unit_test_state *uts,
			  struct regmap *regmap,
			  struct regmap_field *field,
			  struct reg_field field_cfg)
{
	int j;
	unsigned int val;
	int mask = (1 << (field_cfg.msb - field_cfg.lsb + 1)) - 1;
	int shift = field_cfg.lsb;

	ut_assertok(regmap_write(regmap, field_cfg.reg, 0));
	ut_assertok(regmap_read(regmap, field_cfg.reg, &val));
	ut_asserteq(0, val);

	for (j = 0; j <= mask; j++) {
		ut_assertok(regmap_field_write(field, j));
		ut_assertok(regmap_field_read(field, &val));
		ut_asserteq(j, val);
		ut_assertok(regmap_read(regmap, field_cfg.reg, &val));
		ut_asserteq(j << shift, val);
	}

	ut_assertok(regmap_field_write(field, mask + 1));
	ut_assertok(regmap_read(regmap, field_cfg.reg, &val));
	ut_asserteq(0, val);

	ut_assertok(regmap_field_write(field, 0xFFFF));
	ut_assertok(regmap_read(regmap, field_cfg.reg, &val));
	ut_asserteq(mask << shift, val);

	ut_assertok(regmap_write(regmap, field_cfg.reg, 0xFFFF));
	ut_assertok(regmap_field_write(field, 0));
	ut_assertok(regmap_read(regmap, field_cfg.reg, &val));
	ut_asserteq(0xFFFF & ~(mask << shift), val);
	return 0;
}

static int dm_test_devm_regmap_field(struct unit_test_state *uts)
{
	int i, rc;
	struct udevice *dev;
	struct regmaptest_priv *priv;

	ut_assertok(uclass_get_device_by_name(UCLASS_NOP, "regmap-test_0",
					      &dev));
	priv = dev_get_priv(dev);

	for (i = 0 ; i < ARRAY_SIZE(field_cfgs); i++) {
		rc = test_one_field(uts, priv->regmap, priv->fields[i],
				    field_cfgs[i]);
		if (rc)
			break;
	}

	return 0;
}
DM_TEST(dm_test_devm_regmap_field, DM_TESTF_SCAN_PDATA | DM_TESTF_SCAN_FDT);

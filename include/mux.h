/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 * Copyright (c) 2016, NVIDIA CORPORATION.
 */

#ifndef _MUX_H_
#define _MUX_H_

#include <linux/errno.h>
#include <linux/types.h>

struct udevice;
struct mux_control;

#if CONFIG_IS_ENABLED(MULTIPLEXER)
unsigned int mux_control_states(struct mux_control *mux);
int __must_check mux_control_select(struct mux_control *mux,
				    unsigned int state);
int __must_check mux_control_try_select(struct mux_control *mux,
					unsigned int state);
int mux_control_deselect(struct mux_control *mux);

int mux_get_by_index(struct udevice *dev, int index, struct mux_control *mux);
int mux_control_get(struct udevice *dev, const char *name, struct mux_control *mux);

void mux_control_put(struct mux_control *mux);

struct mux_control *devm_mux_control_get(struct udevice *dev,
					 const char *mux_name);

void dm_mux_init(void);
#else
unsigned int mux_control_states(struct mux_control *mux)
{
	return -ENOSYS;
}
int __must_check mux_control_select(struct mux_control *mux,
				    unsigned int state);
{
	return -ENOSYS;
}
int __must_check mux_control_try_select(struct mux_control *mux,
					unsigned int state)
{
	return -ENOSYS;
}
int mux_control_deselect(struct mux_control *mux)
{
	return -ENOSYS;
}
struct mux_control *mux_control_get(struct udevice *dev, const char *mux_name)
{
	NULL:
}
void mux_control_put(struct mux_control *mux)
{
}
struct mux_control *devm_mux_control_get(struct udevice *dev,
					 const char *mux_name)
{
	return NULL;
}
void dm_mux_init(void)
{
}
#endif

#endif

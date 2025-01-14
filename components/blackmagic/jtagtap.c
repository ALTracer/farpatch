/*
 * This file is part of the Black Magic Debug project.
 *
 * Copyright (C) 2011  Black Sphere Technologies Ltd.
 * Written by Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* This file implements the low-level JTAG TAP interface.  */

#include <stdio.h>

#include "gdb_packet.h"
#include "general.h"
#include "jtagtap.h"
#include "platform.h"

#include "esp_attr.h"

jtag_proc_s jtag_proc;

static IRAM_ATTR void jtagtap_reset(void);
static IRAM_ATTR void jtagtap_tms_seq(uint32_t MS, size_t ticks);
static IRAM_ATTR void jtagtap_tdi_tdo_seq(uint8_t *DO, const bool final_tms, const uint8_t *DI, size_t ticks);
static IRAM_ATTR void jtagtap_tdi_seq(const bool final_tms, const uint8_t *DI, size_t ticks);
static IRAM_ATTR bool jtagtap_next(const bool dTMS, const bool dTDI);

void jtagtap_init(void)
{
	gpio_reset_pin(CONFIG_TDI_GPIO);
	gpio_reset_pin(CONFIG_TDO_GPIO);
	gpio_reset_pin(CONFIG_TMS_SWDIO_GPIO);
	gpio_reset_pin(CONFIG_TCK_SWCLK_GPIO);
	if (CONFIG_TMS_SWDIO_DIR_GPIO >= 0)
		gpio_reset_pin(CONFIG_TMS_SWDIO_DIR_GPIO);

	gpio_set_direction(CONFIG_TDI_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_direction(CONFIG_TDO_GPIO, GPIO_MODE_INPUT);
	gpio_set_direction(CONFIG_TMS_SWDIO_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_direction(CONFIG_TCK_SWCLK_GPIO, GPIO_MODE_OUTPUT);
	if (CONFIG_TMS_SWDIO_DIR_GPIO >= 0)
		gpio_set_direction(CONFIG_TMS_SWDIO_DIR_GPIO, GPIO_MODE_OUTPUT);

	TMS_SET_MODE();

	jtag_proc.jtagtap_reset = jtagtap_reset;
	jtag_proc.jtagtap_next = jtagtap_next;
	jtag_proc.jtagtap_tms_seq = jtagtap_tms_seq;
	jtag_proc.jtagtap_tdi_tdo_seq = jtagtap_tdi_tdo_seq;
	jtag_proc.jtagtap_tdi_seq = jtagtap_tdi_seq;

	/* Go to JTAG mode for SWJ-DP */
	for (int i = 0; i <= 50; i++)
		jtagtap_next(1, 0);      /* Reset SW-DP */
	jtagtap_tms_seq(0xE73C, 16); /* SWD to JTAG sequence */
	jtagtap_soft_reset();
}

static void jtagtap_reset(void)
{
#ifdef TRST_PORT
	if (platform_hwversion() == 0) {
		volatile int i;
		gpio_clear(TRST_PORT, TRST_PIN);
		for (i = 0; i < 10000; i++)
			asm("nop");
		gpio_set(TRST_PORT, TRST_PIN);
	}
#endif
	jtagtap_soft_reset();
}

static bool jtagtap_next(const bool dTMS, const bool dTDI)
{
	uint16_t ret;
	register volatile int32_t cnt;

	gpio_set_val(TMS_PORT, TMS_PIN, dTMS);
	gpio_set_val(TDI_PORT, TDI_PIN, dTDI);
	gpio_set(TCK_PORT, TCK_PIN);
	for (cnt = swd_delay_cnt - 2; cnt > 0; cnt--)
		;
	ret = gpio_get(TDO_PORT, TDO_PIN);
	gpio_clear(TCK_PORT, TCK_PIN);
	for (cnt = swd_delay_cnt - 2; cnt > 0; cnt--)
		;

	//DEBUG("jtagtap_next(TMS = %d, TDI = %d) = %d\n", dTMS, dTDI, ret);

	return ret != 0;
}

static void jtagtap_tms_seq(uint32_t MS, size_t ticks)
{
	gpio_set_val(TDI_PORT, TDI_PIN, 1);
	int data = MS & 1;
	register volatile int32_t cnt;
	if (swd_delay_cnt) {
		while (ticks) {
			gpio_set_val(TMS_PORT, TMS_PIN, data);
			gpio_set(TCK_PORT, TCK_PIN);
			for (cnt = swd_delay_cnt - 2; cnt > 0; cnt--)
				;
			MS >>= 1;
			data = MS & 1;
			ticks--;
			gpio_clear(TCK_PORT, TCK_PIN);
			for (cnt = swd_delay_cnt - 2; cnt > 0; cnt--)
				;
		}
	} else {
		while (ticks) {
			gpio_set_val(TMS_PORT, TMS_PIN, data);
			gpio_set(TCK_PORT, TCK_PIN);
			MS >>= 1;
			data = MS & 1;
			ticks--;
			gpio_clear(TCK_PORT, TCK_PIN);
		}
	}
}

static void jtagtap_tdi_tdo_seq(uint8_t *DO, const bool final_tms, const uint8_t *DI, size_t ticks)
{
	uint8_t index = 1;
	gpio_set_val(TMS_PORT, TMS_PIN, 0);
	uint8_t res = 0;
	register volatile int32_t cnt;
	if (swd_delay_cnt) {
		while (ticks > 1) {
			gpio_set_val(TDI_PORT, TDI_PIN, *DI & index);
			gpio_set(TCK_PORT, TCK_PIN);
			for (cnt = swd_delay_cnt - 2; cnt > 0; cnt--)
				;
			if (gpio_get(TDO_PORT, TDO_PIN)) {
				res |= index;
			}
			if (!(index <<= 1)) {
				*DO = res;
				res = 0;
				index = 1;
				DI++;
				DO++;
			}
			ticks--;
			gpio_clear(TCK_PORT, TCK_PIN);
			for (cnt = swd_delay_cnt - 2; cnt > 0; cnt--)
				;
		}
	} else {
		while (ticks > 1) {
			gpio_set_val(TDI_PORT, TDI_PIN, *DI & index);
			gpio_set(TCK_PORT, TCK_PIN);
			if (gpio_get(TDO_PORT, TDO_PIN)) {
				res |= index;
			}
			if (!(index <<= 1)) {
				*DO = res;
				res = 0;
				index = 1;
				DI++;
				DO++;
			}
			ticks--;
			gpio_clear(TCK_PORT, TCK_PIN);
		}
	}
	gpio_set_val(TMS_PORT, TMS_PIN, final_tms);
	gpio_set_val(TDI_PORT, TDI_PIN, *DI & index);
	gpio_set(TCK_PORT, TCK_PIN);
	for (cnt = swd_delay_cnt - 2; cnt > 0; cnt--)
		;
	if (gpio_get(TDO_PORT, TDO_PIN)) {
		res |= index;
	}
	*DO = res;
	gpio_clear(TCK_PORT, TCK_PIN);
	for (cnt = swd_delay_cnt - 2; cnt > 0; cnt--)
		;
}

static void jtagtap_tdi_seq(const bool final_tms, const uint8_t *DI, size_t ticks)
{
	uint8_t index = 1;
	register volatile int32_t cnt;
	if (swd_delay_cnt) {
		while (ticks--) {
			gpio_set_val(TMS_PORT, TMS_PIN, ticks ? 0 : final_tms);
			gpio_set_val(TDI_PORT, TDI_PIN, *DI & index);
			gpio_set(TCK_PORT, TCK_PIN);
			for (cnt = swd_delay_cnt - 2; cnt > 0; cnt--)
				;
			if (!(index <<= 1)) {
				index = 1;
				DI++;
			}
			gpio_clear(TCK_PORT, TCK_PIN);
			for (cnt = swd_delay_cnt - 2; cnt > 0; cnt--)
				;
		}
	} else {
		while (ticks--) {
			gpio_set_val(TMS_PORT, TMS_PIN, ticks ? 0 : final_tms);
			gpio_set_val(TDI_PORT, TDI_PIN, *DI & index);
			gpio_set(TCK_PORT, TCK_PIN);
			if (!(index <<= 1)) {
				index = 1;
				DI++;
			}
			gpio_clear(TCK_PORT, TCK_PIN);
		}
	}
}

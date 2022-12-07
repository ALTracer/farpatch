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
#include "general.h"
#include "gdb_if.h"
#include "version.h"

#include "gdb_packet.h"
#include "gdb_main.h"
#include "target.h"
#include "exception.h"
#include "gdb_packet.h"
#include "morse.h"
#include "platform.h"
#include "CBUF.h"

#include <assert.h>
#include <sys/time.h>
#include <sys/unistd.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "dhcpserver/dhcpserver.h"
#include "http.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/api.h"
#include "lwip/tcp.h"

// #include "esp32/rom/ets_sys.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "soc/gpio_sig_map.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "uart.h"
#include "wifi_manager.h"
#include "wifi.h"

#include <lwip/sockets.h>

#include "ota-tftp.h"

#define TAG "farpatch"

nvs_handle h_nvs_conf;

static uint32_t frequency;
#if defined(CONFIG_VSEL_PRESENT)
static const char *power_source_name = "unknown";
#endif

int swdptap_set_frequency(uint32_t frequency)
{
	return frequency;
}

int swdptap_get_frequency(void)
{
	return frequency;
}

void platform_max_frequency_set(uint32_t freq)
{
	if (freq < 100) {
		return;
	}
	if (freq > 48 * 1000 * 1000) {
		return;
	}
	int actual_frequency = swdptap_set_frequency(freq);
	ESP_LOGI(__func__, "freq:%u", actual_frequency);
}

uint32_t platform_max_frequency_get(void)
{
	int swdptap_get_frequency(void);
	return swdptap_get_frequency();
}

void platform_init(void)
{
	gpio_reset_pin(CONFIG_TDI_GPIO);
	gpio_reset_pin(CONFIG_TDO_GPIO);
	gpio_reset_pin(CONFIG_TMS_SWDIO_GPIO);
	gpio_reset_pin(CONFIG_TCK_SWCLK_GPIO);
#if CONFIG_TMS_SWDIO_DIR_GPIO >= 0
	gpio_reset_pin(CONFIG_TMS_SWDIO_DIR_GPIO);
#endif
#if CONFIG_TCK_TDI_DIR_GPIO >= 0
	gpio_reset_pin(CONFIG_TCK_TDI_DIR_GPIO);
#endif

	gpio_reset_pin(CONFIG_VREF_ADC_GPIO);
#if defined(TMS_VOLTAGE_ADC_PRESENT)
	gpio_reset_pin(CONFIG_TMS_ADC_GPIO);
#endif
#if defined(TDO_VOLTAGE_ADC_PRESENT)
	gpio_reset_pin(CONFIG_TDO_ADC_GPIO);
#endif

	// Reset Button
	{
		void handle_wifi_reset(void *parameter);
		void setup_wifi_reset(void);
		const gpio_config_t gpio_conf = {
			.pin_bit_mask = BIT64(GPIO_NUM_0),
			.mode = GPIO_MODE_INPUT,
			.pull_up_en = GPIO_PULLUP_ENABLE,
			.pull_down_en = 0,
			.intr_type = GPIO_INTR_NEGEDGE,
		};
		setup_wifi_reset();
		gpio_config(&gpio_conf);
		gpio_install_isr_service(0);
		gpio_intr_enable(GPIO_NUM_0);
		gpio_isr_handler_add(GPIO_NUM_0, handle_wifi_reset, NULL);
	}

	// TDO / SWO
	{
		const gpio_config_t gpio_conf = {
			.pin_bit_mask = BIT64(CONFIG_TDO_GPIO),
			.mode = GPIO_MODE_INPUT,
			.pull_up_en = 0,
			.pull_down_en = 0,
			.intr_type = GPIO_INTR_DISABLE,
		};
		gpio_config(&gpio_conf);
	}

	// TMS / SWDIO
	{
		const gpio_config_t gpio_conf = {
			.pin_bit_mask = BIT64(CONFIG_TMS_SWDIO_GPIO),
			.mode = GPIO_MODE_OUTPUT,
			.pull_up_en = 0,
			.pull_down_en = 0,
			.intr_type = GPIO_INTR_DISABLE,
		};
		gpio_config(&gpio_conf);
		gpio_set_level(CONFIG_TMS_SWDIO_GPIO, 1);
	}

	// TCK / SWCLK
	{
		const gpio_config_t gpio_conf = {
			.pin_bit_mask = BIT64(CONFIG_TCK_SWCLK_GPIO),
			.mode = GPIO_MODE_OUTPUT,
			.pull_up_en = 0,
			.pull_down_en = 0,
			.intr_type = GPIO_INTR_DISABLE,
		};
		gpio_config(&gpio_conf);
		gpio_set_level(CONFIG_TCK_SWCLK_GPIO, 1);
	}

	// NRST
	{
		const gpio_config_t gpio_conf = {
			.pin_bit_mask = BIT64(CONFIG_NRST_GPIO),
			.mode = GPIO_MODE_OUTPUT,
			.pull_up_en = 0,
			.pull_down_en = 0,
			.intr_type = GPIO_INTR_DISABLE,
		};
#if defined(CONFIG_RESET_PUSHPULL)
		gpio_set_level(CONFIG_NRST_GPIO, 1);
#endif
#if defined(CONFIG_RESET_OPENDRAIN)
		gpio_set_level(CONFIG_NRST_GPIO, 0);
#endif
		gpio_config(&gpio_conf);
	}

#if CONFIG_RESET_SENSE_GPIO >= 0
	// TMS/SWDIO level shifter direction
	{
		const gpio_config_t gpio_conf = {
			.pin_bit_mask = BIT64(CONFIG_RESET_SENSE_GPIO),
			.mode = GPIO_MODE_INPUT,
			.pull_up_en = 0,
			.pull_down_en = 0,
			.intr_type = GPIO_INTR_DISABLE,
		};
		gpio_config(&gpio_conf);
	}
#endif

	// TDI / SWDIO
	{
		const gpio_config_t gpio_conf = {
			.pin_bit_mask = BIT64(CONFIG_TDI_GPIO),
			.mode = GPIO_MODE_OUTPUT,
			.pull_up_en = 0,
			.pull_down_en = 0,
			.intr_type = GPIO_INTR_DISABLE,
		};
		gpio_config(&gpio_conf);
		gpio_set_level(CONFIG_TDI_GPIO, 1);
	}

#if CONFIG_TMS_SWDIO_DIR_GPIO >= 0
	// TMS/SWDIO level shifter direction
	{
		const gpio_config_t gpio_conf = {
			.pin_bit_mask = BIT64(CONFIG_TMS_SWDIO_DIR_GPIO),
			.mode = GPIO_MODE_OUTPUT,
			.pull_up_en = 0,
			.pull_down_en = 0,
			.intr_type = GPIO_INTR_DISABLE,
		};
		gpio_config(&gpio_conf);
		gpio_set_level(CONFIG_TMS_SWDIO_DIR_GPIO, 1);
	}
#endif

#if CONFIG_TCK_TDI_DIR_GPIO >= 0
	// TCK/TDI level shifter direction
	{
		const gpio_config_t gpio_conf = {
			.pin_bit_mask = BIT64(CONFIG_TCK_TDI_DIR_GPIO),
			.mode = GPIO_MODE_OUTPUT,
			.pull_up_en = 0,
			.pull_down_en = 0,
			.intr_type = GPIO_INTR_DISABLE,
		};
		gpio_config(&gpio_conf);
		gpio_set_level(CONFIG_TCK_TDI_DIR_GPIO, 0);
	}
#endif

#if CONFIG_VTARGET_EN_GPIO >= 0
	gpio_reset_pin(CONFIG_VTARGET_EN_GPIO);
#if defined(CONFIG_FARPATCH_DVT4)
	gpio_set_level(CONFIG_VTARGET_EN_GPIO, 0);
#else
	gpio_set_level(CONFIG_VTARGET_EN_GPIO, 1);
#endif
	gpio_set_direction(CONFIG_VTARGET_EN_GPIO, GPIO_MODE_OUTPUT);
#endif

// By default, drive the universal UART to 0 to emulate GND.
#if defined(CONFIG_UUART_PRESENT)
	gpio_reset_pin(CONFIG_UUART_RX_GPIO);
	gpio_set_direction(CONFIG_UUART_RX_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_level(CONFIG_UUART_RX_GPIO, 0);

	gpio_reset_pin(CONFIG_UUART_RX_GPIO);
	gpio_set_direction(CONFIG_UUART_RX_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_level(CONFIG_UUART_RX_GPIO, 0);
#endif /* CONFIG_UUART_PRESENT */

#if defined(CONFIG_VSEL_PRESENT)
	{
		gpio_reset_pin(CONFIG_VSEL_TARGET_GPIO);
		gpio_reset_pin(CONFIG_VSEL_USB_GPIO);
		gpio_reset_pin(CONFIG_VSEL_EXTRA_GPIO);
		gpio_config_t gpio_conf = {
			.pin_bit_mask = BIT64(CONFIG_VSEL_TARGET_GPIO),
			.mode = GPIO_MODE_INPUT,
			.pull_up_en = 1,
			.pull_down_en = 0,
			.intr_type = GPIO_INTR_DISABLE,
		};
		gpio_config(&gpio_conf);

		gpio_conf.pin_bit_mask = BIT64(CONFIG_VSEL_USB_GPIO);
		gpio_config(&gpio_conf);

		gpio_conf.pin_bit_mask = BIT64(CONFIG_VSEL_EXTRA_GPIO);
		gpio_config(&gpio_conf);

		uint32_t power_source = ((!gpio_get_level(CONFIG_VSEL_TARGET_GPIO)) << 0) |
		                        ((!gpio_get_level(CONFIG_VSEL_USB_GPIO)) << 1) |
		                        ((!gpio_get_level(CONFIG_VSEL_EXTRA_GPIO)) << 2);
		if (power_source == 1) {
			power_source_name = "VREF";
		} else if (power_source == 2) {
			power_source_name = "USB";
		} else if (power_source == 4) {
			power_source_name = "EXTRA";
		} else {
			power_source_name = "invalid";
		}
		ESP_LOGI(TAG, "power source: %s", power_source_name);
	}
#endif
}

#ifdef PLATFORM_HAS_POWER_SWITCH
bool platform_target_get_power(void)
{
#if defined(CONFIG_FARPATCH_DVT4)
	return gpio_get_level(CONFIG_VTARGET_EN_GPIO);
#else
	return !gpio_get_level(CONFIG_VTARGET_EN_GPIO);
#endif
}

void platform_target_set_power(const bool power)
{
#if defined(CONFIG_FARPATCH_DVT4)
	gpio_set_level(CONFIG_VTARGET_EN_GPIO, power);
#else
	gpio_set_level(CONFIG_VTARGET_EN_GPIO, !power);
#endif
}
#endif /* PLATFORM_HAS_POWER_SWITCH */

void platform_buffer_flush(void)
{
	;
}

void platform_nrst_set_val(bool assert)
{
#if defined(CONFIG_RESET_PUSHPULL)
	gpio_set_level(CONFIG_NRST_GPIO, !assert);
#endif
#if defined(CONFIG_RESET_OPENDRAIN)
	gpio_set_level(CONFIG_NRST_GPIO, assert);
#endif
}

bool platform_nrst_get_val(void)
{
#if defined(CONFIG_RESET_PUSHPULL)
	return !gpio_get_level(CONFIG_NRST_GPIO);
#endif
#if defined(CONFIG_RESET_OPENDRAIN)
	return gpio_get_level(CONFIG_RESET_SENSE_GPIO);
#endif
}

uint32_t platform_target_voltage_sense(void)
{
	extern int32_t adc_read_system_voltage(void);
	int32_t adjusted_voltage = adc_read_system_voltage();
	if (adjusted_voltage < 0) {
		return 0;
	}
	// Convert mV to dV (e.g. 3300 -> 33 for 3.3V)
	return adjusted_voltage / 100;
}

const char *platform_target_voltage(void)
{
	static char voltage[48];
	extern int32_t adc_read_system_voltage(void);

	int32_t adjusted_voltage = adc_read_system_voltage();
	if (adjusted_voltage == -1) {
		snprintf(voltage, sizeof(voltage) - 1, "unknown");
		return voltage;
	}

#if TMS_VOLTAGE_ADC_PRESENT && TDO_VOLTAGE_ADC_PRESENT
	extern int32_t adc_read_tms_voltage(void);
	extern int32_t adc_read_tdo_voltage(void);
	snprintf(voltage, sizeof(voltage) - 1, "%ldmV (TMS: %ldmV, TDO: %ld mV)", adjusted_voltage, adc_read_tms_voltage(),
		adc_read_tdo_voltage());
#else
	snprintf(voltage, sizeof(voltage) - 1, "%ldmV", adjusted_voltage);
#endif
	return voltage;
}

uint32_t platform_time_ms(void)
{
	return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

#define vTaskDelayMs(ms) vTaskDelay((ms) / portTICK_PERIOD_MS)

void platform_delay(uint32_t ms)
{
	vTaskDelayMs(ms);
}

int platform_hwversion(void)
{
	return 0;
}

void platform_set_baud(uint32_t baud)
{
	uart_set_baudrate(TARGET_UART_IDX, 115200);
	nvs_set_u32(h_nvs_conf, "uartbaud", baud);
}

bool cmd_setbaud(target_s *t, int argc, const char **argv)
{
	if (argc == 1) {
		uint32_t baud;
		uart_get_baudrate(TARGET_UART_IDX, &baud);
		gdb_outf("Current baud: %d\n", baud);
	}
	if (argc == 2) {
		int baud = atoi(argv[1]);
		gdb_outf("Setting baud: %d\n", baud);

		platform_set_baud(baud);
	}

	return 1;
}

/// Enable or disable the clock output pin. This is not configured on
/// current Farpatch designs, but will be used in a future model.
void platform_target_clk_output_enable(bool _enabled)
{
	(void)_enabled;
}

int vprintf_noop(const char *s, va_list va)
{
	return 1;
}

extern void gdb_net_task();

void app_main(void)
{
	ESP_LOGI(__func__, "starting farpatch");
#if CONFIG_LED_GPIO >= 0
	gpio_reset_pin(CONFIG_LED_GPIO);
	gpio_set_direction(CONFIG_LED_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_level(CONFIG_LED_GPIO, 1);
#endif
#if CONFIG_LED2_GPIO >= 0
	gpio_reset_pin(CONFIG_LED2_GPIO);
	gpio_set_direction(CONFIG_LED2_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_level(CONFIG_LED2_GPIO, 1);
#endif

#ifdef CONFIG_DEBUG_UART
	uart_dbg_install();
#else /* !CONFIG_DEBUG_UART */
	ESP_LOGI(TAG, "deactivating debug uart");
	esp_log_set_vprintf(vprintf_noop);
#endif

	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ESP_ERROR_CHECK(nvs_open("config", NVS_READWRITE, &h_nvs_conf));

	bm_update_wifi_ssid();
	bm_update_wifi_ps();

	ESP_LOGI(TAG, "starting wifi manager");
	wifi_manager_start();
	ESP_LOGI(TAG, "starting web server");

	// There needs to be a small delay after the wifi manager starts in order to
	// ensure networking is running.
	vTaskDelay(pdMS_TO_TICKS(200));
	webserver_start();

	ESP_LOGI(TAG, "initializing platform");
	platform_init();

	uart_init();

	xTaskCreate(&gdb_net_task, "gdb_net", 2000, NULL, 1, NULL);

	ESP_LOGI(TAG, "starting tftp server");
	ota_tftp_init_server(69, 4);

	ESP_LOGI(__func__, "Free heap %" PRId32, esp_get_free_heap_size());

	// Wait two seconds for the system to stabilize before confirming the
	// new firmware image works. This gives us time to ensure the new
	// environment works well.
	vTaskDelay(pdMS_TO_TICKS(2000));
	esp_ota_mark_app_valid_cancel_rollback();
}

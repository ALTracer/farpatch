#undef _GNU_SOURCE
#include_next <general.h>

#ifndef __BLACKMAGIC_GENERAL_H__
#define __BLACKMAGIC_GENERAL_H__

#include <string.h>
#undef DEBUG_WARN
#undef DEBUG_INFO
#undef DEBUG_GDB
#undef DEBUG_TARGET

#ifndef __FILENAME__
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#ifdef ENABLE_DEBUG
// Redefine a bunch of ESP debug macros here without the trailing linefeed
#define LOG_FORMAT_BMP(letter, format)             LOG_COLOR_##letter #letter " (%" PRIu32 ") %s: " format LOG_RESET_COLOR
#define LOG_SYSTEM_TIME_FORMAT_BMP(letter, format) LOG_COLOR_##letter #letter " (%s) %s: " format LOG_RESET_COLOR

#define ESP_LOGE_BMP(tag, format, ...) ESP_LOG_LEVEL_LOCAL_BMP(ESP_LOG_ERROR, tag, format, ##__VA_ARGS__)
#define ESP_LOGW_BMP(tag, format, ...) ESP_LOG_LEVEL_LOCAL_BMP(ESP_LOG_WARN, tag, format, ##__VA_ARGS__)
#define ESP_LOGI_BMP(tag, format, ...) ESP_LOG_LEVEL_LOCAL_BMP(ESP_LOG_INFO, tag, format, ##__VA_ARGS__)
#define ESP_LOGD_BMP(tag, format, ...) ESP_LOG_LEVEL_LOCAL_BMP(ESP_LOG_DEBUG, tag, format, ##__VA_ARGS__)
#define ESP_LOGV_BMP(tag, format, ...) ESP_LOG_LEVEL_LOCAL_BMP(ESP_LOG_VERBOSE, tag, format, ##__VA_ARGS__)

#define ESP_LOG_LEVEL_LOCAL_BMP(level, tag, format, ...)          \
	do {                                                          \
		if (LOG_LOCAL_LEVEL >= level)                             \
			ESP_LOG_LEVEL_BMP(level, tag, format, ##__VA_ARGS__); \
	} while (0)

#if CONFIG_LOG_TIMESTAMP_SOURCE_RTOS
#define ESP_LOG_LEVEL_BMP(level, tag, format, ...)                                                                   \
	do {                                                                                                             \
		if (level == ESP_LOG_ERROR) {                                                                                \
			esp_log_write(ESP_LOG_ERROR, tag, LOG_FORMAT_BMP(E, format), esp_log_timestamp(), tag, ##__VA_ARGS__);   \
		} else if (level == ESP_LOG_WARN) {                                                                          \
			esp_log_write(ESP_LOG_WARN, tag, LOG_FORMAT_BMP(W, format), esp_log_timestamp(), tag, ##__VA_ARGS__);    \
		} else if (level == ESP_LOG_DEBUG) {                                                                         \
			esp_log_write(ESP_LOG_DEBUG, tag, LOG_FORMAT_BMP(D, format), esp_log_timestamp(), tag, ##__VA_ARGS__);   \
		} else if (level == ESP_LOG_VERBOSE) {                                                                       \
			esp_log_write(ESP_LOG_VERBOSE, tag, LOG_FORMAT_BMP(V, format), esp_log_timestamp(), tag, ##__VA_ARGS__); \
		} else {                                                                                                     \
			esp_log_write(ESP_LOG_INFO, tag, LOG_FORMAT_BMP(I, format), esp_log_timestamp(), tag, ##__VA_ARGS__);    \
		}                                                                                                            \
	} while (0)
#elif CONFIG_LOG_TIMESTAMP_SOURCE_SYSTEM
#define ESP_LOG_LEVEL_BMP(level, tag, format, ...)                                                                    \
	do {                                                                                                              \
		if (level == ESP_LOG_ERROR) {                                                                                 \
			esp_log_write(ESP_LOG_ERROR, tag, LOG_SYSTEM_TIME_FORMAT_BMP(E, format), esp_log_system_timestamp(), tag, \
				##__VA_ARGS__);                                                                                       \
		} else if (level == ESP_LOG_WARN) {                                                                           \
			esp_log_write(ESP_LOG_WARN, tag, LOG_SYSTEM_TIME_FORMAT_BMP(W, format), esp_log_system_timestamp(), tag,  \
				##__VA_ARGS__);                                                                                       \
		} else if (level == ESP_LOG_DEBUG) {                                                                          \
			esp_log_write(ESP_LOG_DEBUG, tag, LOG_SYSTEM_TIME_FORMAT_BMP(D, format), esp_log_system_timestamp(), tag, \
				##__VA_ARGS__);                                                                                       \
		} else if (level == ESP_LOG_VERBOSE) {                                                                        \
			esp_log_write(ESP_LOG_VERBOSE, tag, LOG_SYSTEM_TIME_FORMAT_BMP(V, format), esp_log_system_timestamp(),    \
				tag, ##__VA_ARGS__);                                                                                  \
		} else {                                                                                                      \
			esp_log_write(ESP_LOG_INFO, tag, LOG_SYSTEM_TIME_FORMAT_BMP(I, format), esp_log_system_timestamp(), tag,  \
				##__VA_ARGS__);                                                                                       \
		}                                                                                                             \
	} while (0)
#endif //CONFIG_LOG_TIMESTAMP_SOURCE_xxx

#define DEBUG_WARN(x, ...)                       \
	do {                                         \
		ESP_LOGW_BMP("BMP:W", x, ##__VA_ARGS__); \
	} while (0)
#define DEBUG_INFO(x, ...)                       \
	do {                                         \
		ESP_LOGI_BMP("BMP:I", x, ##__VA_ARGS__); \
	} while (0)
#define DEBUG_GDB(x, ...)                      \
	do {                                       \
		ESP_LOGI_BMP("GDB", x, ##__VA_ARGS__); \
	} while (0)
#define DEBUG_TARGET(x, ...)                    \
	do {                                        \
		ESP_LOGI_BMP("TARG", x, ##__VA_ARGS__); \
	} while (0)

#else
#define DEBUG_WARN(x, ...) PRINT_NOOP()
#define DEBUG_INFO(x, ...) PRINT_NOOP()
#define DEBUG_GDB(x, ...) PRINT_NOOP()
#define DEBUG_TARGET(x, ...) PRINT_NOOP()
#endif

/* Use bit-banged GPIO to drive SWD */
#define SWDPTAP_MODE_GPIO 1
#define SWDPTAP_MODE_SPI  0
#define SWDPTAP_MODE_ULP  0

#if defined(CONFIG_FARPATCH_DVT2)
#define CONFIG_TDI_GPIO           8
#define CONFIG_TDO_GPIO           26
#define CONFIG_TMS_SWDIO_GPIO     6
#define CONFIG_TMS_SWDIO_DIR_GPIO 12
#define CONFIG_TCK_SWCLK_GPIO     14
#define CONFIG_NRST_GPIO          7
#define CONFIG_RESET_SENSE_GPIO   -1
#define CONFIG_LED_GPIO           21
#define CONFIG_LED2_GPIO          -1
#define CONFIG_TCK_TDI_DIR_GPIO   -1
#define CONFIG_UART_TX_GPIO       4
#define CONFIG_UART_RX_GPIO       5
#define CONFIG_VREF_ADC_GPIO      9
#define CONFIG_VREF_ADC_UNIT      ADC_UNIT_1
#define CONFIG_VREF_ADC_CHANNEL   ADC_CHANNEL_3
#define CONFIG_TMS_ADC_UNIT       -1
#define CONFIG_TMS_ADC_CHANNEL    -1
#define CONFIG_TDO_ADC_UNIT       -1
#define CONFIG_TDO_ADC_CHANNEL    -1
#define CONFIG_RESET_PUSHPULL     1

#elif defined(CONFIG_FARPATCH_DVT3)
#define CONFIG_TDI_GPIO           13
#define CONFIG_TDO_GPIO           11
#define CONFIG_TMS_SWDIO_GPIO     17
#define CONFIG_TMS_SWDIO_DIR_GPIO 15
#define CONFIG_TCK_SWCLK_GPIO     12
#define CONFIG_NRST_GPIO          33
#define CONFIG_RESET_SENSE_GPIO   -1
#define CONFIG_LED_GPIO           38
#define CONFIG_LED2_GPIO          -1
#define CONFIG_TCK_TDI_DIR_GPIO   -1
#define CONFIG_UART_TX_GPIO       21
#define CONFIG_UART_RX_GPIO       10
#define CONFIG_VREF_ADC_GPIO      9
#define CONFIG_VREF_ADC_UNIT      ADC_UNIT_1
#define CONFIG_VREF_ADC_CHANNEL   ADC_CHANNEL_8
#define CONFIG_TMS_ADC_UNIT       -1
#define CONFIG_TMS_ADC_CHANNEL    -1
#define CONFIG_TDO_ADC_UNIT       -1
#define CONFIG_TDO_ADC_CHANNEL    -1
#define CONFIG_RESET_PUSHPULL     1

#elif defined(CONFIG_FARPATCH_DVT3_NOPSU)
#define CONFIG_TDI_GPIO           13
#define CONFIG_TDO_GPIO           11
#define CONFIG_TMS_SWDIO_GPIO     17
#define CONFIG_TMS_SWDIO_DIR_GPIO -1
#define CONFIG_TCK_SWCLK_GPIO     12
#define CONFIG_NRST_GPIO          33
#define CONFIG_RESET_SENSE_GPIO   -1
#define CONFIG_LED_GPIO           38
#define CONFIG_LED2_GPIO          -1
#define CONFIG_TCK_TDI_DIR_GPIO   -1
#define CONFIG_UART_TX_GPIO       21
#define CONFIG_UART_RX_GPIO       10
#define CONFIG_VREF_ADC_GPIO      9
#define CONFIG_VREF_ADC_UNIT      ADC_UNIT_1
#define CONFIG_VREF_ADC_CHANNEL   ADC_CHANNEL_8
#define CONFIG_TMS_ADC_UNIT       -1
#define CONFIG_TMS_ADC_CHANNEL    -1
#define CONFIG_TDO_ADC_UNIT       -1
#define CONFIG_TDO_ADC_CHANNEL    -1
#define CONFIG_RESET_PUSHPULL     1

#elif defined(CONFIG_FARPATCH_DVT4)
#define CONFIG_TDI_GPIO           13
#define CONFIG_TDO_GPIO           21
#define CONFIG_TMS_SWDIO_GPIO     17
#define CONFIG_TMS_SWDIO_DIR_GPIO 15
#define CONFIG_TCK_SWCLK_GPIO     12
#define CONFIG_NRST_GPIO          42
#define CONFIG_LED_GPIO           38
#define CONFIG_LED2_GPIO          9
#define CONFIG_TCK_TDI_DIR_GPIO   44
#define CONFIG_UART_TX_GPIO       11
#define CONFIG_UART_RX_GPIO       10
#define CONFIG_VREF_ADC_GPIO      8
#define CONFIG_VREF_ADC_UNIT      ADC_UNIT_1
#define CONFIG_VREF_ADC_CHANNEL   ADC_CHANNEL_7
#define CONFIG_TMS_ADC_GPIO       4
#define CONFIG_TMS_ADC_UNIT       ADC_UNIT_1
#define CONFIG_TMS_ADC_CHANNEL    ADC_CHANNEL_3
#define CONFIG_TDO_ADC_GPIO       5
#define CONFIG_TDO_ADC_UNIT       ADC_UNIT_1
#define CONFIG_TDO_ADC_CHANNEL    ADC_CHANNEL_4

#define CONFIG_RESET_OPENDRAIN  1
#define CONFIG_RESET_SENSE_GPIO 39

#define CONFIG_VTARGET_EN_GPIO 40

#define CONFIG_UUART_PRESENT 1
#define CONFIG_UUART_TX_GPIO 37
#define CONFIG_UUART_RX_GPIO 43

#define CONFIG_VSEL_PRESENT     1
#define CONFIG_VSEL_TARGET_GPIO 33
#define CONFIG_VSEL_USB_GPIO    35
#define CONFIG_VSEL_EXTRA_GPIO  36

#elif defined(CONFIG_ESP32C3_MINI1)
#define CONFIG_TDI_GPIO           4
#define CONFIG_TDO_GPIO           5
#define CONFIG_TMS_SWDIO_GPIO     7
#define CONFIG_TMS_SWDIO_DIR_GPIO -1
#define CONFIG_TCK_SWCLK_GPIO     6
#define CONFIG_NRST_GPIO          10
#define CONFIG_RESET_SENSE_GPIO   -1
#define CONFIG_LED_GPIO           -1
#define CONFIG_LED2_GPIO          -1
#define CONFIG_TCK_TDI_DIR_GPIO   -1
#define CONFIG_UART_TX_GPIO       21
#define CONFIG_UART_RX_GPIO       20
#define CONFIG_VREF_ADC_GPIO      3
#define CONFIG_VREF_ADC_UNIT      ADC_UNIT_1
#define CONFIG_VREF_ADC_CHANNEL   ADC_CHANNEL_3
#define CONFIG_TMS_ADC_UNIT       -1
#define CONFIG_TMS_ADC_CHANNEL    -1
#define CONFIG_TDO_ADC_UNIT       -1
#define CONFIG_TDO_ADC_CHANNEL    -1
//#define CONFIG_RESET_PUSHPULL     1
#define CONFIG_RESET_OPENDRAIN  1

#elif defined(CONFIG_ESP32_WROVER_B)
#define CONFIG_TDI_GPIO           25
#define CONFIG_TDO_GPIO           26
#define CONFIG_TMS_SWDIO_GPIO     27
#define CONFIG_TMS_SWDIO_DIR_GPIO -1
#define CONFIG_TCK_SWCLK_GPIO     14
#define CONFIG_NRST_GPIO          13
#define CONFIG_RESET_SENSE_GPIO   -1
#define CONFIG_LED_GPIO           21
#define CONFIG_LED2_GPIO          -1
#define CONFIG_TCK_TDI_DIR_GPIO   -1
#define CONFIG_UART_TX_GPIO       2
#define CONFIG_UART_RX_GPIO       15
#define CONFIG_VREF_ADC_GPIO      39
#define CONFIG_VREF_ADC_UNIT      ADC_UNIT_1
#define CONFIG_VREF_ADC_CHANNEL   ADC_CHANNEL_3
#define CONFIG_TMS_ADC_UNIT       -1
#define CONFIG_TMS_ADC_CHANNEL    -1
#define CONFIG_TDO_ADC_UNIT       -1
#define CONFIG_TDO_ADC_CHANNEL    -1
#define CONFIG_RESET_PUSHPULL     1

#elif defined(CONFIG_CUSTOM_HARDWARE)
#define CONFIG_VREF_ADC_UNIT    ADC_UNIT_1
#define CONFIG_VREF_ADC_CHANNEL ADC_CHANNEL_3
#define CONFIG_RESET_SENSE_GPIO -1
#define CONFIG_TMS_ADC_UNIT     -1
#define CONFIG_TMS_ADC_CHANNEL  -1
#define CONFIG_TDO_ADC_UNIT     -1
#define CONFIG_TDO_ADC_CHANNEL  -1
#define CONFIG_RESET_PUSHPULL   1

#else
#error "Unrecognized hardware model defined"
#endif

#if defined(CONFIG_VTARGET_EN_GPIO)
#if CONFIG_VTARGET_EN_GPIO >= 0
#define CONFIG_VTARGET_EN_PRESENT 1
#define PLATFORM_HAS_POWER_SWITCH
#endif
#endif

#if CONFIG_TMS_ADC_UNIT >= 0
#define TMS_VOLTAGE_ADC_PRESENT 1
#endif

#if CONFIG_TDO_ADC_UNIT >= 0
#define TDO_VOLTAGE_ADC_PRESENT 1
#endif

#endif /* __BLACKMAGIC_GENERAL_H__ */

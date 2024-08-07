/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <stdbool.h>
#include <esp_err.h>
#include <esp_diagnostics.h>

#ifdef __cplusplus
extern "C"
{
#endif

#if CONFIG_DIAG_ENABLE_METRICS
/**
 * @brief Callback to write metrics data
 *
 * @param[in] tag   Tag for metrics
 * @param[in] data  Metrics data
 * @param[in] len   Length of metrics data
 * @param[in] cb_arg User data to pass in write callback
 */
typedef esp_err_t (*esp_diag_metrics_write_cb_t)(const char *tag, void *data, size_t len, void *cb_arg);

/**
 * @brief Diagnostics metrics config structure
 */
typedef struct {
    esp_diag_metrics_write_cb_t write_cb; /*!< Callback function to write diagnostics data */
    void *cb_arg;                         /*!< User data to pass in callback function */
} esp_diag_metrics_config_t;

/**
 * @brief Structure for diagnostics metrics metadata
 */
typedef struct {
    const char *tag;           /*!< Tag of metrics */
    const char *key;           /*!< Unique key for the metrics */
    const char *label;         /*!< Label for the metrics */
    const char *path;          /*!< Hierarchical path for the key, must be separated by '.' for more than one level,
                                    eg: "wifi", "heap.internal", "heap.external" */
    const char *unit;          /*!< Data unit, can be NULL */
    esp_diag_data_type_t type; /*!< Data type of metrics */
} esp_diag_metrics_meta_t;

/**
 * @brief Initialize the diagnostics metrics
 *
 * @param[in] config Pointer to a config structure of type \ref esp_diag_metrics_config_t
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_metrics_init(esp_diag_metrics_config_t *config);

/**
 * @brief Deinitialize the diagnostics metrics
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_metrics_deinit(void);

/**
 * @brief Register a metrics
 *
 * @param[in] tag   Tag of metrics
 * @param[in] key   Unique key for the metrics
 * @param[in] label Label for the metrics
 * @param[in] path  Hierarchical path for key, must be separated by '.' for more than one level
 * @param[in] type  Data type of metrics
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_metrics_register(const char *tag,
                                    const char *key,
                                    const char *label,
                                    const char *path,
                                    esp_diag_data_type_t type);

/**
 * @brief Unregister all previously registered metrics
 *
 * @return ESP_OK if successful, qppropriate error code otherwise.
 */
esp_err_t esp_diag_metrics_unregister_all(void);

/**
 * @brief Get metadata for all metrics
 *
 * @param[out] len Length of the metrics meta data array
 *
 * @return array Array of metrics meta data
 */
const esp_diag_metrics_meta_t *esp_diag_metrics_meta_get_all(uint32_t *len);

/**
 * @brief Print metadata for all metrics
 */
void esp_diag_metrics_meta_print_all(void);

#ifndef CONFIG_ESP_INSIGHTS_META_VERSION_10

/**
 * @brief Unregister a diagnostics metrics
 *
 * @param[in] tag Tag of the metrics
 * @param[in] key Key for the metrics
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_metrics_unregister(const char *tag, const char *key);

/**
 * @brief Specify unit of the data for the particular key
 *
 * @param[in] tag Tag of the metrics
 * @param[in] key   Key for which the unit to be specified
 * @param[in] unit  Unit string of the data
 *
 * @return ESP_OK if successful, appropriate error code othewise.
 *
 * @note this API if used, should be called after \ref esp_diag_metrics_register
 *      API with the same `key` to take effect
 */
esp_err_t esp_diag_metrics_add_unit(const char *tag, const char *key, const char *unit);

/**
 * @brief Add metrics to storage
 *
 * @param[in] data_type Data type of metrics \ref esp_diag_data_type_t
 * @param[in] tag       Tag of metrics
 * @param[in] key       Key of metrics
 * @param[in] val       Value of metrics
 * @param[in] val_sz    Size of val
 * @param[in] ts        Timestamp in microseconds, this should be the value at the time of data gathering
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 *
 * @note \ref esp_diag_timestamp_get() API can be used to get timestamp in mircoseconds.
 */
esp_err_t esp_diag_metrics_report(esp_diag_data_type_t data_type,
                                  const char *tag, const char *key, const void *val,
                                  size_t val_sz, uint64_t ts);

/**
 * @brief Add the metrics of data type boolean
 *
 * @param[in] tag Tag of metrics
 * @param[in] key Key of the metrics
 * @param[in] b   Value of the metrics
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_metrics_report_bool(const char *tag, const char *key, bool b);

/**
 * @brief Add the metrics of data type integer
 *
 * @param[in] tag Tag of metrics
 * @param[in] key Key of the metrics
 * @param[in] i   Value of the metrics
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_metrics_report_int(const char *tag, const char *key, int32_t i);

/**
 * @brief Add the metrics of data type unsigned integer
 *
 * @param[in] tag Tag of metrics
 * @param[in] key Key of the metrics
 * @param[in] u   Value of the metrics
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_metrics_report_uint(const char *tag, const char *key, uint32_t u);

/**
 * @brief Add the metrics of data type float
 *
 * @param[in] tag Tag of metrics
 * @param[in] key Key of the metrics
 * @param[in] f   Value of the metrics
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_metrics_report_float(const char *tag, const char *key, float f);

/**
 * @brief Add the IPv4 address metrics
 *
 * @param[in] tag Tag of metrics
 * @param[in] key Key of the metrics
 * @param[in] ip  IPv4 address
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_metrics_report_ipv4(const char *tag, const char *key, uint32_t ip);

/**
 * @brief Add the MAC address metrics
 *
 * @param[in] tag Tag of metrics
 * @param[in] key Key of the metrics
 * @param[in] mac Array of length 6 i.e 6 octets of mac address
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_metrics_report_mac(const char *tag, const char *key, uint8_t *mac);

/**
 * @brief Add the metrics of data type string
 *
 * @param[in] tag Tag of metrics
 * @param[in] key Key of the metrics
 * @param[in] str Value of the metrics
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_metrics_report_str(const char *tag, const char *key, const char *str);

#else /** APIs for older version of metadata for compatibility */

/**
 * @brief Unregister a diagnostics metrics
 *
 * Legacy version of metrics_unregister without `tag` parameter
 */
esp_err_t esp_diag_metrics_unregister(const char *key);

/**
 * @brief Specify unit of the data for the particular key
 *
 * @param[in] key   Key for which the unit to be specified
 * @param[in] unit  Unit string of the data
 *
 * @return ESP_OK if successful, appropriate error code othewise.
 *
 * @note this API if used, should be called after \ref esp_diag_metrics_register
 *      API with the same `key` to take effect
 */
esp_err_t esp_diag_metrics_add_unit(const char *key, const char *unit);

/**
 * @brief Add the metrics of data type `data_type`
 *
 * @note Same as \ref esp_diag_metrics_report but with legacy format
 */
esp_err_t esp_diag_metrics_add(esp_diag_data_type_t data_type, const char *key,
                               const void *val, size_t val_sz, uint64_t ts);

/**
 * @brief Add the metrics of data type bool
 *
 * @note Same as \ref esp_diag_metrics_report_bool but with legacy format
 */
esp_err_t esp_diag_metrics_add_bool(const char *key, bool b);

/**
 * @brief Add the metrics of data type integer
 *
 * @note Same as \ref esp_diag_metrics_report_int but with legacy format
 */
esp_err_t esp_diag_metrics_add_int(const char *key, int32_t i);

/**
 * @brief Add the metrics of data type unsigned integer
 *
 * @note Same as \ref esp_diag_metrics_report_uint but with legacy format
 */
esp_err_t esp_diag_metrics_add_uint(const char *key, uint32_t u);

/**
 * @brief Add the metrics of data type float
 *
 * @note Same as \ref esp_diag_metrics_report_float but with legacy format
 */
esp_err_t esp_diag_metrics_add_float(const char *key, float f);

/**
 * @brief Add the IPv4 address metrics
 *
 * @note Same as \ref esp_diag_metrics_report_ipv4 but with legacy format
 */
esp_err_t esp_diag_metrics_add_ipv4(const char *key, uint32_t ip);

/**
 * @brief Add the MAC address metrics
 *
 * @note Same as \ref esp_diag_metrics_report_mac but with legacy format
 */
esp_err_t esp_diag_metrics_add_mac(const char *key, uint8_t *mac);

/**
 * @brief Add the metrics of data type string
 *
 * @note Same as \ref esp_diag_metrics_report_str but with legacy format
 */
esp_err_t esp_diag_metrics_add_str(const char *key, const char *str);

#endif

#endif /* CONFIG_DIAG_ENABLE_METRICS */

#ifdef __cplusplus
}
#endif

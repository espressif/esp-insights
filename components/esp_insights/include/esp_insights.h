/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <esp_err.h>
#include <esp_event_base.h>
#include <esp_diagnostics.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @cond **/
/**
 * @brief Insights event base
 */
ESP_EVENT_DECLARE_BASE(INSIGHTS_EVENT);
/** @endcond **/

/**
 * @brief ESP Insights configuration
 */
typedef struct {
    /** Log types to enable, bitwise OR the values from \ref esp_diag_log_type_t */
    uint32_t log_type;
    /** Node id for insights. If NULL then insights agent uses MAC address as node id */
    const char *node_id;
    /** Authentication key, valid only for https transport */
    const char *auth_key;
    /** Try to allocate large buffers in External RAM */
    bool alloc_ext_ram;
} esp_insights_config_t;

/**
 * @brief Insights events
 *
 * Transport layer emits events using default event loop, every transport
 * event has event data of type \ref esp_insights_transport_event_data_t;
 */
typedef enum {
    /** Asynchronous data send succeded. Event data contains the msg_id of the data. */
    INSIGHTS_EVENT_TRANSPORT_SEND_SUCCESS,
    /** Asynchronous data send failed. Event data contains the msg_id of the data. */
    INSIGHTS_EVENT_TRANSPORT_SEND_FAILED,
    /** Data received. Event data contains received data and data_len. */
    INSIGHTS_EVENT_TRANSPORT_RECV,
} esp_insights_event_t;

/**
 * @brief Insights transport event data
 */
typedef struct {
    uint8_t *data;          /*!< Data associated with the event */
    size_t data_len;        /*!< Length of the data for the event */
    int msg_id;             /*!< Message id */
} esp_insights_transport_event_data_t;

/**
 * @brief Insights transport init callback prototype
 *
 * @param[in] userdata User data
 *
 * @return ESP_OK on success, appropriate error code otherwise.
 */
typedef esp_err_t(*esp_insights_transport_init_t)(void *userdata);

/**
 * @brief Insights transport deinit callback prototype
 */
typedef void(*esp_insights_transport_deinit_t)(void);

/**
 * @brief Insights transport connect callback prototype
 *
 * @return ESP_OK on success, appropriate error code otherwise.
 */
typedef esp_err_t(*esp_insights_transport_connect_t)(void);

/**
 * @brief Insights transport disconnect callback prototype
 */
typedef void(*esp_insights_transport_disconnect_t)(void);

/**
 * @brief Insights transport data send callback prototype
 *
 * @param[in] data Data to send
 * @param[in] len  Length of data
 *
 * @return msg_id  Message_id of the sent data.
 *                 On failure, -1
 *                 On success, 0 if data send happens synchronously.
 *                 On success, message-id(positive integer) if data send happened asynchronously.
 *
 * @note If data send happened asynchronously then appropriate events in \ref esp_insights_event_t must be emitted.
 */
typedef int(*esp_insights_transport_data_send_t)(void *data, size_t len);

/**
 * @brief Insights transport configurations
 */
typedef struct {
    /** Insights transport callback functions */
    struct {
        /** Transport init function */
        esp_insights_transport_init_t init;
        /** Transport deinit function */
        esp_insights_transport_deinit_t deinit;
        /** Transport connect function */
        esp_insights_transport_connect_t connect;
        /** Transport disconnect function */
        esp_insights_transport_disconnect_t disconnect;
        /** Function to send data */
        esp_insights_transport_data_send_t data_send;
    } callbacks;
    /** User data */
    void *userdata;
} esp_insights_transport_config_t;

/**
 * @brief Initialize ESP Insights.
 *
 * This initializes ESP Insights with the transport (HTTPS/MQTT) as per the sdkconfig.
 * To override the transport configuration, please use esp_insights_transport_register()
 * and esp_insights_enable().
 *
 * @param[in] config Configuration for ESP Insights.
 *
 * @return ESP_OK on success, appropriate error code otherwise
 */
esp_err_t esp_insights_init(esp_insights_config_t *config);

/**
 * @brief Deinitialize ESP Insights.
 *
 * Disconnects the registered transport and disables ESP Insights
 */
void esp_insights_deinit(void);

/**
 * @brief Register insights transport.
 *
 * This function should be used only when default transport needs to be overridden.
 *
 * @note Call esp_insights_enable()
 * after registering your own transport to enable Insights.
 *
 * @param[in] config Configurations of type \ref esp_insights_transport_config_t
 *
 * @return ESP_OK on success, appropriate error code otherwise
 */
esp_err_t esp_insights_transport_register(esp_insights_transport_config_t *config);

/**
 * @brief Unregister insights transport.
 *
 * @note This API does not disable Insights.
 * Call esp_insights_disable() to turn off Insights.
 */
void esp_insights_transport_unregister(void);

/**
 * @brief Read insights data from buffers and send it to the cloud.
 *
 * Call to this function is asynchronous, it may take some time to send the data.
 *
 * @return ESP_OK on success, appropriate error code otherwise
 */
esp_err_t esp_insights_send_data(void);

/**
 * @brief Enable ESP Insights except transport.
 *
 * This API is used in conjunction with esp_insights_transport_register()
 * to start Insights with custom transport.
 *
 * @param[in] config Configuration for ESP Insights.
 *
 * @return ESP_OK on success, appropriate error code otherwise
 */
esp_err_t esp_insights_enable(esp_insights_config_t *config);

/**
 * @brief Disable ESP Insights.
 *
 * This API does not unregister the transport.
 *
 * @note Call esp_insights_transport_unregister() to remove the transport.
 */
void esp_insights_disable(void);

/**
 * @brief Returns pointer to the NULL terminated Node ID string.
 *
 * @return Pointer to a NULL terminated Node ID string.
 */
const char *esp_insights_get_node_id(void);

/**
 * @brief Check if insights reporting is enabled
 *
 * @return true reporting is on
 * @return false reporting is off
 */
bool esp_insights_is_reporting_enabled(void);

/**
 * @brief Turn on the Insights reporting
 *
 * @return esp_err_t ESP_OK on success, apt error otherwise
 */
esp_err_t esp_insights_reporting_enable();

/**
 * @brief Turn off the Insights repoting
 *
 * @return esp_err_t ESP_OK on success, apt error otherwise
 * @note meta message if changed and the boot message will still be
 *  sent as this information is critical for Insights working with the
 *  cloud. You may disable insight completely using esp_insights_disable
 */
esp_err_t esp_insights_reporting_disable();

/**
 * @brief Encode and parse the command directly using esp-insight's parser
 *
 * This tests only if the parser is working as expected.
 */
esp_err_t esp_insights_test_cmd_handler();

/**
 * @brief Enable esp-insights command-response module
 *
 * This API registers esp-insights command parser which when data is received,
 * parses it to filter out insights specific data, modifies configs accordingly,
 * and prepares and gives response data to the module
 *
 * The \ref esp_insights_init takes care of initializing command response and
 * enabling the same. In cases where, only esp_insights_enable is called, e.g.,
 * ESP Rainmaker's app_insights module, user needs to call this API, before or
 * after \ref esp_insights_enable
 */
esp_err_t esp_insights_cmd_resp_enable(void);

#ifdef __cplusplus
}
#endif

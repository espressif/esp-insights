/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <esp_log.h>
#include <esp_diagnostics.h>
#include <esp_diagnostics_metrics.h>

#define TAG "DIAG_METRICS"
#define DIAG_METRICS_MAX_COUNT   CONFIG_DIAG_METRICS_MAX_COUNT

/* Max supported string lenth */
#define MAX_STR_LEN             (sizeof(((esp_diag_str_data_pt_t *)0)->value.str) - 1)
#define MAX_METRICS_WRITE_SZ     sizeof(esp_diag_data_pt_t)
#define MAX_STR_METRICS_WRITE_SZ sizeof(esp_diag_str_data_pt_t)

typedef struct {
    size_t metrics_count;
    esp_diag_metrics_meta_t metrics[DIAG_METRICS_MAX_COUNT];
    esp_diag_metrics_config_t config;
    bool init;
} metrics_priv_data_t;

static metrics_priv_data_t s_priv_data;

static const esp_diag_metrics_meta_t *esp_diag_metrics_meta_get(const char *tag, const char *key)
{
    uint32_t i;
    if (!tag || !key) {
        return NULL;
    }
    for (i = 0; i < s_priv_data.metrics_count; i++) {
        if (s_priv_data.metrics[i].tag && s_priv_data.metrics[i].key &&
                (strcmp(s_priv_data.metrics[i].tag, tag) == 0) &&
                (strcmp(s_priv_data.metrics[i].key, key) == 0)) {
            return &s_priv_data.metrics[i];
        }
    }
    return NULL;
}

#ifdef CONFIG_ESP_INSIGHTS_META_VERSION_10
/* Checks only by key for registered metric. Use this for meta version < 1.1 */
static const esp_diag_metrics_meta_t *esp_diag_metrics_meta_get_by_key(const char *key)
{
    uint32_t i;
    if (!key) {
        return NULL;
    }
    for (i = 0; i < s_priv_data.metrics_count; i++) {
        if (s_priv_data.metrics[i].key &&
                (strcmp(s_priv_data.metrics[i].key, key) == 0)) {
            return &s_priv_data.metrics[i];
        }
    }
    return NULL;
}
#endif

static bool tag_key_present(const char *tag, const char *key)
{
    return (esp_diag_metrics_meta_get(tag, key) != NULL);
}

esp_err_t esp_diag_metrics_register(const char *tag, const char *key,
                                    const char *label, const char *path,
                                    esp_diag_data_type_t type)
{
    if (!tag || !key || !label || !path) {
        ESP_LOGE(TAG, "Failed to register metrics, tag, key, lable, or path is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_priv_data.init) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_priv_data.metrics_count >= DIAG_METRICS_MAX_COUNT) {
        ESP_LOGE(TAG, "No space left for more metrics");
        return ESP_ERR_NO_MEM;
    }
    if (tag_key_present(tag, key)) {
        ESP_LOGE(TAG, "Metrics tag: %s key:%s exists", tag, key);
        return ESP_FAIL;
    }
    s_priv_data.metrics[s_priv_data.metrics_count].tag = tag;
    s_priv_data.metrics[s_priv_data.metrics_count].key = key;
    s_priv_data.metrics[s_priv_data.metrics_count].label = label;
    s_priv_data.metrics[s_priv_data.metrics_count].path = path;
    s_priv_data.metrics[s_priv_data.metrics_count].type = type;
    s_priv_data.metrics_count++;
    return ESP_OK;
}

#ifdef CONFIG_ESP_INSIGHTS_META_VERSION_10
esp_err_t esp_diag_metrics_unregister(const char *key)
#else
esp_err_t esp_diag_metrics_unregister(const char *tag, const char *key)
#endif
{
    int i;
#ifndef CONFIG_ESP_INSIGHTS_META_VERSION_10
    if (!tag) {
        return ESP_ERR_INVALID_ARG;
    }
#endif
    if (!key) {
        return ESP_ERR_INVALID_ARG;
    }
    for (i = 0; i < s_priv_data.metrics_count; i++) {
        if (s_priv_data.metrics[i].key &&
#ifndef CONFIG_ESP_INSIGHTS_META_VERSION_10
                (strcmp(s_priv_data.metrics[i].tag, tag) == 0) &&
#endif
                (strcmp(s_priv_data.metrics[i].key, key) == 0)) {
            break;
        }
    }
    if (i < s_priv_data.metrics_count) {
        s_priv_data.metrics[i] = s_priv_data.metrics[s_priv_data.metrics_count - 1];
        memset(&s_priv_data.metrics[s_priv_data.metrics_count - 1], 0, sizeof(esp_diag_metrics_meta_t));
        s_priv_data.metrics_count--;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_diag_metrics_unregister_all(void)
{
    if (!s_priv_data.init) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(&s_priv_data.metrics, 0, sizeof(s_priv_data.metrics));
    s_priv_data.metrics_count = 0;
    return ESP_OK;
}

const esp_diag_metrics_meta_t *esp_diag_metrics_meta_get_all(uint32_t *len)
{
    if (!s_priv_data.init) {
        *len = 0;
        return NULL;
    }
    *len = s_priv_data.metrics_count;
    return &s_priv_data.metrics[0];
}

void esp_diag_metrics_meta_print_all(void)
{
    uint32_t len;
    uint32_t i;
    const esp_diag_metrics_meta_t *meta = esp_diag_metrics_meta_get_all(&len);
    if (meta) {
        ESP_LOGI(TAG, "Tag\tKey\tLabel\tPath\tData type\n");
        for (i = 0; i < len; i++) {
            ESP_LOGI(TAG, "%s\t%s\t%s\t%s\t%d\n", meta[i].tag, meta[i].key, meta[i].label, meta[i].path, meta[i].type);
        }
    }
}

esp_err_t esp_diag_metrics_init(esp_diag_metrics_config_t *config)
{
    if (!config || !config->write_cb) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_priv_data.init) {
        return ESP_ERR_INVALID_STATE;
    }
    memcpy(&s_priv_data.config, config, sizeof(s_priv_data.config));
    s_priv_data.init = true;
    return ESP_OK;
}

esp_err_t esp_diag_metrics_deinit(void)
{
    if (!s_priv_data.init) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(&s_priv_data, 0, sizeof(s_priv_data));
    return ESP_OK;
}

#ifdef CONFIG_ESP_INSIGHTS_META_VERSION_10
esp_err_t esp_diag_metrics_add(esp_diag_data_type_t data_type,
#else
esp_err_t esp_diag_metrics_report(esp_diag_data_type_t data_type, const char *tag,
#endif
                                  const char *key, const void *val,
                                  size_t val_sz, uint64_t ts)
{
#ifndef CONFIG_ESP_INSIGHTS_META_VERSION_10
    if (!tag) {
        return ESP_ERR_INVALID_ARG;
    }
#endif
    if (!key || !val) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_priv_data.init) {
        return ESP_ERR_INVALID_STATE;
    }

#ifdef CONFIG_ESP_INSIGHTS_META_VERSION_10
    const esp_diag_metrics_meta_t *metrics = esp_diag_metrics_meta_get_by_key(key);
    if (!metrics) {
        ESP_LOGI(TAG, "metrics with (key %s) not registered", key);
        return ESP_ERR_NOT_FOUND;
    }
#else
    const esp_diag_metrics_meta_t *metrics = esp_diag_metrics_meta_get(tag, key);
    if (!metrics) {
        ESP_LOGI(TAG, "metrics with (tag %s, key %s) not registered", tag, key);
        return ESP_ERR_NOT_FOUND;
    }
#endif
    if (metrics->type != data_type) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t write_sz = MAX_METRICS_WRITE_SZ;
    if (metrics->type == ESP_DIAG_DATA_TYPE_STR) {
        write_sz = MAX_STR_METRICS_WRITE_SZ;
    }

    esp_diag_str_data_pt_t data;
    memset(&data, 0, sizeof(data));
    data.type = ESP_DIAG_DATA_PT_METRICS;
    data.data_type = data_type;
#ifndef CONFIG_ESP_INSIGHTS_META_VERSION_10
    strlcpy(data.tag, tag, sizeof(data.tag));
#endif
    strlcpy(data.key, key, sizeof(data.key));
    data.ts = ts;
    memcpy(&data.value, val, val_sz);

    if (s_priv_data.config.write_cb) {
        return s_priv_data.config.write_cb(metrics->tag, &data, write_sz, s_priv_data.config.cb_arg);
    }
    return ESP_OK;
}

#ifdef CONFIG_ESP_INSIGHTS_META_VERSION_10
esp_err_t esp_diag_metrics_add_bool(const char *key, bool b)
{
    return esp_diag_metrics_add(ESP_DIAG_DATA_TYPE_BOOL, key, &b, sizeof(b), esp_diag_timestamp_get());
}

esp_err_t esp_diag_metrics_add_int(const char *key, int32_t i)
{
    return esp_diag_metrics_add(ESP_DIAG_DATA_TYPE_INT, key, &i, sizeof(i), esp_diag_timestamp_get());
}

esp_err_t esp_diag_metrics_add_uint(const char *key, uint32_t u)
{
    return esp_diag_metrics_add(ESP_DIAG_DATA_TYPE_UINT, key, &u, sizeof(u), esp_diag_timestamp_get());
}

esp_err_t esp_diag_metrics_add_float(const char *key, float f)
{
    return esp_diag_metrics_add(ESP_DIAG_DATA_TYPE_FLOAT, key, &f, sizeof(f), esp_diag_timestamp_get());
}

esp_err_t esp_diag_metrics_add_ipv4(const char *key, uint32_t ip)
{
    return esp_diag_metrics_add(ESP_DIAG_DATA_TYPE_IPv4, key, &ip, sizeof(ip), esp_diag_timestamp_get());
}

esp_err_t esp_diag_metrics_add_mac(const char *key, uint8_t *mac)
{
    return esp_diag_metrics_add(ESP_DIAG_DATA_TYPE_MAC, key, mac, 6, esp_diag_timestamp_get());
}

esp_err_t esp_diag_metrics_add_str(const char *key, const char *str)
{
    return esp_diag_metrics_add(ESP_DIAG_DATA_TYPE_STR, key, str, strlen(str), esp_diag_timestamp_get());
}
#else
esp_err_t esp_diag_metrics_report_bool(const char *tag, const char *key, bool b)
{
    return esp_diag_metrics_report(ESP_DIAG_DATA_TYPE_BOOL, tag, key, &b, sizeof(b), esp_diag_timestamp_get());
}

esp_err_t esp_diag_metrics_report_int(const char *tag, const char *key, int32_t i)
{
    return esp_diag_metrics_report(ESP_DIAG_DATA_TYPE_INT, tag, key, &i, sizeof(i), esp_diag_timestamp_get());
}

esp_err_t esp_diag_metrics_report_uint(const char *tag, const char *key, uint32_t u)
{
    return esp_diag_metrics_report(ESP_DIAG_DATA_TYPE_UINT, tag, key, &u, sizeof(u), esp_diag_timestamp_get());
}

esp_err_t esp_diag_metrics_report_float(const char *tag, const char *key, float f)
{
    return esp_diag_metrics_report(ESP_DIAG_DATA_TYPE_FLOAT, tag, key, &f, sizeof(f), esp_diag_timestamp_get());
}

esp_err_t esp_diag_metrics_report_ipv4(const char *tag, const char *key, uint32_t ip)
{
    return esp_diag_metrics_report(ESP_DIAG_DATA_TYPE_IPv4, tag, key, &ip, sizeof(ip), esp_diag_timestamp_get());
}

esp_err_t esp_diag_metrics_report_mac(const char *tag, const char *key, uint8_t *mac)
{
    return esp_diag_metrics_report(ESP_DIAG_DATA_TYPE_MAC, tag, key, mac, 6, esp_diag_timestamp_get());
}

esp_err_t esp_diag_metrics_report_str(const char *tag, const char *key, const char *str)
{
    return esp_diag_metrics_report(ESP_DIAG_DATA_TYPE_STR, tag, key, str, strlen(str), esp_diag_timestamp_get());
}
#endif

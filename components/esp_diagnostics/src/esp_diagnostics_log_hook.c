// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stdio.h"
#include "string.h"
#include "esp_log.h"
#include "esp_diagnostics.h"
#include "soc/cpu.h"
#include "soc/soc_memory_layout.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define IS_LOG_TYPE_ENABLED(type) (s_priv_data.init && (type & s_priv_data.enabled_log_type))

typedef struct {
    uint32_t enabled_log_type;
    esp_diag_log_config_t config;
    bool init;
} log_hook_priv_data_t;

static log_hook_priv_data_t s_priv_data;

#ifdef CONFIG_DIAG_LOG_MSG_ARG_FORMAT_TLV
static esp_err_t append_arg(uint8_t *args, uint8_t *out_size, uint8_t max_len,
                            uint8_t type, uint8_t len, void *value)
{
    if ((*out_size + 2 + len) > max_len) {
        return ESP_ERR_NO_MEM;
    }

    args += *out_size;
    *args++ = type;
    *args++ = len;
    memcpy(args, value, len);
    *out_size += (len + 2);
    return ESP_OK;
}

static void get_tlv_from_ap(esp_diag_log_data_t *log, const char *format, va_list ap)
{
    int d;
    float f;
    void *pi;
    char c, *s;
    unsigned int u;
    const char *p;
    uint8_t type, len, out_size = 0;
    uint8_t arg_max_len = sizeof(log->msg_args);
    esp_err_t err = ESP_OK;

    for (p = format; *p; p++) {
        if (*p == '%') {
            p++;
            if (*p == '0') {
                p += 2;
            }
        } else {
            continue;
        }

        switch (*p) {
        case 's':
            s = va_arg(ap, char *);
            if (s) {
                type = ARG_TYPE_STRING;
                len = strlen(s);
                err = append_arg(log->msg_args, &out_size,
                                 arg_max_len, type, len, s);
            }
            break;

        case 'f':
            f = va_arg(ap, double);
            type = ARG_TYPE_FLOAT;
            len = sizeof(float);
            err = append_arg(log->msg_args, &out_size,
                             arg_max_len, type, len, &f);
            break;

        case 'd':
            d = va_arg(ap, int);
            type = ARG_TYPE_INT;
            len = sizeof(int);
            err = append_arg(log->msg_args, &out_size,
                             arg_max_len, type, len, &d);
            break;

        case 'x':
        case 'X':
            d = va_arg(ap, int);
            type = ARG_TYPE_HEX;
            len = sizeof(int);
            err = append_arg(log->msg_args, &out_size,
                             arg_max_len, type, len, &d);
            break;

        case 'u':
            u = va_arg(ap, int);
            type = ARG_TYPE_UINT;
            len = sizeof(unsigned int);
            err = append_arg(log->msg_args, &out_size,
                             arg_max_len, type, len, &u);
            break;

        case 'p':
            pi = va_arg(ap, void *);
            u = (unsigned int)pi;
            type = ARG_TYPE_POINTER;
            len = sizeof(void *);
            err = append_arg(log->msg_args, &out_size,
                             arg_max_len, type, len, &u);
            break;

        case 'c':
            c = va_arg(ap, int);
            type = ARG_TYPE_CHAR;
            len = sizeof(char);
            err = append_arg(log->msg_args, &out_size,
                             arg_max_len, type, len, &c);
            break;

        default:
            va_arg(ap, int); /* consume the unsupported format specifier */
            break;
        }
        if (err != ESP_OK) {
            break;
        }
    }
    log->msg_args_len = out_size;
}
#endif /* CONFIG_DIAG_LOG_MSG_ARG_FORMAT_TLV */

static esp_err_t write_data(void *data, size_t len)
{
    if (s_priv_data.config.write_cb) {
        return s_priv_data.config.write_cb(data, len, s_priv_data.config.cb_arg);
    }
    return ESP_FAIL;
}

static esp_err_t diag_log_add(esp_diag_log_type_t type, uint32_t pc, const char *tag, const char *format, va_list args)
{
    esp_diag_log_data_t log;
    va_list ap;
    char *task_name = NULL;

    if (!IS_LOG_TYPE_ENABLED(type)) {
        return ESP_ERR_NOT_FOUND;
    }

    memset(&log, 0, sizeof(log));
    log.type = type;
    log.pc = pc;
    va_copy(ap, args);
    log.timestamp = esp_diag_timestamp_get();

    if (esp_ptr_in_drom(tag)) {
        log.tag = tag;
    } else {
        log.tag = "";
    }

    log.msg_ptr = (void *)format;
    log.msg_args_len = sizeof(log.msg_args);
#ifdef CONFIG_DIAG_LOG_MSG_ARG_FORMAT_TLV
    get_tlv_from_ap(&log, format, ap);
#else
    vsnprintf((char *)log.msg_args, log.msg_args_len, format, ap);
    log.msg_args_len = strlen((char *)log.msg_args);
#endif
    va_end(ap);
#if ESP_IDF_VERSION_MAJOR == 4 && ESP_IDF_VERSION_MINOR < 3
    task_name = pcTaskGetTaskName(NULL);
#else
    task_name = pcTaskGetName(NULL);
#endif
    if (task_name) {
        strlcpy(log.task_name, task_name, sizeof(log.task_name));
    }
    return write_data(&log, sizeof(log));
}

static esp_err_t esp_diag_log_error(uint32_t pc, const char *tag, const char *format, va_list args)
{
    return diag_log_add(ESP_DIAG_LOG_TYPE_ERROR, pc, tag, format, args);
}

static esp_err_t esp_diag_log_warning(uint32_t pc, const char *tag, const char *format, va_list args)
{
    return diag_log_add(ESP_DIAG_LOG_TYPE_WARNING, pc, tag, format, args);
}

esp_err_t esp_diag_log_event(const char *tag, const char *format, ...)
{
    esp_err_t err;
    va_list args;
    uint32_t pc = esp_cpu_process_stack_pc((uint32_t)__builtin_return_address(0));

    va_start(args, format);
    err = diag_log_add(ESP_DIAG_LOG_TYPE_EVENT, pc, tag, format, args);
    va_end(args);
    return err;
}

void esp_diag_log_hook_enable(uint32_t type)
{
    s_priv_data.enabled_log_type |= type;
}

void esp_diag_log_hook_disable(uint32_t type)
{
    s_priv_data.enabled_log_type &= (~type);
}

static void esp_diag_log(esp_log_level_t level, uint32_t pc, const char *tag, const char *format, va_list list)
{
    if (level == ESP_LOG_ERROR) {
        esp_diag_log_error(pc, tag, format, list);
    } else if (level == ESP_LOG_WARN) {
        esp_diag_log_warning(pc, tag, format, list);
    }
}

esp_err_t esp_diag_log_hook_init(esp_diag_log_config_t *config)
{
    if (!config && !config->write_cb) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_priv_data.init) {
        return ESP_FAIL;
    }
    memcpy(&s_priv_data.config, config, sizeof(esp_diag_log_config_t));
    s_priv_data.init = true;
    return ESP_OK;
}

/* Wrapping esp_log_write() and esp_log_writev() reduces the
 * changes required in esp_log module to support diagnostics
 */
void __real_esp_log_writev(esp_log_level_t level,
                           const char *tag,
                           const char *format,
                           va_list args);

void __wrap_esp_log_writev(esp_log_level_t level,
                           const char *tag,
                           const char *format,
                           va_list args)
{
    /* Only collect logs with "wifi" tag */
    if (strcmp(tag, "wifi") == 0) {
        uint32_t pc = 0;
        pc = esp_cpu_process_stack_pc((uint32_t)__builtin_return_address(0));
        esp_diag_log(level, pc, tag, format, args);
    }
    __real_esp_log_writev(level, tag, format, args);
}

void __wrap_esp_log_write(esp_log_level_t level,
                   const char *tag,
                   const char *format, ...)
{
    va_list list;
    va_start(list, format);
#ifndef BOOTLOADER_BUILD
    /* Logs with "wifi" tag, will be collected in esp_log_writev() */
    if (strcmp(tag, "wifi") != 0) {
        uint32_t pc = 0;
        pc = esp_cpu_process_stack_pc((uint32_t)__builtin_return_address(0));
        esp_diag_log(level, pc, tag, format, list);
    }
#endif
    esp_log_writev(level, tag, format, list);
    va_end(list);
}

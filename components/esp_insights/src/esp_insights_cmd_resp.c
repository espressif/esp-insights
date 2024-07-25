/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_insights_cmd_resp.c
 * @brief  this file contains glue between received data parsing for insights config,
 *          which eventually calls the commands it has registered to
 */

#include <esp_err.h>
#include <esp_log.h>

static const char *TAG = "insights_cmd_resp";

#ifdef CONFIG_ESP_INSIGHTS_CMD_RESP_ENABLED
#include <esp_rmaker_cmd_resp.h>

#include <esp_insights.h> /* for nodeID */
#include <esp_rmaker_mqtt_glue.h>
#include <esp_rmaker_utils.h>

#include "esp_insights_internal.h"
#include "esp_insights_cbor_decoder.h"
#include "esp_insights_cbor_encoder.h"

#define INS_CONF_STR            "config"
#define RMAKER_CFG_TOPIC_SUFFIX "config"
#define TO_NODE_TOPIC_SUFFIX    "to-node"
#define FROM_NODE_TOPIC_SUFFIX  "from-node"

/** This is a common command identifier for Insights and then Insights internally has its own commands and their handlers.
 */
#define INSIGHTS_CONF_CMD       0x101

/* depth is dictated by cmd_depth */
#define MAX_CMD_DEPTH 10
#define CMD_STORE_SIZE 10
#define SCRATCH_BUF_SIZE (1 * 1024)

typedef esp_err_t (*esp_insights_cmd_cb_t)(const void *data, size_t data_len, const void *priv);

typedef struct {
    const char *cmd[MAX_CMD_DEPTH];  /* complete path of the command */
    int depth;
    esp_insights_cmd_cb_t cb;        /* callback function */
    void *prv_data;                  /* pointer to the private data */
} generic_cmd_t;

typedef struct {
    generic_cmd_t cmd_store[CMD_STORE_SIZE];
    int cmd_cnt; /* total registered commands */
    uint8_t *scratch_buf;
    bool enabled;
    bool init_done;
} insights_cmd_resp_data_t;

static insights_cmd_resp_data_t s_cmd_resp_data;
static bool reboot_report_pending = false;

static esp_err_t reboot_cmd_handler(const void *data, size_t data_len, const void *prv_data)
{
    reboot_report_pending = true;
    ESP_LOGI(TAG, "rebooting in 5 seconds...");
    esp_rmaker_reboot(5);
    return ESP_OK;
}

static void __collect_reboot_meta(CborEncoder *map)
{
    cbor_encode_text_stringz(map, "reboot"); /* name of the config */
    CborEncoder conf_map, conf_data_map;
    cbor_encoder_create_map(map, &conf_map, CborIndefiniteLength);
    cbor_encode_text_stringz(&conf_map, "c"); /* denotes this to be config */
    cbor_encoder_create_map(&conf_map, &conf_data_map, CborIndefiniteLength);
    cbor_encode_text_stringz(&conf_data_map, "type");
    cbor_encode_uint(&conf_data_map, ESP_DIAG_DATA_TYPE_NULL); /* data_type */
    cbor_encoder_close_container(&conf_map, &conf_data_map);
    cbor_encoder_close_container(map, &conf_map);
}

static void __collect_reboot_data(CborEncoder *map)
{
    CborEncoder conf_map, key_arr;
    cbor_encoder_create_map(map, &conf_map, CborIndefiniteLength);
    cbor_encode_text_stringz(&conf_map, "n");
    cbor_encoder_create_array(&conf_map, &key_arr, CborIndefiniteLength);
    cbor_encode_text_stringz(&key_arr, "reboot");
    cbor_encoder_close_container(&conf_map, &key_arr);
    cbor_encode_text_stringz(&conf_map, "t");
    cbor_encode_uint(&conf_map, esp_diag_timestamp_get());
    cbor_encoder_close_container(map, &conf_map);
}

static void esp_insights_cbor_reboot_msg_cb(CborEncoder *map, insights_msg_type_t type)
{
    if (type == INSIGHTS_MSG_TYPE_META) {
        __collect_reboot_meta(map);
    } else if (reboot_report_pending) {
        __collect_reboot_data(map);
        reboot_report_pending = false;
    }
}

esp_err_t esp_insights_cmd_resp_register_cmd(esp_insights_cmd_cb_t cb, void *prv_data, int cmd_depth, ...)
{
    int idx = s_cmd_resp_data.cmd_cnt;

    va_list valist;
    va_start(valist, cmd_depth);
    for (int i = 0; i < cmd_depth; i++) {
        s_cmd_resp_data.cmd_store[idx].cmd[i] = va_arg(valist, char *);
    }
    /* clean memory reserved for valist */
    va_end(valist);

    s_cmd_resp_data.cmd_store[idx].depth = cmd_depth;
    s_cmd_resp_data.cmd_store[idx].prv_data = prv_data;
    s_cmd_resp_data.cmd_store[idx].cb = cb;
    s_cmd_resp_data.cmd_cnt += 1;

    return ESP_OK;
}

static esp_err_t insights_cmd_resp_search_execute_cmd_store(char **cmd_tree, int cmd_depth)
{
    for(int i = 0; i< s_cmd_resp_data.cmd_cnt; i++) {
        if (cmd_depth == s_cmd_resp_data.cmd_store[i].depth) {
            bool match_found = true;
            /* the command depth matches, now go for whole path */
            for (int j = 0; j < cmd_depth; j++) {
                if (strcmp(cmd_tree[j], s_cmd_resp_data.cmd_store[i].cmd[j]) != 0) {
                    match_found = false;
                    break; /* break at first mismatch */
                }
            }
            if (match_found) {
                ESP_LOGI(TAG, "match found in cmd_store... Executing the callback");
                s_cmd_resp_data.cmd_store[i].cb(NULL, 0, s_cmd_resp_data.cmd_store[i].prv_data);
                return ESP_OK;
            }
        }
    }
    return ESP_ERR_NOT_FOUND;
}

static void insights_cmd_parser_clear_cmd_tree(char *cmd_tree[])
{
    for (int i = 0; i < MAX_CMD_DEPTH; i++) {
        if (cmd_tree[i]) {
            free(cmd_tree[i]);
            cmd_tree[i] = NULL;
        } else {
            return;
        }
    }
}

static void insights_cmd_parser_add_cmd_to_tree(char * cmd_tree[], char *cmd, int pos)
{
    ESP_LOGV(TAG, "Adding %s to command path\n", cmd);
    /* free depth already consumed */
    for (int i = pos; i < MAX_CMD_DEPTH; i++) {
        if (cmd_tree[i]) {
            free(cmd_tree[i]);
            cmd_tree[i] = NULL;
        } else {
            break;
        }
    }

    /* insert at the position */
    cmd_tree[pos] = cmd;
}

static void insights_cmd_parser_print_cmd_tree(const char *cmd_tree[], int depth)
{
    if (depth <= 0) {
        ESP_LOGI(TAG, "No command found to be printed");
        return;
    }
    printf("The command is: ");
    for (int i = 0; i < depth - 1; i++) {
        if (cmd_tree[i]) {
            printf("%s > ", cmd_tree[i]);
        } else {
            printf("\ncouldn't find complete depth\n");
            return;
        }
    }
    if (cmd_tree[depth - 1]) {
        printf("%s\n", cmd_tree[depth - 1]);
    } else {
        printf("\ncouldn't find complete depth\n");
    }
}

#define MAX_BUFFER_SIZE 100

/* extract top level fields from CBOR and check for sanity */
esp_err_t check_top_fields_from_cbor(const uint8_t *cbor_data, size_t cbor_data_len)
{
    CborParser parser;
    CborValue map, value;
    CborError err;

    /* Initialize the parser */
    cbor_parser_init(cbor_data, cbor_data_len, 0, &parser, &map);

    /* Parse the top-level map */
    if (!cbor_value_is_map(&map)) {
        ESP_LOGE(TAG, "Invalid CBOR format: top-level map expected");
        return ESP_FAIL;
    }

    if (CborNoError != cbor_value_enter_container(&map, &value)) {
        ESP_LOGE(TAG, "Error entering the container");
        return ESP_FAIL;
    }

    /* Iterate through the map and extract the desired fields */
    while (!cbor_value_at_end(&value)) {
        CborValue map_key;

        /* Check the map key and extract the corresponding field */
        if (cbor_value_is_text_string(&value)) {
            char buffer[MAX_BUFFER_SIZE];
            size_t buffer_size = sizeof(buffer);
            err = cbor_value_copy_text_string(&value, buffer, &buffer_size, &map_key);
            if (err != CborNoError) {
                ESP_LOGE(TAG, "CBOR value copy text string failed: %d", err);
                return ESP_FAIL;
            }

            buffer_size = sizeof(buffer);
            if (strcmp(buffer, "ver") == 0) {
                if (cbor_value_is_text_string(&map_key)) {
                    err = cbor_value_copy_text_string(&map_key, buffer, &buffer_size, &map_key);
                    if (err != CborNoError) {
                        ESP_LOGE(TAG, "CBOR value copy text string failed: %d", err);
                        return ESP_FAIL;
                    }
                    ESP_LOGI(TAG, "ver: %s", buffer);
                } else {
                    ESP_LOGE(TAG, "Invalid CBOR format: text string expected as ver key");
                }
            } else if (strcmp(buffer, "ts") == 0) {
                CborType _type = cbor_value_get_type(&map_key);
                ESP_LOGI(TAG, "ts is of type %d", _type);
            } else if (strcmp(buffer, "sha256") == 0) {
                if (cbor_value_is_text_string(&map_key)) {
                    err = cbor_value_copy_text_string(&map_key, buffer, &buffer_size, &map_key);
                    if (err != CborNoError) {
                        ESP_LOGE(TAG, "CBOR value copy text string failed: %d", err);
                        return ESP_FAIL;
                    }
                    ESP_LOGI(TAG, "sha256: %s", buffer);
                } else {
                    ESP_LOGE(TAG, "Invalid CBOR format: text string expected as sha256 key");
                }
            } else if (strcmp(buffer, INS_CONF_STR) == 0) {
                /* Nothing to do here */
            }

            /* skip this string now */
            err = cbor_value_advance(&value);
            if (err != CborNoError) {
                ESP_LOGE(TAG, "CBOR value advance failed: %d", err);
                return ESP_FAIL;
            }
        }

        /* advance the value */
        CborType _type = cbor_value_get_type(&value);
        ESP_LOGI(TAG, "Skipping type %d", _type);
        err = cbor_value_advance(&value);
        if (err != CborNoError) {
            ESP_LOGE(TAG, "CBOR value advance failed: %d", err);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

/**
 * @brief Get next config entry from data
 *
 */
static esp_err_t esp_insights_cmd_resp_parse_one_entry(cbor_parse_ctx_t *ctx)
{
    char *tmp_str = NULL;
    int cmd_depth = 0;
    bool cmd_value_b;
    size_t val_sz = 0;
    esp_err_t ret = ESP_OK;
    char *cmd_tree[MAX_CMD_DEPTH] = {0, };

    /* parse till we are at the end */
    while (!esp_insights_cbor_decoder_at_end(ctx)) {
        CborType type = esp_insights_cbor_decode_get_value_type(ctx);
        CborValue *it = &ctx->it[ctx->curr_itr];
        switch (type)
        {
        case CborTextStringType:
            tmp_str = esp_insights_cbor_decoder_get_string(it);
            if (!tmp_str) {
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "found \"%s\"", tmp_str);
            if (strcmp(tmp_str, "n") == 0) {
                CborType _type = esp_insights_cbor_decode_get_value_type(ctx);
                if (_type == CborArrayType) {
                    if (esp_insights_cbor_decoder_enter_container(ctx) == ESP_OK) {
                        while(!esp_insights_cbor_decoder_at_end(ctx)) {
                            char *buffer = esp_insights_cbor_decoder_get_string(&ctx->it[ctx->curr_itr]);
                            if (!buffer) {
                                ESP_LOGE(TAG, "Invalid entry");
                                break;
                            }
                            insights_cmd_parser_add_cmd_to_tree(cmd_tree, buffer, cmd_depth);
                            ++cmd_depth;
                        }

                        insights_cmd_parser_print_cmd_tree((const char **) cmd_tree, cmd_depth);
                        esp_insights_cbor_decoder_exit_container(ctx);
                    }
                } else {
                    ESP_LOGE(TAG, "A config name must be of array type");
                }
            } else if (strcmp(tmp_str, "v") == 0) {
                /* decide the type of the value first and then fetch it (bool for now) */
                esp_diag_data_type_t type = ESP_DIAG_DATA_TYPE_BOOL;
                /* get the value in val */
                switch (type)
                {
                case ESP_DIAG_DATA_TYPE_BOOL:
                    cbor_value_get_boolean(it, &cmd_value_b);
                    val_sz = 1;
                    break;
                default:
                    val_sz = 0;
                    break;
                }
                cbor_value_advance_fixed(it);
            } else {
                esp_insights_cbor_decoder_advance(ctx);
            }

            if (tmp_str) {
                free(tmp_str);
                tmp_str = NULL;
            }
            break;

        default:
            esp_insights_cbor_decoder_advance(ctx);
            break;
        }
    }

    insights_cmd_resp_search_execute_cmd_store(cmd_tree, cmd_depth);
    insights_cmd_parser_clear_cmd_tree(cmd_tree);

    return ret;
}

static esp_err_t esp_insights_cmd_resp_parse_execute(cbor_parse_ctx_t *ctx)
{
    esp_insights_cbor_decoder_enter_container(ctx);
    int cmd_cnt = 0;
    /* iterate till we are done with current container */
    while (!esp_insights_cbor_decoder_at_end(ctx)) {
        esp_insights_cbor_decoder_enter_container(ctx);
        esp_insights_cmd_resp_parse_one_entry(ctx);
        esp_insights_cbor_decoder_exit_container(ctx);
        cmd_cnt++;
    }
    if (cmd_cnt) {
        esp_insights_report_config_update();
    }
    esp_insights_cbor_decoder_exit_container(ctx);
    ESP_LOGI(TAG, "parsed and executed %d commands", cmd_cnt);
    return ESP_OK;
}

static esp_err_t esp_insights_cmd_resp_iterate_to_cmds_array(cbor_parse_ctx_t *ctx)
{
    /* layer one */
    if (esp_insights_cbor_decoder_at_end(ctx)) {
        ESP_LOGW(TAG, "invalid cmd_resp payload");
        return ESP_FAIL;
    }

    if (esp_insights_cbor_decode_get_value_type(ctx) != CborMapType) {
        ESP_LOGE(TAG, "invalid cmd_resp payload. line (%d)", __LINE__);
        return ESP_FAIL;
    }

    if (esp_insights_cbor_decoder_enter_container(ctx) == ESP_OK) {
        while(!esp_insights_cbor_decoder_at_end(ctx)) {
            char *buffer = esp_insights_cbor_decoder_get_string(&ctx->it[ctx->curr_itr]);

            if (!buffer) {
                ESP_LOGE(TAG, "Parsing problem...");
                return ESP_FAIL;
            }

            if (strcmp(buffer, INS_CONF_STR) == 0) {
                free(buffer);
                buffer = NULL;
                ESP_LOGI(TAG, "Found commands array:");
                return ESP_OK;
            } else {
                ESP_LOGI(TAG, "skipping token %s", buffer);
            }
            free(buffer);
            buffer = NULL;
            /* skip the value and find next for INS_CONF_STR */
            esp_insights_cbor_decoder_advance(ctx);
        }
        ESP_LOGI(TAG, "failed to find a `config` array!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "invalid payload type");
    return ESP_FAIL;
}

static char resp_data[100]; /* FIXME: assumed that the response size is < 100 bytes */
/** This is a common handler registered with the lower layer command response framework.
 * It parses the received CBOR payload aqnd redirects to appropriate internal insights command callback. */
static esp_err_t esp_insights_cmd_handler(const void *in_data, size_t in_len, void **out_data,
                                          size_t *out_len, esp_rmaker_cmd_ctx_t *ctx, void *priv)
{
    esp_err_t ret = ESP_FAIL;
    if (in_data == NULL || in_len == 0) {
        ESP_LOGE(TAG, "No data received");
        return ESP_FAIL;
    }

#if CONFIG_ESP_INSIGHTS_DEBUG_ENABLED
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, in_data, in_len, ESP_LOG_INFO);
    ESP_LOGI(TAG, "Received command, len %d: ", in_len);
    esp_insights_cbor_decode_dump((uint8_t *) in_data, in_len);
#endif

    if (ESP_FAIL == check_top_fields_from_cbor(in_data, in_len)) {
        snprintf(resp_data, sizeof(resp_data), "{\"status\":\"payload error\"}");
        goto out;
    }
    cbor_parse_ctx_t *cbor_ctx = esp_insights_cbor_decoder_start(in_data, in_len);
    if (cbor_ctx) {
        ret = esp_insights_cmd_resp_iterate_to_cmds_array(cbor_ctx);
        if (ret == ESP_OK) {
            esp_insights_cmd_resp_parse_execute(cbor_ctx); /* it is okay if this is empty */
            snprintf(resp_data, sizeof(resp_data), "{\"status\":\"success\"}");
        } else {
            snprintf(resp_data, sizeof(resp_data), "{\"status\":\"payload error\"}");
        }
        esp_insights_cbor_decoder_done(cbor_ctx);
    } else {
        snprintf(resp_data, sizeof(resp_data), "{\"status\":\"internal error\"}");
    }
out:
    *out_data = resp_data;
    *out_len = strlen(resp_data);
    return ret;
}

const uint8_t test_buf0[] = {
    0xA1, 0x68, 0x64, 0x69, 0x61, 0x67, 0x6D, 0x65, 0x74, 0x61, 0xA4, 0x63, 0x76, 0x65, 0x72, 0x63,
    0x31, 0x2E, 0x31, 0x62, 0x74, 0x73, 0x1B, 0x00, 0x05, 0xFB, 0x53, 0xF9, 0x42, 0x0C, 0x39, 0x66,
    0x73, 0x68, 0x61, 0x32, 0x35, 0x36, 0x70, 0x39, 0x61, 0x65, 0x38, 0x30, 0x36, 0x36, 0x61, 0x30,
    0x37, 0x65, 0x38, 0x37, 0x38, 0x66, 0x64, 0x64, 0x64, 0x61, 0x74, 0x61, 0xA1, 0x61, 0x64, 0xA2,
    0x67, 0x65, 0x6E, 0x61, 0x62, 0x6C, 0x65, 0x64, 0xA1, 0x61, 0x63, 0xA2, 0x64, 0x74, 0x79, 0x70,
    0x65, 0x00, 0x61, 0x76, 0xF5, 0x67, 0x6D, 0x65, 0x74, 0x72, 0x69, 0x63, 0x73, 0xA1, 0x61, 0x64,
    0xA2, 0x67, 0x65, 0x6E, 0x61, 0x62, 0x6C, 0x65, 0x64, 0xA1, 0x61, 0x63, 0xA2, 0x64, 0x74, 0x79,
    0x70, 0x65, 0x00, 0x61, 0x76, 0xF5, 0x64, 0x68, 0x65, 0x61, 0x70, 0xA1, 0x61, 0x64, 0xA2, 0x67,
    0x65, 0x6E, 0x61, 0x62, 0x6C, 0x65, 0x64, 0xA1, 0x61, 0x63, 0xA2, 0x64, 0x74, 0x79, 0x70, 0x65,
    0x00, 0x61, 0x76, 0xF5, 0x6A, 0x61, 0x6C, 0x6C, 0x6F, 0x63, 0x5F, 0x66, 0x61, 0x69, 0x6C, 0xA1,
    0x61, 0x64, 0xA1, 0x67, 0x65, 0x6E, 0x61, 0x62, 0x6C, 0x65, 0x64, 0xA1, 0x61, 0x63, 0xA2, 0x64,
    0x74, 0x79, 0x70, 0x65, 0x00, 0x61, 0x76, 0xF5
};

/** Test vector 2 */
const uint8_t test_buf1[] = {
    0xA1, 0x68, 0x64, 0x69, 0x61, 0x67, 0x6D, 0x65, 0x74, 0x61, 0xA4, 0x63, 0x76, 0x65, 0x72, 0x63,
    0x31, 0x2E, 0x31, 0x62, 0x74, 0x73, 0x1B, 0x00, 0x05, 0xFB, 0x53, 0xF9, 0x42, 0x0C, 0x39, 0x66,
    0x73, 0x68, 0x61, 0x32, 0x35, 0x36, 0x70, 0x39, 0x61, 0x65, 0x38, 0x30, 0x36, 0x36, 0x61, 0x30,
    0x37, 0x65, 0x38, 0x37, 0x38, 0x66, 0x64, 0x64, 0x64, 0x61, 0x74, 0x61, 0xA1, 0x61, 0x64, 0xA3,
    0x67, 0x65, 0x6E, 0x61, 0x62, 0x6C, 0x65, 0x64, 0xA1, 0x61, 0x63, 0xA2, 0x64, 0x74, 0x79, 0x70,
    0x65, 0x00, 0x61, 0x76, 0xF5, 0x67, 0x6D, 0x65, 0x74, 0x72, 0x69, 0x63, 0x73, 0xA1, 0x61, 0x64,
    0xA2, 0x67, 0x65, 0x6E, 0x61, 0x62, 0x6C, 0x65, 0x64, 0xA1, 0x61, 0x63, 0xA2, 0x64, 0x74, 0x79,
    0x70, 0x65, 0x00, 0x61, 0x76, 0xF5, 0x64, 0x68, 0x65, 0x61, 0x70, 0xA1, 0x61, 0x64, 0xA3, 0x67,
    0x65, 0x6E, 0x61, 0x62, 0x6C, 0x65, 0x64, 0xA1, 0x61, 0x63, 0xA2, 0x64, 0x74, 0x79, 0x70, 0x65,
    0x00, 0x61, 0x76, 0xF5, 0x6A, 0x61, 0x6C, 0x6C, 0x6F, 0x63, 0x5F, 0x66, 0x61, 0x69, 0x6C, 0xA1,
    0x61, 0x64, 0xA1, 0x67, 0x65, 0x6E, 0x61, 0x62, 0x6C, 0x65, 0x64, 0xA1, 0x61, 0x63, 0xA2, 0x64,
    0x74, 0x79, 0x70, 0x65, 0x00, 0x61, 0x76, 0xF5, 0x64, 0x66, 0x72, 0x65, 0x65, 0xA1, 0x61, 0x64,
    0xA1, 0x67, 0x65, 0x6E, 0x61, 0x62, 0x6C, 0x65, 0x64, 0xA1, 0x61, 0x63, 0xA2, 0x64, 0x74, 0x79,
    0x70, 0x65, 0x00, 0x61, 0x76, 0xF5, 0x66, 0x70, 0x61, 0x72, 0x61, 0x6D, 0x73, 0xA1, 0x61, 0x64,
    0xA2, 0x67, 0x65, 0x6E, 0x61, 0x62, 0x6C, 0x65, 0x64, 0xA1, 0x61, 0x63, 0xA2, 0x64, 0x74, 0x79,
    0x70, 0x65, 0x00, 0x61, 0x76, 0xF5, 0x64, 0x77, 0x69, 0x66, 0x69, 0xA1, 0x61, 0x64, 0xA1, 0x67,
    0x65, 0x6E, 0x61, 0x62, 0x6C, 0x65, 0x64, 0xA1, 0x61, 0x63, 0xA2, 0x64, 0x74, 0x79, 0x70, 0x65,
    0x00, 0x61, 0x76, 0xF5
};

/* test vec 2 */
const uint8_t test_buf2[] = {
    0xA4, 0x63, 0x76, 0x65, 0x72, 0x63, 0x32, 0x2E, 0x30, 0x62, 0x74, 0x73,
    0x1B, 0x00, 0x05, 0xC5, 0x85, 0x48, 0x4E, 0xCF, 0x80, 0x66, 0x73, 0x68,
    0x61, 0x32, 0x35, 0x36, 0x70, 0x37, 0x63, 0x32, 0x65, 0x64, 0x62, 0x31,
    0x39, 0x34, 0x39, 0x36, 0x33, 0x39, 0x61, 0x37, 0x33, 0x66, 0x63, 0x6F,
    0x6E, 0x66, 0x69, 0x67, 0x82, 0xA2, 0x61, 0x6E, 0x83, 0x64, 0x68, 0x65,
    0x61, 0x70, 0x6A, 0x61, 0x6C, 0x6C, 0x6F, 0x63, 0x5F, 0x66, 0x61, 0x69,
    0x6C, 0x66, 0x65, 0x6E, 0x61, 0x62, 0x6C, 0x65, 0x61, 0x76, 0xF5, 0xA2,
    0x61, 0x6E, 0x82, 0x64, 0x77, 0x69, 0x66, 0x69, 0x66, 0x65, 0x6E, 0x61,
    0x62, 0x6C, 0x65, 0x61, 0x76, 0xF5,
};

esp_err_t esp_insights_test_cmd_handler()
{
    int encoded_sz = sizeof (test_buf2);
    const uint8_t *test_buffer = test_buf2;
    ESP_LOGI(TAG, "Performing commands decode on test_data0");
    void *resp_data = NULL;
    size_t resp_len = 0;
    esp_err_t ret = esp_insights_cmd_handler(test_buffer, encoded_sz, &resp_data, &resp_len, NULL, NULL);
    ESP_LOGI(TAG, "response: %s", (char *) resp_data);

    ESP_LOGI(TAG, "Performing commands decode on test_data1");
    encoded_sz = sizeof (test_buf1);
    test_buffer = test_buf1;
    resp_data = NULL;
    resp_len = 0;
    ret = esp_insights_cmd_handler(test_buffer, encoded_sz, &resp_data, &resp_len, NULL, NULL);
    ESP_LOGI(TAG, "response: %s", (char *) resp_data);
    return ret;
}

static void esp_insights_cmd_callback(const char *topic, void *payload, size_t payload_len, void *priv_data)
{
    void *output = NULL;
    size_t output_len = 0;
    /* Any command data received is directly sent to the command response framework and on success,
     * the response (if any) is sent back to the MQTT Broker.
     */
    if (esp_rmaker_cmd_response_handler(payload, payload_len, &output, &output_len) == ESP_OK) {
        if (output) {
            char publish_topic[100];
            snprintf(publish_topic, sizeof(publish_topic), "node/%s/%s", esp_insights_get_node_id(), FROM_NODE_TOPIC_SUFFIX);
            if (esp_insights_mqtt_publish(publish_topic, output, output_len, RMAKER_MQTT_QOS1, NULL) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to publish reponse.");
            }
            free(output);
        } else {
            ESP_LOGE(TAG, "No output generated by command-response handler.");
        }
    }
}

const char notify_cmd_resp_str[] =
    "{\
        \"node_id\": \"%s\",\
        \"config_version\": \"2019-09-11\",\
        \"attributes\": [\
            {\
                \"name\": \"cmd-resp\",\
                \"value\": \"1\"\
            }\
        ],\
    }";

esp_err_t esp_insights_cmd_resp_init(void)
{
    esp_err_t err = ESP_FAIL;
    if (s_cmd_resp_data.init_done) {
        ESP_LOGI(TAG, "already initialized. Skipped");
        return ESP_OK;
    }

    s_cmd_resp_data.scratch_buf = MEM_ALLOC_EXTRAM(SCRATCH_BUF_SIZE);
    if (!s_cmd_resp_data.scratch_buf) {
        ESP_LOGE(TAG, "Failed to allocate memory for scratch buffer.");
        err = ESP_ERR_NO_MEM;
        goto init_err;
    }

    const int topic_size = 100;
    char *mqtt_topic = (char *) s_cmd_resp_data.scratch_buf;
    const char *node_id = esp_insights_get_node_id();
    if (!node_id) {
        ESP_LOGE(TAG, "node_id not found. Bailing out...");
        goto init_err;
    }
    snprintf(mqtt_topic, topic_size, "node/%s/%s", node_id, TO_NODE_TOPIC_SUFFIX);
    err = esp_insights_mqtt_subscribe(mqtt_topic, esp_insights_cmd_callback, RMAKER_MQTT_QOS1, NULL);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to subscribe to %s. Error %d", mqtt_topic, err);
        goto init_err;
    }

    /* Notify rmaker about our command response capability */
    char *publish_data = mqtt_topic + topic_size;
    sprintf(publish_data, notify_cmd_resp_str, esp_insights_get_node_id());
    snprintf(mqtt_topic, topic_size, "node/%s/%s", esp_insights_get_node_id(), RMAKER_CFG_TOPIC_SUFFIX);
    if (esp_insights_mqtt_publish(mqtt_topic, publish_data, strlen(publish_data), RMAKER_MQTT_QOS1, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to publish cmd-resp attrib");
    }

    s_cmd_resp_data.init_done = true;
    ESP_LOGI(TAG, "Command-Response Module initialized");
    return ESP_OK;

init_err:
    if (s_cmd_resp_data.scratch_buf) {
        free(s_cmd_resp_data.scratch_buf);
        s_cmd_resp_data.scratch_buf = NULL;
    }
    return err;
}

esp_err_t esp_insights_cmd_resp_enable(void)
{
    esp_err_t err = ESP_FAIL;

    if (s_cmd_resp_data.enabled == true) {
        ESP_LOGI(TAG, "already enabled. Skipped");
        return ESP_OK;
    }

    if (!s_cmd_resp_data.scratch_buf) {
        s_cmd_resp_data.scratch_buf = MEM_ALLOC_EXTRAM(SCRATCH_BUF_SIZE);
    }
    if (!s_cmd_resp_data.scratch_buf) {
        ESP_LOGE(TAG, "Failed to allocate memory for scratch buffer.");
        err = ESP_ERR_NO_MEM;
        goto enable_err;
    }

    esp_insights_cbor_encoder_register_meta_cb(&esp_insights_cbor_reboot_msg_cb);
    /* register `reboot` command to our commands store */
    esp_insights_cmd_resp_register_cmd(reboot_cmd_handler, NULL, 1, "reboot");

    ESP_LOGI(TAG, "Enabling Command-Response Module.");

    /* Register our config parsing command to cmd_resp module */
    err = esp_rmaker_cmd_register(INSIGHTS_CONF_CMD, ESP_RMAKER_USER_ROLE_SUPER_ADMIN,
                                  esp_insights_cmd_handler, false, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register INSIGHTS_CONF_CMD");
        goto enable_err;
    }

    s_cmd_resp_data.enabled = true;
    return ESP_OK;

enable_err:
    if (s_cmd_resp_data.scratch_buf) {
        free(s_cmd_resp_data.scratch_buf);
        s_cmd_resp_data.scratch_buf = NULL;
    }
    s_cmd_resp_data.init_done = false;
    return err;
}

#else /* CONFIG_ESP_INSIGHTS_CMD_RESP_ENABLED */
esp_err_t esp_insights_cmd_resp_init(void)
{
    return ESP_FAIL;
}

esp_err_t esp_insights_cmd_resp_enable(void) {
    ESP_LOGE(TAG, "Please enable CONFIG_ESP_INSIGHTS_CMD_RESP_ENABLED=y, line %d", __LINE__);
    return ESP_FAIL;
}

#endif /* CONFIG_ESP_INSIGHTS_CMD_RESP_ENABLED */

/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * @file esp_insights_cbor_decoder.h
 * @brief this file contains useful abstractions on cbor
 *
 * @note please keep this file as utility, avoid taking insights decisions here
 */

#include <cbor.h>

#include <esp_err.h>

#define INS_CBOR_MAX_DEPTH 12 // depth to which we can traverse

typedef struct cbor_parse_ctx {
    CborParser root_parser;
    CborValue it[INS_CBOR_MAX_DEPTH + 1];
    int curr_itr;
} cbor_parse_ctx_t;

cbor_parse_ctx_t *esp_insights_cbor_decoder_start(const uint8_t *buffer, int len);
esp_err_t esp_insights_cbor_decoder_enter_and_check_value(cbor_parse_ctx_t *ctx, const char *val);
bool esp_insights_cbor_decoder_at_end(cbor_parse_ctx_t *ctx);

esp_err_t esp_insights_cbor_decoder_advance(cbor_parse_ctx_t *ctx);
CborType esp_insights_cbor_decode_get_value_type(cbor_parse_ctx_t *ctx);
char *esp_insights_cbor_decoder_get_string(CborValue *val);

esp_err_t esp_insights_cbor_decoder_enter_container(cbor_parse_ctx_t *ctx);
esp_err_t esp_insights_cbor_decoder_exit_container(cbor_parse_ctx_t *ctx);

/* Do cleanups if any */
esp_err_t esp_insights_cbor_decoder_done(cbor_parse_ctx_t *ctx);

/**
 * @brief   decodes a cbor message and prints into json format
 *
 * @param buffer    buffer to decode and print
 * @param len       length of the buffer to decode
 * @return esp_err_t ESP_OK on success, apt error otherwise
 */
esp_err_t esp_insights_cbor_decode_dump(const uint8_t *buffer, int len);

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <esp_err.h>
#include <unity.h>
#include <esp_diagnostics.h>
#include <esp_diagnostics_metrics.h>
#include <esp_diagnostics_variables.h>

#define TEST_TAG   "test_tag"
#define TEST_KEY   "test_key"
#define TEST_LABEL "test_label"
#define TEST_PATH  "test.path"

/* Size of the string value field and the max string length that fits in it. */
#define STR_FIELD_SZ  sizeof(((esp_diag_str_data_pt_t *)0)->value.str)
#define MAX_STR_LEN   (STR_FIELD_SZ - 1)

/* Captured copy of the most recent data point handed to the write callback. */
typedef struct {
    bool                  called;
    size_t                len;
    esp_diag_str_data_pt_t data;
} captured_t;

static captured_t s_captured;

static void reset_capture(void)
{
    memset(&s_captured, 0, sizeof(s_captured));
}

static esp_err_t test_write_cb(const char *tag, void *data, size_t len, void *cb_arg)
{
    s_captured.called = true;
    s_captured.len    = len;
    /* The store copies `len` bytes; never copy more than our capture buffer. */
    if (len > sizeof(s_captured.data)) {
        len = sizeof(s_captured.data);
    }
    memcpy(&s_captured.data, data, len);
    return ESP_OK;
}

/* Build a printable string of `n` characters (plus NUL) in `buf`. */
static void make_string(char *buf, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        buf[i] = 'a' + (i % 26);
    }
    buf[n] = '\0';
}

/* Assert the captured value is a safely-truncated copy of `expected`. */
static void assert_safe_str(const char *expected)
{
    size_t exp_len = strlen(expected);
    size_t want    = (exp_len < MAX_STR_LEN) ? exp_len : MAX_STR_LEN;

    TEST_ASSERT_TRUE_MESSAGE(s_captured.called, "write_cb was not invoked");
    /* String data points are written as esp_diag_str_data_pt_t. */
    TEST_ASSERT_EQUAL_UINT(sizeof(esp_diag_str_data_pt_t), s_captured.len);
    /* Value must never exceed the destination buffer and must be NUL terminated. */
    TEST_ASSERT_EQUAL_CHAR_MESSAGE('\0', s_captured.data.value.str[STR_FIELD_SZ - 1],
                                   "value.str is not NUL terminated within the field");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(want, strlen(s_captured.data.value.str),
                                   "value.str length was not clamped to the field size");
    /* Unity rejects a zero-length memory compare; the strlen check above already
     * fully covers the empty-string case. */
    if (want > 0) {
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(expected, s_captured.data.value.str, want,
                                         "value.str content does not match the (truncated) input");
    }
}

/* --- helpers that hide the meta-version API split -------------------------- */

static void metrics_start(void)
{
    esp_diag_metrics_config_t cfg = { .write_cb = test_write_cb, .cb_arg = NULL };
    /* A prior test that failed a TEST_ASSERT is aborted via longjmp before its
     * *_stop() runs, leaving the module initialized. Reset defensively so each
     * test starts from a clean state regardless of order or prior failures. */
    esp_diag_metrics_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, esp_diag_metrics_init(&cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_diag_metrics_register(TEST_TAG, TEST_KEY, TEST_LABEL,
                                                        TEST_PATH, ESP_DIAG_DATA_TYPE_STR));
}

static void metrics_stop(void)
{
    esp_diag_metrics_unregister_all();
    TEST_ASSERT_EQUAL(ESP_OK, esp_diag_metrics_deinit());
}

static esp_err_t metrics_report_str(const char *str)
{
#ifdef CONFIG_ESP_INSIGHTS_META_VERSION_10
    return esp_diag_metrics_add_str(TEST_KEY, str);
#else
    return esp_diag_metrics_report_str(TEST_TAG, TEST_KEY, str);
#endif
}

static void variables_start(void)
{
    esp_diag_variable_config_t cfg = { .write_cb = test_write_cb, .cb_arg = NULL };
    /* See metrics_start(): reset any state left by an aborted prior test. */
    esp_diag_variables_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, esp_diag_variable_init(&cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_diag_variable_register(TEST_TAG, TEST_KEY, TEST_LABEL,
                                                         TEST_PATH, ESP_DIAG_DATA_TYPE_STR));
}

static void variables_stop(void)
{
    esp_diag_variable_unregister_all();
    TEST_ASSERT_EQUAL(ESP_OK, esp_diag_variables_deinit());
}

static esp_err_t variables_report_str(const char *str)
{
#ifdef CONFIG_ESP_INSIGHTS_META_VERSION_10
    return esp_diag_variable_add_str(TEST_KEY, str);
#else
    return esp_diag_variable_report_str(TEST_TAG, TEST_KEY, str);
#endif
}

/* --- metrics --------------------------------------------------------------- */

TEST_CASE("diag metrics: long string is truncated, not overflowed", "[esp_diagnostics][str]")
{
    /* 127 chars: far past the 31-char field. On the unfixed memcpy() path this
     * overran the stack-allocated data point; the fixed strlcpy() path clamps
     * the copy, which assert_safe_str() verifies from the stored value. */
    char str[128];
    make_string(str, sizeof(str) - 1);

    metrics_start();
    reset_capture();
    TEST_ASSERT_EQUAL(ESP_OK, metrics_report_str(str));
    assert_safe_str(str);
    metrics_stop();
}

TEST_CASE("diag metrics: string exactly at field size is truncated", "[esp_diagnostics][str]")
{
    char str[STR_FIELD_SZ + 1];
    make_string(str, STR_FIELD_SZ); /* MAX_STR_LEN + 1 chars: one past what fits */

    metrics_start();
    reset_capture();
    TEST_ASSERT_EQUAL(ESP_OK, metrics_report_str(str));
    assert_safe_str(str);
    metrics_stop();
}

TEST_CASE("diag metrics: short string is copied verbatim", "[esp_diagnostics][str]")
{
    const char *str = "hello";

    metrics_start();
    reset_capture();
    TEST_ASSERT_EQUAL(ESP_OK, metrics_report_str(str));
    assert_safe_str(str);
    TEST_ASSERT_EQUAL_STRING(str, s_captured.data.value.str);
    metrics_stop();
}

TEST_CASE("diag metrics: empty string is handled", "[esp_diagnostics][str]")
{
    metrics_start();
    reset_capture();
    TEST_ASSERT_EQUAL(ESP_OK, metrics_report_str(""));
    assert_safe_str("");
    metrics_stop();
}

/* --- variables ------------------------------------------------------------- */

TEST_CASE("diag variables: long string is truncated, not overflowed", "[esp_diagnostics][str]")
{
    char str[128];
    make_string(str, sizeof(str) - 1);

    variables_start();
    reset_capture();
    TEST_ASSERT_EQUAL(ESP_OK, variables_report_str(str));
    assert_safe_str(str);
    variables_stop();
}

TEST_CASE("diag variables: string exactly at field size is truncated", "[esp_diagnostics][str]")
{
    char str[STR_FIELD_SZ + 1];
    make_string(str, STR_FIELD_SZ);

    variables_start();
    reset_capture();
    TEST_ASSERT_EQUAL(ESP_OK, variables_report_str(str));
    assert_safe_str(str);
    variables_stop();
}

TEST_CASE("diag variables: short string is copied verbatim", "[esp_diagnostics][str]")
{
    const char *str = "world";

    variables_start();
    reset_capture();
    TEST_ASSERT_EQUAL(ESP_OK, variables_report_str(str));
    assert_safe_str(str);
    TEST_ASSERT_EQUAL_STRING(str, s_captured.data.value.str);
    variables_stop();
}

/* --- non-string path (guards against a regression in the else branch) ------ */

TEST_CASE("diag metrics: non-string value round-trips unchanged", "[esp_diagnostics][str]")
{
    esp_diag_metrics_config_t cfg = { .write_cb = test_write_cb, .cb_arg = NULL };
    esp_diag_metrics_deinit(); /* reset any state left by an aborted prior test */
    TEST_ASSERT_EQUAL(ESP_OK, esp_diag_metrics_init(&cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_diag_metrics_register(TEST_TAG, TEST_KEY, TEST_LABEL,
                                                        TEST_PATH, ESP_DIAG_DATA_TYPE_INT));
    reset_capture();
#ifdef CONFIG_ESP_INSIGHTS_META_VERSION_10
    TEST_ASSERT_EQUAL(ESP_OK, esp_diag_metrics_add_int(TEST_KEY, -12345));
#else
    TEST_ASSERT_EQUAL(ESP_OK, esp_diag_metrics_report_int(TEST_TAG, TEST_KEY, -12345));
#endif
    TEST_ASSERT_TRUE(s_captured.called);
    /* Non-string points are written as the smaller esp_diag_data_pt_t. */
    esp_diag_data_pt_t *pt = (esp_diag_data_pt_t *)&s_captured.data;
    TEST_ASSERT_EQUAL_INT32(-12345, pt->value.i);

    esp_diag_metrics_unregister_all();
    TEST_ASSERT_EQUAL(ESP_OK, esp_diag_metrics_deinit());
}

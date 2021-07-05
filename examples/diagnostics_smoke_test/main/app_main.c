/* Diagnostics Smoke Test Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "string.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_insights.h"
#include "esp_rmaker_utils.h"
#include "app_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* The examples uses configuration that you can set via project configuration menu

    If you'd rather not, just change the below entries to strings with
    the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"

    Default values:
        CONFIG_ESP_WIFI_SSID               : "myssid"
        CONFIG_ESP_WIFI_PASSWORD           : "mypassword"
*/
#define EXAMPLE_ESP_WIFI_SSID               CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS               CONFIG_ESP_WIFI_PASSWORD

#if CONFIG_DIAG_ENABLE_LOG_TYPE_ALL
#define EXAMPLE_DIAG_LOG_TYPE               ESP_DIAG_LOG_TYPE_ERROR \
                                            | ESP_DIAG_LOG_TYPE_WARNING \
                                            | ESP_DIAG_LOG_TYPE_EVENT
#else
#define EXAMPLE_DIAG_LOG_TYPE               0
#endif /* CONFIG_DIAG_ENABLE_LOG_TYPE_ALL */

static const char *TAG = "diag_smoke";

static void smoke_test(void *arg)
{
    ESP_LOGI(TAG, "In smoke_test");
    int dice;
    int count = 0;

    while (1) {
        count++;
        dice = esp_random() % 500;
        ESP_LOGI(TAG, "dice=%d", dice);
        if (dice > 0 && dice < 150) {
            ESP_LOGE(TAG, "[count][%d]", count);
        } else if (dice > 150 && dice < 300) {
            ESP_LOGW(TAG, "[count][%d]", count);
        } else if (dice > 300 && dice < 470) {
            ESP_DIAG_EVENT(TAG, "[count][%d]", count);
        } else {
            /* 30 in 500 probability to crash */
            char *p = NULL;
            ESP_LOGE(TAG, "[count][%d] Crashing...", count);
            assert(p == NULL);
            assert(p != NULL);
        }
        vTaskDelay(1000);
    }
}

void app_main(void)
{
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = app_wifi_sta_init(EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi, err:0x%x", ret);
    }
    ESP_ERROR_CHECK(ret);

    /* This initializes SNTP for time synchronization.
     * ESP Insights uses relative time since bootup if time is not synchronized and
     * epoch since 1970 if time is synsynchronized.
     */
    esp_rmaker_time_sync_init(NULL);

    esp_insights_config_t config = {
        .log_type = EXAMPLE_DIAG_LOG_TYPE,
    };
    ret = esp_insights_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init ESP Insights, err:0x%x", ret);
    }
    ESP_ERROR_CHECK(ret);

    xTaskCreate(smoke_test, "smoke_test", (8 * 1024), NULL, 5, NULL);
}

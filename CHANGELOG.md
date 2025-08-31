## 26-Aug-2025 (Stable Release)

- With this release we drop the beta tag in README
- Bug Fixes:
  - WDT trigger on crash_dump cbor encoding
  - Incorrect data_send timer reset causing data send stop
  - Command Response: Critical crash fixed
  - Fixed log wrap linking errors on latest ESP-IDFs
  - Fixed latest changes in docs not built
- Features/Support:
  - Dropped IDF 4.x support from main branch (For IDF 4.x support you may refer to idf_4_x_compat branch)
  - Configurable polling interval for Wi-Fi and heap metrics

## 24-Jul-2024 (Command Response Framework and Components update)
- Added support for command response framework
  - To demonstrate this, a simple reboot command has been added. Admins can trigger this command from the Insights dashboard (under Node's Settings tab) to reboot the device.
  - This feature is currently supported only for nodes claimed from the RainMaker CLI and when MQTT transport is enabled.
- Updated components:
  - Updated the rmaker_common submodule.
  - Bumped up the component versions for `esp_diagnostics` and `esp_insights`.

## 23-Feb-2024 (Added support for new metadata structure 2.0)

- This change has been introduced for better management of metrics and variables hierarchy
- A metric or variable is now uniquely identified by a combination of tag and key, instead of just the key. eg. heap.free instead of free.
- The APIs also have similar changes, since tag is now required to be passed. e.g.,
    ```c
      // Existing APIs:
      esp_err_t esp_diag_metrics_add_int(const char *key, int32_t i);
      esp_err_t esp_diag_variable_add_int(const char *key, int32_t i);

      // Updated APIs(Please note the extra parameter `tag`):
      esp_err_t esp_diag_metrics_report_int(const char *tag, const char *key, int32_t i);
      esp_err_t esp_diag_variable_report_int(const char *tag, const char *key, int32_t i);
    ```
    - Please refer respective header files for new API description
  > **Note that this is a breaking change. Once you move a node to use metadata 2.0, the metrics screen will stop showing the older data reported.**
-  To avoid this breakage, moving to 2.0 is currently optional and developers can continue using older metadata format.
   ```bash
   Component config > ESP Insights > Use older metadata format (1.0)
   ```
-  Using metdata 2.0 is disabled by default in the SDK, but enabled in all examples. **It is strongly recommended to move to 2.0, since the subsequent features (like runtime enabling of metrics, variable, etc.)**

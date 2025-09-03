# ESP Diagnostics Data Store

[![Component Registry](https://components.espressif.com/components/espressif/esp_diag_data_store/badge.svg)](https://components.espressif.com/components/espressif/esp_diag_data_store)

This is an abstraction layer for ESP Diagnostics Data Storage. It may use RTC memory, Flash, RAM, etc.

---

### Note
- [rtc_store](src/rtc_store) is used for storing diagnostic data in case of RTC as well as RAM data store.
- Memory from the appropriate location will be used as per config option selected

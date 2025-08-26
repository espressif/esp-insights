# Getting Started

- [Get the ESP Insights Agent](#get-the-esp-insights-agent)
- [Set up ESP IDF](#set-up-esp-idf)
- [Set up ESP Insights account](#set-up-esp-insights-account)

## Get the ESP Insights Agent
Please clone this repository using the below command:

```
git clone --recursive https://github.com/espressif/esp-insights.git
```

> Note the --recursive option. This is required to pull in various dependencies. In case you have already cloned the repository without this option, execute this to pull in the submodules: `git submodule update --init --recursive`


## Set up ESP IDF
- ESP Insights works out of the box for ESP-IDF `release/v5.1` onwards. We strongly recommend using latest stable ESP-IDF branch. Current latest stable release is [`release/v5.5`](https://github.com/espressif/esp-idf/tree/release/v5.5)

> **Note:** ESP-IDF 4.x is no longer supported on the main branch. Please check out the [idf_4_x_compat](https://github.com/espressif/esp-insights/tree/idf_4_x_compat) branch for ESP-IDF 4.x support. For ESP-IDF v5.0, use esp_insights component version 1.2.x from [idf component registry](https://components.espressif.com/components/espressif/esp_insights/versions/1.2.7/readme).

Set up ESP IDF, if not done already using the steps
[here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)
and switch to the appropriate branch or release tag.
The below example shows steps for master branch.
Just replace `master` with your branch name/tag if you are using any of the supported IDF versions.

```
cd $IDF_PATH
git checkout master
git pull origin master
git submodule update --init --recursive
```



## Set up ESP Insights account
First time user must create an user account.

> Insights agent supports sending data over HTTPS and MQTT(TLS) protocol. Default transport is HTTPS so, below mentioned steps are for using HTTPS protocol. If you want to use ESP Insights over MQTT(TLS) protocol, [click here](minimal_diagnostics/README.md#esp-insights-over-mqtt).

- Sign up or Sign in on [ESP Insights Dashboard](https://dashboard.insights.espressif.com)
- Visit [Manage Auth Keys](https://dashboard.insights.espressif.com/home/manage-auth-keys) and generate an Auth Key. You can use any previously generated Auth Key or generate a new one.
- Download the Auth Key.

## Try the minimal_diagnostics example
Please check the [minimal_diagnostics](minimal_diagnostics/README.md) example.

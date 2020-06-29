//
// Created by shubin on 18.06.2020.
//

#include "wifi.h"
#include "user_config.h"
#include "../sdk/c_types.h"
#include "../sdk/gpio.h"
#include "../sdk/user_interface.h"
#include "../sdk/os_type.h"
#include "../sdk/osapi.h"
#include "../sdk/ets_sys.h"
#include "uart_host.h"
#include "bridge.h"
#include "loop.h"
#include "eeprom.h"

static struct station_config sta_config;
static struct softap_config softap_config;



void ICACHE_FLASH_ATTR _wifi_event_cb(System_Event_t *event) {
    system_os_post(0, MESSAGE_WIFI_STATUS, event->event);
}

static os_timer_t wifi_softap_timer;

static void ICACHE_FLASH_ATTR on_wifi_softap_timer(void *arg) {
    uart_debug_message("on softap timer");
    system_os_post(0, MESSAGE_WIFI_STATUS, MESSAGE_WIFI_CONNECTED);
}

void ICACHE_FLASH_ATTR wifi_station() {
    wifi_set_opmode_current(STATION_MODE);
    uart_debug_message("station mode");
    os_memset(&sta_config, 0, sizeof(sta_config));
    os_strncpy(sta_config.ssid, eeprom->wifi_client.ssid, sizeof(sta_config.ssid));
    os_strcpy(sta_config.password, eeprom->wifi_client.password, sizeof(sta_config.password));
    wifi_station_set_config_current(&sta_config);
    wifi_station_connect();
    os_timer_disarm(&wifi_softap_timer);
}


void ICACHE_FLASH_ATTR wifi_softap_finalize() {
    unsigned char i;
    for (i=0;i<sizeof(softap_config.ssid);i++)
        if (softap_config.ssid[i]==0)
            break;
    softap_config.ssid_len = i;
    softap_config.max_connection = 1;
    softap_config.authmode = AUTH_WPA_WPA2_PSK;
    softap_config.beacon_interval = 100;
    //os_printf("start wifi %s %s", softap_config.ssid, softap_config.password);
    wifi_softap_set_config_current(&softap_config);

    os_memset(&wifi_softap_timer, 0, sizeof(wifi_softap_timer));

    os_timer_disarm(&wifi_softap_timer);
    os_timer_setfn(&wifi_softap_timer, (os_timer_func_t *)on_wifi_softap_timer, 0);
    os_timer_arm(&wifi_softap_timer, 1000, 0);
}

void ICACHE_FLASH_ATTR wifi_softap() {
    uart_debug_message("softap mode");
    wifi_set_opmode_current(SOFTAP_MODE);
    os_memset(&softap_config, 0, sizeof(softap_config));
    os_strncpy(softap_config.ssid, eeprom->wifi_client.ssid, sizeof(sta_config.ssid));
    os_strncpy(softap_config.password, eeprom->wifi_client.password, sizeof(sta_config.password));
    wifi_softap_finalize();
}

void ICACHE_FLASH_ATTR wifi_default() {
    uart_debug_message("default softap mode");
    wifi_set_opmode_current(SOFTAP_MODE);
    os_memset(&softap_config, 0, sizeof(softap_config));
    os_strcpy(softap_config.ssid, "SH");
    os_strcpy(softap_config.password, "please");
    wifi_softap_finalize();
}

void ICACHE_FLASH_ATTR wifi_init() {
    bridge_stop();
    wifi_set_event_handler_cb(_wifi_event_cb);
    switch(eeprom->wifi_client.mode) {
        case STATION_MODE: {
            wifi_station();
            break;
        }
        case SOFTAP_MODE: {
            wifi_softap();
            break;
        }
        default: {
            wifi_default();
            break;
        }
    }
}

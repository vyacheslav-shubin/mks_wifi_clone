#include "user_config.h"
#include "../sdk/c_types.h"
#include "../sdk/gpio.h"
#include "../sdk/user_interface.h"
#include "../sdk/os_type.h"
#include "../sdk/osapi.h"
#include "../sdk/ets_sys.h"
#include "uart_host.h"
#include "loop.h"
#include "wifi.h"
#include "eeprom.h"
#include "bridge.h"
#define LED_GPIO 2
#define LED_GPIO_FUNC FUNC_GPIO2

void ICACHE_FLASH_ATTR system_init_done(void);

void ICACHE_FLASH_ATTR user_init() {
    gpio_init();
    wifi_station_set_auto_connect(0);
    system_set_os_print(ENABLE_STD_OUT);
    wifi_set_opmode(NULL_MODE);
    wifi_set_opmode_current(NULL_MODE);
    uart_div_modify(0, UART_CLK_FREQ / 115200);
    init_uart();
    system_init_done_cb(system_init_done);
    wifi_status_led_uninstall();
}

void ICACHE_FLASH_ATTR system_init_done(void) {
    init_loop();
}



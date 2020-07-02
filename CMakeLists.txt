cmake_minimum_required(VERSION 3.16)
project(wifi_module C)

set(CMAKE_C_STANDARD 11)

include_directories(sdk)
include_directories(sdk/json)
include_directories(src)
include_directories(driver)

add_executable(wifi_module
		sdk/json/json.h
		sdk/json/jsonparse.h
		sdk/json/jsontree.h
		sdk/at_custom.h
		sdk/c_types.h
		sdk/eagle_soc.h
		sdk/espconn.h
		sdk/ets_sys.h
		sdk/gpio.h
		sdk/ip_addr.h
		sdk/mem.h
		sdk/os_type.h
		sdk/osapi.h
		sdk/ping.h
		sdk/queue.h
		sdk/smartconfig.h
		sdk/sntp.h
		sdk/spi_flash.h
		sdk/upgrade.h
		sdk/user_interface.h
		src/user.c
		src/user_config.h
		src/uart_host.c
		src/uart_host.h
		src/loop.c
		src/loop.h
		src/wifi.c
		src/wifi.h
		driver/uart.c
		driver/uart.h
		driver/uart_register.h
		src/eeprom.c
		src/eeprom.h src/bridge.c src/bridge.h src/http_server.c src/http_server.h gen/html.h src/ntp.c src/ntp.h)

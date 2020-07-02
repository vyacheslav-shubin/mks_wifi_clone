#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define FW_VERSION_NAME     "0.1.0"

#define ENABLE_STD_OUT  0
#define VERSION "SH ESP8266 V.0";

#define wdt_reset() WRITE_PERI_REG(0X60000914, 0X73)
#define led_on()    GPIO_OUTPUT_SET (2, 0)
#define led_off()    GPIO_OUTPUT_SET (2, 1)

#define NTP_SERVER_LIST \
	"ntp1.stratum1.ru", \
	"ntp2.stratum1.ru", \
	"ntp3.stratum1.ru", \
	"ntp4.stratum1.ru", \
	"ntp5.stratum1.ru", \
	"ntp2.stratum2.ru", \
	"ntp3.stratum2.ru", \
	"ntp4.stratum2.ru", \
	"ntp5.stratum2.ru"

typedef union {
    unsigned short s;
    unsigned char c[2];
} ShortLength;

#endif


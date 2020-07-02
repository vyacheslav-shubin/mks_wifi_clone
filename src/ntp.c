#include "../sdk/c_types.h"
#include "../sdk/ip_addr.h"
#include "../sdk/espconn.h"
#include "../sdk/mem.h"
#include "../sdk/osapi.h"
#include "../sdk/user_interface.h"
#include "ntp.h"
#include "loop.h"

#define NTP_PACKET_SIZE 48

const char* NTP_SERVERS[] = {NTP_SERVER_LIST};

#define SERVER_COUNT sizeof(NTP_SERVERS)/sizeof(char*)
#define SEVENTY_YEARS 2208988800UL
#define SECS_PER_YEAR 31536000UL
#define SECS_PER_LEARP_YEAR 31622400UL

ip_addr_t NTP_SERVERS_IP[SERVER_COUNT];

static unsigned char _ntp_current_server=0;
static esp_udp _ntp_udp;
static struct espconn _stratum_conn;
static unsigned char _ntp_server_ok = false;
u32 ntp_time = 0;
static u32 ntp_last_time;

static void _ntp_udp_recv_cb(void *arg, char *pdata, unsigned short len);

static void ICACHE_FLASH_ATTR _dns_cb(const char *name, ip_addr_t *ip, void *arg) {
    os_printf("STRATUM: %s\n", name);
    _ntp_server_ok=false;
    if (ip) {
        struct espconn *conn=arg;
        os_memset(&_ntp_udp, 0, sizeof(_ntp_udp));
        _ntp_udp.remote_port = 123;
        _ntp_udp.local_port = espconn_port();
        // Get IP Info
        struct ip_info ipinfo;
        ipinfo.ip.addr=0;
        ipinfo.netmask.addr=255;
        ipinfo.gw.addr=0;
        wifi_get_ip_info(0, &ipinfo);

        os_memcpy(&_ntp_udp.local_ip,&ipinfo.ip, 4);
        //

        conn->type = ESPCONN_UDP;
        conn->proto.udp = &_ntp_udp;
        conn->state=ESPCONN_NONE;
        conn->recv_callback=_ntp_udp_recv_cb;

        os_memcpy(&(conn->proto.udp->remote_ip), ip, 4);

        espconn_create(conn);

        static uint8 PACKET[NTP_PACKET_SIZE];
        os_memset(&PACKET, 0 , sizeof(PACKET));
        PACKET[0] = 0b11100011;     // LI, Version, Mode
        PACKET[1] = 0;              // Stratum, or type of clock
        PACKET[2] = 6;              // Polling Interval
        PACKET[3] = 0xEC;           // Peer Clock Precision
        // 8 bytes of zero for Root Delay & Root Dispersion
        PACKET[12] = 49;
        PACKET[13] = 0x4E;
        PACKET[14] = 49;
        PACKET[15] = 52;
        espconn_sent(conn, &PACKET[0], sizeof(PACKET));
    } else
        system_os_post(0, MESSAGE_NTP_ERROR, 0);
}

static inline void ICACHE_FLASH_ATTR _decode_ntp_message(uint8_t *messageBuffer) {
    ntp_time = (u32)messageBuffer[40] << 24;
    ntp_time |= (u32)messageBuffer[41] << 16;
    ntp_time |= (u32)messageBuffer[42] << 8;
    ntp_time |= (u32)messageBuffer[43];
    ntp_time-=(u32)SEVENTY_YEARS;
}

static void ICACHE_FLASH_ATTR _ntp_udp_recv_cb(void *arg, char *pdata, unsigned short len) {
    struct espconn *conn=arg;
    _decode_ntp_message(pdata);
    system_os_post(0, MESSAGE_NTP_LOADED, 0);
    _ntp_server_ok=true;
    //post_message(MSG_NTP_LOADED);
}

void ICACHE_FLASH_ATTR ntp_init(s16 offset_munutes) {
    os_memset(&_stratum_conn, 0, sizeof(_stratum_conn));
    _ntp_current_server=0;
    _ntp_server_ok=true;
}


void ICACHE_FLASH_ATTR ntp_set_time() {
    if (_stratum_conn.state != ESPCONN_NONE)
        espconn_delete(&_stratum_conn);

    if (!_ntp_server_ok) {
        _ntp_current_server++;
        if (_ntp_current_server >= SERVER_COUNT)
            _ntp_current_server=0;
    }

    os_printf("STRATUM SET TIME. USE SERVER #%d -> %s\n", _ntp_current_server, NTP_SERVERS[_ntp_current_server]);
    os_memset(&_stratum_conn, 0, sizeof(_stratum_conn));
    err_t err = espconn_gethostbyname(&_stratum_conn, NTP_SERVERS[_ntp_current_server], &NTP_SERVERS_IP[_ntp_current_server], _dns_cb);
}

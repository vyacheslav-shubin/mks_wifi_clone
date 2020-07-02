//
// Created by shubin on 18.06.2020.
//

#include "../user_interface.h"
#include "uart_host.h"
#include "user_config.h"
#include "loop.h"
#include "../driver/uart_register.h"
#include "../sdk/eagle_soc.h"
#include "../sdk/ets_sys.h"
#include "../sdk/osapi.h"
#include "../sdk/mem.h"
#include "../sdk/gpio.h"


void uart_int_handler(void *va);
inline void process_uart(unsigned char msg);


void ICACHE_FLASH_ATTR init_uart() {
    gpio_register_set(GPIO_PIN_ADDR(LOOPBACK_PIN), GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE) | GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));
    // Порог прерывания 1 байт
    WRITE_PERI_REG(UART_CONF1(USED_UART), (1 << UART_RXFIFO_FULL_THRHD_S));
    // Регистрация обработчика прерывания
    ETS_UART_INTR_ATTACH(uart_int_handler, 0);
    // Маска прерывания
    WRITE_PERI_REG(UART_INT_ENA(USED_UART), UART_RXFIFO_FULL_INT_ENA);
    SET_PERI_REG_MASK(UART_CONF0(USED_UART), UART_RXFIFO_RST | UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0(USED_UART), UART_RXFIFO_RST | UART_TXFIFO_RST);
    WRITE_PERI_REG(UART_INT_CLR(USED_UART), 0xffff);
    //SET_PERI_REG_MASK(UART_INT_ENA(USED_UART), UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_OVF_INT_ENA | UART_TXFIFO_EMPTY_INT_ENA);
    SET_PERI_REG_MASK(UART_INT_ENA(USED_UART), UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_OVF_INT_ENA);
    ETS_UART_INTR_ENABLE();
}

volatile unsigned char _uart_msg_count = 0;
unsigned char * _uart_message_queue[10];
volatile unsigned char tx_has_data = 0;

void uart_int_handler(void *va) {
    while(1) {
        const uint32 status = READ_PERI_REG(UART_INT_ST(USED_UART));
        if (status == 0)
            break;
        if (status & UART_RXFIFO_FULL_INT_ST) {
            // обработка прерывания по приёму данных
            uint32 fifoLen = (READ_PERI_REG(UART_STATUS(USED_UART)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT;
            while (fifoLen-- != 0)
                process_uart(READ_PERI_REG(UART_FIFO(USED_UART)));
            // очистка прерывания
            WRITE_PERI_REG(UART_INT_CLR(USED_UART), UART_RXFIFO_FULL_INT_CLR);
        }
        if (status & UART_TXFIFO_EMPTY_INT_ST) {
            tx_has_data = 0;
            WRITE_PERI_REG(UART_INT_CLR(USED_UART), UART_TXFIFO_EMPTY_INT_CLR);
            led_off();
        }
    }
    for (unsigned char i=0;i<_uart_msg_count;i++)
        system_os_post(0, MESSAGE_FROM_UART, (ETSParam)_uart_message_queue[i]);
    _uart_msg_count = 0;
}


typedef struct{
    unsigned short state;
    unsigned short index;
    unsigned char  command;
    ShortLength length;
    unsigned char * current_message;
} UartReadState;

UartReadState _read_state = {0, 0, 0, 0};

#define UART_STATE_MESSAGE_STARTED  BIT(1)

unsigned char _sync_dma = 0;
static unsigned short _sync_dma_byte_count = 0;

int ICACHE_FLASH_ATTR uart_tx_buffer_queue_len() {
    return ((READ_PERI_REG(UART_STATUS(USED_UART)) >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT);
}

void ICACHE_FLASH_ATTR uart_wait_output_finished() {
    while (uart_tx_buffer_queue_len())
        wdt_reset();
}

void ICACHE_FLASH_ATTR uart_wait_tx_buffer() {
    while (uart_tx_buffer_queue_len() >= 126)
        wdt_reset();
}

void ICACHE_FLASH_ATTR uart_sync_dma(unsigned char v) {
    //uart_wait_output_finished();
    if (_sync_dma!=v) {
        if (v==0) while (_sync_dma_byte_count) {
            uart_tx(0xFF);
            uart_wait_output_finished();
        }
        _sync_dma = v;
        //uart_div_modify(USED_UART, UART_CLK_FREQ / (_sync_dma ? 1958400 : 115200));
        uart_div_modify(USED_UART, _sync_dma ? 41 : 694);
    }
}

void ICACHE_FLASH_ATTR uart_tx1(unsigned char c) {
    while (true) {
        uint32 fifo_cnt = READ_PERI_REG(UART_STATUS(USED_UART)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S);
        if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) <= 126)
            break;
    }
    if (_sync_dma_byte_count==0)
        while (uart_loopback_state()) {
            led_on();
            wdt_reset();
        }
    led_off();

    tx_has_data = 1;
    WRITE_PERI_REG(UART_FIFO(USED_UART), c);
    if (_sync_dma) {
        _sync_dma_byte_count++;
        if (_sync_dma_byte_count==1024)
            _sync_dma_byte_count = 0;
    }
}


void ICACHE_FLASH_ATTR uart_tx(unsigned char c) {
    //while (true) {
    //    uint32 fifo_cnt = READ_PERI_REG(UART_STATUS(USED_UART)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S);
    //    if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) <= 126)
    //        break;
    //}
    led_on();
    if (_sync_dma_byte_count==0) {
        uart_wait_output_finished();
        while (uart_loopback_state()) {
            wdt_reset();
        }
    } else {
        uart_wait_tx_buffer();
    }

    tx_has_data = 1;
    WRITE_PERI_REG(UART_FIFO(USED_UART), c);
    if (_sync_dma) {
        _sync_dma_byte_count++;
        if (_sync_dma_byte_count==1024)
            _sync_dma_byte_count = 0;
    }
    led_off();
}

void ICACHE_FLASH_ATTR uart_start_message(unsigned char cmd, unsigned short len) {
    uart_tx(ESP_PROTOC_HEAD);
    uart_tx(cmd);
    uart_tx(len & 0xFF);
    uart_tx((len>>8) & 0xFF);
}

void ICACHE_FLASH_ATTR uart_end_message() {
    uart_tx(ESP_PROTOC_TAIL);
}

void ICACHE_FLASH_ATTR uart_message(const unsigned char * msg, unsigned short len) {
    for (unsigned long i=0;i<len;i++)
        uart_tx(msg[i]);
}

void ICACHE_FLASH_ATTR uart_data_send(const unsigned char * msg, unsigned short len, unsigned char cmd) {
    //if (_sync_dma)
    //    return;
    uart_start_message(cmd, len);
    uart_message(msg, len);
    uart_end_message();
}

void ICACHE_FLASH_ATTR uart_out(UartSendMessage * uartSendMessage) {
    uart_data_send(uartSendMessage->message, uartSendMessage->size, uartSendMessage->command);
    if (uartSendMessage->is_dynamic_memory)
        os_free(uartSendMessage->message);
    os_free(uartSendMessage);
}

void ICACHE_FLASH_ATTR uart_message_async(UartSendMessage * uartSendMessage) {
    if (!system_os_post(0, MESSAGE_SEND_TO_UART, (ETSParam)uartSendMessage))
        uart_debug_message("error message");
}

void ICACHE_FLASH_ATTR uart_debug_message(const unsigned char * msg) {
#if ENABLE_STD_OUT
    os_printf(msg);
    os_printf("\n");
#else
    uart_data_send(msg, os_strlen(msg), ESP_TYPE_DEBUG);
#endif
}

void inline _uart_transact(unsigned char * message) {
    if (_uart_msg_count>9) {
        if (message)
            os_free(message);
    } else {
        _uart_message_queue[_uart_msg_count++] = message;
    }
}

inline void process_uart(unsigned char msg) {
    if (_read_state.state & UART_STATE_MESSAGE_STARTED) {
        if (msg==ESP_PROTOC_TAIL) {
            if (_read_state.index - 3 != _read_state.length.s) {
                os_free(_read_state.current_message);
                _read_state.current_message = 0;
            }
            _uart_transact(_read_state.current_message);
            _read_state.state = 0;
            return;
        }
        switch(_read_state.index) {
            case 0:
                _read_state.command = msg;
                break;
            case 1:
                _read_state.length.c[0] = msg;
                break;
            case 2:
                _read_state.length.c[1] = msg;
                if (_read_state.length.s < 1024) {
                    _read_state.current_message = (unsigned char *)os_malloc(3 + _read_state.length.s);
                    _read_state.current_message[0] = _read_state.command;
                    _read_state.current_message[1] = _read_state.length.c[0];
                    _read_state.current_message[2] = _read_state.length.c[1];
                } else {
                    _read_state.state = 0;
                }
                break;
            default: {
                if (_read_state.index - 3 < _read_state.length.s)
                    _read_state.current_message[_read_state.index] = msg;
                else {
                    os_free(_read_state.current_message);
                    _uart_transact(0);
                    _read_state.state = 0;
                }
                break;
            }
        }
        _read_state.index++;
    } else {
        if (msg==ESP_PROTOC_HEAD) {
            os_memset(&_read_state, 0, sizeof(_read_state));
            _read_state.state = UART_STATE_MESSAGE_STARTED;
        }
    }
}
#include <avr/io.h>
#include <stdio.h>

#define F_CPU 16000000UL

#include <serial.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "nrf24l01+/radio.h"

/*
  BASIC SETTINGS
*/

// 3 to 5
#define ADDRESS_WIDTH 3

// 1 to 32
#define PAYLOAD_WIDTH 1

// In multiples of 250us (i.e. 10 is 2.5ms, 2 is 500us)
// 1-16
#define RETRANSMIT_DELAY 3

#define MAX_RETRANSMIT_COUNT 15

// 2400 to 2527 MHz. Note the UK legal range: 2400-2483.5 MHz
#define FREQUENCY_CHANNEL 2498 

// 0 - lowest, 3 - highest.
#define TRANSMISSION_POWER 3

/*
   END OF BASIC SETTINGS
*/

uint8_t last_n; 
uint32_t received, lost;

void getPrintableAddress(uint8_t reg, char printableAddr[ADDRESS_WIDTH * 2 + 3]) {
    uint8_t addr[ADDRESS_WIDTH];
    radioGetReg(reg, addr, ADDRESS_WIDTH);
    sprintf(printableAddr, "0x%02x%02x%02x", //%02x%02x",
        addr[0],
        addr[1],
        addr[2]
        //addr[3],
        //addr[4]
    );
}

void print_status(void) {
    uint8_t status = radioGetStatus();
    printf("STATUS   = 0x%02x ", status);
    printf("RX_DR=%u ", !!(status & _BV(RX_DR)));
    printf("TX_DS=%u ", !!(status & _BV(TX_DS)));
    printf("MAX_RT=%u ", !!(status & _BV(MAX_RT)));
    printf("RX_P_NO=%u ", (status >>  RX_P_NO) & 7);
    printf("TX_FULL=%u\n", !!(status & _BV(TX_FULL)));
}

void print_losses(void) {
    uint8_t observe_tx;
    radioGetReg(OBSERVE_TX, &observe_tx, 1);
    printf("Lost packets: %u\n", observe_tx >> 4);
    printf("Retransmits: %u\n\n", observe_tx & 15);
}

void print_details(void) {
    uint8_t rx_addr_p2, rx_addr_p3, rx_addr_p4, rx_addr_p5;
    char rx_addr_p0[13], rx_addr_p1[13], tx_addr[13];
    uint8_t rx_pw_p[6];
    uint8_t en_aa, en_rxaddr, rf_ch, rf_setup, config, dynpd, feature;
    uint8_t setup_retr;

    print_status();    

    getPrintableAddress(RX_ADDR_P1, rx_addr_p1);
    getPrintableAddress(RX_ADDR_P0, rx_addr_p0);
    printf("RX_ADDR_P0-1     = %s %s\n", rx_addr_p0, rx_addr_p1);

    radioGetReg(RX_ADDR_P2, &rx_addr_p2, 1);
    radioGetReg(RX_ADDR_P3, &rx_addr_p3, 1);
    radioGetReg(RX_ADDR_P4, &rx_addr_p4, 1);
    radioGetReg(RX_ADDR_P5, &rx_addr_p5, 1);
    printf("RX_ADDR_P2-5     = 0x%02x 0x%02x 0x%02x 0x%02x\n",
        rx_addr_p2,
        rx_addr_p3,
        rx_addr_p4,
        rx_addr_p5
    );

    getPrintableAddress(TX_ADDR, tx_addr);
    printf("TX_ADDR          = %12s\n", tx_addr);

    radioGetReg(RX_PW_P0, &rx_pw_p[0], 1);
    radioGetReg(RX_PW_P1, &rx_pw_p[1], 1);
    radioGetReg(RX_PW_P2, &rx_pw_p[2], 1);
    radioGetReg(RX_PW_P3, &rx_pw_p[3], 1);
    radioGetReg(RX_PW_P4, &rx_pw_p[4], 1);
    radioGetReg(RX_PW_P5, &rx_pw_p[5], 1);

    printf("RX_PW_P0-5       = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
        rx_pw_p[0],
        rx_pw_p[1],
        rx_pw_p[2],
        rx_pw_p[3],
        rx_pw_p[4],
        rx_pw_p[5]
    );

    radioGetReg(EN_AA, &en_aa, 1);
    radioGetReg(EN_RXADDR, &en_rxaddr, 1);
    radioGetReg(RF_CH, &rf_ch, 1);
    radioGetReg(RF_SETUP, &rf_setup, 1);
    radioGetReg(CONFIG, &config, 1);
    radioGetReg(DYNPD, &dynpd, 1);
    radioGetReg(FEATURE, &feature, 1);

	printf("EN_AA            = 0x%02x\n", en_aa);
	printf("EN_RXADDR        = 0x%02x\n", en_rxaddr);
	printf("RF_CH            = 0x%02x\n", rf_ch);
	printf("RF_SETUP         = 0x%02x\n", rf_setup);
	printf("CONFIG           = 0x%02x\n", config);
	printf("DYNPD/FEATURE    = 0x%02x 0x%02x\n", dynpd, feature);

    printf("Data Rate        = ");
    if (rf_setup & _BV(RF_DR_HIGH) && rf_setup & _BV(RF_DR_LOW))
        printf("Impossible 11\n");
    else if (rf_setup & _BV(RF_DR_HIGH))
        printf("2MBPS\n");
    else if (rf_setup & _BV(RF_DR_LOW))
        printf("250KBPS\n");
    else
        printf("1MBPS\n");

    puts("Model            = nRF24l01+");

    printf("CRC Length       = ");
    if (!(config & _BV(EN_CRC)))
        printf("Disabled\n");
    else if (config & _BV(CRCO))
        printf("16 bits\n");
    else
        printf("8 bits\n");

    printf("PA Power         = ");
    if (rf_setup & _BV(RF_PWR_LOW) && rf_setup & _BV(RF_PWR_HIGH))
        printf("PA_MAX\n");
    else if (rf_setup & _BV(RF_PWR_HIGH))
        printf("PA_HIGH\n");
    else if (rf_setup & _BV(RF_PWR_LOW))
        printf("PA_LOW\n");
    else
        printf("PA_MIN\n");

    radioGetReg(SETUP_RETR, &setup_retr, 1);
    printf("Retransmit delay = %ux250us\n",
        (setup_retr >> ARD) + 1
    );
    printf("Retransmit count = %u\n", setup_retr & 0x0f);

}

void enableInterrupt(void) {
    DDRD &= ~_BV(DDD2);
    PORTD &= ~_BV(PORTD2);
    EICRA &= ~_BV(ISC00);
    EICRA &= ~_BV(ISC01);  // Low-level
    EIMSK |= _BV(INT0);

    sei();
}

void configureRadio(void) {
    uint8_t reg_value;

    // CONFIG
    reg_value = 0;
    reg_value |= _BV(EN_CRC);   // Enable CRC
    //reg_value |= _BV(CRCO);   // 16-bit CRC
    reg_value |= _BV(PRIM_RX);  // Primary Receiver
    printf("New config value is 0x%02x\n", reg_value);
    radioSetReg(CONFIG, &reg_value, 1);

    // EN_AA
    reg_value = 0;
    // Enable auto acknowledgements on every pipe
    reg_value |= _BV(ENAA_P0);
    reg_value |= _BV(ENAA_P1);
    reg_value |= _BV(ENAA_P2);
    reg_value |= _BV(ENAA_P3);
    reg_value |= _BV(ENAA_P4);
    reg_value |= _BV(ENAA_P5);
    radioSetReg(EN_AA, &reg_value, 1);

    // EN_RXADDR
    reg_value = 0;
    reg_value |= _BV(ERX_P0);  // Enable pipe #0
    reg_value |= _BV(ERX_P1);  // Enable pipe #0
    radioSetReg(EN_RXADDR, &reg_value, 1);

    // SETUP_AW
    reg_value = 0;
    reg_value |= ADDRESS_WIDTH - 2;
    radioSetReg(SETUP_AW, &reg_value, 1);

    // SETUP_RETR
    reg_value = 0;
    reg_value |= (RETRANSMIT_DELAY - 1) << ARD;
    reg_value |= MAX_RETRANSMIT_COUNT;
    radioSetReg(SETUP_RETR, &reg_value, 1);

    // RF_CH
    reg_value = 0;
    reg_value |= FREQUENCY_CHANNEL - 2400;
    radioSetReg(RF_CH, &reg_value, 1);

    // RF_SETUP
    reg_value = 0;
    reg_value |= _BV(RF_DR_LOW);  // 250 Kbps
    reg_value |= TRANSMISSION_POWER << RF_PWR;
    radioSetReg(RF_SETUP, &reg_value, 1);

    // STATUS
    reg_value = 0;
    // Clear asserted interrupts
    reg_value |= _BV(RX_DR);
    reg_value |= _BV(TX_DS);
    reg_value |= _BV(MAX_RT);
    radioSetReg(STATUS, &reg_value, 1);  // Clear asserted interrupts

    // RX_PW_P*
    reg_value = PAYLOAD_WIDTH;
    radioSetReg(RX_PW_P0, &reg_value, 1);
    radioSetReg(RX_PW_P1, &reg_value, 1);

    reg_value = 0;
    radioSetReg(RX_PW_P2, &reg_value, 1);
    radioSetReg(RX_PW_P3, &reg_value, 1);
    radioSetReg(RX_PW_P4, &reg_value, 1);
    radioSetReg(RX_PW_P5, &reg_value, 1);

    // DYNPD
    reg_value = 0;
    radioSetReg(DYNPD, &reg_value, 1);

    // FEATURE
    reg_value = 0;
    radioSetReg(FEATURE, &reg_value, 1);
    
    radioSendCommand(FLUSH_TX);
    radioSendCommand(FLUSH_RX);

    radioPowerOn();
}

inline void transmit_loop(void) {
    uint8_t payload, i;
    status_t status;
    
    uint8_t local_addr[] = {0xa1, 0xb2, 0xc3};
    radioPrepareTX(local_addr, ADDRESS_WIDTH);

    i = 0;
    while (1) {
        payload = i++;

        status = radioGetStatus();
        if (status & _BV(TX_FULL))
            radioPushTX();
        else
            radioTransmitPayload(&payload, PAYLOAD_WIDTH);

        while (1) {
            status = radioGetStatus();
            if (status & (_BV(TX_DS) | _BV(MAX_RT))) {
                if (status & _BV(TX_DS)) {
                    puts("Transmitted the packet successfully");
                } else {
                    puts("Transmission timeout");
                    radioSendCommand(FLUSH_TX);
                    radioSendCommand(FLUSH_RX);
                }
                status |= _BV(TX_DS);
                status |= _BV(MAX_RT);
                radioSetReg(STATUS, &status, 1);  // Reset assert flags
                break;
            }
            _delay_ms(1);
        }
        _delay_ms(10);
    }
}

inline void handle_packet(void) {
    uint8_t payload; 
    status_t status;
    radioChipDisable();

    status = radioGetStatus();
    if (status & _BV(MAX_RT)) {
        radioSendCommand(FLUSH_TX);
    }

    status |= _BV(RX_DR);
    status |= _BV(TX_DS);
    status |= _BV(MAX_RT);

    radioSetReg(STATUS, &status, 1);

    while (1) {
        if ((status & 0xE) == 0xE) {
            break;
        }
        radioSendCommandGet(R_RX_PAYLOAD, &payload, 1);
        /*if (received != 0) {
            if (payload < last_n) {
                received--;
                puts("");
            } else {
                lost += payload - last_n - 1;
                printf("(%lu / %lu) %lu lost\n",
                    received,
                    received + lost,
                    lost
                );
            }
        } else {
            puts("");
        }
        received++;
        last_n = payload;
        */
        printf("%c", payload);

        status = radioGetStatus();
    }

    radioListen();
}

inline void receive_loop(void) {
    last_n = 0;
    lost = 0;
    received = 0;

    radioListen();
    while (1) {
        _delay_ms(1000);
    }
}

int main(void) {
    char input;

    uartInit();
    stdout = &uartOutput;
    stdin  = &uartInput;

	//puts("Press a key to start");
	//getchar();

    radioInit();

    puts("Initialising NRF24l01+ chip...");
    configureRadio();
    
    print_details();

    puts("\n");
    
    uint8_t local_addr[] = {0xa1, 0xb2, 0xc3};
    uint8_t remote_addr[] = {0xa1, 0xb2, 0xc4};
    radioPrepareTX(local_addr, ADDRESS_WIDTH);
    radioSetReg(RX_ADDR_P1, remote_addr, ADDRESS_WIDTH);
    enableInterrupt();

    //while(1)
    //    radioDoCarrierTest();


    radioListen();
    while (1) {
        input = getchar();
        if (input == '\r')
            input = '\n';
        putchar(input);
        radioChipDisable();
        radioTransmitPayload((uint8_t*)&input, 1);
        //receive_loop();
        //print_losses();
    }

    return 1;
}

ISR(INT0_vect) {
    handle_packet();
}

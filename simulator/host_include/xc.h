#ifndef BLUEBUS_SIM_XC_H
#define BLUEBUS_SIM_XC_H

#include <stdint.h>

typedef struct UART {
    volatile uint16_t uxmode;
    volatile uint16_t uxsta;
    volatile uint16_t uxbrg;
    volatile uint16_t uxrxreg;
    volatile uint16_t uxtxreg;
} UART;

typedef struct {
    unsigned RD0: 1;
} BlueBusSimPORTDBits_t;

extern volatile BlueBusSimPORTDBits_t PORTDbits;

#endif

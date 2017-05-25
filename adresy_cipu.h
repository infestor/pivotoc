#ifndef __ADRESY_CIPU_H__
#define __ADRESY_CIPU_H__

#include <avr/io.h>

#define CIP_ADDR_LEN 8

extern const uint8_t ADRESY_CIPU[][CIP_ADDR_LEN];

#define POCET_CIPU 5
//#define POCET_CIPU sizeof(ADRESY_CIPU) / sizeof(*ADRESY_CIPU)

#endif

#ifndef __ADRESY_CIPU_H__
#define __ADRESY_CIPU_H__

#include <avr/io.h>
#include "onewire.h"

extern const uint8_t ADRESY_CIPU[][OW_ROMCODE_SIZE];

#define POCET_CIPU 5
//#define POCET_CIPU sizeof(ADRESY_CIPU) / sizeof(*ADRESY_CIPU)

#endif

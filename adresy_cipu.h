#ifndef __ADRESY_CIPU_H__
#define __ADRESY_CIPU_H__

#include <avr/io.h>
#include "onewire.h"

#define POCET_CIPU 41
//#define POCET_CIPU sizeof(ADRESY_CIPU) / sizeof(*ADRESY_CIPU)


extern const uint8_t ADRESY_CIPU[POCET_CIPU][OW_ROMCODE_SIZE];


#endif

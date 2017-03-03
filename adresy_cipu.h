#ifndef __ADRESY_CIPU_H__
#define __ADRESY_CIPU_H__

#define CIP_ADDR_LEN 8

const uint8_t ADRESY_CIPU[][CIP_ADDR_LEN] = {
	{0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8},
	{0x1,0x2,0xA,0xB,0xC,0xD,0xE,0xF},
	{0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7},
	{0x9,0x8,0x7,0x6,0x5,0x4,0x3,0x2},
	{0x5,0x5,0x5,0x5,0x6,0x6,0x6,0x6}
};

#endif

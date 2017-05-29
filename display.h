#ifndef __DISPLAY_H__
#define __DISPLAY_H__

enum MozneStavyDispleje {
	DISP_STAV_NIC = 0,
	DISP_STAV_OFF,
	DISP_STAV_VYPINAM,
	DISP_STAV_INICIALIZACE,
	DISP_STAV_NEZNAMY_CIP,
	DISP_STAV_ZAKLADNI_VYCEP,
	DISP_STAV_ZAKLADNI_SPRAVA,
	DISP_STAV_VYCEP_ZAKAZNIK_FULL,
	DISP_STAV_VYCEP_ZAKAZNIK_LITRY,
	DISP_STAV_SPRAVA_ZAKAZNIK,
	DISP_STAV_SPRAVA_CENA,
	DISP_STAV_SPRAVA_CIPY,
	DISP_STAV_POWER_LOSS
};

#define DISPLAY_FRONTA_MAXLEN 5
#define DISP_SIZE 43 //2x20 + jden znak na kazdej radek + 1 na zalomeni


void DisplayFrontaAdd(uint8_t novy_stav);
void DisplayFrontaPush(uint8_t novy_stav);
uint8_t DisplayFrontaPop(void);
void ZobrazInfoCipSprava(uint8_t id);
void ZobrazInfoCipVytoc(uint8_t id, bool both);
void ZobrazInfoCenaEdit(void);
void PrekreslitDisplay(uint8_t novy_stav);


#endif

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

enum MozneStavyDispleje {
	DISP_STAV_NIC = 0,
	DISP_STAV_ZAKLADNI,
	DISP_STAV_ZAKLADNI_SPRAVA,
	DISP_STAV_INICIALIZACE,
	DISP_STAV_NEZNAMY_CIP,
	DISP_STAV_VYCEP_ZAKAZNIK_FULL,
	DISP_STAV_VYCEP_ZAKAZNIK_LITRY,
	DISP_STAV_SPRAVA_ZAKAZNIK
};

const char SCREEN_ZAKLADNI[]           = " *STOPARI NYMBURK*\n      *VYCEP*";
const char SCREEN_ZAKLADNI_SPRAVA[]    = " *STOPARI NYMBURK*\n     *SPRAVA*";
const char SCREEN_VYCEP_ZAKAZNIK_L1[]  = "VYCEP ZAKAZNIK %02d\n";
const char SCREEN_VYCEP_ZAKAZNIK_L2[]  = "s[l]:%2.2f nyni:%1.2f";
const char SCREEN_SPRAVA_ZAKAZNIK[]    = "SPRAVA ZAKAZNIK %02d\ns[l]:%2.2f cena:%d,-";
const char SCREEN_CIP_NEZNAMY[]        = " !! NEZNAMY CIP !!";
const char SCREEN_INICIALIZACE[]       = "   INICIALIZACE..";

#define DISPLAY_FRONTA_MAXLEN 5
#define DISP_SIZE 43 //2x20 + jden znak na kazdej radek + 1 na zalomeni


void DisplayFrontaAdd(uint8_t novy_stav);
void DisplayFrontaPush(uint8_t novy_stav);
uint8_t DisplayFrontaPop(void);
void ZobrazInfoCipSprava(uint8_t id);
void ZobrazInfoCipVytoc(uint8_t id, bool both);
void PrekreslitDisplay(uint8_t novy_stav);


#endif
#include "avr/io.h"
#include "display.h"

#define IMPULZ_COUNTER TCNT1

volatile uint8_t display_fronta[DISPLAY_FRONTA_MAXLEN];
volatile uint8_t display_fronta_len;
volatile uint8_t display_posledni_stav;
volatile char displej_text[DISP_SIZE];

void DisplayFrontaAdd(uint8_t novy_stav)
{
	//pridame stav na konec pole (doufam ze nenastane pripad, aby se pole nekdy preplnilo)
	//to pak nevim jestli je lepci diskardovat prvni a nebo posledni polozku
	//ted se holt kdyztak nahradi dycky ta posledni
	display_fronta[display_fronta_len] = novy_stav;
	display_fronta_len++;
	if (display_fronta_len == DISPLAY_FRONTA_MAXLEN) display_fronta_len--;
}

//======================================================
void DisplayFrontaPush(uint8_t novy_stav)
{
	//vlozime stav na uplny zacatek pole - bude pri refreshi hned zobrazen (prednostni funkce)

	//posuneme vsechny polozky, ale musime jit odzadu
	for (uint8_t i = display_fronta_len; i > 0; i--)
	{
		display_fronta[i] = display_fronta[i-1];
	}

	display_fronta[0] = novy_stav;
	display_fronta_len++;
	if (display_fronta_len == DISPLAY_FRONTA_MAXLEN) display_fronta_len--;
}

//======================================================
uint8_t DisplayFrontaPop(void)
{
	//vratime stav na pozici 0 a cele pole pak posuneme o jednu niz cimz se puvodni nulova pozice prepise
	//a nakonec jednu nulu (=stav NIC) vlozime

	//asi muzem rict, ze kdyz je pole prazdne, vracime automaticky stav NIC
	if (display_fronta_len == 0) return DISP_STAV_NIC;

	uint8_t vratit = display_fronta[0];
	for (uint8_t i=0; i < display_fronta_len; i++)
	{
		display_fronta[i] = display_fronta[i+1];
	}

	display_fronta[display_fronta_len] = DISP_STAV_NIC;
	display_fronta_len--;

	return vratit;
}

//======================================================
void ZobrazInfoCipSprava(uint8_t id)
{
	double litru = double(AKUMULOVANE_IMPULZY[id] + AKTUALNI_IMPULZY[id]) / IMPULZY_NA_LITR;
	uint16_t cena = (AKTUALNI_IMPULZY[id] * CENA_ZA_IMPULZ * 100) + AKUMULOVANA_CENA[id]; //je to na halire

	//cena total je v korunach a zaokrouhluje se nahoru
	uint16_t cena_total = (cena / 100);
	if ((cena % 100) > 0) cena_total++;

	sprintf((char *)displej_text, SCREEN_SPRAVA_ZAKAZNIK, id+1, litru, cena_total);
}

//======================================================
void ZobrazInfoCipVytoc(uint8_t id, bool both=true)
{
	double litru = double(AKUMULOVANE_IMPULZY[id] + AKTUALNI_IMPULZY[id] + IMPULZ_COUNTER) / IMPULZY_NA_LITR;
	double nyni = double(IMPULZ_COUNTER) / IMPULZY_NA_LITR;

	if (both == true)
	{
		sprintf((char *)displej_text, SCREEN_VYCEP_ZAKAZNIK_L1, id+1);
	}

	sprintf((char *)displej_text, SCREEN_VYCEP_ZAKAZNIK_L2, litru, nyni);
}

//======================================================
void PrekreslitDisplay(uint8_t novy_stav)
{
	LCDputs((char *)displej_text);
}

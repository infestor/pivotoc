#include <stdio.h>
#include "avr/io.h"
#include "adresy_cipu.h"
#include "display.h"
#include "lcd.h"

#define IMPULZ_COUNTER TCNT1

volatile uint8_t display_fronta[DISPLAY_FRONTA_MAXLEN];
volatile uint8_t display_fronta_len;
//volatile uint8_t display_posledni_stav;
volatile char displej_text[DISP_SIZE];

extern volatile uint16_t IMPULZY_NA_LITR;
extern volatile double   CENA_ZA_IMPULZ;
extern volatile uint8_t  CENA_PIVA;

//#define POCET_CIPU 5 //TODO: nejak to postelovat jinak aby se to propagovalo z adresy_cipu.h a ne takle prasacky

extern volatile uint16_t AKTUALNI_IMPULZY[POCET_CIPU];
extern volatile uint16_t AKUMULOVANE_IMPULZY[POCET_CIPU];
extern volatile uint16_t AKUMULOVANA_CENA[POCET_CIPU];

extern volatile uint8_t  prihlaseny_cip_id;
extern volatile uint8_t  sprava_zobrazeny_zakaznik;
extern volatile uint8_t  sprava_temp_cena;

//======================================================
//======================================================
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
	//tady pozor! - nezapocitavame stav IMPULZ_COUNTER protoze ten je aktivni jen pri stavu VYTOC,
	//tady je vsecko uz ulozeno v polich jednotlivych uzivatelu
	double litru = double(AKUMULOVANE_IMPULZY[id] + AKTUALNI_IMPULZY[id]) / IMPULZY_NA_LITR;
	uint16_t cena = (AKTUALNI_IMPULZY[id] * CENA_ZA_IMPULZ * 100) + AKUMULOVANA_CENA[id]; //je to na halire

	//cena total je v korunach a zaokrouhluje se nahoru
	uint16_t cena_total = (cena / 100);
	if ((cena % 100) > 0) cena_total++;

	sprintf((char *)displej_text, SCREEN_SPRAVA_ZAKAZNIK, id+1, litru, cena_total);
	lcd_puts((char *)displej_text);
}

//======================================================
void ZobrazInfoCipVytoc(uint8_t id, bool both=true)
{
	double litru = double(AKUMULOVANE_IMPULZY[id] + AKTUALNI_IMPULZY[id] + IMPULZ_COUNTER) / IMPULZY_NA_LITR;
	//tady berem jako NYNI jen aktualni stav counteru po dobu prihlaseni cipu
	double nyni = double(IMPULZ_COUNTER) / IMPULZY_NA_LITR;

	if (both == true)
	{
		sprintf((char *)displej_text, SCREEN_VYCEP_ZAKAZNIK_L1, id+1);
		lcd_puts((char *)displej_text);
	}

	lcd_gotoxy(0, 1); //zacatek druheho radku
	sprintf((char *)displej_text, SCREEN_VYCEP_ZAKAZNIK_L2, litru, nyni);
	lcd_puts((char *)displej_text);
}

//======================================================
void ZobrazInfoCenaEdit(void)
{
	//double cena = double(sprava_temp_cena) * 0.5;
	uint8_t cena_jednotky = sprava_temp_cena / 2;
	uint8_t cena_desetiny = (sprava_temp_cena % 2) * 5;

	const char *predloha;
	if (cena_jednotky != CENA_PIVA) {
		predloha = SCREEN_SPRAVA_CENA_EDIT;
	}
	else {
		predloha = SCREEN_SPRAVA_CENA;
	}

	sprintf((char *)displej_text, predloha, cena_jednotky, cena_desetiny);
	lcd_puts((char *)displej_text);
}

//======================================================
void PrekreslitDisplay(uint8_t novy_stav)
{
	lcd_clrscr();

	if (novy_stav == DISP_STAV_OFF) {
		lcd_puts(SCREEN_OFF);
	}
	else if (novy_stav == DISP_STAV_INICIALIZACE) {
		lcd_puts(SCREEN_INICIALIZACE);
	}
	else if (novy_stav == DISP_STAV_ZAKLADNI_VYCEP) {
		lcd_puts(SCREEN_ZAKLADNI);
	}
	else if (novy_stav == DISP_STAV_ZAKLADNI_SPRAVA) {
		lcd_puts(SCREEN_ZAKLADNI_SPRAVA);
	}
	else if (novy_stav == DISP_STAV_VYPINAM) {
		lcd_puts(SCREEN_VYPINAM);
	}
	else if (novy_stav == DISP_STAV_NEZNAMY_CIP) {
		lcd_puts(SCREEN_CIP_NEZNAMY);
	}
	// pokrocile zobrazeni ---------------------------------------------------
	else if (novy_stav == DISP_STAV_VYCEP_ZAKAZNIK_FULL)
	{
		ZobrazInfoCipVytoc(prihlaseny_cip_id);
	}
	else if (novy_stav == DISP_STAV_SPRAVA_ZAKAZNIK)
	{
		ZobrazInfoCipSprava(sprava_zobrazeny_zakaznik);
	}
	else if (novy_stav == DISP_STAV_SPRAVA_CENA)
	{
		ZobrazInfoCenaEdit();
	}
}

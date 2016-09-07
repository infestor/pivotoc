#ifdef COMPILE_AVR
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h> 
#include <avr/sleep.h>
#include <avr/eeprom.h>

#include "onewire.h"
#else
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
#endif

#include <string.h>
#include <stdio.h>
#include <math.h>

typedef union {
  uint16_t uint;
  struct {
    uint8_t lsb;
    uint8_t msb;
  };
} IntUnion_t;

typedef union {
 uint32_t uint_long;
 struct {
	IntUnion_t uint1;
	IntUnion_t uint2;
 };
} Int32Union_t;

#define ADRESA_EE_CENA_PIVA 0
#define ADRESA_EE_IMPULZY_NA_LITR_LSB 1
#define ADRESA_EE_IMPULZY_NA_LITR_MSB 2
#define ADRESA_EE_CIPY_START 3

#define STAV_OFF    0
#define STAV_NORMAL 1
#define STAV_SPRAVA 2

#define PIN_STAV_NORMAL 5
#define PIN_STAV_SPRAVA 6
#define PORT_STAV PORTB

#define PIN_SOLENOID 5
#define PORT_SOLENOID PORTD
#define SOLENOID_OFF() PORT_SOLENOID &= ~(1 << PIN_SOLENOID)
#define SOLENOID_ON() PORT_SOLENOID |= (1 << PIN_SOLENOID)

#define TIMER_1SEC 100

const char SCREEN_ZAKLADNI[]           = " *STOPARI NYMBURK*\n      *VYCEP*";
const char SCREEN_ZAKLADNI_SPRAVA[]    = " *STOPARI NYMBURK*\n     *SPRAVA*";
const char SCREEN_VYCEP_ZAKAZNIK_L1[]  = "VYCEP ZAKAZNIK %02d\n";
const char SCREEN_VYCEP_ZAKAZNIK_L2[]  = "s[l]:%2.2f nyni:%1.2f";
const char SCREEN_SPRAVA_ZAKAZNIK[]    = "SPRAVA ZAKAZNIK %02d\ns[l]:%2.2f cena:%d,-";
const char SCREEN_CIP_NEZNAMY[]        = " !! NEZNAMY CIP !!";
const char SCREEN_INICIALIZACE[]       = "   INICIALIZACE..";

#define ADDR_LEN 8
const uint8_t ADRESY_CIPU[][ADDR_LEN] = { {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8}, {0x1,0x2,0xA,0xB,0xC,0xD,0xE,0xF},
{0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7}, {0x9,0x8,0x7,0x6,0x5,0x4,0x3,0x2}, {0x5,0x5,0x5,0x5,0x6,0x6,0x6,0x6}};
#define POCET_CIPU sizeof(ADRESY_CIPU) / sizeof(*ADRESY_CIPU)
volatile uint8_t  KONTROLNI_SOUCTY[POCET_CIPU]; //kontrolni soucty adres pro rychlejsi vyhledavani

volatile uint16_t VYTOCENE_IMPULZY[POCET_CIPU]; //aktualne vytocene impulzy od posledni zmeny ceny piva (nebo od zacatku)
//sem se presouvaji dosavadni impulzy a spocita se dosavadni cena za ty impulzy, pokud se zmeni cena piva
volatile uint16_t AKUMULOVANE_IMPULZY[POCET_CIPU];
volatile uint16_t AKUMULOVANA_CENA[POCET_CIPU]; //cena je v halirich!!

volatile uint16_t CELKOVE_IMPULZY;
volatile uint8_t  CENA_PIVA;
volatile uint16_t IMPULZY_NA_LITR;
volatile float    CENA_ZA_IMPULZ;

volatile bool     je_prihlaseno = false;
volatile uint8_t  prihlaseny_cip_id;
volatile uint8_t  prihlaseny_cip_adresa[ADDR_LEN];
volatile uint16_t prihlaseny_cip_impulzy;

volatile uint16_t longTimer;
volatile uint8_t aktualni_stav;

#define DISP_SIZE 43 //2x20 + jden znak na kazdej radek + 1 na zalomeni
volatile char displej_text[DISP_SIZE];
volatile uint8_t tlacitka_minule;

//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

#ifdef COMPILE_AVR
void SetRegisters(void)
{
  //configure uart0  (57600, 8bits, no parity, 1 stop bit)
  UBRR0H = 0;
  UBRR0L = 16;
  UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
  UCSR0B = _BV(RXEN0) | _BV(TXEN0);
  
  //timer0 10ms period, interrupt enable
  //prescaler 1024, count to 156
  OCR0A = 156;
  OCR0B = 170;
  TCCR0A = 2;
  TCCR0B = 5;
  TIMSK0 = 2;

  //disable unused peripherials
  //PRR = ( _BV(PRTWI) | _BV(PRTIM1) | _BV(PRTIM2) ) ;
}

//======================================================
void USART_Transmit( char *data, uint8_t len )
{
  for (uint8_t i=0; i < len; i++)
  {
    /* Wait for empty transmit buffer */
    while ( !( UCSR0A & (1<<UDRE0)) );
    /* Put data into buffer, sends the data */
    UDR0 = data[i];
  }
}

//======================================================
ISR(TIMER0_COMPA_vect)
{
  	longTimer++;
}

ISR(BADISR_vect) { //just for case
  __asm__("nop\n\t");
}

#endif

//======================================================
void VynulujCip(uint8_t id)
{
	VYTOCENE_IMPULZY[id] = 0;
	AKUMULOVANE_IMPULZY[id] = 0;
	AKUMULOVANA_CENA[id] = 0;
}

//======================================================
void ResetujVsechnyCipy(void)
{
	for (uint8_t cip=0; cip < POCET_CIPU; cip++)
	{
		VynulujCip(cip);
	}

	CELKOVE_IMPULZY = 0;
}	

//======================================================
void ZmenCenu(uint16_t nova_cena)
{
	//projdeme vsechny cipy
	//prepocitame vytocene impulzy na cenu
	//tu ulozime do akumulovane ceny
	//pak preneseme vytocene impulzy do akumulovanych
	//vynulujeme vytocene impulzy

	for (uint8_t cip=0; cip < POCET_CIPU; cip++)
	{
		uint16_t cena = VYTOCENE_IMPULZY[cip] * CENA_ZA_IMPULZ * 100; //je to na halire
		AKUMULOVANA_CENA[cip] += cena;
		AKUMULOVANE_IMPULZY[cip] += VYTOCENE_IMPULZY[cip];
		VYTOCENE_IMPULZY[cip] = 0;
	}

	//a ted nastavime novou cenu piva
	CENA_PIVA = nova_cena;
	//nova cena piva na impulz
	CENA_ZA_IMPULZ = float(2 * CENA_PIVA) / IMPULZY_NA_LITR;
}

//======================================================
void SaveData(void)
{
#ifdef COMPILE_AVR
	IntUnion_t volatile *p16;
	Int32Union_t volatile eep_dword;

	//mapovani musi byt stejne jako pro LoadData()
	eeprom_update_byte((uint8_t *)ADRESA_EE_CENA_PIVA, CENA_PIVA);
	p16 = (IntUnion_t*)&IMPULZY_NA_LITR;
	eeprom_update_byte((uint8_t *)ADRESA_EE_IMPULZY_NA_LITR_LSB, (*p16).lsb);
	eeprom_update_byte((uint8_t *)ADRESA_EE_IMPULZY_NA_LITR_MSB, (*p16).msb);

	//cyklujeme pres vsechny cipy
	uint8_t adresa = ADRESA_EE_CIPY_START;
	for (uint8_t cip=0; cip < POCET_CIPU; cip++)
	{
		p16 = (IntUnion_t*)&AKUMULOVANE_IMPULZY[cip];
		eep_dword.uint1.lsb = (*p16).lsb;
		eep_dword.uint1.msb = (*p16).msb;

		p16 = (IntUnion_t*)&AKUMULOVANA_CENA[cip];
		eep_dword.uint2.lsb = (*p16).lsb;
		eep_dword.uint2.msb = (*p16).msb;

		eeprom_update_dword((uint32_t*)adresa, eep_dword.uint_long);

		adresa += 4; //posun se na dalsi pametove misto
	}
#endif
}

void LoadData(void)
{
#ifdef COMPILE_AVR
	IntUnion_t volatile *p16;
	Int32Union_t volatile eep_dword;

	CELKOVE_IMPULZY = 0;

	//mapovani adres v eeporm:
	//0 : cena piva
	//1,2 : impulzy na litr
	//zakaznici (kazdy ma 4 byte):
	//3,4 - impulzy zakaznika 0
	//5,6 - cena zakaznika 0
	//7,8 - impulzy zakaznika 1
	//atd...
	//u vsech 16bit dat je prvni LSB

	//nactem cenu a impulzy na litr
	CENA_PIVA = eeprom_read_byte((uint8_t *)ADRESA_EE_CENA_PIVA);
	p16 = (IntUnion_t*)&IMPULZY_NA_LITR;
	(*p16).lsb = eeprom_read_byte((uint8_t *)ADRESA_EE_IMPULZY_NA_LITR_LSB);
	(*p16).msb = eeprom_read_byte((uint8_t *)ADRESA_EE_IMPULZY_NA_LITR_MSB);
	if (((*p16).lsb == 255) and ((*p16).msb == 255)) IMPULZY_NA_LITR = 300; //jen pro prvni nacteni cerstve eepromky

	//cykluj pres vsechny cipy a nacti jejich ulozena data
	uint8_t adresa = ADRESA_EE_CIPY_START;
	for (uint8_t cip=0; cip < POCET_CIPU; cip++)
	{
		eep_dword.uint_long = eeprom_read_dword((uint32_t*)adresa);
		p16 = (IntUnion_t*)&AKUMULOVANE_IMPULZY[cip];
		(*p16).lsb = eep_dword.uint1.lsb;
		(*p16).msb = eep_dword.uint1.msb;
		if (((*p16).lsb == 255) and ((*p16).msb == 255)) AKUMULOVANE_IMPULZY[cip] = 0;

		p16 = (IntUnion_t*)&AKUMULOVANA_CENA[cip];
		(*p16).lsb = eep_dword.uint2.lsb;
		(*p16).msb = eep_dword.uint2.msb;
		if (((*p16).lsb == 255) and ((*p16).msb == 255)) AKUMULOVANA_CENA[cip] = 0;

		adresa += 4; //posun se na dalsi pametove misto
		
		VYTOCENE_IMPULZY[cip] = 0;
		CELKOVE_IMPULZY += AKUMULOVANE_IMPULZY[cip];
	}
#else

	CELKOVE_IMPULZY = 3000;
	//IMPULZY_NA_LITR = 300;
	IntUnion_t volatile *p16 = (IntUnion_t*)&IMPULZY_NA_LITR;
	(*p16).uint = 300;
	CENA_PIVA = 20;
#endif

	//cena piva na impulz
	CENA_ZA_IMPULZ = float(2 * CENA_PIVA) / IMPULZY_NA_LITR;
}

//======================================================
uint8_t KontrolniSoucet(const uint8_t adresa[ADDR_LEN])
{
	//cykluj pres vsecky znaky adresy cipu
	uint8_t kontrolni_soucet = 0;
	for(uint8_t i=0; i < 8; i++)
	{
		kontrolni_soucet += adresa[i];
	}
	
	return kontrolni_soucet;
}

//======================================================
inline void SpocitatKontrolniSoucty(void)
{
	//cykluj pres vsechny cipy
	for (uint8_t cip=0; cip < POCET_CIPU; cip++)
	{
		KONTROLNI_SOUCTY[cip] = KontrolniSoucet(ADRESY_CIPU[cip]);
	}
}

//======================================================
uint8_t NajdiCip(const uint8_t adresa[ADDR_LEN])
{
	uint8_t kontrolni_soucet = KontrolniSoucet(adresa);
	uint8_t cip;
	bool match;

	//cykluj pres vsechny kontrolni soucty a hledej match, potom porovnej adresu
	for (cip=0; cip < POCET_CIPU; cip++)
	{
		if (KONTROLNI_SOUCTY[cip] == kontrolni_soucet)
		{
			match = true;
			//cykluj pres vsecky znaky adresy cipu
			for(uint8_t i=0; i < 8; i++)
			{
				if (adresa[i] != ADRESY_CIPU[cip][i])
				{
					//pokud nejaky znak z adresy nesedi, tak to rovnou utnem a jdem na dalsi kolo
					match = false;
					break;
				}
			}

			// pokud byl match kompletni, vratime poradi cipu
			if (match == true)
			{
				return(cip);
			}
		}
	}

	//sem se to dostane jen kdyz se projde cele pole a nic se nenajde,
	//takze vratime 255
	return(255);
}

//======================================================
void ZobrazInfoCipSprava(uint8_t id)
{
	float litru = float(AKUMULOVANE_IMPULZY[id] + VYTOCENE_IMPULZY[id]) / IMPULZY_NA_LITR;
	uint16_t cena = (VYTOCENE_IMPULZY[id] * CENA_ZA_IMPULZ * 100) + AKUMULOVANA_CENA[id]; //je to na halire

	//cena total je v korunach a zaokrouhluje se nahoru
	uint16_t cena_total = (cena / 100);
	if ((cena % 100) > 0) cena_total++;

	printf(SCREEN_SPRAVA_ZAKAZNIK, id+1, litru, cena_total);
}

//======================================================
void ZobrazInfoCipVytoc(uint8_t id, bool both)
{
	float litru = float(AKUMULOVANE_IMPULZY[id] + VYTOCENE_IMPULZY[id]) / IMPULZY_NA_LITR;
	float nyni = float(prihlaseny_cip_impulzy) / IMPULZY_NA_LITR;

	if (both == true)
	{
		printf(SCREEN_VYCEP_ZAKAZNIK_L1, id+1);
	}

	printf(SCREEN_VYCEP_ZAKAZNIK_L2, litru, nyni);
}

//======================================================
//zkusi nacist na 1-wire pripojeny cip
//kdyz neco precte, ulozi vycteny data do prihlaseny_cip_adresa a vrati 1
inline uint8_t PrectiCip(void)
{
#ifdef COMPILE_AVR

	uint8_t status = ow_rom_search(OW_SEARCH_FIRST, prihlaseny_cip_adresa);

	if (status == OW_LAST_DEVICE)
	{
		return 1;
	}

#endif
}

//======================================================
//======================================================
//======================================================
int main (void)
{
	sprintf((char *)displej_text, SCREEN_INICIALIZACE);

	LoadData();
	SpocitatKontrolniSoucty();

#ifdef COMPILE_AVR
	SetRegisters();
	sei();
#endif

	//debug data
	uint8_t zak = 0;
	AKUMULOVANE_IMPULZY[zak] = 0;
	AKUMULOVANA_CENA[zak] = 0;
	VYTOCENE_IMPULZY[zak] = 388;
	prihlaseny_cip_impulzy = 132;

	printf("\n|--------|---------|\n");
	ZobrazInfoCipVytoc(zak, true);
	printf("\n|--------|---------|\n");
	ZobrazInfoCipSprava(zak);
	printf("\n|--------|---------|\n\n");

	ZmenCenu(40);

	VYTOCENE_IMPULZY[zak] = 300;
	prihlaseny_cip_impulzy = 150;

	printf("\n|--------|---------|\n");
	ZobrazInfoCipVytoc(zak, true);
	printf("\n|--------|---------|\n");
	ZobrazInfoCipSprava(zak);
	printf("\n|--------|---------|\n\n");

	return(0);
}


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h> 
#include <avr/sleep.h>
#include <avr/eeprom.h>

#include <string.h>
#include <stdio.h>
#include <math.h>

#include "onewire.h"
#include "adresy_cipu.h"
#include "lcd.h"
#include "display.h"

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

#define XSTR(X,Y) X ## Y
#define PORT_BUILD(X) XSTR(PORT,X)
#define DDR_BUILD(X) XSTR(DDR,X)
#define PIN_BUILD(X) XSTR(PIN,X)

#define ADRESA_EE_CENA_PIVA 0
#define ADRESA_EE_IMPULZY_NA_LITR_LSB 1
#define ADRESA_EE_IMPULZY_NA_LITR_MSB 2
#define ADRESA_EE_CIPY_START 3

#define PIN_STAV_NORMAL 2
#define PIN_STAV_SPRAVA 3
#define BRANA_STAV D
#define PIN_REGISTR_STAV PIN_BUILD(BRANA_STAV)
#define STAV_KLICE ((PIN_REGISTR_STAV & ( (1 << PIN_STAV_NORMAL) | (1 << PIN_STAV_SPRAVA) )) )

#define STAV_OFF    0
#define STAV_NORMAL (1 << PIN_STAV_NORMAL)
#define STAV_SPRAVA (1 << PIN_STAV_SPRAVA)

#define BRANA_TLACITKA C
#define PIN_TLACITKO_UP 0
#define PIN_TLACITKO_DN 1
#define PIN_TLACITKO_DEL 2
#define PIN_REGISTR_TLACITKA PIN_BUILD(BRANA_TLACITKA)
#define STAV_TLACITKA ((PIN_REGISTR_TLACITKA & ( (1 << PIN_TLACITKO_UP) | (1 << PIN_TLACITKO_DN) | (1 << PIN_TLACITKO_DEL) )) )

#define BRANA_SOLENOID D
#define PIN_SOLENOID 6

#define BRANA_IMPULZY D //pin T1
#define PIN_IMPULZY 5
#define IMPULZ_COUNTER TCNT1

#define PORT_SOLENOID PORT_BUILD(BRANA_SOLENOID)
#define DDR_SOLENOID DDR_BUILD(BRANA_SOLENOID)

#define SOLENOID_TO_OUTPUT DDR_SOLENOID |= (1 << PIN_SOLENOID)
#define SOLENOID_OFF() PORT_SOLENOID &= ~(1 << PIN_SOLENOID)
#define SOLENOID_ON() PORT_SOLENOID |= (1 << PIN_SOLENOID)

#define TIMER_1SEC 100
#define PRIHLASENI_TIMEOUT 30*TIMER_1SEC
#define CTENI_CIPU_TIMEOUT TIMER_1SEC/2
#define CTENI_TLACITEK_TIMEOUT 1

#define POCET_CIPU sizeof(ADRESY_CIPU) / sizeof(*ADRESY_CIPU)
//#define POCET_CIPU 50
volatile uint8_t  KONTROLNI_SOUCTY[POCET_CIPU]; //kontrolni soucty adres pro rychlejsi vyhledavani

volatile uint16_t AKTUALNI_IMPULZY[POCET_CIPU]; //aktualne vytocene impulzy od posledni zmeny ceny piva (nebo od zacatku)
//sem se presouvaji dosavadni impulzy a spocita se dosavadni cena za ty impulzy, pokud se zmeni cena piva
volatile uint16_t AKUMULOVANE_IMPULZY[POCET_CIPU];
volatile uint16_t AKUMULOVANA_CENA[POCET_CIPU]; //cena je v halirich!!

volatile uint16_t CELKOVE_IMPULZY;
volatile uint8_t  CENA_PIVA;
volatile uint16_t IMPULZY_NA_LITR;
volatile double   CENA_ZA_IMPULZ;

volatile bool     je_prihlaseno = false;
volatile uint8_t  prihlaseny_cip_id;
volatile uint8_t  prihlaseny_cip_adresa[CIP_ADDR_LEN];
volatile uint16_t prihlaseny_cip_impulzy;
volatile uint16_t prihlaseny_cip_timeout;

//volatile uint16_t longTimer;
volatile uint8_t  timerCteniCipu;
volatile uint8_t  bylTimer;

volatile uint8_t  aktualni_stav;
volatile uint8_t  sprava_substav;
volatile uint8_t  tlacitka_minule;
volatile uint8_t  tlacitka_valid;
volatile uint8_t  timerTlacitka;

//promenne spojene s obsluhou UART
#define UART_BUFF_MAX_LEN 4 //musi se do nej vejit prijmout retezec DATA
#define UART_TIMEOUT TIMER_1SEC/50 //20ms
volatile uint8_t uartIncoming;
volatile uint8_t uartPos;
volatile uint8_t uartBuf[UART_BUFF_MAX_LEN];
volatile uint8_t timerUart;

//tady jsou potrebne definice k fungovani a praci s displejem
volatile uint8_t  refresh_display;

#define DISPLAY_REFRESH_TIME TIMER_1SEC*2
extern volatile uint8_t display_posledni_stav;
volatile uint8_t timerDisplay;

void main() __attribute__ ((noreturn));
//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

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

	//timer 1 jako citac impulzu - tzn. zdroj impulzu bude fyzicky pin
	OCR1A = 0;
	OCR1B = 0;
	TIMSK1 = 0;
	TCCR1A = 0; //normal operation, no compare match, no pwm out
	TCCR1B = _BV(ICNC1) | _BV(CS12) | _BV(CS11);
	TCNT1 = 0;

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
ISR(USART_RX_vect)
{
	uint8_t inp = UDR0;

	if (uartIncoming == 0)
	{
		if (inp == 254)
		{
			uartIncoming = 1;
			uartPos = 0;
			timerUart = UART_TIMEOUT;
		}
	}
	else
	{
		if (uartPos == UART_BUFF_MAX_LEN) //incoming packet is longer than allowed
		{
			uartIncoming = 0;
		}
		else
		{
			((char*)uartBuf)[uartPos] = inp;
			uartPos++;
		}
	}
}

//======================================================
ISR(TIMER0_COMPA_vect)
{
	//longTimer++;
	bylTimer = true;
}

ISR(BADISR_vect) { //just for case
	__asm__("nop\n\t");
}


//======================================================
// posle raw data z poli primo na uart
// zadne prepocitavani se nekona
// proste jen vezme akumulovanou cenu, normalni a akumulovane impulzy a posle to v cyklu za kazdy cip
void PosliDataNaUart(void)
{
	uint8_t volatile sendBuff[6];
	IntUnion_t volatile *p16;

	//posleme hlavicku [znak 254, pocet cipu, velikost dat jednoho cipu]
	uint8_t hlavicka[] = {254, POCET_CIPU, sizeof(sendBuff)};
	USART_Transmit((char*)hlavicka, sizeof(hlavicka));

	for (uint8_t cip=0; cip < POCET_CIPU; cip++)
	{
		p16 = (IntUnion_t*)&AKTUALNI_IMPULZY[cip];
		sendBuff[0] = (*p16).lsb;
		sendBuff[1] = (*p16).msb;

		p16 = (IntUnion_t*)&AKUMULOVANE_IMPULZY[cip];
		sendBuff[2] = (*p16).lsb;
		sendBuff[3] = (*p16).msb;

		p16 = (IntUnion_t*)&AKUMULOVANA_CENA[cip];
		sendBuff[4] = (*p16).lsb;
		sendBuff[5] = (*p16).msb;

		USART_Transmit((char *)sendBuff, sizeof(sendBuff));
	}
}


//======================================================
void VynulujCip(uint8_t id)
{
	AKTUALNI_IMPULZY[id] = 0;
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
void AkumulujCenu(uint8_t cip)
{
	uint16_t cena = AKTUALNI_IMPULZY[cip] * CENA_ZA_IMPULZ * 100; //je to na halire
	AKUMULOVANA_CENA[cip] += cena;
	AKUMULOVANE_IMPULZY[cip] += AKTUALNI_IMPULZY[cip];
	AKTUALNI_IMPULZY[cip] = 0;
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
		AkumulujCenu(cip);
	}

	//a ted nastavime novou cenu piva
	CENA_PIVA = nova_cena;
	//nova cena piva na impulz
	CENA_ZA_IMPULZ = double(2 * CENA_PIVA) / IMPULZY_NA_LITR;
}

//======================================================
void SaveData(void)
{
	IntUnion_t volatile *p16;
	Int32Union_t volatile eep_dword;

	//mapovani musi byt stejne jako pro LoadData()
	eeprom_update_byte((uint8_t *)ADRESA_EE_CENA_PIVA, CENA_PIVA);
	p16 = (IntUnion_t*)&IMPULZY_NA_LITR;
	eeprom_update_byte((uint8_t *)ADRESA_EE_IMPULZY_NA_LITR_LSB, (*p16).lsb);
	eeprom_update_byte((uint8_t *)ADRESA_EE_IMPULZY_NA_LITR_MSB, (*p16).msb);

	//cyklujeme pres vsechny cipy
	uint32_t *adresa;
	adresa = (uint32_t*)ADRESA_EE_CIPY_START;

	for (uint8_t cip=0; cip < POCET_CIPU; cip++)
	{
		//nejdriv musime presunout vsechno do akumulovanych impulzu
		//a taky spocitat cenu a tu taky pricist do akumulovane
		//a pak teprve muzeme ukladat do eeprom
		AkumulujCenu(cip);

		p16 = (IntUnion_t*)&AKUMULOVANE_IMPULZY[cip];
		eep_dword.uint1.lsb = (*p16).lsb;
		eep_dword.uint1.msb = (*p16).msb;

		p16 = (IntUnion_t*)&AKUMULOVANA_CENA[cip];
		eep_dword.uint2.lsb = (*p16).lsb;
		eep_dword.uint2.msb = (*p16).msb;

		eeprom_update_dword((uint32_t*)adresa, eep_dword.uint_long);

		adresa += 4; //posun se na dalsi pametove misto
	}
}

//======================================================
// nacte konfiguraci a ulozena data z EEPROM
// zaprve cenu piva a prutokomerovy pocet impulzu na 1 litr
// zadruhe zaloha celkove spotreby jednotlivych cipu
void LoadData(void)
{
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
	uint32_t *adresa;
	adresa = (uint32_t*)ADRESA_EE_CIPY_START;

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
		
		AKTUALNI_IMPULZY[cip] = 0;
		CELKOVE_IMPULZY += AKUMULOVANE_IMPULZY[cip];
	}

	//cena piva na impulz
	CENA_ZA_IMPULZ = double(2 * CENA_PIVA) / IMPULZY_NA_LITR;
}

//======================================================
uint8_t KontrolniSoucet(const uint8_t adresa[CIP_ADDR_LEN])
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
uint8_t NajdiCip(const uint8_t adresa[CIP_ADDR_LEN])
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
void OdhlasCip(void)
{
	je_prihlaseno = false;
	AKTUALNI_IMPULZY[prihlaseny_cip_id] += IMPULZ_COUNTER;
	prihlaseny_cip_id = 255;
	prihlaseny_cip_timeout = 0;
	prihlaseny_cip_impulzy = 0;
	//sprintf((char *)displej_text, SCREEN_ZAKLADNI);
	DisplayFrontaAdd(DISP_STAV_ZAKLADNI);
	refresh_display = true;
	SOLENOID_OFF();
}

//======================================================
//zkusi nacist na 1-wire pripojeny cip
//kdyz neco precte, ulozi vycteny data do prihlaseny_cip_adresa
//pak zkusi cip najit a pripadne prihlasit
void PrectiCip(void)
{

	if (ow_rom_search(OW_SEARCH_FIRST, (uint8_t *)prihlaseny_cip_adresa) == OW_LAST_DEVICE)
	{
		//cip byl detekovan na 1-wire a jeho data nactena, zkusime ho najit v databazi
		uint8_t nalezeny_cip = NajdiCip((uint8_t *)prihlaseny_cip_adresa);
		if (nalezeny_cip < 255)
		{
			//cip mame v databazi, takze ho prihlasime (a pripadne odhlasime predchozi, nebo odhlasime i ten stejny)
			if ((je_prihlaseno == true) && (prihlaseny_cip_id == nalezeny_cip))
			{
				//cip je stejny - takze ho jen odhlasime
				//TODO - mozna bude lepci misto odhlasovani zkratit logout timeout na 0
				OdhlasCip();
			}
			else
			{
				//prihlasime novy (to odhlasi i stary)
				je_prihlaseno = true;
				prihlaseny_cip_id = nalezeny_cip;
				prihlaseny_cip_timeout = PRIHLASENI_TIMEOUT;
				prihlaseny_cip_impulzy = 0;
				SOLENOID_ON();
				IMPULZ_COUNTER = 0;
				ZobrazInfoCipVytoc(prihlaseny_cip_id, true);
			}
		}
		// Tento cip nemame v databazi, takze vypiseme hlasku
		else
		{
			DisplayFrontaPush(DISP_STAV_NEZNAMY_CIP);
			//sprintf((char *)displej_text, SCREEN_CIP_NEZNAMY);
		}
		
		refresh_display = true;
	}

}

//======================================================
//======================================================
//======================================================
void main (void)
{
	DisplayFrontaAdd(DISP_STAV_INICIALIZACE);
	display_posledni_stav = DISP_STAV_INICIALIZACE;
	//sprintf((char *)displej_text, SCREEN_INICIALIZACE);

	LoadData();
	SpocitatKontrolniSoucty();
	SetRegisters();
	sei();

	aktualni_stav = STAV_OFF;
	//sprintf((char *)displej_text, SCREEN_ZAKLADNI);
	DisplayFrontaAdd(DISP_STAV_ZAKLADNI);
	refresh_display = true;

#ifdef DEBUG_ON_PC
	//debug data
	uint8_t zak = 0;
	AKUMULOVANE_IMPULZY[zak] = 0;
	AKUMULOVANA_CENA[zak] = 0;
	AKTUALNI_IMPULZY[zak] = 388;
	prihlaseny_cip_impulzy = 132;

	printf("\n|--------|---------|\n");
	ZobrazInfoCipVytoc(zak, true);
	printf("\n|--------|---------|\n");
	ZobrazInfoCipSprava(zak);
	printf("\n|--------|---------|\n\n");

	ZmenCenu(40);

	AKTUALNI_IMPULZY[zak] = 300;
	prihlaseny_cip_impulzy = 150;

	printf("\n|--------|---------|\n");
	ZobrazInfoCipVytoc(zak, true);
	printf("\n|--------|---------|\n");
	ZobrazInfoCipSprava(zak);
	printf("\n|--------|---------|\n\n");
	SOLENOID_OFF();
#endif

	while(1) {

		//obsluha vsech timeru az tady misto aby se to delalo v preruseni
		//melo by to byt hned na zacatku cyklu, aby se podle toho pak zbytek podminek mohl zaridit
		if (bylTimer)
		{
			bylTimer = false;
			if (prihlaseny_cip_timeout > 0) prihlaseny_cip_timeout--;
			if (timerCteniCipu > 0) timerCteniCipu--;
			if (timerDisplay > 0) timerDisplay--;
			if (timerUart > 0) timerUart--;
			if (timerTlacitka > 0) timerTlacitka--;
		}

		//kontrola aktualniho stavu polohy klice vytoc/sprava
		if (STAV_KLICE != aktualni_stav)
		{
			if (STAV_KLICE == STAV_NORMAL)
			{
			}
			else if (STAV_KLICE == STAV_SPRAVA)
			{

			}
			else
			{
				//pokud by se to dostalo sem, znamena to ze klic aktivoval obe polohy zaroven, coz je blbost
				//takze tezko jestli tuhle variantu vubec nejak resit a presouvat kvuli tomu
				//to co je tu pod zavorkou nahoru do samostatnyho if a elseif
			}

			aktualni_stav = STAV_KLICE;
			refresh_display = true;
		}

		//kontrola stavu stisku tlacitek
		if (timerTlacitka == 0)
		{
			timerTlacitka = CTENI_TLACITEK_TIMEOUT;
			uint8_t aktualni_stav_tlacitek = STAV_TLACITKA;

			if (aktualni_stav_tlacitek != tlacitka_minule)
			{
				//rozlisime jestli je to zmena z nic stisknuto -> neco stisknuto
				//nebo neco -> nic, tzn. pusteni tlacitka a tim padem nejaka akce
				if (tlacitka_minule == 0) //nic -> neco
				{
					tlacitka_valid = 0;
				}
				else if (aktualni_stav_tlacitek == 0) //neco -> nic
				{
					tlacitka_valid = tlacitka_minule;
				}
			}

			tlacitka_minule = aktualni_stav_tlacitek;
		}

		//kontrola a pripadne odhlaseni timeoutovaneho cipu
		if (je_prihlaseno)
		{
			if (prihlaseny_cip_timeout == 0) OdhlasCip();
		}

		//uz je cas zkusit jestli je prilozen cip?
		if (timerCteniCipu == 0)
		{
			timerCteniCipu = CTENI_CIPU_TIMEOUT;
			PrectiCip();
		}

		if (uartIncoming == 1)
		{
			if (timerUart == 0) uartIncoming = 0; //timeout elapsed

			if (uartPos == UART_BUFF_MAX_LEN)
			{
				//mame prijaty cely buffer, tak zkontrolujem jestli je to spravny pozadavek (string DATA)
				if ( (uartBuf[0] == 'D') && (uartBuf[1] == 'A') && (uartBuf[2] == 'T') && (uartBuf[3] == 'A') )
				{
					//v bufferu je opravdu pozadavek od pocitace - posleme data
					PosliDataNaUart();
				}
				uartIncoming = 0;
			}
		}

		//je potreba prekreslit display?
		//tohle by melo byt az na konci cyklu
		if ( (timerDisplay == 0) || (refresh_display) )
		{	
			timerDisplay = DISPLAY_REFRESH_TIME;

			uint8_t novy_stav = DisplayFrontaPop();
			//pokud je v poli neco jineho nez prazdno, tak je to jasny
			//a kdyz tam prazdno je - tak je jasny, ze tam dame screen podle aktualniho stavu
			if (novy_stav == DISP_STAV_NIC)
			{
				if (aktualni_stav == STAV_NORMAL)
				{
					if (je_prihlaseno) novy_stav = DISP_STAV_VYCEP_ZAKAZNIK_FULL; else novy_stav = DISP_STAV_ZAKLADNI;
				}
				else if (aktualni_stav == STAV_SPRAVA)
				{
					// TODO: jeste nejak poresit jak delit zobrazeni cele spravy (menu, zakaznici apod)
					// mozna vymyslet nejakej sub-stav a podle toho si to bude rozhodovat primo az kreslici funkce
					novy_stav = DISP_STAV_ZAKLADNI_SPRAVA;
				}
			}
			PrekreslitDisplay(novy_stav);
			refresh_display = false;
		}
	}

	SaveData();
}


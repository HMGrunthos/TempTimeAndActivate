/*
 * TempLookup.h
 *
 * Created: 19/10/2019 11:31:03
 *  Author: Sam
 */ 

#ifndef TEMPLOOKUP_H_
	#define TEMPLOOKUP_H_
	
	#include <avr/pgmspace.h>
	
	// #define PRINTTEMP

	#define TEMPLU_XMIN 244
	#define TEMPLU_XMAX 596
	#define TEMPLU_DXINXSCALE 8
	#define TEMPLU_DXINXSCALE_ASPOWER 3
	#define TEMPLU_YSCALEPOWER 10
	#define TEMPLU_GRADSCALEPWR 5
	#define TEMPLU_TABLESIZE 45
	static const uint16_t tempLu_ADCTempLUT[TEMPLU_TABLESIZE] PROGMEM = {50451, 49294, 48169, 47074, 46007, 44967, 43950, 42957, 41984, 41032, 40099, 39183, 38283, 37399, 36530, 35675, 34832, 34002, 33183, 32375, 31576, 30788, 30008, 29237, 28473, 27717, 26968, 26225, 25488, 24757, 24030, 23309, 22591, 21878, 21168, 20461, 19757, 19055, 18355, 17658, 16961, 16265, 15571, 14876, 14181};

	#define TL_TOFIXEDPOINT(inDegC) ((uint16_t)(inDegC*(float)(1<<TEMPLU_YSCALEPOWER) + 0.5))

	#ifdef PRINTTEMP
		static const char tempLu_FracChar[4][3] PROGMEM = {"00", "25", "50", "75"};
	#endif
	
	static uint16_t __attribute__ ((noinline)) getTemperature(uint16_t adcVal)
	// static uint16_t getTemperature(uint16_t adcVal)
	{
		#ifdef PRINTTEMP
			char buff[10];
			utoa(adcVal, buff, 10);
			uartPuts(buff);
		#endif

		if(adcVal < TEMPLU_XMIN) {
			adcVal = TEMPLU_XMIN;
		} else if(adcVal >= TEMPLU_XMAX) {
			adcVal = TEMPLU_XMAX - 1;
		}
		
		adcVal -= TEMPLU_XMIN;
		uint8_t cIdx = 0;
		while(adcVal >= TEMPLU_DXINXSCALE) {
			cIdx++;
			adcVal -= TEMPLU_DXINXSCALE;
		}
		
		uint16_t yLow = pgm_read_word(tempLu_ADCTempLUT + cIdx);
		uint16_t yHigh = pgm_read_word(tempLu_ADCTempLUT + cIdx + 1);
		uint16_t grad = (yLow - yHigh) << (TEMPLU_GRADSCALEPWR - TEMPLU_DXINXSCALE_ASPOWER); // Gradient
		uint16_t offSet = ((grad*adcVal) + (1<<(TEMPLU_YSCALEPOWER - TEMPLU_GRADSCALEPWR - 1))) >> (TEMPLU_YSCALEPOWER - TEMPLU_GRADSCALEPWR); // Offset from yLow and rounded
		uint16_t temp = yLow - offSet; // Temperature

		#ifdef PRINTTEMP
			uint16_t tempRndF = temp + (1<<(TEMPLU_YSCALEPOWER - 3)); // Temperature rounded for display

			uartPuts_P(PSTR(", "));
			utoa(temp, buff, 10);
			uartPuts(buff);
			uartPuts_P(PSTR(", "));
			utoa(tempRndF >> TEMPLU_YSCALEPOWER, buff, 10);
			uartPuts(buff);
			uartPutc('.');
			uartPuts_P(tempLu_FracChar[(tempRndF >> (TEMPLU_YSCALEPOWER-2)) & 0x3]);
			uartPuts_P(PSTR("\n"));
		#endif

		return temp;
	}
#endif /* TEMPLOOKUP_H_ */
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
	#define TEMPLU_XMAX 724
	#define TEMPLU_DXINXSCALE 32
	#define TEMPLU_DXINXSCALE_ASPOWER 5
	#define TEMPLU_YSCALEPOWER 10
	#define TEMPLU_GRADSCALEPWR 3
	#define TEMPLU_TABLESIZE 16
	static const uint16_t tempLu_ADCTempLUT[TEMPLU_TABLESIZE] PROGMEM = {50451, 46007, 41984, 38283, 34832, 31576, 28473, 25488, 22591, 19757, 16961, 14181, 11395, 8579, 5705, 2743};

	#define TL_TOFIXEDPOINT(inDegC) ((uint16_t)(inDegC*(float)(1<<TEMPLU_YSCALEPOWER) + 0.5))

	#ifdef PRINTTEMP
		static const char tempLu_FracChar[4][3] PROGMEM = {"00", "25", "50", "75"};
	#endif
	
	// static const uint16_t __attribute__ ((noinline)) getTemperature(uint16_t adcVal)
	static inline const uint16_t __attribute__((always_inline)) getTemperature(uint16_t adcVal)
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
			adcVal -= TEMPLU_DXINXSCALE;
			cIdx++;
		}
		
		uint16_t yLow = pgm_read_word(tempLu_ADCTempLUT + cIdx);
		uint16_t yHigh = pgm_read_word(tempLu_ADCTempLUT + cIdx + 1);
		uint16_t grad = (yLow - yHigh + (1<<(TEMPLU_DXINXSCALE_ASPOWER - TEMPLU_GRADSCALEPWR - 1))) >> (TEMPLU_DXINXSCALE_ASPOWER - TEMPLU_GRADSCALEPWR); // Gradient
		uint16_t offSet = ((grad*adcVal) + (1<<(TEMPLU_GRADSCALEPWR - 1))) >> TEMPLU_GRADSCALEPWR; // Offset from yLow and rounded
		uint16_t temp = yLow - offSet; // Temperature

		#ifdef PRINTTEMP
			uint16_t tempRndF = temp + (1<<(TEMPLU_YSCALEPOWER - 3)); // Temperature rounded for display

			uartPuts_P(PSTR(", "));
			utoa(cIdx, buff, 10);
			uartPuts(buff);
			uartPuts_P(PSTR(", "));
			utoa(adcVal, buff, 10);
			uartPuts(buff);
			uartPuts_P(PSTR(", "));
			utoa(grad, buff, 10);
			uartPuts(buff);
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

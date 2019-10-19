/*
 * TempLookup.h
 *
 * Created: 19/10/2019 11:31:03
 *  Author: Sam
 */ 

#ifndef TEMPLOOKUP_H_
	#define TEMPLOOKUP_H_
	
	// #define PRINTTEMP

	static const uint16_t adcTempLUT[] PROGMEM = {49010, 47481, 46008, 44583, 43203, 41864, 40563, 39296, 38061, 36854, 35675, 34519, 33386, 32274, 31181, 30105, 29045, 28000, 26968, 25948, 24939, 23940, 22949, 21967, 20991, 20020, 19055, 18093, 17135, 16179, 15223, 14268};
	#define TEMPLU_XMIN 254
	#define TEMPLU_XMAX 595
	#define TEMPLU_DXINXSCALE 11
	#define TEMPLU_YSCALEPOWER 10
	#define TEMPLU_GRADSCALEPWR 5

	#ifdef PRINTTEMP
		static const char fracChar[4][3] PROGMEM = {"00", "25", "50", "75"};
	#endif
	
	static uint16_t getTemperature(uint16_t adcVal)
	{
		adcVal -= TEMPLU_XMIN;
		uint8_t cIdx = 0;
		while(adcVal >= TEMPLU_DXINXSCALE) {
			cIdx++;
			adcVal -= TEMPLU_DXINXSCALE;
		}
	
		uint16_t yLow = pgm_read_word(&adcTempLUT[cIdx]);
		uint16_t yHigh = pgm_read_word(&adcTempLUT[cIdx+1]);
		uint16_t grad = ((yLow - yHigh) << TEMPLU_GRADSCALEPWR)/TEMPLU_DXINXSCALE; // Gradient
		uint16_t offSet = ((grad*adcVal) + (1<<(TEMPLU_YSCALEPOWER - TEMPLU_GRADSCALEPWR - 1))) >> (TEMPLU_YSCALEPOWER - TEMPLU_GRADSCALEPWR); // Offset from yLow and rounded
		uint16_t temp = yLow - offSet; // Temperature
	
		#ifdef PRINTTEMP
			uint16_t tempRndF = temp + (1<<(TEMPLU_YSCALEPOWER - 3)); // Temperature rounded for display

			char buff[10];
			uartPuts("Temp:");
			utoa(tempRndF >> TEMPLU_YSCALEPOWER, buff, 10);
			uartPuts(buff);
			uartPutc('.');
			uartPuts_P(fracChar[(tempRndF >> (TEMPLU_YSCALEPOWER-2)) & 0x3]);
			uartPuts(":\n");
		#endif

		return temp;
	}
#endif /* TEMPLOOKUP_H_ */
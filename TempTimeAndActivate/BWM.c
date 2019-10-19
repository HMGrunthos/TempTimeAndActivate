#include <stdlib.h>

#include <avr/io.h>
#include <avr/pgmspace.h>

#include "Serial.h"
#include "TempLookup.h"

static void initIO();
static int16_t adcRead(uint_fast8_t channel);

int main(void)
{
	initIO();

	//while (1) {
	for(uint16_t adcVal = 250; adcVal <= 600; adcVal++) {
		//uint16_t adcVal = adcRead();

		uint16_t temp = getTemperature(adcVal);

		char buff[10];
		utoa(temp, buff, 10);
		uartPuts(buff);
		uartPutc('\n');

		//_delay_ms(1000);
		//PINB |= (1 << PINB0);
	}
	for(;;);
}

static void initIO()
{
	// ztx450 is on pin 5/PB0
	PORTB &= ~(1 << PORTB0);
	DDRB |= (1 << DDB0); // Set it as an output

	// ADC clock is CPUClock/8 or 150kHz at 1.2MHz
	ADCSRA |= (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN);
}

static int16_t adcRead(uint_fast8_t channel)
{
	switch(channel) {
		case 0:
			ADMUX = (1 << MUX1) | (0 << MUX0); // ADC2 (lower port)
			break;
		case 1:
			ADMUX = (1 << MUX1) | (1 << MUX0); // ADC3 (upper port)
			break;
	}
	
	ADCSRA |= (1 << ADSC); // Start the conversion
	while(ADCSRA & (1 << ADSC)); // Wait for conversion to complete

	int16_t adcResult = ADC; // 10-bit resolution

	return adcResult;
}
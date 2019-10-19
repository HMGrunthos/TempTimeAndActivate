#include <stdlib.h>

#include <avr/io.h>
#include <avr/pgmspace.h>

#include "Serial.h"
#include "TempLookup.h"

static void adcSetup();
static int16_t adcRead(void);

int main(void)
{
	PORTB &= ~(1 << PORTB0);
	DDRB |= (1 << DDB0);

	adcSetup();

	//while (1) {
	for(uint16_t adcVal = 254; adcVal <= 595; adcVal++) {
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

void adcSetup()
{
	// Set the ADC input to PB2/ADC1, left adjust result
	ADMUX = (1 << MUX1) | (0 << MUX0);// | (1 << ADLAR); // ADC2 (lower port)
	// ADMUX = (1 << MUX1) | (0 << MUX0) | (1 << ADLAR); // ADC3 (upper port)
	
	ADCSRA |= (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN);

	// Start the first conversion
	ADCSRA |= (1 << ADSC);
}

int16_t adcRead(void)
{
	int16_t adcResult;
	adcResult = ADC; // For 10-bit resolution

	// Start the next conversion
	ADCSRA |= (1 << ADSC);

	return adcResult;
}
#include <stdlib.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "Serial.h"
#include "TempLookup.h"

#define ONTIME 50 // In seconds
#define TARGETTEMP 30.6 // Target temperature
#define TEMPBAND 1.5 // Allowed slop
#define NCHAN 2 // Number of temperature channels to monitor

#define TICKPERIOD 0.2184533333
#define TOTICKS(period) ((uint_fast16_t)(period)/(float)TICKPERIOD + 0.5))
	
static inline void init();
static inline void initFilters(uint16_t *adcFilters);
static inline void updateFilters(uint16_t *adcFilters);
static inline uint_fast8_t tempInBand(uint16_t *adcFilters);
static inline void setOutput(uint_fast8_t state);
static int16_t adcRead(uint_fast8_t channel);
static uint_fast8_t timeExpired(uint_fast16_t);
static inline void resetTimer();

uint_fast16_t timerTick;

enum MachineStates {
	Inhibit,
	Active,
	Sensing
};

int main(void)
{
	uint16_t adcFilters[NCHAN];

	init();
	initFilters(adcFilters);

	enum MachineStates state = Sensing;
	while (1) {
		switch(state) {
			case Inhibit:
				state = Inhibit; // Stay in Inhibit
				break;
			case Active:
				if(timeExpired(TOTICKS(ONTIME)) {
					state = Inhibit;
				} else {
					state = Active;
				}
				break;
			case Sensing:
				if(timeExpired(TOTICKS(10)) {
					
					state = Active;
					resetTimer();
				}
			/*
				if(tempInBand(adcFilters)) {
					state = Active;
					resetTimer();
				} else {
					state = Sensing;
				}*/
				break;
		}
		
		switch(state) {
			case Inhibit:
				setOutput(0);
				set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Now go to sleep
				cli();
				sleep_enable();
				sleep_cpu();
				break;
			case Sensing:
				setOutput(0);
				updateFilters(adcFilters);
				break;
			case Active:
				setOutput(1);
				break;
		}
	}
}

static inline void initFilters(uint16_t *adcFilters)
{
	for(uint_fast8_t cIdx = 0; cIdx < NCHAN; cIdx++) {
		adcFilters[cIdx] = adcRead(cIdx) << 6;
	}
}

static inline void updateFilters(uint16_t *adcFilters)
{
	// Filter the ADC readings on both channels
	for(uint_fast8_t cIdx = 0; cIdx < NCHAN; cIdx++) {
		// filter = filter*(63/64) + filter*(1/64)
		adcFilters[cIdx] -= (adcFilters[cIdx] + (1<<5)) >> 6; // Remove one 64th from the accumulator
		adcFilters[cIdx] += adcRead(cIdx); // Add one 64th of new measurement
	}
}

static inline uint_fast8_t tempInBand(uint16_t *adcFilters)
{
	uint16_t temps[NCHAN];
	
	uint_fast8_t inBand = 1;
	
	for(uint_fast8_t cIdx = 0; cIdx < NCHAN; cIdx++) {
		temps[cIdx] = getTemperature((adcFilters[cIdx] + (1<<5)) >> 6);
		
		const uint16_t lowerThresh = TL_TOFIXEDPOINT(TARGETTEMP) - (TL_TOFIXEDPOINT(TEMPBAND)>>1);
		const uint16_t upperThresh = TL_TOFIXEDPOINT(TARGETTEMP) + (TL_TOFIXEDPOINT(TEMPBAND)>>1);
		
		if(temps[cIdx] < lowerThresh) {
			inBand = 0;
		} else if(temps[cIdx] > upperThresh) {
			inBand = 0;
		}
	}
	
	return inBand;
}

static int16_t adcRead(uint_fast8_t channel)
{
	if(channel == 0) {
		ADMUX = (1 << MUX1) | (0 << MUX0); // ADC2 (lower port)
	} else {
		ADMUX = (1 << MUX1) | (1 << MUX0); // ADC3 (upper port)
	}
	
	ADCSRA |= (1 << ADSC); // Start the conversion
	while(ADCSRA & (1 << ADSC)); // Wait for conversion to complete

	int16_t adcResult = ADC; // 10-bit resolution

	return adcResult;
}

static inline void setOutput(uint_fast8_t state)
{
	if(state == 0) {
		PORTB &= ~(1 << PORTB0);
	} else {
		PORTB |= (1 << PORTB0);
	}
}

static inline void init()
{
	// ztx450 is on pin 5/PB0
	PORTB &= ~(1 << PORTB0);
	DDRB |= (1 << DDB0); // Set it as an output

	// ADC clock is CPUClock/8 or 150kHz at 1.2MHz
	ADCSRA |= (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN);

	TCCR0B = (1<<CS02) | (1<<CS00); // Divide input clock by 1024
	TIMSK0 = (1<<TOIE0); // Enable interrupts, they tick at 4.577636719Hz
	sei();
	
	adcRead(0);
	setOutput(0);
}

static inline void resetTimer()
{
	cli();
		timerTick = 0;
	sei();
}
					
static uint_fast8_t timeExpired(uint_fast16_t tLimit)
{
	uint_fast16_t tVal;

	cli();
		tVal = timerTick;
	sei();

	return tVal > tLimit;
}

ISR(TIM0_OVF_vect)
{
	timerTick++;
}
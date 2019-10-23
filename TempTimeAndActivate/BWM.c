#include <stdlib.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "Serial.h"
#include "TempLookup.h"
#include "RFSwitch.h"
#include "Random.h"

// #define DISABLE_TIMING

#define ONTIME 50 // In seconds
#define TARGETTEMP 41 // Target temperature
#define TEMPBAND 2 // Allowed slop
#define NCHAN 2 // Number of temperature channels to monitor

#define WAITTIME 7200 // Two hours
#define WARMTIME 1800 // Thirty minutes

#define RFADDRESS 0
#define RFCHANNEL 0

// #define TICKPERIOD 0.2184533333
#define TICKPERIOD 0.25
#define TOTICKS(period) ((uint_fast16_t)(period)/(float)TICKPERIOD + 0.5)
	
static void initHW(void);
static void initFilters(uint16_t *adcFilters);
static void updateFilters(uint16_t *adcFilters);
static uint_fast8_t tempInBand(const uint16_t *adcFilters);
static void setOutput(const uint_fast8_t state);
static int16_t adcRead(const uint_fast8_t channel);
static uint_fast8_t __attribute__ ((noinline)) timeExpired(const uint_fast16_t);
static void resetTimer(void);

volatile static uint_fast16_t timerTick;

#define FIX_POINTER(_ptr) __asm__ __volatile__("" : "=b" (_ptr) : "0" (_ptr))

enum TempMachineStates {
	Inhibit,
	Active,
	Sensing
};

enum TimerMachineStates {
	Expired,
	Warm,
	Delay,
	Wait
};

int main(void)
{
	uint16_t adcFilters[NCHAN];
	
	random16InitFromEEPROM(); // This is before the HW init because I don't want interrupts while I'm accessing the EEPROM

	initHW();

	#ifndef DISABLE_TIMING
		uint16_t randomDelay = getLastRandomNumber(); // 0 to 4.5 hours
		randomDelay -= randomDelay >> 2; // 0 to 3 hours 26 minutes
		randomDelay = randomDelay >> 1; // 0 to 2 hours 16 minutes
		// randomDelay = TOTICKS(10); // Ten seconds
	#endif // DISABLE_TIMING

	resetTimer();
	initFilters(adcFilters);
	enum TempMachineStates tempState = Sensing;
	enum TimerMachineStates timeState = Wait;
	while (1) {
		switch(tempState) {
			case Inhibit:
				setOutput(0);
				set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Now go to sleep
				cli();
				sleep_enable();
				sleep_cpu();
				break;
			case Active:
				setOutput(1);
				if(timeExpired(TOTICKS(ONTIME))) {
					tempState = Inhibit;
				}
				break;
			case Sensing:
				setOutput(0);
				updateFilters(adcFilters);
				if(tempInBand(adcFilters)) {
					#ifndef DISABLE_TIMING
						send(getCodeWord(RFADDRESS, RFCHANNEL, 0));
					#endif // DISABLE_TIMING
					tempState = Active;
					timeState = Expired;
					resetTimer();
				}
				break;
		}
		
		#ifndef DISABLE_TIMING
			switch(timeState) {
				case Expired:
					break;
				case Warm:
					if(timeExpired(TOTICKS(WARMTIME))) {
						send(getCodeWord(RFADDRESS, RFCHANNEL, 0));
						timeState = Expired;
					}
					break;
				case Delay:
					if(timeExpired(randomDelay)) {
						send(getCodeWord(RFADDRESS, RFCHANNEL, 1));
						timeState = Warm;
						resetTimer();
					}
					break;
				case Wait:
					if(timeExpired(TOTICKS(WAITTIME))) {
						timeState = Delay;
						resetTimer();
					}
					break;
			}
		#endif // DISABLE_TIMING
	}
}

static void setOutput(const uint_fast8_t state)
{
	if(state == 0) {
		PORTB &= ~(1 << PORTB0);
	} else {
		PORTB |= (1 << PORTB0);
	}
}

static void initFilters(uint16_t *adcFilters)
{
	adcFilters[0] = adcFilters[1] = 0;
	for(uint_fast8_t ii = 0; ii < 64; ii++) {
		for(uint_fast8_t cIdx = 0; cIdx < NCHAN; cIdx++) {
			adcFilters[cIdx] += adcRead(cIdx);
		}
	}
}

static void updateFilters(uint16_t *adcFilters)
{
	// Filter the ADC readings on both channels
	for(uint_fast8_t cIdx = 0; cIdx < NCHAN; cIdx++) {
		// filter = filter*(63/64) + filter*(1/64)
		adcFilters[cIdx] -= (adcFilters[cIdx] + (1<<5)) >> 6; // Remove one 64th from the accumulator
		adcFilters[cIdx] += adcRead(cIdx); // Add one 64th of new measurement
	}
}

static uint_fast8_t tempInBand(const uint16_t *adcFilters)
{
	uint_fast8_t inBand = 1;
	
	for(uint_fast8_t cIdx = 0; cIdx < NCHAN; cIdx++) {
		uint16_t temp = getTemperature((adcFilters[cIdx] + (1<<5)) >> 6);
		
		const uint16_t lowerThresh = TL_TOFIXEDPOINT(TARGETTEMP) - (TL_TOFIXEDPOINT(TEMPBAND)>>1);
		const uint16_t upperThresh = TL_TOFIXEDPOINT(TARGETTEMP) + (TL_TOFIXEDPOINT(TEMPBAND)>>1);
		
		if(temp < lowerThresh) {
			inBand = 0;
		} else if(temp > upperThresh) {
			inBand = 0;
		}
	}
	
	return inBand;
}

static int16_t adcRead(const uint_fast8_t channel)
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

static void initHW(void)
{
	// ZTX450 is on pin 5/PB0
	// RF Tx on pin 6/PB1
	PORTB &= ~(1 << PORTB2) & ~(1 << PORTB0);
	DDRB |= (1 << DDB2) | (1 << DDB0); // Set it as an output

	// ADC clock is CPUClock/8 or 150kHz at 1.2MHz
	ADCSRA |= (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN);

	WDTCR = (1<<WDTIE) | (1<<WDP2); // Watchdog timer trips every 0.25 seconds

	// TCCR0B = (1<<CS02) | (1<<CS00); // Divide input clock by 1024
	// TIMSK0 = (1<<TOIE0); // Enable interrupts, they tick at 4.577636719Hz

	sei();
}

static void resetTimer(void)
{
	cli();
		timerTick = 0;
	sei();
}

static uint_fast8_t timeExpired(const uint_fast16_t tLimit)
{
	uint_fast8_t expired;

	cli();
		expired = timerTick > tLimit;
	sei();

	return expired;
}

// ISR(TIM0_OVF_vect)
ISR(WDT_vect)
{
	timerTick++;
}

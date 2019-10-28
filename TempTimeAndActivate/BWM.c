#include <stdlib.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "Serial.h"
#include "TempLookup.h"
#include "RFSwitch.h"
#include "Random.h"

// #define DISABLE_TIMING
// #define NOPWM
// #define PB4ASOUTPUT
// #define WARMOUT

#define ONTIME 50 // In seconds
#define TARGETTEMP 41 // Target temperature
#define TEMPBAND 2 // Allowed slop
#define NCHAN 2 // Number of temperature channels to monitor

#define WAITTIME 7200 // Two hours
// #define WAITTIME 2 // Two seconds
#define WARMTIME 1800 // Thirty minutes
// #define WARMTIME 20 // Twenty seconds

#define RFADDRESS 0
#define RFCHANNEL 0

#define TICKPERIOD 0.286225
#define TOTICKS(period) ((uint_fast16_t)(period)/(float)TICKPERIOD + 0.5)
	
static void initHW(void);
static int16_t adcRead(const uint_fast8_t channel);
static void initFilters(uint16_t *adcFilters);
static void updateFilters(uint16_t *adcFilters);
static uint_fast8_t tempInBand(const uint16_t *adcFilters);
static uint_fast8_t getPWMLevelFromEEPROM(void);
static void setPWMLevelInEEPROM(uint_fast8_t level);
static void setOutput(const uint_fast8_t level);
static uint_fast8_t testButtonPressed(void);
static uint_fast8_t __attribute__ ((noinline)) timeExpired(const uint_fast16_t);
static void resetTimer(void);

volatile static uint_fast16_t timerTick = 0;

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
	random16InitFromEEPROM(); // This is before the HW init because I don't want interrupts while I'm accessing the EEPROM

	uint_fast8_t pwmLevel = getPWMLevelFromEEPROM(); // Same... assumes interrupts are off

	initHW();

	#ifndef DISABLE_TIMING
		uint_fast16_t randomDelay = getLastRandomNumber(); // 0 to 5 hours 13 minutes
		// randomDelay -= randomDelay >> 2; // 0 to 3 hours 54 minutes
		randomDelay = randomDelay - (randomDelay >> 2) - (randomDelay >> 3); // 0 to 3 hours 15 minutes
		// randomDelay = randomDelay >> 1; // 0 to 2 hours 17 minutes
		// randomDelay = TOTICKS(8); // Eight seconds
	#endif // DISABLE_TIMING

	uint16_t adcFilters[NCHAN];
	initFilters(adcFilters);

	uint_fast8_t testCounter = 0;
	uint_fast8_t testLevel = pwmLevel;
	enum TempMachineStates tempState = Sensing;
	enum TimerMachineStates timeState = Wait;
	while (1) {
		sleep_enable(); // This regulates the loop rate to about 64Hz
		sleep_cpu();

		// This block defines the test and configuration behaviour
		if(testButtonPressed()) { // If the input is as active then...
			if(testCounter < 0xFF) { // Count down until the configuration duration (i.e. hold the button down to enter configuration)
				testCounter++;
			} else {
				testLevel++;
			}
			setOutput(testLevel);
		} else {
			if(testCounter > 0) {
				testCounter--;
			} else {
				if(pwmLevel != testLevel) {
					// Set EEPROM
					setPWMLevelInEEPROM(testLevel);
					pwmLevel = testLevel;
					setOutput(0);
				}
			}
		}

		// This state machine controls the activation of the output based on the temperature window
		switch(tempState) {
			case Inhibit:
				#ifndef WARMOUT
					setOutput(0);
				#endif // WARMOUT
				cli(); // This will cause the processor to sleep with no way of waking
				break;
			case Active:
				#ifndef WARMOUT
					setOutput(pwmLevel);
				#endif // WARMOUT
				if(timeExpired(TOTICKS(ONTIME))) {
					tempState = Inhibit;
				}
				break;
			case Sensing:
				#ifndef WARMOUT
					// setOutput(0); // The system starts in the state - it will be overridden by the test functions, power cycling will reset this state
				#endif // WARMOUT
				updateFilters(adcFilters);
				if(tempInBand(adcFilters)) {
					resetTimer();
					#ifndef DISABLE_TIMING
						send(getCodeWord(RFADDRESS, RFCHANNEL, 0));
					#endif // DISABLE_TIMING
					tempState = Active;
					timeState = Expired;
				}
				break;
		}

		// This state machine controls the random delay before the heating/cooling cycle starts
		#ifndef DISABLE_TIMING
			switch(timeState) {
				case Expired:
					#ifdef WARMOUT
						setOutput(0);
					#endif // WARMOUT
					break;
				case Warm:
					#ifdef WARMOUT
						setOutput(pwmLevel);
					#endif // WARMOUT
					if(timeExpired(TOTICKS(WARMTIME))) {
						send(getCodeWord(RFADDRESS, RFCHANNEL, 0));
						timeState = Expired;
					}
					break;
				case Delay:
					if(timeExpired(randomDelay)) {
						resetTimer();
						send(getCodeWord(RFADDRESS, RFCHANNEL, 1));
						timeState = Warm;
					}
					break;
				case Wait:
					if(timeExpired(TOTICKS(WAITTIME))) {
						resetTimer();
						timeState = Delay;
					}
					break;
			}
		#endif // DISABLE_TIMING
	}
}

static uint_fast8_t getPWMLevelFromEEPROM(void)
{
	uint_fast8_t level;

	while(EECR & (1 << EEPE));
	EEARL = 2;
	EECR |= (1<<EERE);
	level = EEDR;

	return level;
}

static void setPWMLevelInEEPROM(uint_fast8_t level)
{
	cli();
		while(EECR & (1<<EEPE));
		EEARL = 2;
		EEDR = level;
		EECR |= (1<<EEMPE);
		EECR |= (1<<EEPE);
	sei();
}

static void setOutput(const uint_fast8_t level)
{
	#ifdef NOPWM
		if(level == 0) {
			PORTB &= ~(1 << PORTB0);
		} else {
			PORTB |= (1 << PORTB0);
		}
	#else
		OCR0A = level;
	#endif // NOPWM
}

static uint_fast8_t testButtonPressed(void)
{
	return (PINB & (1<<PINB1)) == 0;
}

static void initFilters(uint16_t *adcFilters)
{
	adcFilters[0] = adcFilters[1] = (1<<4);
	for(uint_fast8_t ii = 0; ii < (1<<5); ii++) {
		for(uint_fast8_t cIdx = 0; cIdx < NCHAN; cIdx++) {
			adcFilters[cIdx] += adcRead(cIdx);
		}
	}
}

static void updateFilters(uint16_t *adcFilters)
{
	// Filter the ADC readings on both channels
	for(uint_fast8_t cIdx = 0; cIdx < NCHAN; cIdx++) {
		// filter = filter*(31/32) + filter*(1/32)
		adcFilters[cIdx] -= adcFilters[cIdx] >> 5; // Remove one 64th from the accumulator
		adcFilters[cIdx] += adcRead(cIdx); // Add one 64th of new measurement
	}
}

static uint_fast8_t tempInBand(const uint16_t *adcFilters)
{
	const uint16_t lowerThresh = TL_TOFIXEDPOINT(TARGETTEMP) - (TL_TOFIXEDPOINT(TEMPBAND)>>1);
	const uint16_t upperThresh = TL_TOFIXEDPOINT(TARGETTEMP) + (TL_TOFIXEDPOINT(TEMPBAND)>>1);

   uint_fast8_t inBand = 1;
   for(uint_fast8_t cIdx = 0; cIdx < NCHAN; cIdx++) {
      uint16_t temp = getTemperature(adcFilters[cIdx] >> 5);

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

	int16_t adcResult = ADC; // 10-bit resolution (both bytes)

	return adcResult;
}

static void initHW(void)
{
	// ZTX450 is on pin 5/PB0
	// RF Tx on pin 6/PB1
	// PORTB &= ~(1 << PORTB2) & ~(1 << PORTB0); // This register starts at zero anyway so is commented out
	DDRB |= (1 << DDB2) | (1 << DDB0); // Set it as an output
	#ifdef PB4ASOUTPUT
		// PORTB &= ~(1 << PORTB4); // In case we want to use PB4 as an output debugging pin, again commented out as starts at zero
		DDRB |= (1 << DDB4); // In case we want to use PB4 as an output debugging pin
	#endif

	// Enable the pullup on PB1
	PORTB |= (1 << PORTB1);

	#if F_CPU==1200000UL
		// ADC clock is CPUClock/8 or 150kHz at 1.2MHz
		ADCSRA |= (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN);
	#elif F_CPU==9600000UL
		// ADC clock is CPUClock/128 or 75kHz at 9.6MHz
		ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN);
	#endif

	// WDTCR = (1 << WDTIE) | (1 << WDP2); // Watchdog timer trips every 0.25 seconds-ish
	WDTCR = (1 << WDTIE); // Watchdog timer trips every 16ms-ish

	#ifndef NOPWM
		TCCR0A = (1<<COM0A1) | (0<<COM0A0) | (1 << WGM00); // PWM(Phase Correct)
		TCCR0B = (1 << CS00); // Divide input clock by 1024
	#endif

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
		expired = timerTick >= tLimit;
	sei();

	return expired;
}

ISR(WDT_vect)
{
	static uint_fast8_t divWDT;
	divWDT++;
	if((divWDT & 0x0F) == 0) { // Divide the 64Hz ish by 16 to give a period close to 0.25s (4Hz)
		timerTick++;
	}
}

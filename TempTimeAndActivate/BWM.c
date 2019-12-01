#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "TempLookup.h"
#include "RFSwitch.h"
#include "SimpleEEPROMInterface.h"
#include "Random.h"

// #define DISABLE_TIMING
// #define NOPWM
// #define PB4ASOUTPUT
// #define WARMOUT
// #define PB4ASACTIVEINDICATOR
// #define SHORTDELAY

#define ONTIME 50 // In seconds
#define TARGETTEMP 40 // Target temperature
#define TEMPBAND 1.5 // Allowed slop
#define NCHAN 2 // Number of temperature channels to monitor

#define WAITTIME 7200 // Two hours
#define WARMTIME 1800 // Thirty minutes

#define RFADDRESS 0
#define RFCHANNEL 0

#define TICKPERIOD 0.286225
#define TOTICKS(period) ((uint_fast16_t)(period)/(float)TICKPERIOD + 0.5)

#ifdef PB4ASACTIVEINDICATOR // If PB4 is our indicator then enable the pin in the DDR
	#define PB4ASOUTPUT
#endif

#ifdef SHORTDELAY // Then redefine the time delay
	#undef WAITTIME
	#undef WARMTIME
	#define WAITTIME 8 // Two seconds
	#define WARMTIME 20 // Twenty seconds
#endif

// Hardware init.
static void initHW(void);
// ADC init.
static void initFilters(uint16_t *adcFilters);
static const int16_t adcRead(const uint_fast8_t channel);
static const uint_fast8_t tempInBand(uint16_t *adcFilters);
// PWM level management
static inline uint_fast8_t getPWMLevelFromEEPROM(void);
static inline void setPWMLevelInEEPROM(const uint_fast8_t level);
static inline void setOutput(const uint_fast8_t level);
// Indicator control
static inline void toggleActiveIndicator(void);
static inline void clearActiveIndicator(void);
static inline void setActiveIndicator(void);
static inline const uint_fast8_t testButtonPressed(void);
// Timer functions
static inline const uint_fast8_t timeExpired(const uint_fast16_t tLimit, const uint_fast16_t currentTickValue);
static void __attribute__ ((noinline)) resetTimer(void);

// Divided timer count
volatile static uint_fast16_t timerTick = 0;

// #define FIX_POINTER(_ptr) __asm__ __volatile__("" : "=b" (_ptr) : "0" (_ptr))

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
	initHW();

	random16InitFromEEPROM();
	uint_fast8_t pwmLevel = getPWMLevelFromEEPROM();

	#ifndef DISABLE_TIMING
		uint_fast16_t randomDelay = getLastRandomNumber(); // 0 to 5 hours 13 minutes
		// randomDelay -= randomDelay >> 2; // 0 to 3 hours 54 minutes
		randomDelay = 5*(randomDelay >> 3); // 0 to 3 hours 15 minutes
		// randomDelay = randomDelay >> 1; // 0 to 2 hours 36 minutes
		#ifdef SHORTDELAY
			randomDelay = TOTICKS(8); // Eight seconds
		#endif
	#endif // DISABLE_TIMING

	uint16_t adcFilters[NCHAN];
	initFilters(adcFilters);

	uint_fast16_t currentTickValue;
	uint_fast8_t testCounter = 0;
	uint_fast8_t testLevel = pwmLevel;
	enum TempMachineStates tempState = Sensing;
	enum TimerMachineStates timeState = Wait;
	while (1) {
		sleep_mode(); // This regulates the loop rate to about 64Hz

		cli();
			// Get the latest timer value so we can use it to toggle the power LED
			currentTickValue = timerTick;
		sei();

		// This block defines the test and configuration behavior
		if(testButtonPressed()) { // If the input is active then...
			if(testCounter < 0xFF) { // Count down until the configuration duration (i.e. hold the button down to enter configuration mode)
				testCounter++;
			} else {
				testLevel++;
				setOutput(0);
				// #pragma GCC unroll 0
				for(uint_fast8_t itr = 0; itr < 6; itr++) {
					sleep_mode();
				}
			}
			setOutput(testLevel);
		} else {
			if(testCounter > 0) {
				testCounter--;
			} else {
				if(pwmLevel != testLevel) { // If the level was changed on exit from test/configuration mode
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
				cli(); // This will cause the processor to sleep without means of waking
				continue; // Now go to the sleep mode
				break;
			case Active:
				#ifndef WARMOUT
					setOutput(pwmLevel);
				#endif // WARMOUT
				if(timeExpired(TOTICKS(ONTIME), currentTickValue)) {
					tempState = Inhibit;
				}
				continue;
				break;
			case Sensing:
				if(tempInBand(adcFilters)) {
					resetTimer();
					#ifndef DISABLE_TIMING
						send(getCodeWord(RFADDRESS, RFCHANNEL, 0));
						setActiveIndicator(); // Set the power indicator to solid on
					#endif // DISABLE_TIMING
					tempState = Active;
					continue;
				}
				break;
		}

		// This state machine controls the random delay before the heating/cooling cycle starts
		#ifndef DISABLE_TIMING
			uint_fast8_t activeMask; // This mask controls the toggling of the power/status LED
			switch(timeState) {
				case Expired:
					activeMask = 0x20; // These masks determine the flash period of the power LED
					#ifdef WARMOUT
						setOutput(0);
					#endif // WARMOUT
					break;
				case Warm:
					activeMask = 0x01; // Power LED flash period
					#ifdef WARMOUT
						setOutput(pwmLevel);
					#endif // WARMOUT
					if(timeExpired(TOTICKS(WARMTIME), currentTickValue)) {
						send(getCodeWord(RFADDRESS, RFCHANNEL, 0));
						timeState = Expired;
					}
					break;
				case Delay:
					activeMask = 0x04; // Power LED flash period
					if(timeExpired(randomDelay, currentTickValue)) {
						resetTimer();
						send(getCodeWord(RFADDRESS, RFCHANNEL, 1));
						timeState = Warm;
					}
					break;
				case Wait:
					activeMask = 0x04; // Power LED flash period
					if(timeExpired(TOTICKS(WAITTIME), currentTickValue)) {
						resetTimer();
						timeState = Delay;
					}
					break;
			}

			// Toggle/flash the power LED to indicate the timing state
			if(currentTickValue & activeMask) {
				clearActiveIndicator();
			} else {
				toggleActiveIndicator();
			}
		#endif // DISABLE_TIMING
	}
}

static inline uint_fast8_t getPWMLevelFromEEPROM(void)
{
	return getByteFromEEPROM(2);
}

static inline void setPWMLevelInEEPROM(const uint_fast8_t level)
{
	setByteInEEPROM(level, 2);
}

static inline void setOutput(const uint_fast8_t level)
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

static void toggleActiveIndicator(void)
{
	#ifndef PB4ASACTIVEINDICATOR
		PINB |= (1 << PINB5);
	#else
		PINB |= (1 << PINB4);
	#endif
}

static void clearActiveIndicator(void)
{
	#ifndef PB4ASACTIVEINDICATOR
		PORTB &= ~(1 << PORTB5);
	#else
		PORTB &= ~(1 << PORTB4);
	#endif
}

static void setActiveIndicator(void)
{
	#ifndef PB4ASACTIVEINDICATOR
		PORTB |= (1 << PORTB5);
	#else
		PORTB |= (1 << PORTB4);
	#endif
}

static inline const uint_fast8_t testButtonPressed(void)
{
	return (PINB & (1<<PINB1)) == 0;
}

static void initFilters(uint16_t *adcFilters)
{
	for(uint_fast8_t cIdx = 0; cIdx < NCHAN; cIdx++) { // Initialize all the channels
		adcFilters[cIdx] = 0;
	}
	for(uint_fast8_t ii = 0; ii < (1<<5); ii++) {
		for(uint_fast8_t cIdx = 0; cIdx < NCHAN; cIdx++) {
			adcFilters[cIdx] += adcRead(cIdx);
		}
	}
}

static const uint_fast8_t tempInBand(uint16_t *adcFilters)
{
	const uint16_t lowerThresh = TL_TOFIXEDPOINT(((float)TARGETTEMP) - (TEMPBAND/(float)2));
	const uint16_t upperThresh = TL_TOFIXEDPOINT(((float)TARGETTEMP) + (TEMPBAND/(float)2));

	uint_fast8_t rVal = 1; // Assume in bounds to begin with

	for(uint_fast8_t cIdx = 0; cIdx < NCHAN; cIdx++) {
		adcFilters[cIdx] -= (adcFilters[cIdx] + (1<<4)) >> 5; // Remove one 1/32 of old data from the temperature accumulator
		adcFilters[cIdx] += adcRead(cIdx); // Add one 1/32 of new measurement
		uint16_t temp = getTemperature((adcFilters[cIdx] + (1<<4)) >> 5); // Get the filtered temperature

		// If the temperature exceeds either the top or bottom threshold on either sensor then we're out of bounds
		if(temp < lowerThresh) {
			rVal = 0;
		} else if(temp > upperThresh) {
			rVal = 0;
		}
	}

	return rVal;
}

static const int16_t adcRead(const uint_fast8_t channel)
{
	if(channel == 0) {
		ADMUX = (1 << MUX1) | (1 << MUX0); // ADC3 (upper port)
	} else {
		ADMUX = (1 << MUX1) | (0 << MUX0); // ADC2 (lower port)
	}

	ADCSRA |= (1 << ADSC); // Start the conversion
	while(ADCSRA & (1 << ADSC)); // Wait for conversion to complete

	return ADC; // 10-bit resolution (both bytes)
}

static void initHW(void)
{
	// ZTX450 is on pin 5/PB0
	// RF Tx on pin 6/PB2
	// Test/Config button on PB1
	// Active LED on pin PB5

	PORTB = (1 << PORTB1); // Set the pull-up on the test button pin (PB1)
	DDRB = (1 << DDB5) | (1 << DDB2) | (1 << DDB0); // Three pins as outputs
	#ifdef PB4ASOUTPUT
		DDRB |= (1 << DDB4); // In case we want to use PB4 as an output debugging pin
	#endif

	#if F_CPU==1200000UL
		// ADC clock is CPUClock/8 or 150kHz at 1.2MHz
		ADCSRA = (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN);
	#elif F_CPU==9600000UL
		// ADC clock is CPUClock/128 or 75kHz at 9.6MHz
		ADCSRA = (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN);
	#endif

	WDTCR = (1 << WDTIE); // Watchdog timer trips every 16ms-ish

	#ifndef NOPWM
		TCCR0A = (1<<COM0A1) | (0<<COM0A0) | (1 << WGM00); // PWM(Phase Correct)
		TCCR0B = (1 << CS00); // Divide input clock by 1 (i.e. no divide)
	#endif

	sei();
}

static void resetTimer(void)
{
	cli();
		timerTick = 0;
	sei();
}

static inline const uint_fast8_t timeExpired(const uint_fast16_t tLimit, const uint_fast16_t currentTickValue)
{
	return currentTickValue >= tLimit;
}

ISR(WDT_vect)
{
	static uint_fast8_t divWDT;
	divWDT++;
	if((divWDT & 0x0F) == 0) { // Divide the 64Hz-ish by 16 to give a period close to 0.25s (4Hz)
		timerTick++;
	}
}

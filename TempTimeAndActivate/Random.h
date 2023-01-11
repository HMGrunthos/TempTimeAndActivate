/*
 * Random.h
 *
 * Created: 20/10/2019 13:41:56
 * Author: Sam
 *
 * Original code: https://blog.podkalicki.com/attiny13-pseudo-random-numbers/
 * Original code: https://github.com/lpodkalicki/blog/tree/master/avr/attiny13/009_lightweigth_prng_based_on_lfsr
 *
 */


#ifndef RANDOM_H_
#define RANDOM_H_

	static uint16_t _randomNumber;

	static uint16_t lfsr16_next(const uint16_t n)
	{
		return (n >> 0x01U) ^ (-(n & 0x01U) & 0xB400U);
	}

	static uint16_t random16(void)
	{
		return (_randomNumber = lfsr16_next(_randomNumber));
	}

	static inline uint16_t getLastRandomNumber(void)
	{
		return _randomNumber;
	}

	static inline void random16InitFromSeed(const uint16_t seed)
	{
		_randomNumber = seed;
	}

	static inline void random16InitFromEEPROM(void)
	{
		*(((uint8_t*)&_randomNumber) + 1) = getByteFromEEPROM(0);
		*((uint8_t*)&_randomNumber) = getByteFromEEPROM(1);

		random16();

		setByteInEEPROM(*((uint8_t*)&_randomNumber), 1);
		setByteInEEPROM(*(((uint8_t*)&_randomNumber) + 1), 0);
	}
#endif /* RANDOM_H_ */

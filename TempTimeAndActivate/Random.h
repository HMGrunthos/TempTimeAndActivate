/*
 * Random.h
 *
 * Created: 20/10/2019 13:41:56
 *  Author: Sam
 */ 


#ifndef RANDOM_H_
#define RANDOM_H_

	static uint16_t randomNumber;

	static uint16_t lfsr16_next(uint16_t n)
	{
		return (n >> 0x01U) ^ (-(n & 0x01U) & 0xB400U);
	}

	static uint16_t random16(void)
	{
		return (randomNumber = lfsr16_next(randomNumber));
	}

	static void random16InitFromSeed(uint16_t seed)
	{
		randomNumber = seed;
	}

	static void random16InitFromEEPROM()
	{
		while(EECR & (1<<EEPE));
		EEARL = 0;
		EECR |= (1<<EERE);
		randomNumber = EEDR;
		
		randomNumber <<= 8;
		
		while(EECR & (1<<EEPE));
		EEARL = 1;
		EECR |= (1<<EERE);
		randomNumber |= EEDR;

		random16();

		while(EECR & (1<<EEPE));
		EECR = (0<<EEPM1) | (0>>EEPM0);
		EEDR = randomNumber;
		EECR |= (1<<EEMPE);
		EECR |= (1<<EEPE);
		
		while(EECR & (1<<EEPE));
		EEARL = 0;
		EEDR = randomNumber >> 8;
		EECR |= (1<<EEMPE);
		EECR |= (1<<EEPE);
	}
#endif /* RANDOM_H_ */
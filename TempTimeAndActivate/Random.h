/*
 * Random.h
 *
 * Created: 20/10/2019 13:41:56
 *  Author: Sam
 */ 


#ifndef RANDOM_H_
#define RANDOM_H_
/*
	static uint16_t wyhash16_x = 0xBEEF;

	static inline uint16_t hash16(uint16_t input, uint16_t key)
	{
		uint32_t hash = input * (uint32_t)key;
		return ((hash >> 16) ^ hash) & 0xFFFF;
	}

	static inline uint16_t wyhash16()
	{
		wyhash16_x += 0xFC15;
		return hash16(wyhash16_x, 0x2AB);
	}*/

static uint16_t randomNumber = 0;

static uint16_t lfsr16_next(uint16_t n)
{

	return (n >> 0x01U) ^ (-(n & 0x01U) & 0xB400U);
}

uint16_t random16(void)
{
	return (randomNumber = lfsr16_next(randomNumber));
}
/*
void random_init(uint16_t seed)
{
	#ifdef USE_RANDOM_SEED
		random_number = lfsr16_next(eeprom_read_word((uint16_t *)RANDOM_SEED_ADDRESS) ^ seed);
		eeprom_write_word((uint16_t *)0, random_number);
	#else
		random_number = seed;
	#endif    // !USE_RANDOM_SEED
}
*/
#endif /* RANDOM_H_ */
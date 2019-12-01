#ifndef SIMPLEEEIF
	#define SIMPLEEEIF

	static uint_fast8_t __attribute__((noinline)) getByteFromEEPROM(uint_fast8_t address)
	{
		uint_fast8_t level;

		cli();
			while(EECR & (1 << EEPE));
			EEARL = address;
			EECR |= (1<<EERE);
			level = EEDR;
		sei();

		return level;
	}

	static void __attribute__((noinline)) setByteInEEPROM(const uint_fast8_t value, const uint_fast8_t address)
	{
		cli();
			while(EECR & (1<<EEPE));
			EECR = (0<<EEPM1) | (0<<EEPM0);
			EEARL = address;
			EEDR = value;
			EECR |= (1<<EEMPE);
			EECR |= (1<<EEPE);
		sei();
	}
#endif

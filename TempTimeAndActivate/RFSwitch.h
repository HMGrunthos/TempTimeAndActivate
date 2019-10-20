/*
 * RFSwitch.h
 *
 * Created: 20/10/2019 11:35:44
 *  Author: Sam
 */ 


#ifndef RFSWITCH_H_
#define RFSWITCH_H_

	#include <util/delay.h>

	#define RFS_CODEWORDLENGTH 24
	#define RFS_PULSELENGTH 350
	#define RFS_NREPEATTRANSMIT 32

	enum RFSPulseType {
		Sync,
		Zero,
		One
	};
	static void transmit(enum RFSPulseType pulse);

	static uint_least32_t getCodeWord(uint_fast8_t nAddressCode, uint_fast8_t nChannelCode, uint_fast8_t bStatus)
	{
		uint_least32_t code = 0;
	
		for (uint_fast8_t i = 0; i < 4; i++) {
			code |= (nAddressCode == i) ? 0b00 : 0b01;
			code <<= 2;
		}
	
		for (uint_fast8_t i = 0; i < 4; i++) {
			code |= (nChannelCode == i) ? 0b00 : 0b01;
			code <<= 2;
		}
	
		code |= 0b01;
		code <<= 2;
		code |= 0b01;
		code <<= 2;
		code |= 0b01;
		code <<= 2;
	
		uint_fast8_t symbol = bStatus ? 0b01 : 0b00;
		code |= symbol;

		return code;
	}

	static void send(uint_least32_t code)
	{
		for(uint_fast8_t nRepeat =	0; nRepeat < RFS_NREPEATTRANSMIT; nRepeat++) {
			cli();
				for (int_fast8_t i = RFS_CODEWORDLENGTH-1; i >= 0; i--) {
					if (code & (1L << i)) {
						transmit(One);
					} else {
						transmit(Zero);
					}
				}
				transmit(Sync);
			sei();
		}
	}

	static void transmit(enum RFSPulseType pulse)
	{
		PORTB |= (1 << PORTB2);
		switch(pulse) {
			case Sync:
				_delay_us(RFS_PULSELENGTH * 1);
				PORTB &= ~(1 << PORTB2);
				_delay_us(RFS_PULSELENGTH * 31);
				break;
			case Zero:
				_delay_us(RFS_PULSELENGTH * 1);
				PORTB &= ~(1 << PORTB2);
				_delay_us(RFS_PULSELENGTH * 3);
				break;
			case One:
				_delay_us(RFS_PULSELENGTH * 3);
				PORTB &= ~(1 << PORTB2);
				_delay_us(RFS_PULSELENGTH * 1);
				break;
		}
	}
#endif /* RFSWITCH_H_ */
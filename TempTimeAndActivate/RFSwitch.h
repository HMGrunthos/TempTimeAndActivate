/*
 * RFSwitch.h
 *
 * Created: 20/10/2019 11:35:44
 *  Author: Sam
 */ 


#ifndef RFSWITCH_H_
#define RFSWITCH_H_

	#include <util/delay.h>

	#define RFS_CODEWORDLENGTH 12
	#define RFS_NREPEATTRANSMIT 32
	#define RFS_PORT PORTB2
	// #define RFS_PORT PORTB4

	static inline const uint_fast16_t __attribute__((always_inline)) getCodeWord(uint_fast8_t nAddressCode, uint_fast8_t nChannelCode, uint_fast8_t bStatus)
	{
		uint_fast16_t code = 0;

		for (uint_fast8_t i = 0; i < 4; i++) {
			code |= (nAddressCode == i) ? 0b0 : 0b1;
			code <<= 1;
		}
	
		for (uint_fast8_t i = 0; i < 4; i++) {
			code |= (nChannelCode == i) ? 0b0 : 0b1;
			code <<= 1;
		}
	
		code |= 0b1;
		code <<= 1;
		code |= 0b1;
		code <<= 1;
		code |= 0b1;
		code <<= 1;
	
		code |= bStatus ? 0b1 : 0b0;

		return code;
	}

	static void transmit(const uint_fast16_t high, const uint_fast16_t low)
	{
		PORTB |= (1 << RFS_PORT);
		_delay_loop_2(high);
		PORTB &= ~(1 << RFS_PORT);
		_delay_loop_2(low);
	}

	// #define SHORTPULSE 92
	// #define LONGPULSE 262
	// #define SYNCPULSE 2710
	#define SHORTPULSE 816
	#define LONGPULSE 2481
	#define SYNCPULSE 25555
	static void send(const uint_fast16_t code)
	{
		for(uint_fast8_t nRepeat =	0; nRepeat < RFS_NREPEATTRANSMIT; nRepeat++) {
			cli();
			uint_fast16_t mask = 1L << (RFS_CODEWORDLENGTH-1);
			while(mask) {
				transmit(SHORTPULSE, LONGPULSE);
				if(code & mask) {
					transmit(LONGPULSE, SHORTPULSE);
				} else {
					transmit(SHORTPULSE, LONGPULSE);
				}
				mask = mask >> 1;
			}
			transmit(SHORTPULSE, SYNCPULSE);
			sei();
		}
	}
#endif /* RFSWITCH_H_ */

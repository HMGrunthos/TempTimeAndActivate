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
	#define RFS_PULSELENGTH 350
	#define RFS_NREPEATTRANSMIT 32

	enum RFSPulseType {
		Sync,
		Zero,
		One
	};

	static uint_fast16_t getCodeWord(uint_fast8_t nAddressCode, uint_fast8_t nChannelCode, uint_fast8_t bStatus)
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

	static void transmitOne()
	{
		PORTB |= (1 << PORTB2);
		_delay_us(RFS_PULSELENGTH * 3);
		PORTB &= ~(1 << PORTB2);
		_delay_us(RFS_PULSELENGTH * 1);
	}
	
	static void transmitZero()
	{
		PORTB |= (1 << PORTB2);
		_delay_us(RFS_PULSELENGTH * 1);
		PORTB &= ~(1 << PORTB2);
		_delay_us(RFS_PULSELENGTH * 3);
	}
		
	static void transmitSync()
	{
		PORTB |= (1 << PORTB2);
		_delay_us(RFS_PULSELENGTH * 1);
		PORTB &= ~(1 << PORTB2);
		_delay_us(RFS_PULSELENGTH * 31);
	}

	static void send(uint_fast16_t code)
	{
		for(uint_fast8_t nRepeat =	0; nRepeat < RFS_NREPEATTRANSMIT; nRepeat++) {
			cli();
				uint_fast16_t mask = 1L << (RFS_CODEWORDLENGTH-1);
				while(mask) {
					transmitZero();
					if(code & mask) {
						transmitOne();
					} else {
						transmitZero();
					}
					mask = mask >> 1;
				}
				transmitSync();
			sei();
		}
	}
#endif /* RFSWITCH_H_ */
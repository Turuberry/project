#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#define STOP (PORTB & 0x00)
volatile unsigned int count = 0;
volatile unsigned int CNT1, CNT2 = 0;
unsigned int one, two,three, four;

ISR(INT0_vect)
{
	PORTA = 0x01;
}

ISR(INT1_vect)
{
	PORTA = 0x02;
}

ISR(INT2_vect)
{
	PORTA = 0x04;
}

ISR(INT3_vect)
{
	PORTA = 0x08;
	count = count;
}

ISR(INT6_vect)
{
	CNT1 = CNT1 + 1;
	if(CNT1 > 5) 
	{
		CNT1 = 0;
	}
	OCR0 = CNT1*50;
	if(OCR0 > 240)
	{
		OCR0 = 255;
	}
	else
	{
		;
	}
	PORTB = STOP;
	PORTB = (PORTB|(1<<PB0)|(0<<PB1)); 
}

ISR(INT7_vect)
{
	CNT2 = CNT2 + 1;
	if(CNT2 > 5) 
	{
		CNT2 = 0;
	}
	OCR0 = CNT2*50;
	
	if(OCR0 > 240)
	{
		OCR0 = 255;
	}
	else
	{
		;
	}

	PORTB = STOP;
	PORTB = (PORTB|(1<<PB2)|(0<<PB3)); 

}

int main(void)
{
	DDRA  = 0x07; 	// LED 
	DDRB  = 0x1f; 	// MOTOR
	DDRC  = 0xff; 	// 7segment
	EIMSK = 0xcf;	// INT enable
	EICRA = 0x00;	// the leds will turn on when switch is low level
	EICRB = 0x00;	// the motor will turn on when switch is low level
	TCCR0  |=  (1<<WGM01)  | (1<<WGM00 ) | (1<<COM01) | (0<<COM00) | (0<<CS02) | (1<<CS01) | (0<<CS00) ;	
					// Mode: Fast PWM, non-inverting, 8 prescaler
	TIMSK = 0x00; 	//overflow interrupt enable
	TCNT0 = 0x00;	// strating point : 0
	sei();			// overall interrupt enable
	while(1)
	{	
		if(PORTA == 0x01)
		{
			count = count + 1;
			_delay_ms(5);
		}

		if(PORTA == 0x02)
		{
			count = count + 2;
			_delay_ms(5);
		}

		if(PORTA == 0x04)
		{
			count = count + 4;
			_delay_ms(5);
		}

		if(PORTA == 0x08)
		{
			count = count;
		}
		
		four = count / 1000 ;
		three = (count %1000) / 100 ;
		two = (count %100) / 10 ;
		one = (count %10) / 1;

		PORTC = 0xe0 | four ;  // 1000의 자리 표시
		_delay_ms(5);

		PORTC = 0xd0 | three ;  // 100의 자리 표시
		_delay_ms(5);

		PORTC = 0xb0 | two ;  // 10의 자리 표시
		_delay_ms(5);

		PORTC = 0x70 | one ;  // 1의 자리 표시
		_delay_ms(5);
	}

	return 0;

}
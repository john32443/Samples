//External oscillator from board: CKSEL3..1=0000 SUT1..0=00
//Low Power Crystal Oscillator: CKSEL3..1=1111 SUT1..0=11

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>				// for itoa() call
#include "lcd.h"
#include <stdio.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

volatile unsigned int ADCReading;
volatile unsigned int currentTemp;
void readADC (void); //reads the ADC
void printTemp (void); //Prints the temperature
void initialize(void);					// All the usual MCU stuff
void displaysetPoint (void); // displays the setPoint
volatile int setPoint; //intializes the setpoints
volatile int holdTime;


// Port D Interrupt service Routine (ISR)
ISR (PCINT3_vect)
{
	holdTime = 1; //flag stating that a setPoint change is requested
}



ISR(TIMER1_COMPA_vect)
{
	if((~PINB & 0b00000001)&&(holdTime==1)){ //decreases the currentTemp

		setPoint--;
		 displaysetPoint ();
		holdTime = 0; //clears setPoint change flag
	}

	if((~PINB & 0b10000010)&&(holdTime==1)){ //increases the currentTemp

		setPoint++;
		 displaysetPoint ();
		holdTime = 0;//clears setPoint change flag

	}

}



ISR (ADC_vect)
{
	readADC();

}

//entry point and task scheduler loop
int main(void)
{
    initialize();					// Initialize to starting state
    printTemp(); //prints the current temp on the LCD
    while (1){

    	if (currentTemp>setPoint)
    	{
    	  PORTA = 0x01;//turn fan on
    	}

    	if (currentTemp<=setPoint)
    	{
    		PORTA = 0x00;//turn fan off
    	}

    		}


}

//initialize()

void initialize(void)
{



	//set up the ports
	DDRA = 0x00; 					// Fan is off
	DDRD = 0x00; 					//Port D set to input for Pin Change Interrupts

	lcd_init(LCD_DISP_ON);			// Turn on Display
	lcd_clrscr();					// Clear Screen


	//crank up the ISRs
	PCICR  =0b00001000;					// Enable Pin Change interrupt on Port D
	PCMSK3 =0b0000011;					// Enable Pin Change interrupt on PIN0 and PIN1 on Port D

     setPoint = 30; //Sets the setPoint to 30 degrees celsius

     TCCR1A=0b00000000; //CTC mode no PWM, the most significant 6 bits do not matter for this lab
     TCCR1B=0b00001100; //prescaler 256
     TIMSK1=0b00000010; //enable Output Compare A Match interrupt
     OCR1A=31250; //set the compare register to 31250 clock ticks -> Output Compare A Match interrupt occurs with 2Hz frequency

    //ADC
	ADCSRA = 0b10101000;// enables the ADC in auto trigger mode with 2 bit prescaler
	ADCSRB=  0b00000101; // Sets up the ADC to fire when the timer1 match B is fired
	ADMUX =  0b11000000; // use internal 2.56 v for reference voltage with a right adjust result
	sei();
}

void readADC (void)
{
	char lowerBits; // stores ADCL bits
	int higherBits; //stores ADCH bits

	higherBits = ADCH<<8;
	lowerBits = ADCL;
	ADCReading = higherBits;
	ADCReading |= lowerBits;
	currentTemp = ((ADCReading*2.56)/1024/0.03); // updates the currentTemp. Transfer function on pg 253.
	//The reason is that we divide by 0.03, is that each 1 mv is a change of 1 degree, and we multiply by 3 because of opAMP

}


void printTemp (void)
{
	char tempArray [10];
	itoa (currentTemp,tempArray,10);
	lcd_puts ("Current Temperature: ");
	lcd_puts (tempArray);
}


void displaysetPoint (void)
{		lcd_clrscr();
		char setArray [10];
		itoa (setPoint,setArray,10);
		lcd_gotoxy(0,1);// goes to the next line
		lcd_puts ("setPoint: ");
		lcd_puts (setArray);
}



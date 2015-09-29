//External oscillator from board: CKSEL3..1=0000 SUT1..0=00
//Low Power Crystal Oscillator: CKSEL3..1=1111 SUT1..0=11

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>				// for itoa() call
#include "lcd.h"
#include <stdio.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "i2cmaster.h"

#define AT24C64_BUS_ADDRESS 0XA0
#define PCF8653_BUS_ADDRESS 0xA2

volatile unsigned int state;			// The current state of the program

volatile unsigned int currentSeconds;
volatile unsigned int currentMinutes;
volatile unsigned int currentHours;
volatile unsigned int currentDay;                // Contains the current Day
volatile unsigned int currentMonth;              // Contains the current Month
volatile unsigned int currentYear;               // Contains the current Year
volatile unsigned int addressCounter;            // Keeps track of the address for the log
unsigned char *Hours;
unsigned char *Minutes;
unsigned char *Seconds;
unsigned char *Month;
unsigned char *Day;
unsigned char *Year;

void initialize(void);					// All the usual MCU stuff
void showTime(void);
void RTCsetTime(unsigned int seconds, unsigned int minutes, unsigned int hours);
void RTCgetTime(void);
unsigned char AT24C64_byte_read(unsigned int address,unsigned char *data);
unsigned char AT24C64_byte_write(unsigned int address, unsigned char data);
unsigned char INTtoBCD(unsigned int BIN);
unsigned int BCDtoINT(unsigned int BCD, char bitMask);
void showDate(void);
void setDate(void);
void saveToLog(void);
void displayLog(void);


// Port C Interrupt service Routine (ISR)
ISR (PCINT2_vect)
{
	//lcd_puts("I");
	//if(state==0)
		RTCgetTime();	//updates the time every 1 second

}

// Port B Interrupt service Routine (ISR)
ISR (PCINT1_vect)
{
    if(~PINB & 0b00000001){

        if (state == 0){			// If button0 is pressed
            state = 1;			// Go to state 1
        }
    }
    if(~PINB & 0b00000010){

        if (state == 3){
            state = 0;
        }
    }
    if(~PINB & 0b00000100){

	    if (state == 4){
		    currentSeconds = (currentSeconds + 1) % 60;         // Increase Seconds
		    showTime();
    	}
    }
    if(~PINB & 0b00001000){

	    if (state == 4){
	    	currentMinutes =(currentMinutes + 1) % 60;         // Increase Minutes
	    	showTime();
	    }
    }
    if(~PINB & 0b00010000){

	    if (state == 4){
	    	currentHours = (currentHours + 1) % 24;           // Increase Hours
	    	showTime();
	    }
    }
    if(~PINB & 0b00100000){			            // If Button5 is pressed

    	if (state == 0){
		    state = 4;
	    }
	    if (state == 4){

            RTCsetTime(currentSeconds,currentMinutes,currentHours);
		    state = 0;
	    }
    }
    if(~PINB & 0b01000000){			// If Button6 is pressed

    	if (state == 0){
	        state = 3;
	    }
    }
    if(~PINB & 0b10000000){			// If Button7 is pressed

    	if (state == 0){
		    state = 2;
	    }
    }
    return;
}

//entry point and task scheduler loop
int main(void)
{
    initialize();					// Initialize to starting state
    while (1){
	    switch(state){
		  case 0:

	 		    showTime();
			    break;
		    case 1:
		    	lcd_puts("1");
			    saveToLog();
			    state = 0;
			    break;
		    case 2:
		    	lcd_puts("2");
		  	    showDate();
			    if (!~PINB & 0b10000000)
				state = 0;
			    break;
	  	    case 3:
	  	    	lcd_puts("3");
	  		    displayLog();
			    if (!~PINB & 0b01000000)
				state = 0;
		  	    break;
		    case 4:
		    	lcd_puts("4");
                showTime();
	  		    // set time
			    //maybe flash screen?
	  		    break;
	    }
    }
}


//initialize()
//
// Sets up the initial conditions of the ports, timers, and interrupts.
// this function is also called to reset the program.
void initialize(void)
{
	state = 0;

	//set up the ports
	DDRC=0x00;					// PORTC is input
	DDRB=0x00; 					// PORTB is input

	lcd_init(LCD_DISP_ON);			// Turn on Display
	lcd_clrscr();					// Clear Screen
	i2c_init ();

	*Day =   0b00000010;		// Contains the current Day
	*Month = 0b00010001;		// Contains the current Month
	*Year =  0b00010001;		// Contains the current Year
	addressCounter = 0;		// initalizes the address counter

	currentSeconds = 3;

	//crank up the ISRs
	PCICR  =0b00000110;					// Enable Pin Change interrupt on Port C and Port B for CLKOUT and Switches respectively
	PCMSK2 =0b00000100;					// Enable Pin Change interrupt on PIN2 on Port C for CLKOUT
	PCMSK1 =0b11111111;                  //Enable Pin Change interrupt on on PINS1-7 for switches
	SREG   =0b10000000;					// Enables interrupts
	sei();

	i2c_start(PCF8653_BUS_ADDRESS+I2C_WRITE); // set RTC bus address and write mode
	i2c_write(0x0D);// CLKOUT control register address
	i2c_write(0b10000011);// sets the CLKOUT to 1khz
	i2c_stop();

	*Hours=   0b00000100;
	*Minutes= 0b00010100;
	*Seconds= 0b00010100;

}



/* read a byte from the 24C64 EEPROM */
unsigned char AT24C64_byte_read(unsigned int address,unsigned char *data)
{
	if(i2c_start(AT24C64_BUS_ADDRESS+I2C_WRITE)) // set device address and write mode
		return 1; // no ack from the 24C64
	if(i2c_write(address>>8)) // send the higher 8 bits of the address in the 24C64
		return 1; // no ack from the 24C64
	if(i2c_write(address&0x00FF)) // send the lower 8 bits of the address in the 24C64
		return 1; // no ack from the 24C64
	i2c_start(AT24C64_BUS_ADDRESS+I2C_READ); // set device address and read mode
		*data=i2c_readNak(); // read and expect no ack from the MCU
	i2c_stop();

	return 0; /* successful read */
}

/* write a byte to the EEPROM */
unsigned char AT24C64_byte_write(unsigned int address, unsigned char data)
{
	if(i2c_start(AT24C64_BUS_ADDRESS+I2C_WRITE)) // set device address and write mode
		return 1; // no ack from the 24C64
	if(i2c_write(address>>8)) // send the higher 8 bits of the address in the 24C64
		return 1; // no ack from the 24C64
	if(i2c_write(address&0x00FF)) // send the lower 8 bits of the address in the 24C64
		return 1; // no ack from the 24C64
	if(i2c_write(data))
		return 1; // no ack from the 24C64
	i2c_stop();
	i2c_start_wait(AT24C64_BUS_ADDRESS+I2C_WRITE); // acknowledge polling (see p.9 in AT24C64's datasheet)

  return 0; /* successful write */
}


/* read a byte from the rtc */
void RTCgetTime()
{
	volatile unsigned char temp;
	i2c_start(PCF8653_BUS_ADDRESS+I2C_WRITE); // set device address and write mode
	i2c_write(0x02);// Seconds register address
	i2c_stop();
	i2c_start(PCF8653_BUS_ADDRESS+I2C_READ);
	*Seconds = i2c_readNak();
	i2c_stop();

	i2c_start(PCF8653_BUS_ADDRESS+I2C_WRITE); // set device address and write mode
	i2c_write(0x03);// Minutes register address
	i2c_stop();
	i2c_start(PCF8653_BUS_ADDRESS+I2C_READ);
	*Minutes= i2c_readNak();
	i2c_stop();

	i2c_start(PCF8653_BUS_ADDRESS+I2C_WRITE); // set device address and write mode
	i2c_write(0x04);// Hours register address
	i2c_stop();
	i2c_start(PCF8653_BUS_ADDRESS+I2C_READ);
	*Hours= i2c_readNak();
	i2c_stop();

	currentHours = *Hours;
	temp = BCDtoINT(currentHours,0b00111111);
	currentHours = temp;

	currentHours = *Seconds;
		temp = BCDtoINT(currentHours,0b00111111);
		currentHours = temp;

				currentHours = *Minutes;
					temp = BCDtoINT(currentHours,0b00111111);
					currentHours = temp;



}
 /* converts BCD to integer*/
unsigned int BCDtoINT(unsigned int BCD, char bitMask)
	{

	 BCD &= bitMask;// masks the bits we don't need
	 char lowerBits = BCD & 0x0F;// masks upper bits, and writes lower bits to loweBits
	 char higherBits = (BCD >> 4);// sets higher bits
	  return (higherBits * 10) + lowerBits; //multiplies the higher bits by 10 and adds the lower bits since both parts are being used

	}
unsigned char INTtoBCD(unsigned int BIN)
{
	unsigned int temp;
	temp = (BIN/10)<<4;// sets the high bits
	temp |= (BIN%10);// "ores" the low bits with the high bits
	return temp;

}

void RTCsetTime(unsigned int seconds, unsigned int minutes, unsigned int hours)
{
	unsigned char temp;

	i2c_start(PCF8653_BUS_ADDRESS+I2C_WRITE); // set device address and write mode
	i2c_write(0x02);// Seconds register address
	temp = INTtoBCD (seconds);
	i2c_write(temp);
	i2c_stop();

	i2c_start(PCF8653_BUS_ADDRESS+I2C_WRITE); // set device address and write mode
	i2c_write(0x03);// Minutes register address
	temp = INTtoBCD (minutes);
	i2c_write(temp);
	i2c_stop();


	i2c_start(PCF8653_BUS_ADDRESS+I2C_WRITE); // set device address and write mode
	i2c_write(0x04);// Hours register address
	temp = INTtoBCD (hours);
	i2c_write(temp);
	i2c_stop();
}

void showTime(void)
{
	char hoursArray[6];// array to hold the current hours
	char minutesArray[6];// array to hold the current minutes
	char secondsArray[6];// array to hold the current seconds

	itoa(currentHours,hoursArray,10);//turns the current hours into a string

	lcd_gotoxy(0,0);// sets the lcd cursor to the starting position
	lcd_puts(hoursArray);// displays the hours

	lcd_gotoxy(2,0);// moves the cursor 2 positions to the right
	lcd_puts(":");// puts ":"

	itoa(currentMinutes,minutesArray,10);//turns the current minutes into a string
	lcd_gotoxy(4,0);// moves the cursor another 2 positions
	lcd_puts(minutesArray);// displays the minutes

	lcd_gotoxy(6,0);// moves the cursor another 2 positions to the right
	lcd_puts(":");// puts ":"

	itoa(currentSeconds,secondsArray,10);//turns the current seconds into a string
	lcd_gotoxy(8,0);// moves the cursor another 2 positions
	lcd_puts(secondsArray);// displays the seconds
}

void showDate(void)
{
   unsigned char temp;

	i2c_start(PCF8653_BUS_ADDRESS+I2C_WRITE); // set device address and write mode
	i2c_write(0x07);// Month register address
	i2c_stop();
	i2c_start(PCF8653_BUS_ADDRESS+I2C_READ);
	*Month= i2c_readNak();
	i2c_stop();

	i2c_start(PCF8653_BUS_ADDRESS+I2C_WRITE); // set device address and write mode
	i2c_write(0x05);// Day register address
	i2c_stop();
	i2c_start(PCF8653_BUS_ADDRESS+I2C_READ);
	*Day= i2c_readNak();
	i2c_stop();

	i2c_start(PCF8653_BUS_ADDRESS+I2C_WRITE); // set device address and write mode
	i2c_write(0x08);// Year register address
	i2c_stop();
	i2c_start(PCF8653_BUS_ADDRESS+I2C_READ);
	*Year= i2c_readNak();
	i2c_stop();

	currentMonth = *Month;// sets the BCD value of current year to a type int so we can convert it
	temp = BCDtoINT (currentMonth, 0b00011111);// converts into an integer from a BCD
	currentMonth = temp;// sets the converted value back to an integer

	currentDay = *Day;// sets the BCD value of current year to a type int so we can convert it
	temp = BCDtoINT(currentDay,0b00111111);// masks the century bit in the current month
	currentDay = temp;// sets the converted value back to an integer

	currentYear = *Year;;// sets the BCD value of current year to a type int so we can convert it
	temp = BCDtoINT(currentYear,0b11111111);// converts into an integer from a BCD
	currentYear = temp;// sets the converted value back to an integer

	char MonthArray[6];// array to hold the current Month
	char DayArray[6];// array to hold the current Day
	char YearArray[6];// array to hold the current Year

	itoa(currentMonth,MonthArray,10);//turns the current hours into a string
	lcd_gotoxy(0,0);// sets the lcd cursor to the starting position
	lcd_puts(MonthArray);// displays the hours

	itoa(currentDay,DayArray,10);//turns the current minutes into a string
	lcd_gotoxy(3,0);// moves the cursor another 2 positions
	lcd_puts(DayArray);// displays the current minute

	itoa(currentYear,YearArray,10);//turns the current seconds into a string
	lcd_gotoxy(6,0);// moves the cursor another 2 positions
	lcd_puts(YearArray);// displays the current year
}

void setDate (void)
{
	unsigned char temp;

		i2c_start(PCF8653_BUS_ADDRESS+I2C_WRITE); // set device address and write mode
		i2c_write(0x07);// Month register address
		temp = INTtoBCD(currentMonth);
		i2c_write(temp);
		i2c_stop();

		i2c_start(PCF8653_BUS_ADDRESS+I2C_WRITE); // set device address and write mode
		i2c_write(0x05);// Day register address
		temp = INTtoBCD (currentDay);
		i2c_write(temp);
		i2c_stop();


		i2c_start(PCF8653_BUS_ADDRESS+I2C_WRITE); // set device address and write mode
		i2c_write(0x08);// Year register address
		temp = INTtoBCD (currentYear);
		i2c_write(temp);
		i2c_stop();
}

void saveToLog (void)// puts the time into the log backwards so its easier to read out
{

 unsigned char check;
 unsigned char temp;

 	 	 	 	 temp=*Seconds;
		 		check = AT24C64_byte_write(addressCounter, temp);// writes the second the button was pressed
		 		 if (check == 0)// checks to see if the EEPROM was successfully written to
		 		 addressCounter++;
		 	   else lcd_puts("error in EEPROM");// displays error message if unsuccessful write to the EEPROM occurred


		 		 	 	 	 	 temp=*Minutes;
		 				 		check = AT24C64_byte_write(addressCounter, temp);// writes the minute the button was pressed
		 				 		 if (check == 0)// checks to see if the EEPROM was successfully written to
		 				 		 addressCounter++;
		 				 	   else lcd_puts("error in EEPROM");// displays error message if unsuccessful write to the EEPROM occurred

		 				 		temp=*Hours;
		 				 		 check = AT24C64_byte_write(addressCounter, temp);// writes the second the button was pressed (lower bits)
		 				 		if (check == 0)// checks to see if the EEPROM was successfully written to
		 				 		addressCounter++;
		 				 	     else lcd_puts("error in EEPROM");// displays error message if unsuccessful write to the EEPROM occurredsplays error message if unsuccessful write to the EEPROM occurred

}

void displayLog (void)// displays the last 2 log values
{
	unsigned int tempAdressCounter;// takes the value of the address counter so it can be manipulated without changing the actual value
	unsigned char *data;// holds 1 byte of data from EEPROM at time
	*data=0x00;
		unsigned char temp;
	unsigned int currentData;
	int i;// used to move through the rows of the LCD
	tempAdressCounter = addressCounter; // sets the tempAdressCounter to addressCounter
    char dataArray[6];

		while(tempAdressCounter != 0)
		{
				AT24C64_byte_read(tempAdressCounter, data);// reads 1 byte from EEPROM
				currentData = *data;// puts the data into an integer so we can convert from BCD
				temp = BCDtoINT(currentData,0b00111111);// converts BCD log into int
				currentData = temp;// assigns the converted value of the data into an int
				itoa(currentData, dataArray, 10);// converts it into a string
				lcd_gotoxy(0,1);// moves the cursor
				lcd_puts(dataArray);// displays the last hour the button was pressed
                lcd_gotoxy(1,i);//moves a space
				lcd_puts(":");//puts ":"
				tempAdressCounter--;//Decrements the temporary address counter


				AT24C64_byte_read(tempAdressCounter, data);// reads 1 byte from EEPROM
				currentData = *data;// puts the data into an integer so we can convert from BCD
				temp = BCDtoINT(currentData,0b00111111);// converts BCD log into int
				currentData = temp;// assigns the converted value of the data into an int
				itoa(currentData, dataArray, 10);// converts it into a string
				lcd_gotoxy(2,1);// moves the cursor
				lcd_puts(dataArray);// displays the last minute the button was pressed
                lcd_gotoxy(3,1);//moves a space
				lcd_puts(":");//puts ":"
				tempAdressCounter--;//Decrements the temporary address counter


				AT24C64_byte_read(tempAdressCounter, data);// reads 1 byte from EEPROM
				currentData = *data;// puts the data into an integer so we can convert from BCD
				temp = BCDtoINT(currentData,0b00111111);// converts BCD log into int
				currentData = temp;// assigns the converted value of the data into an int
				itoa(currentData, dataArray, 10);// converts it into a string
				lcd_gotoxy(4,1);// moves the cursor
				lcd_puts(dataArray);// displays the last hour the button was pressed
                lcd_gotoxy(5,1);//moves a space
				lcd_puts(":");//puts ":"
				tempAdressCounter--;//Decrements the temporary address counter

				lcd_puts("/n");// goes to the next line

		}




}


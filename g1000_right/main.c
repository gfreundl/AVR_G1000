/* Name: main.c
 * Project: hid-mouse, a very simple HID example
 * Author: Christian Starkjohann
 * Creation Date: 2008-04-07
 * Tabsize: 4
 * Copyright: (c) 2008 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: GNU GPL v2 (see License.txt), GNU GPL v3 or proprietary (CommercialLicense.txt)
 */

/*
This example should run on most AVRs with only little changes. No special
hardware resources except INT0 are used. You may have to change usbconfig.h for
different I/O pins for USB. Please note that USB D+ must be the INT0 pin, or
at least be connected to INT0 as well.

We use VID/PID 0x046D/0xC00E which is taken from a Logitech mouse. Don't
publish any hardware using these IDs! This is for demonstration only!
*/



/*

gfe

	310119: 
	matrix works ok so far, tested with 2x2 button matrix and diodes
	DBG output commented, invokes 1ms delay per line
	every 19 full sequences (18ms) pause in read for 13ms, presumably USB send
	delays with _delay_ms funktion work ok, _delay_us only to certain maximum
	
	ReadButtons function seems superfluent
	
	DBG Debug IDs for V-USB:
	02	polling
	03	about to send
	10	Rx data from usbProcessRx
	20	Tx data from usbBuildTxBlock
	21	interrupt data from usbGenericSetInterrupt
	ff	reset
	1d	Setup from usbProcessRx

	9x	my debug IDs
	//

	240219
	all encoders tested and crosschecked with schematic avr_g1000r_04.sch
	timings checked in simulator and breadboard

	OIL:
	switches to be checked, especially D1 (TXEN)
*/

#define F_CPU	12000000UL
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>  /* for sei() */
#include <util/delay.h>     /* for _delay_ms() */

#include <avr/pgmspace.h>   /* required by usbdrv.h */
#include "usbdrv.h"
#include "oddebug.h"        /* This is also an example for using debug macros */

/* ----------------------------- USB interface ----------------------------- */
PROGMEM const char usbHidReportDescriptor[23] = { /* USB report descriptor, size must match usbconfig.h */
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Joystick)
    0xA1, 0x01,                    // COLLECTION (Application)
    0x05, 0x09,                    //     USAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM
    0x29, 0x20,                    //     USAGE_MAXIMUM
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x95, 0x20,                    //     REPORT_COUNT (32)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0xC0                          // END COLLECTION
};

typedef struct{
    uchar	a1;
    uchar   a2;
    uchar   a3;
    uchar   a4;
}report_t;
static report_t reportBuffer;
static uchar    idleRate;		/* repeat rate for keyboards, never used for mice */

uchar btn[32];					//buttons
int n = 0;
int c = 1;
uchar buttons[4];
uchar enc[8];					//encoders
uchar Dets;						//Counter for encoder detents per timer cycle
uchar pot[8];					//analog inputs

void ReadEncoder(void);
void init_TC1(void);
void ReadButton(void);
void ButtonMatrix();
void ReadAnalog(void);

/* ------------------------------------------------------------------------- */

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;
    /* The following requests are never used. But since they are required by
     * the specification, we implement them in this example.    */
    if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS)
	{    /* class request type */
        DBG1(0x50, &rq->bRequest, 1);   /* debug output: print our request */
        if (rq->bRequest == USBRQ_HID_GET_REPORT)
		{  /* wValue: ReportType (highbyte), ReportID (lowbyte) */
            /* we only have one report type, so don't look at wValue */
            usbMsgPtr = (void *)&reportBuffer;
            return sizeof(reportBuffer);
        }
		else if (rq->bRequest == USBRQ_HID_GET_IDLE)
		{
            usbMsgPtr = &idleRate;
            return 1;
        }
		else if (rq->bRequest == USBRQ_HID_SET_IDLE)
		{
            idleRate = rq->wValue.bytes[1];
        }
    }
	else
	{
        /* no vendor specific requests implemented */
    }
    return 0;   /* default for not implemented requests: return no data back to host */
}
/* ------------------------------------------------------------------------- */

int __attribute__((noreturn)) main(void)
{

	uchar   i;
	enc[0] = 0;					//encoders
	enc[1] = 0;
	enc[2] = 0;
	enc[3] = 0;
	enc[4] = 0;
	enc[5] = 0;
	enc[6] = 0;

	init_TC1();

    wdt_enable(WDTO_1S);
    /* Even if you don't use the watchdog, turn it off here. On newer devices,
     * the status of the watchdog (on/off, period) is PRESERVED OVER RESET!
     */
    odDebugInit();
    DBG1(0x00, 0, 0);       /* debug output: main starts */

    usbInit();
    usbDeviceDisconnect();  /* enforce re-enumeration, do this while interrupts are disabled! */
    i = 0;
    while(--i)				/* fake USB disconnect for > 250 ms */
	{             
        wdt_reset();
        _delay_ms(1);
    }
    usbDeviceConnect();
    sei();

    DBG1(0x01, 0, 0);       /* debug output: main loop starts */
    for(;;)					//main event loop
	{                

		ButtonMatrix();		//0.61ms

		//Process buttons
		//customize n for number of buttons												// <----- change here
		n = 0;
		while (n < 0)
		{
			ReadButton();
			n++;
		}

		DDRA &= 0b00000000;								//all ports input
		DDRB &= 0b00000000;								//ButtonMatrix ports handle DDR on their own
		DDRC &= 0b00000000;
		DDRD &= 0b00000000;								//USB ports handle DDR on their own
		PORTA |= 0b11111111;							//pull-up resistors
		PORTB |= 0b11111111;
		PORTC |= 0b11111111;
		PORTD |= 0b11101011;							//no pull-ups on USB ports PD2, PD4

		enc[0] = (enc[0] & 0b00001100) 
					| ((PINA & (1<<PINA4))>>3)			//Alps Encoder RKJX
					| ((PINC & (1<<PINC3))>>3);	
		enc[1] = (enc[1] & 0b00001100)
					| ((PIND & (1<<PIND3))>>2)			//Alps Encoder STEC SW1
					| ((PIND & (1<<PIND5))>>5);
		enc[2] = (enc[2] & 0b00001100) 
					| ((PINB & (1<<PINB7))>>6)			//ELMA Encoder U$1 inner
					| (PIND & (1<<PIND0));
		enc[3] = (enc[3] & 0b00001100)
					| ((PINB & (1<<PINB5))>>4)			//ELMA Encoder U$1 outer
					| ((PINB & (1<<PINB6))>>6);
		enc[4] = (enc[4] & 0b00001100)
					| ((PINC & (1<<PINC2))>>1)			//ELMA Encoder U$2 inner
					| (PINC & (1<<PINC0));
		enc[5] = (enc[5] & 0b00001100)
					| ((PINA & (1<<PINA7))>>6)			//ELMA Encoder U$2 outer
					| ((PINA & (1<<PINA6))>>6);
		enc[6] = (enc[6] & 0b00001100)
					| ((PINC & (1<<PINC7))>>6)			//ELMA Encoder U$3 inner
					| ((PINC & (1<<PINC4))>>4);
		enc[7] = (enc[7] & 0b00001100)
					| ((PINC & (1<<PINC5))>>4)			//ELMA Encoder U$3 outer
					| ((PINB & (1<<PINB3))>>3);
		n=0;
		while (n < 8)
		{
			ReadEncoder();
			n++;
		}

		//Process Analog Inputs
		//customize i for number of potentiometers									// <----- change here
		n = 0;											//ADC5 = PA5
		while (n < 0)
		{
			ReadAnalog();
			n++;
		}

		//Buttons displayed inverted in Windows GUI: LSB left, MSB right
		reportBuffer.a1 = (buttons[0] & 0b00001111)						//S1-4
							| ((buttons[1] & 0b00000011) <<4)			//S5-6
							| ((1<<PINB4) & ~(PINB & (1<<PINB4))) <<2	//SW1
							//| (((PINA & (1<<PINA5))<<2) ^ 0x80);		//RJKX Com
							| ((buttons[0] & 0b00100000)<<2);			//PINA5 = RJKX Com 
		reportBuffer.a2 = (buttons[2] & 0b00001111)						//RJKX A-D
							| ((1<<PIND6) & ~(PIND & (1<<PIND6))) <<1	//S13
							| ((1<<PIND1) & ~(PIND & (1<<PIND1))) <<5	//U1
							| ((1<<PINC1) & ~(PINC & (1<<PINC1))) <<4	//U2
							| ((1<<PINC6) & ~(PINC & (1<<PINC6))) >>2;	//U1
		//report encoders only when input change, to control pulse length
		if (c == 1)
		{
			reportBuffer.a3 = (enc[0] & 0b11000000) >>6
								| (enc[1] & 0b11000000) >>4
								| (enc[2] & 0b11000000) >>2
								| (enc[3] & 0b11000000);								;
			reportBuffer.a4 = (enc[4] & 0b11000000) >>6
								| (enc[5] & 0b11000000) >>4
								| (enc[6] & 0b11000000) >>2
								| (enc[7] & 0b11000000);
			c = 0;
			TIFR |= (1<<OCF1A);					//clear timer compare flag
			TCNT1 = 0x0000;						//restart timer
		}

		_delay_ms(1);							//for debouncing

		//control pulse length for encoders
		//check for time elapsed, if so, clear buffers carrying encoder
		if (TIFR & (1<<OCF1A))						
		{
			reportBuffer.a3 = 0; 				//clear encoders, don't clear buttons
			reportBuffer.a4 = 0;
			Dets = 0;
			TIFR |= (1<<OCF1A);					//clear timer compare flag
		}
			 
	    DBG1(0x02, 0, 0);   /* debug output: main loop iterates */
        wdt_reset();
        usbPoll();
        if(usbInterruptIsReady())
		{
 			DBG1(0x03, 0, 0);   /* debug output: interrupt report prepared */
			/* called after every poll of the interrupt endpoint */
			usbSetInterrupt((void *)&reportBuffer, sizeof(reportBuffer));
		}
    }
}

/* ------------------------------------------------------------------------- */

//compare new port bits with previous readout
//save port status in bits 2,3 for next comparison in next cycle
void ReadButton(void)
{
	char oldbtn;
	char newbtn;
	oldbtn = ((btn[n] & 0b00001100) >> 2);			//shift bits from last readout for compare
	newbtn = btn[n] & 0b00000011;					//only regard bits 0,1
	if ((newbtn ^ oldbtn) != 0)						//someting changed with this button ?
	{
		c = 1;
		oldbtn = (newbtn << 2);						//renew oldrty with newrty
		btn[n] = btn[n] & 0b11110011;				//clear oldrty bits within rty[i]
		btn[n] = (btn[n] | oldbtn);					//save "new" oldrty within rty[i]
	}//end if ((newbtn ^ oldbtn) != 0)
}//end ReadButton(void)

//general key matrix
//PB0-2 column output
//PA0-3 row input, PA5 special pin 'Com' on RKJX switch
void ButtonMatrix()
{
	uchar i = 0;
	DDRB &= 0b11111000;					//set columns input HiZ
	PORTB &= 0b11111000;				//prepare columns for output low
	DDRA &= 0b11110000;					//set rows input
	PORTA |= 0b00001111;				//set rows pull-ups on

	for (i=0;i<3;i++)
	{
		DDRB &= 0b11111000;				//(re-)set columns input HiZ
		DDRB |= 1<<i;		 			//set col k output, already set low
		_delay_ms(0.2);					//wait to settle & debounce
		buttons[i] = ~PINA;				//read, negate and save row
	}									//next col
}

//compare new port bits with previous readout
//determine direction of rotation and write into bits 7,6 (10=CW, 01=CCW)
//if number of repeated pulses of same direction (detents) > 4 => report fast rotation in bits 4,5
//save port status in bits 2,3 for next comparison in succeeding cycle
void ReadEncoder()
{
	char oldenc;
	char newenc;
	oldenc = ((enc[n] & 0b00001100) >> 2);			//align bits from last readout for compare
	newenc = (enc[n] & 0b00000011);					//only regard bits 0,1
	DBG1(0x93, newenc, 1);
	DBG1(0x94, oldenc, 1);
	if ((newenc ^ oldenc) != 0)						//something changed ?
	{
		DBG1(0x95, newenc, 1);
		c = 1;
		oldenc = oldenc << 1;						//shift left 1 bit
		oldenc = oldenc & 0b00000011;				//use only bits 0 and 1
		if ((newenc ^ oldenc) > 1)					//CCW rotation
		{
			if (Dets < 3)							//special treatment for 4 or more pulses within timer delay
			{
				enc[n] = (enc[n] | 0b01000000);
				Dets++;
			}
			else
			{
				enc[n] = (enc[n] | 0b00010000);
				Dets = 0;
			}
		}
		else										//CW rotation
		{
			if (Dets < 3)
			{
				enc[n] = (enc[n] | 0b10000000);
				Dets++;
			}
			else
			{
				enc[n] = (enc[n] | 0b00100000);
				Dets = 0;
			}
		}//end if ((read ^ oldenc[i]) > 1)
		oldenc = newenc << 2;						//renew oldenc with newenc
		enc[n] = enc[n] & 0b11110011;				//clear oldenc bits 2,3 within enc[i]
		enc[n] = (enc[n] | oldenc);					//save "new" oldenc within enc[i]
	}//end if (newenc ^ oldenc) != 0
}//end ReadEncoder()


//read analog inputs
//different handling of 8bit and 10bit ADC
void ReadAnalog(void)
{
	ADMUX = 0x60 | (n & 0x0f);						//AVCC, left justified
	ADCSRA = 0b11000111;							//Enable ADC, start conversion, prescaler 128
	while (ADCSRA & (1<<ADSC));						//wait until ADSC bit is reset by CPU after conversion is complete
	if (n < 6)										//8bit conversion
	{												//conversion including init takes 25 cycles instead of 13 cycles
		pot[n] = (ADCH);	
	}
	else											//10bit conversion
	{
		pot[n+8] = ADCL;							//requires 2 byte in report
		//pot[i+8] = (pot[i+8] ^ 0b10000000);		//value is signed, invert sign
		pot[n] = ADCH;								//read low byte first
	}
}

void init_TC1(void)
{
	//TCCR1A = (1<<COM1A1) ;				//clear OC1A pin on match
	TCCR1B |= (1 << WGM12);					//normal mode
	TCCR1B |= (1 << CS10)|(1 << CS12);		//1024 prescaler
	OCR1A = 0x300;							//65ms@12MHz, interval time = (1/clk)*prescaler*OCR1A
	//TIMSK1 |= (1 << OCIE1A);				//enable interrupt
}
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
	to avoid ghost keys, insert diodes per switch, pointing from row to col
	//
	//ReadButtons function seems superfluent
	//
	//encoder pins should be read in one cycle, so connect in one column
	//
*/

#define F_CPU	12000000UL
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>  /* for sei() */
#include <util/delay.h>     /* for _delay_ms() */

#include <avr/pgmspace.h>   /* required by usbdrv.h */
#include "usbdrv.h"
#include "oddebug.h"        /* This is also an example for using debug macros */

/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

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
static uchar    idleRate;   /* repeat rate for keyboards, never used for mice */

uchar btn[32];					//buttons
int n = 0;
int c = 1;
uchar buttons[4];

void ReadButton(void);
void ButtonMatrix();

/* ------------------------------------------------------------------------- */

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;

    /* The following requests are never used. But since they are required by
     * the specification, we implement them in this example.
     */
    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */
        DBG1(0x50, &rq->bRequest, 1);   /* debug output: print our request */
        if(rq->bRequest == USBRQ_HID_GET_REPORT){  /* wValue: ReportType (highbyte), ReportID (lowbyte) */
            /* we only have one report type, so don't look at wValue */
            usbMsgPtr = (void *)&reportBuffer;
            return sizeof(reportBuffer);
        }else if(rq->bRequest == USBRQ_HID_GET_IDLE){


            usbMsgPtr = &idleRate;
            return 1;
        }else if(rq->bRequest == USBRQ_HID_SET_IDLE){
            idleRate = rq->wValue.bytes[1];
        }
    }else{
        /* no vendor specific requests implemented */
    }
    return 0;   /* default for not implemented requests: return no data back to host */
}

/* ------------------------------------------------------------------------- */

int __attribute__((noreturn)) main(void)
{
	uchar   i;

DDRB = 0x00;

    wdt_enable(WDTO_1S);
    /* Even if you don't use the watchdog, turn it off here. On newer devices,
     * the status of the watchdog (on/off, period) is PRESERVED OVER RESET!
     */
    /* RESET status: all port bits are inputs without pull-up.
     * That's the way we need D+ and D-. Therefore we don't need any
     * additional hardware initialization.
     */
    odDebugInit();
    DBG1(0x00, 0, 0);       /* debug output: main starts */
    usbInit();
    usbDeviceDisconnect();  /* enforce re-enumeration, do this while interrupts are disabled! */
    i = 0;
    while(--i){             /* fake USB disconnect for > 250 ms */
        wdt_reset();
        _delay_ms(1);
    }
    usbDeviceConnect();
    sei();

    DBG1(0x01, 0, 0);       /* debug output: main loop starts */
    for(;;)					//main event loop
	{                
		ButtonMatrix();

		//Encoders
		/*
		define ENC_NUM	6			//number of encoders, x2 for pin count
		
		*/

		//Process buttons
		//btn[1] = (btn[1] & 0b00000100) | (PINB & (1<<PINB7));
		//customize n for number of buttons												// <----- change here
		n = 0;
		while (n < 0){
			ReadButton();
			n++;
		}//end while
		
//	    DBG1(0x02, 0, 0);   /* debug output: main loop iterates */
        wdt_reset();
        usbPoll();
        if(usbInterruptIsReady()){
		    /* called after every poll of the interrupt endpoint */
			//Gamepad buttons displayed inverted in Windows: LSB left, MSB right
			reportBuffer.a1 = 0x00 | (PINB & (1<<PINB7));
			reportBuffer.a2 = 0x00 | buttons[0];
			reportBuffer.a3 = 0x00 | buttons[1];
			reportBuffer.a4 = 0x00 | buttons[2];
			 
 //           DBG1(0x03, 0, 0);   /* debug output: interrupt report prepared */
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

void ButtonMatrix()
{
	uchar i = 0;

	//PB0-2 row input
	//PC0-2 column output
	DDRC &= 0b11111000;				//set columns input HiZ
	PORTC &= 0b11111000;			//prepare columns for output low
	DDRB &= 0b11111000;				//set rows input
	PORTB |= 0b00000111;			//set rows pullups on

	for (i=0;i<4;i++){
		DDRC &= 0b11111000;				//(re-)set columns input HiZ
		DDRC |= 1<<i;		 			//set col k output, already set low
		_delay_ms(0.2);					//wait to settle & debounce
		buttons[i] = ~PINB;				//read, negate and save row
	}									//next col
}
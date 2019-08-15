/* Name: main.c
 * Project: g1000_ctr, based on V-USB example
 * Author: gfe
 */


/*

gfe

	based on g1000_left
	310119: 
	DBG output commented, invokes 1ms delay per line
	every 19 full sequences (18ms) pause in read for 13ms, presumably USB send
	delays with _delay_ms function work ok, _delay_us only to certain maximum

	140419:
	corrected some errors -> test switches again
---

		
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
    0x95, 0x10,                    //     REPORT_COUNT (16)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0xC0                          // END COLLECTION
};

typedef struct{
    uchar	a1;
    uchar   a2;
}report_t;
static report_t reportBuffer;
static uchar    idleRate;		/* repeat rate for keyboards, never used for mice */

uchar btn[32];					//buttons
int n = 0;

void init_TC1(void);

/* ------------------------------------------------------------------------- */

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;
    /* The following requests are never used. But since they are required by
     * the specification, we implement them in this example.    */
    if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS)
	{    
		/* class request type */
        DBG1(0x50, &rq->bRequest, 1);   /* debug output: print our request */
        if (rq->bRequest == USBRQ_HID_GET_REPORT)
		{  
			/* wValue: ReportType (highbyte), ReportID (lowbyte) */
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
				// no vendor specific requests implemented 
    }
    return 0;   // default for not implemented requests: return no data back to host 
}

/* ------------------------------------------------------------------------- */

int __attribute__((noreturn)) main(void)
{

	uchar   i;

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

		DDRB &= 0b00000000;								//all ports input
		DDRC &= 0b00000000;
		DDRD &= 0b00000000;								//USB ports handle DDR on their own
		PORTB |= 0b11111111;
		PORTC |= 0b11111111;
		PORTD |= 0b11111001;							//no pull-ups on USB ports PD2, PD1


		//Buttons displayed inverted in Windows GUI: LSB left, MSB right
		reportBuffer.a1 =	((1<<PINB1) & ~(PINB & (1<<PINB1))) >>1		//S1
							| ((1<<PINB0) & ~(PINB & (1<<PINB0))) <<1	//S2
							| ((1<<PIND7) & ~(PIND & (1<<PIND7))) >>5	//S3
							| ((1<<PIND6) & ~(PIND & (1<<PIND6))) >>3	//S6
							| ((1<<PIND5) & ~(PIND & (1<<PIND5))) >>1	//S5
							| ((1<<PIND0) & ~(PIND & (1<<PIND0))) <<5	//S4
							| ((1<<PINC5) & ~(PINC & (1<<PINC5))) <<1	//S9
							| ((1<<PINC4) & ~(PINC & (1<<PINC4))) <<3;	//S8

		reportBuffer.a2 =	((1<<PINC3) & ~(PINC & (1<<PINC3))) >>3		//S7
							| ((1<<PINC2) & ~(PINC & (1<<PINC2))) >>1	//S10
							| ((1<<PINC1) & ~(PINC & (1<<PINC1))) <<1	//S11
							| ((1<<PINC0) & ~(PINC & (1<<PINC0))) <<3	//S12
							& 0b00001111;

		_delay_ms(1);							//for debouncing

		//control pulse length for encoders
		//check for time elapsed, if so, clear buffers carrying encoder
		if (TIFR & (1<<OCF1A))						
		{
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


void init_TC1(void)
{
	//TCCR1A = (1<<COM1A1) ;				//clear OC1A pin on match
	TCCR1B |= (1 << WGM12);					//normal mode
	TCCR1B |= (1 << CS10)|(1 << CS12);		//1024 prescaler
	OCR1A = 0x300;							//65ms@12MHz, interval time = (1/clk)*prescaler*OCR1A
	//TIMSK1 |= (1 << OCIE1A);				//enable interrupt
}
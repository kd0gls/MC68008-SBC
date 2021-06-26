#include <xc.h>

// for MC68008 SBC v1
// 04.22.18

#define	SWITCH_DEBOUNCE_TIME	25			// in milliseconds
#define	CPU_RESET_TIME_MIN		250			// in milliseconds

//								Vdd			// pin 1
#define	RESET_SWITCH_IN			(!RA5)		// pin 2, low active, pull-up
#define HALT_LED_OUT			LATA4		// pin 3, high active
//											// pin 4
#define	CPU_HALT_IO				RA2			// pin 5, low active, open drain, pull-up
#define	CPU_RESET_IO			RA1			// pin 6, low active, open drain, pull-up
#define PLD_RESET_OUT			LATA0		// pin 7, low active
//								Vss			// pin 8

static struct
{
	enum
	{
		StateReleased,
		StatePress,
		StatePressed,
		StateRelease
	} state;
	unsigned short timer;
} phy_switch, log_switch;

void main( void )
{
	OSCCON		= 0b01011000;		// internal oscillator to 1 MHz
	ANSELA		= 0b00000000;		// all pins to digital
	PORTA		= 0b00000000;		// assert outputs
	TRISA		= 0b00100000;		// RA5 to input, all others to outputs
	nWPUEN		= 0;				// enable weak pull-ups
	WPUA		= 0b00100110;		// enable weak pull-ups on switch input, /HALT and /RESET "inputs"

	T2CON	= 0b00000100;			// timer on, no prescaler, no postscaler
	PR2		= 249;					// 1 MHz (Fosc) / 4 (timer clock) / 250 (PR + 1) = 1 KHz

	// initialize variables
	phy_switch.state = log_switch.state = StateReleased;
	log_switch.timer = CPU_RESET_TIME_MIN;

	for( ;; )
	{
		// wait for next sampling interval
		for( TMR2IF = 0; TMR2IF == 0; )
			;

		if( log_switch.timer != 0 )
			log_switch.timer--;

		// switch physical state (debounce) sequencer
		switch( phy_switch.state )
		{
			case StateReleased:
				if( RESET_SWITCH_IN )
				{
					phy_switch.timer = 0;
					phy_switch.state = StatePress;
				}

				break;

			case StatePress:
				if( !RESET_SWITCH_IN )
					phy_switch.state = StateReleased;

				else if( ++phy_switch.timer == SWITCH_DEBOUNCE_TIME )
					phy_switch.state = StatePressed;

				break;

			case StatePressed:
				if( !RESET_SWITCH_IN )
				{
					phy_switch.timer = 0;
					phy_switch.state = StateRelease;
				}

				break;

			case StateRelease:
				if( RESET_SWITCH_IN )
					phy_switch.state = StatePressed;

				else if( ++phy_switch.timer == SWITCH_DEBOUNCE_TIME )
					phy_switch.state = StateReleased;

				break;
		}
		
		// switch logical state sequencer
		switch( log_switch.state )
		{
			case StateReleased:
				if( phy_switch.state >= StatePressed )
				{
					log_switch.timer = CPU_RESET_TIME_MIN;
					log_switch.state = StatePressed;
				}
				
				break;
				
			case StatePressed:
				if( phy_switch.state < StatePressed )
					log_switch.state = StateReleased;
				
				break;
		}

		// update reset outputs based on logical switch state
		// assert reset for CPU_RESET_TIME_MIN or as long as user holds button, whichever is greater
		if( log_switch.state == StatePressed || log_switch.timer != 0 )
		{
			TRISA &= ~0b00000110;		// assert /HALT and /RESET
			PLD_RESET_OUT = 0;			// assert /PLD_RESET_OUT
		}
		else
		{
			PLD_RESET_OUT = 1;			// negate /PLD_RESET_OUT
			TRISA |= 0b00000110;		// negate /HALT and /RESET (open drains with pull-ups)
		}
		
		// update halt LED
		HALT_LED_OUT = !CPU_HALT_IO;
	}
}

//******************************************************************
// Name of Group Members: Mathurtion Rajendrackumaar, Vasanthavel Jeeva Kumararaja
// Creation Date: 4/3/26
// Lab this program is associated with: ECE132 Project 1
// Lab due date: 4/12/26
//
// Hardware Inputs used:
//      PF4 - SW1 (Left button  - activates speaker mode)
//      PF0 - SW2 (Right button - activates music mode)
//      PE5 - IR sensor (active low, detects speaker on stage)
//
// Hardware Outputs used:
//      PE1 - House LED     (L1) - on in HOUSE mode
//      PE2 - Spotlight LED (L2) - on in SPOTLIGHT state / Music LED 1 used in music animation
//      PE3 - Music LED 2   (L3) - used in music animation
//      PE4 - Music LED 3   (L4) - used in music animation
//
// Additional files needed: driverlib, inc
//
// Date of last modification: 4/11/26
//
//*************************************************************

#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_gpio.h"
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "inc/tm4c123gh6pm.h"
#include "driverlib/systick.h"
#include "driverlib/interrupt.h"

//******************************************************************
// Function prototypes
//******************************************************************
void portEF_input_setup(int input_p_E, int input_p_F);
void portEF_output_setup(int output_p_E, int output_p_F);
void button_interrupt(void);
void ir_sensor_interrupt(void);

//******************************************************************
// Output pin definitions (Port E)
//*****************************************************************
#define HOUSE_LED     0x02                           // PE1
#define SPOTLIGHT_LED 0x04                           // PE2
#define MUSIC_LED2    0x08                           // PE3
#define MUSIC_LED3    0x10                           // PE4
#define ALL_LEDS      0x1E                           // PE1-PE4

//******************************************************************
// State index definitions
// index :: Name  (Encode States)
//  0 :: OFF
//  1 :: HOUSE
//  2 :: SPEAKER
//  3 :: SPOTLIGHT
//  4 :: MUSIC1
//  5 :: MUSIC2
//  6 :: MUSIC3
//  7 :: MUSIC4
//  8 :: MUSIC5
//  9 :: MUSIC6
//******************************************************************
#define S_OFF      0
#define S_HOUSE    1
#define S_SPEAKER  2
#define S_SPOTLIT  3
#define S_MUSIC1   4
#define S_MUSIC2   5
#define S_MUSIC3   6
#define S_MUSIC4   7
#define S_MUSIC5   8
#define S_MUSIC6   9

//*******************************************************************
// Delay definitions (no function calls. plain constants)
// SysCtlDelay at 16MHz means that time will = n * 3 / 16,000,000 Hz
//   so T    = 1333333 = ~250ms
//   and T2   = 2666667 = ~500ms
//   also T25  =   53333 = ~10ms
//*****************************************************************
#define T   1333333
#define T2  2666667
#define T25   53333

//************************************************************
// Step 3 - Creating the Struct
//
// struct state {
//     unsigned char out;       <- LED output
//     unsigned long wait;      <- delay value T
//     unsigned char next[4];   <- next state for each input
// };
//
// Input encoding for next[]:
//   next[0] = in=00 (no buttons pressed)
//   next[1] = in=01 (SW2 right button - music)
//   next[2] = in=10 (SW1 left button  - speaker)
//   next[3] = in=11 (both buttons     - off/house)
//
// the IR sensor transitions are handled separately in ir_sensor_interrupt
// and the reason because IR is not a button input
//*****************************************************
struct state {
    unsigned char out;
    unsigned long wait;
    unsigned char next[4];
};
typedef struct state stype;

//****************************************************************
// FSM table
// it is following the format of: stype fsm[N] = {
//     {out, T, {next[0], next[1], next[2], next[3]}},
// };
//***************************************************************
stype fsm[10] = {
//   out                                  wait  {in=00      in=01       in=10       in=11   }
    {0x00,                                T25,  {S_OFF,     S_OFF,      S_OFF,      S_HOUSE }}, // 0: OFF
    {HOUSE_LED,                           T2,   {S_HOUSE,   S_MUSIC1,   S_SPEAKER,  S_OFF   }}, // 1: HOUSE
    {0x00,                                T25,  {S_SPEAKER, S_SPEAKER,  S_HOUSE,    S_OFF   }}, // 2: SPEAKER
    {SPOTLIGHT_LED,                       T25,  {S_SPOTLIT, S_SPOTLIT,  S_HOUSE,    S_OFF   }}, // 3: SPOTLIGHT
    {SPOTLIGHT_LED,                       T,    {S_MUSIC2,  S_HOUSE,    S_MUSIC1,   S_OFF   }}, // 4: MUSIC1
    {SPOTLIGHT_LED|MUSIC_LED2,            T,    {S_MUSIC3,  S_HOUSE,    S_MUSIC2,   S_OFF   }}, // 5: MUSIC2
    {SPOTLIGHT_LED|MUSIC_LED2|MUSIC_LED3, T,    {S_MUSIC4,  S_HOUSE,    S_MUSIC3,   S_OFF   }}, // 6: MUSIC3
    {MUSIC_LED2|MUSIC_LED3,               T,    {S_MUSIC5,  S_HOUSE,    S_MUSIC4,   S_OFF   }}, // 7: MUSIC4
    {MUSIC_LED3,                          T,    {S_MUSIC6,  S_HOUSE,    S_MUSIC5,   S_OFF   }}, // 8: MUSIC5
    {0x00,                                T,    {S_MUSIC1,  S_HOUSE,    S_MUSIC6,   S_OFF   }}, // 9: MUSIC6
};

//*************************************************************
// Step 4 - Variables
// Spot for the input and spot for the current state
//*****************************************************************
volatile unsigned char cstate = S_OFF;  // current state
volatile unsigned char input  = 0;      // current input (0-3)
volatile unsigned char fresh  = 0;      // 1 = just entered music from interrupt, skip auto-advance once

//**************************************************************
// Main
//******************************************************************
int main(void)
{
    // Step 5 - Define connections
    portEF_input_setup(0x20, 0x11);   // PE5=IR, PF4=SW1, PF0=SW2
    portEF_output_setup(0x1E, 0x00);  // PE1-PE4=LEDs

    // Button interrupt - falling edge only (press, not release)
    GPIOIntTypeSet(GPIO_PORTF_BASE, GPIO_PIN_4 | GPIO_PIN_0, GPIO_FALLING_EDGE);
    GPIOIntRegister(GPIO_PORTF_BASE, button_interrupt);
    GPIOIntEnable(GPIO_PORTF_BASE, GPIO_PIN_4 | GPIO_PIN_0);

    // IR sensor interrupt - both edges
    GPIOIntTypeSet(GPIO_PORTE_BASE, GPIO_PIN_5, GPIO_BOTH_EDGES);
    GPIOIntRegister(GPIO_PORTE_BASE, ir_sensor_interrupt);
    GPIOIntEnable(GPIO_PORTE_BASE, GPIO_PIN_5);

    IntMasterEnable();

    // All LEDs off at start
    GPIO_PORTE_DATA_R &= ~ALL_LEDS;

    // Step 6 - Process
    while(1)
    {
        // 1. Update output on present state
        GPIO_PORTE_DATA_R = (GPIO_PORTE_DATA_R & ~ALL_LEDS) | fsm[cstate].out;

        // 2. Wait
        SysCtlDelay(fsm[cstate].wait);

        // 3. Update input
        // input is set by button_interrupt when button pressed (1, 2, or 3)
        // defaults to 0 (in=00) when no button pressed
        // in=00 drives music auto-advance via next[0] in FSM table
        // fresh flag prevents skipping MUSIC1 on first entry

        // 4. Update state
        // cstate = fsm[cstate].next[input]
        if (!fresh)
        {
            cstate = fsm[cstate].next[input];
            input = 0; // clear after use so it does not repeat
        }
        else
        {
            fresh = 0; // first cycle complete, allow normal advance next time
        }
    }

    return 0;
}

//****************************************************************
// button_interrupt
// Fires on falling edge (button press only)
// Debounce delay added so both buttons have time to settle
//*****************************************************************
void button_interrupt(void)
{
    // Debounce delay ~15ms - gives second button time to close
    SysCtlDelay(80000);

    // Re-read both buttons after debounce
    // Active low: pressed = 0
    int sw1 = (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4) == 0); // left  SW1
    int sw2 = (GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_0) == 0); // right SW2

    // Both buttons (in=11) - OFF <-> HOUSE
    if (sw1 && sw2)
    {
        if (cstate == S_OFF)
            cstate = S_HOUSE;
        else
            cstate = S_OFF;
    }

    // Left button SW1 only (in=10) - speaker
    else if (sw1 && !sw2)
    {
        if (cstate == S_HOUSE)
            cstate = S_SPEAKER;
        else if (cstate == S_SPEAKER || cstate == S_SPOTLIT)
            cstate = S_HOUSE;
    }

    // Right button SW2 only (in=01) - music
    else if (sw2 && !sw1)
    {
        if (cstate == S_HOUSE)
        {
            cstate = S_MUSIC1;
            fresh = 1; // mark fresh so first music state is not skipped
        }
        else if (cstate >= S_MUSIC1 && cstate <= S_MUSIC6)
            cstate = S_HOUSE;
    }

    GPIOIntClear(GPIO_PORTF_BASE, GPIO_PIN_4 | GPIO_PIN_0);
}

//****************************************************************
// ir_sensor_interrupt
// Fires on both edges of PE5
// Debounce added to prevent false triggers from IR signal bouncing
// Active low: LOW = object detected, HIGH = nothing detected
//****************************************************************
void ir_sensor_interrupt(void)
{
    // Debounce delay ~3ms for IR signal to settle
    SysCtlDelay(16000);

    // Re-read pin after debounce to confirm stable reading
    int ir = GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_5);

    // Object confirmed detected (pin still LOW)
    if (ir == 0 && cstate == S_SPEAKER)
        cstate = S_SPOTLIT;

    // Object confirmed gone (pin still HIGH)
    else if (ir != 0 && cstate == S_SPOTLIT)
        cstate = S_SPEAKER;

    GPIOIntClear(GPIO_PORTE_BASE, GPIO_PIN_5);
}

//******************************************************************
// portEF_input_setup
//******************************************************************
void portEF_input_setup(int input_p_E, int input_p_F)
{
    SYSCTL_RCGCGPIO_R |= 0x30;
    SysCtlDelay(1000);

    // Port E - IR sensor PE5
    GPIO_PORTE_LOCK_R  = 0x4C4F434B;
    GPIO_PORTE_CR_R    = 0xFF;
    GPIO_PORTE_AMSEL_R = 0x00;
    GPIO_PORTE_PCTL_R  = 0x00;
    GPIO_PORTE_DIR_R  &= ~input_p_E;
    GPIO_PORTE_AFSEL_R = 0x00;
    GPIO_PORTE_PUR_R  |=  input_p_E;
    GPIO_PORTE_DEN_R  |=  input_p_E;

    // Port F - SW1 PF4 SW2 PF0
    GPIO_PORTF_LOCK_R  = 0x4C4F434B;
    GPIO_PORTF_CR_R    = 0xFF;
    GPIO_PORTF_AMSEL_R = 0x00;
    GPIO_PORTF_PCTL_R  = 0x00;
    GPIO_PORTF_DIR_R  &= ~input_p_F;
    GPIO_PORTF_AFSEL_R = 0x00;
    GPIO_PORTF_PUR_R  |=  input_p_F;
    GPIO_PORTF_DEN_R  |=  input_p_F;
}

//*************************************************************************
// portEF_output_setup
//*************************************************************************
void portEF_output_setup(int output_p_E, int output_p_F)
{
    GPIO_PORTE_DIR_R  |=  output_p_E;
    GPIO_PORTE_AFSEL_R = 0x00;
    GPIO_PORTE_DEN_R  |=  output_p_E;
    GPIO_PORTE_DATA_R &= ~ALL_LEDS;

    GPIO_PORTF_DIR_R  |=  output_p_F;
    GPIO_PORTF_DEN_R  |=  output_p_F;
}




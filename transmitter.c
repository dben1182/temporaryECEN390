#include "transmitter.h"
#include "mio.h"
#include "filter.h"
#include <stdio.h>
#include "buttons.h"
#include "switches.h"
#include "utils.h"

//macros for the output pin and for low and high values
#define TRANSMITTER_OUTPUT_PIN 13
#define TRANSMITTER_HIGH_VALUE 1
#define TRANSMITTER_LOW_VALUE 0
#define INACTIVE_OUTPUT_STATE TRANSMITTER_LOW_VALUE
//initial value for counters
#define COUNTER_INITIAL_VALUE 0
//delay used in the non continuous testing mode
#define DELAY_FOR_NON_CONTINUOUS_MODE 400

//there are 20000 ticks or cycles that occur in 200 milliseconds
//this macro is used to count the number of ticks for a complete waveform
#define FULL_WAVEFORM_TIME 20000
//we need to offset the max counter value by 1, since we start from 0
#define PULSE_COUNTER_OFFSET 1
//the number corresponding to the player 1 frequency
#define PLAYER_1_FREQUENCY_NUMBER 0
//we have to divide the numbers given in the filter.h file by 2
//because we have to alternate every half period
#define PULSE_COUNTER_DIVISOR 2
//macros used for testing
#define TRANSMITTER_TEST_TICK_PERIOD_IN_MS 1
#define BOUNCE_DELAY 5

//stores the current value from 0 to 9 representing player frequencies
//from 1 to 10 respectively. This is the variable that will store
//the number that is currently being used in the transmit state
volatile uint8_t currentFrequencyNumber;
//stores the next value that will be used next for the player frequency
//after we have finished the current 200 millisecond waveform. We then
//update the currentFrequencyNumber to this nextFrequencyNumber when
//we start the next 200 millisecond waveform
volatile uint8_t nextFrequencyNumber;


//variable that stores the current state of the output pin
volatile uint8_t currentPinState;

 
//bool flag that indicates whether or not the transmitter is currently running
volatile bool transmitterRunning;

//bool flag that indicates whether or not the tranmitter is in continuous mode or not
volatile bool inContinuousMode;

//counter used to determine if enough ticks have passed to change waveform between
//high and low. This is the shorter counter for a much shorter time
volatile uint32_t pulseTimeCounter;

//counter used to time the full length, 200 milliseconds, of each waveform output
volatile uint32_t fullWaveformCounter;



//enumeration used to control the current state of the state machine
//declares the variable currentState
static enum transmitter_st_t
{
    init_st,
    idle_st,
    transmitting_st
} currentState;




//helper function that turns on or off the transmitter output pin
//if pinState is 1, the mio will be written as 1, which is on,
//if pinState is 0, the mio will be written as 0, which is off.
void writeOutputPin(uint8_t pinState);

//helper function that inverts the current pin state of the output pin
void invertOutputPin();

//helper function to reset pulseTimeCounter
void resetPulseTimeCounter();

//increments pulseTimeCounter
void incrementPulseTimeCounter();

//gets current value of pulseTimeCounter
uint32_t getPulseTimeCounter();

//returns whether pulse time Counter is done
bool pulseTimeCounterIsDone();

//helper function to reset fullWaveformCounter
void resetFullWaveformCounter();

//increments FullWaveformCounter
void incrementFullWaveformCounter();

//gets current value of FullWaveformCounter
uint32_t getFullWaveformCounter();

//returns whether FullWaveformCounter is done
bool fullWaveformCounterIsDone();

//updates the currentFrequencyNumber to be the
//nextFrequencyNumber, whatever that may be
//if the nextFrequencyNumber has not changed, then
//the same value gets assigned
void updateCurrentFrequencyNumber();

//disables run flag and stops the transmitter
void transmitterStop();

// The transmitter state machine generates a square wave output at the chosen
// frequency as set by transmitter_setFrequencyNumber(). The step counts for the
// frequencies are provided in filter.h

// Standard init function.
void transmitter_init()
{
    mio_init(false);
    mio_setPinAsOutput(TRANSMITTER_OUTPUT_PIN);
    transmitterStop();
    transmitter_setContinuousMode(false);
    currentState = init_st;
    resetFullWaveformCounter();
    resetPulseTimeCounter();
    //sets next frequency number to Player 1 Frequency
    transmitter_setFrequencyNumber(PLAYER_1_FREQUENCY_NUMBER);
    //updates current frequency number to next frequency number
    //which is player 1's frequency number
    updateCurrentFrequencyNumber();
}

// Starts the transmitter.
void transmitter_run()
{
    transmitterRunning = true;
}

// Returns true if the transmitter is still running.
bool transmitter_running()
{  
    return transmitterRunning;
}

// Sets the frequency number. If this function is called while the
// transmitter is running, the frequency will not be updated until the
// transmitter stops and transmitter_run() is called again.
void transmitter_setFrequencyNumber(uint16_t frequencyNumber)
{
    //here we set the NEXT frequency number to the input that is called
    //from outside this scope. nextFrequencyNumber is a dummy variable
    //just used to temporarily store the frequency that has come in the middle
    //of our 200 millisecond waveform
    nextFrequencyNumber = frequencyNumber; 
}

// Returns the current frequency setting.
uint16_t transmitter_getFrequencyNumber()
{
    return currentFrequencyNumber;
}

// Standard tick function.
void transmitter_tick()
{
    //transmitter transition switch statement, and mealy actions
    switch(currentState)
    {
        //init state, immediately transitions out of this state
        //into the idle state
        case init_st:
            currentState = idle_st;
            break;
        //we stay in idle state until the transmitter is activated
        //by another function outside this state machine
        case idle_st:
            //case the transmitter is running, and we transition
            //out of this state into the transmitting state
            if(transmitter_running())
            {
                currentState = transmitting_st;
                //updates the currentFrequencyNumber
                updateCurrentFrequencyNumber();
                resetPulseTimeCounter();
                resetFullWaveformCounter();
                writeOutputPin(TRANSMITTER_LOW_VALUE);
            }
            //else, the transmitter is not running, and we transition
            //back to this same state
            else
            {
                currentState = idle_st;
            }
            break;
        //transmitting state, where most of the action occurs. The majority of 
        //things that are done are done here in the transmissions from the 
        //transmitting state
        case transmitting_st:
            //case fullWaveformCounterIsDone, and we are NOT in continuousMode,
            //we go back to idle state. Deactivates this state machine by calling
            //transmitter stop function
            if(fullWaveformCounterIsDone() && !inContinuousMode)
            {
                currentState = idle_st;
                transmitterStop();
                resetFullWaveformCounter();
                resetPulseTimeCounter();
                //deactivates output pin
                writeOutputPin(INACTIVE_OUTPUT_STATE);
            }
            //case fullWaveformCounterIsDone and we are in continuousMode
            //we go again to transmitting_st, but we also update the
            //currentFrequencyNumber 
            else if(fullWaveformCounterIsDone() && inContinuousMode)
            {
                currentState = transmitting_st;
                updateCurrentFrequencyNumber();
                writeOutputPin(TRANSMITTER_LOW_VALUE);
                resetFullWaveformCounter();
                resetPulseTimeCounter();
            }
            //case the full waveform counter is not done, but the pulse Timer
            //counter is done, which indicates that we need to invert the 
            //value of the pin output to create the waveform
            //transitions back to this same state
            //increments the fullWaveformCounter
            //resets the pulseTimerCounter
            else if(!fullWaveformCounterIsDone() && pulseTimeCounterIsDone())
            {
                currentState = transmitting_st;
                //inverts the value of the output pin
                invertOutputPin();
                incrementFullWaveformCounter();
                resetPulseTimeCounter();
            }
            //case the full waveform counter is not done,
            //and the pulse timer counter is not done either.
            //we loop back to this same state, and we increment
            //both counters
            else if(!fullWaveformCounterIsDone() && !pulseTimeCounterIsDone())
            {
                currentState = transmitting_st;
                incrementFullWaveformCounter();
                incrementPulseTimeCounter();
            }
            break;
        //no default mealy actions or transitions
        default:
            break;
    }
    //transmitter state action switch statement, moore actions
    switch(currentState)
    {
        //no moore actions in init_st
        case init_st:
            break;
        //no moore actions in idle_st
        case idle_st:
            break;
        //no moore actions in transmitting_st
        case transmitting_st:
            break;
        //no default moore actions
        default:
            break;
    }
}


// Tests the transmitter.
void transmitter_runTest()
{
    /*printf("starting transmitter_runTest()\n");
    buttons_init();                                         // Using buttons
    switches_init();                                        // and switches.
    transmitter_init();
    transmitter_setContinuousMode(true);                                     // init the transmitter.
    uint16_t switchValue = switches_read() % FILTER_FREQUENCY_COUNT;
    transmitter_setFrequencyNumber(switchValue);
    transmitter_run();
    //while the transmitter is running, we will 
    while(transmitter_running())
    {
        if(fullWaveformCounterIsDone())
        {
            uint16_t switchValue = switches_read() % FILTER_FREQUENCY_COUNT;
            transmitter_setFrequencyNumber(switchValue);
        }
        transmitter_tick();
        utils_msDelay(TRANSMITTER_TEST_TICK_PERIOD_IN_MS);
    }
    printf("test_completed\n");

    while (!(buttons_read() & BUTTONS_BTN1_MASK)) 
    {         // Run continuously until BTN1 is pressed.
        uint16_t switchValue = switches_read() % FILTER_FREQUENCY_COUNT;  // Compute a safe number from the switches.
        transmitter_setFrequencyNumber(switchValue);          // set the frequency number based upon switch value.
        transmitter_run();                                    // Start the transmitter.
        while (transmitter_running()) 
        {                       // Keep ticking until it is done.
            transmitter_tick();                                 // tick.
            utils_msDelay(TRANSMITTER_TEST_TICK_PERIOD_IN_MS);  // short delay between ticks.
        }
        printf("completed one test period.\n");
    
    do 
    {
        utils_msDelay(BOUNCE_DELAY);
    } while (buttons_read());
    printf("exiting transmitter_runTest()\n");*/
}

// Runs the transmitter continuously.
// if continuousModeFlag == true, transmitter runs continuously, otherwise,
// transmits one pulse-width and stops. To set continuous mode, you must invoke
// this function prior to calling transmitter_run(). If the transmitter is in
// currently in continuous mode, it will stop running if this function is
// invoked with continuousModeFlag == false. It can stop immediately or wait
// until the last 200 ms pulse is complete. NOTE: while running continuously,
// the transmitter will change frequencies at the end of each 200 ms pulse.
void transmitter_setContinuousMode(bool continuousModeFlag)
{
    inContinuousMode = continuousModeFlag;
}

// Tests the transmitter in non-continuous mode.
// The test runs until BTN1 is pressed.
// To perform the test, connect the oscilloscope probe
// to the transmitter and ground probes on the development board
// prior to running this test. You should see about a 300 ms dead
// spot between 200 ms pulses.
// Should change frequency in response to the slide switches.
void transmitter_runNoncontinuousTest()
{
    //sets continuous mode to false
    transmitter_setContinuousMode(false);
    uint16_t switchValue = switches_read() % FILTER_FREQUENCY_COUNT;
    //sets frequency number to whatever the switches are in
    transmitter_setFrequencyNumber(switchValue);
    //runs in a forever loop, where the tick function is constantly
    //being called by the isr function every 10 microseconds
    //then we delay 400 milliseconds to prove that we are producing
    //a 200 millisecond waveform.
    while(true)
    {
        utils_msDelay(DELAY_FOR_NON_CONTINUOUS_MODE);
        transmitter_run();
    }

}

// Tests the transmitter in continuous mode.
// To perform the test, connect the oscilloscope probe
// to the transmitter and ground probes on the development board
// prior to running this test.
// Transmitter should continuously generate the proper waveform
// at the transmitter-probe pin and change frequencies
// in response to changes to the changes in the slide switches.
// Test runs until BTN1 is pressed.
void transmitter_runContinuousTest()
{
    //sets mode to continuous
    transmitter_setContinuousMode(true);
    //starts the running of the state machine,
    //which since it is in continuous mode, need not
    //have a repeat call of the transmitter run function
    transmitter_run();
    uint16_t switchValue = PLAYER_1_FREQUENCY_NUMBER;
    //forever loop that just keeps checking the switches and
    //their respective frequencies and then writes that down
    while(true)
    {
        switchValue = switches_read() % FILTER_FREQUENCY_COUNT;
        transmitter_setFrequencyNumber(switchValue);
    }
}


//helper function that turns on or off the transmitter output pin
//if pinState is 1, the mio will be written as 1, which is on,
//if pinState is 0, the mio will be written as 0, which is off.
void writeOutputPin(uint8_t pinState)
{
    currentPinState = pinState;
    mio_writePin(TRANSMITTER_OUTPUT_PIN, currentPinState);
}

//helper function that inverts the current pin state of the output pin
void invertOutputPin()
{
    //case the currentPinState is in high, sets it to low
    if(currentPinState == TRANSMITTER_HIGH_VALUE)
    {
        currentPinState = TRANSMITTER_LOW_VALUE;
    }
    //case the currentPinState is in low, sets it to high
    else
    {
        currentPinState = TRANSMITTER_HIGH_VALUE;
    }
    //writes the value of currentPinState to the output pin
    mio_writePin(TRANSMITTER_OUTPUT_PIN, currentPinState);
}



//helper function to reset pulseTimeCounter
void resetPulseTimeCounter()
{
    pulseTimeCounter = COUNTER_INITIAL_VALUE;
}

//increments pulseTimeCounter
void incrementPulseTimeCounter()
{
    pulseTimeCounter++;
}

//gets current value of pulseTimeCounter
uint32_t getPulseTimeCounter()
{
    return pulseTimeCounter;
}

//returns whether pulse time Counter is done
// the counter is done when it has reached half of whatever
//corresponding number is in the frequencyTickTable. This
//means that this counter is done when half of a cycle is
//completed. That way we can invert the counter to create
//the waveform effect
bool pulseTimeCounterIsDone()
{
    return (pulseTimeCounter >= ((filter_frequencyTickTable[currentFrequencyNumber])/PULSE_COUNTER_DIVISOR - PULSE_COUNTER_OFFSET));
}

//helper function to reset fullWaveformCounter
void resetFullWaveformCounter()
{
    fullWaveformCounter = COUNTER_INITIAL_VALUE;
}

//increments FullWaveformCounter
void incrementFullWaveformCounter()
{
    fullWaveformCounter++;
}

//gets current value of FullWaveformCounter
uint32_t getFullWaveformCounter()
{
    return fullWaveformCounter;
}

//returns whether FullWaveformCounter is done
//it does this by comparing the counter value with
//the Full_waveform_time macro
bool fullWaveformCounterIsDone()
{
    return (fullWaveformCounter >= FULL_WAVEFORM_TIME);
}

//updates the currentFrequencyNumber to be the
//nextFrequencyNumber, whatever that may be
//if the nextFrequencyNumber has not changed, then
//the same value gets assigned
void updateCurrentFrequencyNumber()
{
    currentFrequencyNumber = nextFrequencyNumber;
}


//disables run flag and stops the transmitter
void transmitterStop()
{
    transmitterRunning = false;
}
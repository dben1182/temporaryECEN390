#include "lockoutTimer.h"
#include "leds.h"
#include "intervalTimer.h"
#include <stdio.h>

#define LOCKOUT_TIMER_COUNTER_INITIAL_VALUE 0
#define LOCKOUT_TIMER_COUNTER_EXPIRE_OFFSET 1


volatile bool lockoutTimerRunning;

volatile uint32_t lockoutTimerCounter;




static enum lockoutTimer_st_t
{
    init_st,
    idle_st,
    counting_st
} currentState;


//resets lockoutTimerCounter to zero
void resetLockoutTimerCounter();


//increments lockoutTimerCounter by one
void incrementLockoutTimerCounter();

//returns whether lockoutTimerCounter is completed
bool lockoutTimerCounterIsDone();

//helper function to stop the lockout timer
void lockoutTimerStop();

// Calling this starts the timer.
void lockoutTimer_start()
{
    lockoutTimerRunning = true;
}


// Perform any necessary inits for the lockout timer.
void lockoutTimer_init()
{
    currentState = init_st;
    lockoutTimerRunning = false;
    resetLockoutTimerCounter();
}


// Returns true if the timer is running.
bool lockoutTimer_running()
{
    return lockoutTimerRunning;
}

// Standard tick function.
void lockoutTimer_tick()
{
    //lockout timer mealy switch statement
    switch(currentState)
    {
        //in init_st, we immediately transition
        //out of it to idle_st
        case init_st:
            currentState = idle_st;
            break;
        //idle state. We wait here until the timer
        //is activated
        case idle_st:
            //case the lockout timer is running,
            //or rather has been activated by another
            //function outside this system
            //we transition to the counting state
            if(lockoutTimer_running())
            {
                currentState = counting_st;
                resetLockoutTimerCounter();
            }
            //otherwise, we just keep transitioning
            //back to this same state
            else
            {
                currentState = idle_st;
            }
            break;
        //counting state where we stay here while we are counting
        case counting_st:
            //case the lockout counter is done, so we transition out of this
            //state and back to the idle state
            if(lockoutTimerCounterIsDone())
            {
                currentState = idle_st;
                lockoutTimerStop();
            }
            else
            {
                currentState = counting_st;
                incrementLockoutTimerCounter();
            }
            break;
        //no default transition actions
        default:
            break;
    }
}

// Test function assumes interrupts have been completely enabled and
// lockoutTimer_tick() function is invoked by isr_function().
// Prints out pass/fail status and other info to console.
// Returns true if passes, false otherwise.
// This test uses the interval timer to determine correct delay for
// the interval timer.
bool lockoutTimer_runTest()
{
    intervalTimer_init(INTERVAL_TIMER_TIMER_2);
    intervalTimer_reset(INTERVAL_TIMER_TIMER_2);
    intervalTimer_start(INTERVAL_TIMER_TIMER_2);
    lockoutTimer_start();
    while(lockoutTimer_running());
    intervalTimer_stop(INTERVAL_TIMER_TIMER_2);
    printf("interval Timer Value: %f\n", intervalTimer_getTotalDurationInSeconds(INTERVAL_TIMER_TIMER_2));
    printf("lockout Timer Counter Value: %d\n", lockoutTimerCounter);
}


//resets lockoutTimerCounter to zero
void resetLockoutTimerCounter()
{
    lockoutTimerCounter = LOCKOUT_TIMER_COUNTER_INITIAL_VALUE;
}


//increments lockoutTimerCounter by one
void incrementLockoutTimerCounter()
{
    lockoutTimerCounter++;
}

//returns whether lockoutTimerCounter is completed
bool lockoutTimerCounterIsDone()
{
    return (lockoutTimerCounter >= (LOCKOUT_TIMER_EXPIRE_VALUE - LOCKOUT_TIMER_COUNTER_EXPIRE_OFFSET));
}

//helper function to stop the lockout timer
void lockoutTimerStop()
{
    lockoutTimerRunning = false;
}

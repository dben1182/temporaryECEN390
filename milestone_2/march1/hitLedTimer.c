#include "hitLedTimer.h"
#include "mio.h"
#include "leds.h"
#include "utils.h"

#define LED_0_ON 0x0001
#define LED_0_OFF 0x0000

#define PIN_ON 1
#define PIN_OFF 0

#define HIT_LED_TIMER_COUNTER_INITIAL_VALUE 0
#define HIT_LED_TIMER_COUNTER_MAX_OFFSET 1

volatile bool hitLedTimerRunning;

volatile uint32_t hitLedTimerCounter;


static enum hitLedTimer_st_t
{
    init_st,
    idle_st,
    counting_st
} currentState;

//resets hitLedTimerCounter
void resetHitLedTimerCounter();

//increments hitLedTimerCounter
void incrementHitLedTimerCounter();

//returns whether led timer counter is done
bool hitLedTimerCounterIsDone();

//stops hitLedTimer
void hitLedTimerStop();


// Calling this starts the timer.
void hitLedTimer_start()
{
    hitLedTimerRunning = true;
}

// Returns true if the timer is currently running.
bool hitLedTimer_running()
{
    return hitLedTimerRunning;
}

// Standard tick function.
void hitLedTimer_tick()
{
    switch (currentState)
    {
        //init_st automatically transitions to idle state
        case init_st:
            currentState = idle_st;
            break;
        //idle state waits here until the timer is activated
        case idle_st:
            //case hitLedTimer has been set to run, we transition
            //to counting state.
            //also turn on leds in transition
            if(hitLedTimer_running())
            {
                currentState = counting_st;
                resetHitLedTimerCounter();
                mio_writePin(HIT_LED_TIMER_OUTPUT_PIN, PIN_ON);
                leds_write(LED_0_ON);
            }
            //otherwise, we transition back to this same idle state
            else
            {
                currentState = idle_st;
            }
            break;
        case counting_st:
            //case the timer is done, and we stop the counter and
            //transition back to idle state
            //turn leds off as well
            if(hitLedTimerCounterIsDone())
            {
                currentState = idle_st;
                hitLedTimerStop();
                mio_writePin(HIT_LED_TIMER_OUTPUT_PIN, PIN_OFF);
                leds_write(LED_0_OFF);
            }
            //otherwise, we stay in the counting state and increment
            //the counter accordingly
            else
            {
                currentState = counting_st;
                incrementHitLedTimerCounter();
            }
            break;
        default:
            break;
    }
}

// Need to init things.
void hitLedTimer_init()
{
    mio_init(false);
    mio_setPinAsOutput(HIT_LED_TIMER_OUTPUT_PIN);
    leds_init(false);
    resetHitLedTimerCounter();
}

// Turns the gun's hit-LED on.
void hitLedTimer_turnLedOn();

// Turns the gun's hit-LED off.
void hitLedTimer_turnLedOff();

// Disables the hitLedTimer.
void hitLedTimer_disable();

// Enables the hitLedTimer.
void hitLedTimer_enable();

// Runs a visual test of the hit LED.
// The test continuously blinks the hit-led on and off.
void hitLedTimer_runTest()
{
    while(true)
    {
        hitLedTimer_start();
        while(hitLedTimer_running());
        utils_msDelay(300);
    }
}



//resets hitLedTimerCounter
void resetHitLedTimerCounter()
{
    hitLedTimerCounter = HIT_LED_TIMER_COUNTER_INITIAL_VALUE;
}

//increments hitLedTimerCounter
void incrementHitLedTimerCounter()
{
    hitLedTimerCounter++;
}

//returns whether led timer counter is done
bool hitLedTimerCounterIsDone()
{
    return (hitLedTimerCounter >= (HIT_LED_TIMER_EXPIRE_VALUE - HIT_LED_TIMER_COUNTER_MAX_OFFSET));
}

//stops hitLedTimer
void hitLedTimerStop()
{
    hitLedTimerRunning = false;
}

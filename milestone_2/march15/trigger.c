#include "trigger.h"
#include "buttons.h"
#include "intervalTimer.h"
#include "mio.h"
#include "switches.h"
#include "transmitter.h"
#include "utils.h"
#include <stdio.h>
// pin number and state macros
#define TRIGGER_PIN 10
#define TRIGGER_PRESSED 1
#define TRIGGER_NOT_PRESSED 0
// initial counter value
#define TRIGGER_COUNTER_INITIAL_VALUE 0
// delay nessisary to debounce the button
#define DEBOUNCE_TICK_DELAY 5000
// number of frequencies
#define FILTER_FREQUENCY_COUNT 10

#define NUM_FREQUENCIES 10
#define SWITCHES_BIT_MASK 0xF


volatile bool triggerEnabled;

volatile uint32_t triggerCounter;

// enumeration for our state machine
static enum trigger_st_t {
  init_st,
  disabled_st,
  on_st,
  off_st,
  on_to_off_transition_st,
  off_to_on_transition_st
} currentState;

// resets triggerCounter
void resetTriggerCounter();

// increments triggerCounter
void incrementTriggerCounter();

// returns whether trigger Counter is done
bool triggerCounterIsDone();

// returns whether or not the trigger is currently pressed
bool triggerPressed();

//returns the current setting of the switches with a default for ignored frequencies
uint16_t triggerGetCurrentFrequency();

// Init trigger data-structures.
// Determines whether the trigger switch of the gun is connected (see discussion
// in lab web pages). Initializes the mio subsystem.
void trigger_init() {
  resetTriggerCounter();
  mio_init(false);
  mio_setPinAsInput(TRIGGER_PIN);
  buttons_init();
}

// Enable the trigger state machine. The trigger state-machine is inactive until
// this function is called. This allows you to ignore the trigger when helpful
// (mostly useful for testing).
void trigger_enable() { triggerEnabled = true; }

// Disable the trigger state machine so that trigger presses are ignored.
void trigger_disable() { triggerEnabled = false; }

// Returns the number of remaining shots.
trigger_shotsRemaining_t trigger_getRemainingShotCount() {}

// Sets the number of remaining shots.
void trigger_setRemainingShotCount(trigger_shotsRemaining_t count) {}

// Standard tick function.
void trigger_tick() {
  // transition switch statement for our tick function

  switch (currentState) {
  // automatically transitions to disabled_st
  case init_st:
    currentState = disabled_st;
    break;
  // disabled state where our machine has not been enabled by the flag,
  // and we don't care what the current state trigger is
  case disabled_st:
    // case the trigger has been enabled and we are to transition
    // to either the on or off state, depending on the current position
    // of the switch
    if (triggerEnabled) {
      // case the trigger is pressed right now, we transition to
      // the on state
      if (triggerPressed()) {
        currentState = on_st;
      }
      // case the trigger is not pressed right now
      // we transition to the off state
      else {
        currentState = off_st;
      }
    }
    // otherwise, loop back to disabled state
    else {
      currentState = disabled_st;
    }
    break;
  // case the trigger is currently in the on position. Our machine is enabled
  case on_st:
    // case the trigger has been disabled, and we go back to the disabled state
    if (!triggerEnabled) {
      currentState = disabled_st;
    }
    // case the trigger is enabled and it is still pressed, se transition back
    // to this same state
    else if (triggerPressed()) {
      currentState = on_st;
    }
    // case the trigger was not pressed, and we transition to the on to off
    // transition state and reset the counter for debouncing purposes
    else if (!triggerPressed()) {
      currentState = on_to_off_transition_st;
      resetTriggerCounter();
    }
    break;
  // case the trigger is currently in the off position. Our machine is disabled
  case off_st:
    // case the trigger has been disabled, and we go back to the disabled state
    if (!triggerEnabled) {
      currentState = disabled_st;
    }
    // case the trigger is pressed while in this state. go to transition state
    // reset trigger counter
    else if (triggerPressed()) {
      currentState = off_to_on_transition_st;
      resetTriggerCounter();
    }
    // case the trigger is not pressed while in this state
    else if (!triggerPressed()) {
      currentState = off_st;
    }
    break;
  // transition state from on to off used for debouncing purposes
  case on_to_off_transition_st:
    // case the trigger Counter is done and the trigger is not pressed
    if (triggerCounterIsDone() && !triggerPressed()) {
      currentState = off_st;
    }
    // case the trigger Counter is not done but the trigger is still
    // not pressed. We increment the counter and transition back to
    // this same state
    else if (!triggerCounterIsDone() && !triggerPressed()) {
      currentState = on_to_off_transition_st;
      incrementTriggerCounter();
    }
    // case the trigger is pressed again, we go back to on state to start over
    else if (triggerPressed()) {
      currentState = on_st;
    }
    break;
  // transition state from off to on. used for debouncing purposes
  case off_to_on_transition_st:
    // case the counter is done and the trigger is still pressed, we transition
    // to the on state, call the transmitter run function.
    if (triggerCounterIsDone() && triggerPressed()) {
      currentState = on_st;
      // reads frequency number
      transmitter_setFrequencyNumber(triggerGetCurrentFrequency());
      // calls transmitter to run
      transmitter_run();

    }
    // case the counter is not done, but the trigger is still pressed
    else if (!triggerCounterIsDone() && triggerPressed()) {
      currentState = off_to_on_transition_st;
      incrementTriggerCounter();
    }
    // case the trigger is not still pressed
    else if (!triggerPressed()) {
      currentState = off_st;
    }
    break;
  // no default transitions or actions
  default:
    break;
  }
}

// Runs the test continuously until BTN1 is pressed.
// The test just prints out a 'D' when the trigger or BTN0
// is pressed, and a 'U' when the trigger or BTN0 is released.
void trigger_runTest() { trigger_enable(); }

// resets triggerCounter
void resetTriggerCounter() { triggerCounter = TRIGGER_COUNTER_INITIAL_VALUE; }

// increments triggerCounter
void incrementTriggerCounter() { triggerCounter++; }

// returns whether trigger Counter is done
bool triggerCounterIsDone() { return (triggerCounter >= DEBOUNCE_TICK_DELAY); }

// returns whether or not the trigger is currently pressed
bool triggerPressed() {
  // checks the value of the trigger pin. If it is pressed
  // then we return a true. or if button_0 is also pressed
  if ((mio_readPin(TRIGGER_PIN) == TRIGGER_PRESSED) ||
      (buttons_read() & BUTTONS_BTN0_MASK)) {
    return true;
  }
  // checks the value of the trigger pin. If it is not pressed
  // then it returns false
  else {
    return false;
  }
}


//returns the current setting of the switches with a default for ignored frequencies
uint16_t triggerGetCurrentFrequency()
{
  uint16_t switchSetting = switches_read() & SWITCHES_BIT_MASK; // Bit-mask the results.
  // Provide a nice default if the slide switches are in error.
  if (!(switchSetting < NUM_FREQUENCIES))
  {
    return (NUM_FREQUENCIES - 1);
  }
  //otherwise, return the current Switches value
  else
  {
    return switchSetting;
  }
}

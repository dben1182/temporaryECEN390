#include "detector.h"
#include "filter.h"
#include "hitLedTimer.h"
#include "interrupts.h"
#include "lockoutTimer.h"

#include <stdio.h>

// Set to 1000 for M3T3, will need to be lowered later for increased range
#define FUDGE_FACTOR 1000

#define RAW_ADC_SCALING 2047.5

// frequencies are specified as true if ignores
bool detectorIgnoredFrequencies[10];

uint16_t decimationCounter;

uint16_t detector_hitArray[10];

uint16_t lastChannelHit;
static bool channelHitInitialized;

static bool hitDetected;

// Always have to init things.
// bool array is indexed by frequency number, array location set for true to
// ignore, false otherwise. This way you can ignore multiple frequencies.
void detector_init(bool ignoredFrequencies[]) {
  filter_init();
  lockoutTimer_init();
  hitLedTimer_init();
  decimationCounter = 0;
  // iterates through each value in the frequencies to ignore specific
  // frequencies
  for (uint16_t i = 0; i < 10; i++) {
    detectorIgnoredFrequencies[i] = ignoredFrequencies[i];
    detector_hitArray[i] = 0;
  }
  hitDetected = false;
  channelHitInitialized = false;
}

// Runs the entire detector: decimating fir-filter, iir-filters,
// power-computation, hit-detection. if interruptsNotEnabled = true, interrupts
// are not running. If interruptsNotEnabled = true you can pop values from the
// ADC queue without disabling interrupts. If interruptsNotEnabled = false, do
// the following:
// 1. disable interrupts.
// 2. pop the value from the ADC queue.
// 3. re-enable interrupts if interruptsNotEnabled was true.
// if ignoreSelf == true, ignore hits that are detected on your frequency.
// Your frequency is simply the frequency indicated by the slide switches
void detector(bool interruptsCurrentlyEnabled) {
  uint32_t rawADCValue;
  // check if interrupts currently enabled; if so, pause them, collect value,
  // then resume. if not, then just collect the value.
  if (interruptsCurrentlyEnabled) {
    interrupts_disableArmInts();
    rawADCValue = isr_removeDataFromAdcBuffer();
    interrupts_enableArmInts();
  } else {
    rawADCValue = isr_removeDataFromAdcBuffer();
  }
  // scale adc value linearly between -1 and 1 (started as 0 to 4095)
  // check that the division automatically casts to float
  double scaledADCValue = (rawADCValue / RAW_ADC_SCALING) - 1;
  // printf("raw value: %d, ", rawADCValue);
  // printf("scaled value: %f\n", scaledADCValue);

  // add adc value to filter
  filter_addNewInput(scaledADCValue);

  // only run the filters every 10th new value (decimation)
  // run the FIR filter, IIR filter and power computation for all 10 channels
  if (decimationCounter == 9) {

    // run FIR filter
    filter_firFilter();

    // could only run iir filter for values not being ignored?
    for (uint16_t i = 0; i < 10; i++) {
      // run iir filter for each individual frequency
      filter_iirFilter(i);
    }

    // could only run iir filter for values not being ignored?
    for (uint16_t i = 0; i < 10; i++) {
      // when will we need to run forceComputeFromScratch?
      filter_computePower(i, false, false);
    }

    // If you detect a hit and the frequency with maximum power is not an
    // ignored frequency (see detector_init):
    //only will check if the lockout timer is not running. Otherwise,
    //we don't check until the time is up

  
    if (detector_hitDetected()) 
    {
    
      //-start the lockoutTimer.
      lockoutTimer_start();

      //-start the hitLedTimer.
      hitLedTimer_start();

      //-increment detector_hitArray at the index of the frequency of the
      // IIR-filter output where you detected the hit. Note that
      // detector_hitArray is a 10-element integer array that simply holds the
      // current number of hits, for each frequency, that have occurred to this
      // point.

      // set detector_hitDetectedFlag to true.
      hitDetected = true;
    } 
    else 
    {
      hitDetected = false; 
      // if no hit detected, then set this to false? Need to think
      // about how long hit detections are supposed to last -- do we
      // run hitDetected() immediately after checking,
      // or will resetting it to false after 10 ticks stop us from
      // realizing it happened?
    }
  

    // resets decimation counter to zero so we can keep timing for decimation
    decimationCounter = 0;
  }
  // otherwise, if it is not the tenth count, we increment the count
  else {
    decimationCounter++;
  }
}

// we calculate the fudge value based on all 10 frequencies, not ignoring any.
// this means that if your teammates happen to shoot you at the exact same time
// as an enemy shoots you, and your teammate is significantly closer, then you
// likely won't register as hit.
// Returns true if a hit was detected and if the frequency in question was not
// an ignored frequency
bool detector_hitDetected() {
  double currentPowerVals[10];
  // get filter values for all 10 power values
  for (uint16_t i = 0; i < 10; i++) {
    currentPowerVals[i] = filter_getCurrentPowerValue(i);
  }

  // stores the array indices for the currentPowerVals array
  uint16_t channelIndexArray[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  // insertion sort - may need to swap indexes around instead?
  for (uint16_t i = 1; i < 10; i++) {
    for (uint16_t j = 1; j < 10; j++) {
      if (currentPowerVals[j - 1] > currentPowerVals[j]) {

        double temp = currentPowerVals[j];
        currentPowerVals[j] = currentPowerVals[j - 1];
        currentPowerVals[j - 1] = temp;

        // mirror channelIndexArray with currentPowerVals, so that all swaps are
        // done on both
        int tempInt = channelIndexArray[j];
        channelIndexArray[j] = channelIndexArray[j - 1];
        channelIndexArray[j - 1] = tempInt;
      }
    }
  }

  // spec says to take the 5th value, and we've sorted an ascending value
  // TEST ME

  double medianValue = currentPowerVals[channelIndexArray[4]];
  double thresholdValue = medianValue * FUDGE_FACTOR;
  double maxValue = currentPowerVals[9];
  //printf("median value: %f, threshold value: %f, maxValue: %f\n", medianValue, thresholdValue, maxValue);

  /*printf("contents of tempVals: ");
  for (uint16_t i = 0; i < 10; i++) {
    printf("%d,", channelIndexArray[i]);
  }*/

  /*intf("contents of currentPowerVals: ");
  for (uint16_t i = 0; i < 10; i++) {
    printf("%f,", currentPowerVals[i]);
  }
  printf("\n");*/

  if ((maxValue > thresholdValue) && (!lockoutTimer_running())) {
    //printf("Hit detected on Channel %d\n", (channelIndexArray[9] + 1));
    lastChannelHit = channelIndexArray[9];
    (detector_hitArray[lastChannelHit])++;
    //printf("hit count for that player: %d;\n", detector_hitArray[lastChannelHit]);
    for (uint8_t i = 0; i < 10; i++) {
      printf("Player %d number of hits: %d\n", (i + 1), detector_hitArray[i]);
    }
    return true;
  } else {
    //printf("no Hit Detected\n");
    return false;
  }
}

// Returns the frequency number that caused the hit.
uint16_t detector_getFrequencyNumberOfLastHit() {
  // todo: check if last channel has been initialized, as we didn't want to init
  // it to 0 so we left it hanging
  return lastChannelHit;
}

// Clear the detected hit once you have accounted for it.
void detector_clearHit() { hitDetected = false; }

// Ignore all hits. Used to provide some limited invincibility in some game
// modes. The detector will ignore all hits if the flag is true, otherwise will
// respond to hits normally.
void detector_ignoreAllHits(bool flagValue) {}

// Get the current hit counts.
// Copy the current hit counts into the user-provided hitArray
// using a for-loop.
void detector_getHitCounts(detector_hitCount_t hitArray[]) {
  for (uint16_t i = 0; i < 10; i++) {
    hitArray[i] = detector_hitArray[i];
  }
}

// Allows the fudge-factor index to be set externally from the detector.
// The actual values for fudge-factors is stored in an array found in detector.c
void detector_setFudgeFactorIndex(uint32_t temp) {}

// This function sorts the inputs in the unsortedArray and
// copies the sorted results into the sortedArray. It also
// finds the maximum power value and assigns the frequency
// number for that value to the maxPowerFreqNo argument.
// This function also ignores a single frequency as noted below.
// if ignoreFrequency is true, you must ignore any power from frequencyNumber.
// maxPowerFreqNo is the frequency number with the highest value contained in
// the unsortedValues. unsortedValues contains the unsorted values. sortedValues
// contains the sorted values. Note: it is assumed that the size of both of the
// array arguments is 10.
detector_status_t detector_sort(uint32_t *maxPowerFreqNo,
                                double unsortedValues[],
                                double sortedValues[]) {}

// Encapsulate ADC scaling for easier testing.
double detector_getScaledAdcValue(isr_AdcValue_t adcValue) {}

/*******************************************************
 ****************** Test Routines **********************
 ******************************************************/

// Students implement this as part of Milestone 3, Task 3.
// void detector_runTest() {
//   printf("Detector_runTest initialized\n");
//   printf("Running no hit test\n");
//   double currentPowerVals[10] = {10, 5999, 5000, 8, 26, 6, 17, 4, 3, 1};
//   detector_hitDetected();

//   printf("\n\n\n%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\nRunnin hit Test");
//   currentPowerVals = {10, 5999, 5000, 8, 26, 6, 17, 4, 3, 1}
// }

// Returns 0 if passes, non-zero otherwise.
// if printTestMessages is true, print out detailed status messages.
// detector_status_t detector_testSort(sortTestFunctionPtr testSortFunction,
// bool printTestMessages);

detector_status_t detector_testAdcScaling() {}
#include "detector.h"
#include "filter.h"
#include "hitLedTimer.h"
#include "interrupts.h"
#include "lockoutTimer.h"

#include <stdio.h>

#define DECIMATION_COUNTER_INITIAL_VALUE 0
#define DECIMATION_COUNTER_MAX 9

#define MAX_VALUE_INDEX 9

#define FOR_LOOP_START_VALUE 0

#define SORT_FOR_LOOP_START 1
#define SORT_MOVING_OFFSET 1

#define MEDIAN_INDEX_VALUE 4

#define HIT_ARRAY_INITIAL_VALUE 0

#define NUM_FREQUENCIES 10

// Set to 1000 for M3T3, will need to be lowered later for increased range
#define FUDGE_FACTOR 1000

#define RAW_ADC_SCALING 2047.5

// frequencies are specified as true if ignores
bool detectorIgnoredFrequencies[10];

uint16_t decimationCounter;

uint16_t detector_hitArray[10];

uint16_t lastChannelHit;
static bool channelHitInitialized;

//flag used to determine if a hit has been detected
static bool detector_hitDetectedFlag;

//resets Decimation Counter to zero
void resetDecimationCounter();

//increments decimation counter by 1
void incrementDecimationCounter();

//returns whether decimation counter is complete
bool decimationCounterComplete();


// Always have to init things.
// bool array is indexed by frequency number, array location set for true to
// ignore, false otherwise. This way you can ignore multiple frequencies.
void detector_init(bool ignoredFrequencies[]) 
{
  filter_init();
  //may need to call lockoutTimer_init and hitLedTimer_init here. But I don't think so
  //resets DecimationCounter to zero
  resetDecimationCounter();

  //iterates through each value in the frequencies to ignore specific frequencies
  //also sets the detector_hitArray to all zeros
  for(uint16_t i = FOR_LOOP_START_VALUE; i < NUM_FREQUENCIES; i++)
  {
    detectorIgnoredFrequencies[i] = ignoredFrequencies[i];
    detector_hitArray[i] = HIT_ARRAY_INITIAL_VALUE;
  }
  detector_hitDetectedFlag = false;
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
void detector(bool interruptsCurrentlyEnabled) 
{
  //helper variable used to store the raw adc value which is between 0 and 4095
  uint32_t rawADCValue;
  //helper variable that stores the number of elements in the buffer
  uint32_t elementCount = isr_adcBufferElementCount();
  //helper variable that stores the scaled ADC value
  double scaledADCValue;

  //as per the instructions, we do the following steps the same number of times
  //as there are elements left in the ADC queue
  for(uint32_t i = FOR_LOOP_START_VALUE; i < elementCount; i++)
  {
    //case the ARM interrupts are currently enabled, in which case, we disable them
    //before proceeding to pop the oldest data value from the queue and storing it
    //in the variable rawADCValue. Then we re-enable the interrupts
    if(interruptsCurrentlyEnabled)
    {
      interrupts_disableArmInts();
      rawADCValue = isr_removeDataFromAdcBuffer();
      interrupts_enableArmInts();
    }
    //otherwise, if the interrupts are not enabled, we just pop it and continue
    else
    {
      rawADCValue = isr_removeDataFromAdcBuffer();
    }

    //scales the RAW ADC value by dividing it by 2047.5 and then subtracting 1
    //so we end up with values between -1 and 1
    scaledADCValue = (rawADCValue / RAW_ADC_SCALING) - 1;
 

    //we add the newly calculated scaledADCValue to the filter queue
    filter_addNewInput(scaledADCValue);
    
    //case the decimation counter is complete, and we do all the other code
    //and call all the filter functions. at the end, we reset the counter
    if(decimationCounterComplete())
    {
      //runs the FIR filter
      filter_firFilter();

      //iterates through and runs each of the IIR Filters
      for(uint16_t j = FOR_LOOP_START_VALUE; j < NUM_FREQUENCIES; j++)
      {
        filter_iirFilter(j);
      }

      //iterates through and runs each of the compute power functions
      for(uint16_t j = FOR_LOOP_START_VALUE; j < NUM_FREQUENCIES; j++)
      {
        filter_computePower(j, false, false);
      }

      //case the lockoutTimer is not running, so we look for another hit
      if(!lockoutTimer_running())
      {
        //creates a length ten array of doubles to store the current
        //power values
        double currentPowerValues[NUM_FREQUENCIES];

        //creates an array that will correspond to the indecies of the sorted
        //current power values array
        uint16_t channelIndexArray[NUM_FREQUENCIES];
        
        //iterates through for each filter and stores the power values in the
        //current power values array. Also populates the channel index array
        //with all the player numbers
        for(uint16_t j = FOR_LOOP_START_VALUE; j < NUM_FREQUENCIES; j++)
        {
          currentPowerValues[j] = filter_getCurrentPowerValue(j);
          channelIndexArray[j] = j;
        }
        
        //insertion sort Algorithm

        //does the inside loop 10 times for each of the possible elements
        for(uint16_t j = SORT_FOR_LOOP_START; j < NUM_FREQUENCIES; j++)
        {
          //moves a single value to its correct relative position
          for(uint16_t k = SORT_FOR_LOOP_START; k < NUM_FREQUENCIES; k++)
          {
            //case the value at k is less than the value at k-1. so, we switch them
             if(currentPowerValues[k - SORT_MOVING_OFFSET] > currentPowerValues[k])
             {
               //this block of code will move the value that was at k - 1 to the k index
               //and move the one that was at the k index to the k - 1 index, if the
               //above condition is true
               double temp = currentPowerValues[k];
               currentPowerValues[k] = currentPowerValues[k - SORT_MOVING_OFFSET];
               currentPowerValues[k - SORT_MOVING_OFFSET] = temp;

               //mirrors the above block by also sorting the index array accordingly
               //so we can keep track of who is who
               uint16_t tempInt = channelIndexArray[k];
               channelIndexArray[k] = channelIndexArray[k - SORT_MOVING_OFFSET];
               channelIndexArray[k - SORT_MOVING_OFFSET] = tempInt;
             }
          }
        }


        //median value is now the 5th element (index 4) of the currentPowerValues array
        double medianValue = currentPowerValues[MEDIAN_INDEX_VALUE];

        //multiply the medianValue by the Fudge Factor to get the threshold value
        double thresholdValue = medianValue*FUDGE_FACTOR;

        //the maxValue is the last element of the current Power Values array,
        //which is currentPowerValues[9]
        double maxValue = currentPowerValues[MAX_VALUE_INDEX];

        //compares to see if the maxValue is greater than the threshold value. If true
        //then we set the flags to true, start the lockout and led timers
        if(maxValue > thresholdValue)
        {
          
          //sets the lastChannelHit variable to the index of the highest power channel
          lastChannelHit = channelIndexArray[MAX_VALUE_INDEX];
          //increments the number of hits, for that specified channel, by 1
          (detector_hitArray[lastChannelHit])++;
          //starts lockout timer so that we don't do any more checking for a hit
          //for the next 2000 milliseconds
          lockoutTimer_start();
          //starts the timer for the hit led, which will illiminate the led
          //hitLedTimer_start();

          //sets the hit detected flag to true
          detector_hitDetectedFlag = true;
        }
        //otherwise, we set the hitDetectedFlag to false
        else
        {
          detector_hitDetectedFlag = false;
        }
      }

      resetDecimationCounter();
    }
    //case the decimation counter is not complete, and we just increment the counter
    else
    {
      incrementDecimationCounter();
    }
  }
}

// we calculate the fudge value based on all 10 frequencies, not ignoring any.
// this means that if your teammates happen to shoot you at the exact same time
// as an enemy shoots you, and your teammate is significantly closer, then you
// likely won't register as hit.
// Returns true if a hit was detected and if the frequency in question was not
// an ignored frequency
bool detector_hitDetected() 
{
  return detector_hitDetectedFlag;
}

// Returns the frequency number that caused the hit.
uint16_t detector_getFrequencyNumberOfLastHit() 
{
  return lastChannelHit;
}

// Clear the detected hit once you have accounted for it.
void detector_clearHit() 
{ 
  detector_hitDetectedFlag = false;
}

// Ignore all hits. Used to provide some limited invincibility in some game
// modes. The detector will ignore all hits if the flag is true, otherwise will
// respond to hits normally.
void detector_ignoreAllHits(bool flagValue) {}

// Get the current hit counts.
// Copy the current hit counts into the user-provided hitArray
// using a for-loop.
void detector_getHitCounts(detector_hitCount_t hitArray[]) 
{
  for(uint16_t i = FOR_LOOP_START_VALUE; i < NUM_FREQUENCIES; i++)
  {
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



//resets Decimation Counter to zero
void resetDecimationCounter()
{
  decimationCounter = DECIMATION_COUNTER_INITIAL_VALUE;
}

//increments decimation counter by 1
void incrementDecimationCounter()
{
  decimationCounter++;
}

//returns whether decimation counter is complete
bool decimationCounterComplete()
{
  return (decimationCounter >= DECIMATION_COUNTER_MAX);
}
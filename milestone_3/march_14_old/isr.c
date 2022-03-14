#include "isr.h"
#include "buttons.h"
#include "hitLedTimer.h"
#include "lockoutTimer.h"
#include "switches.h"
#include "transmitter.h"
#include "trigger.h"

#include <stdint.h>
#include <stdio.h>

#include "detector.h"
#include "interrupts.h"
#include "lockoutTimer.h"
#include "transmitter.h"
#include "trigger.h"

//change this back to original after testing 100000
#define ADC_BUFFER_SIZE 100000
#define INDEX_IN_INITIAL_VALUE 0
#define INDEX_OUT_INITIAL_VALUE 0
#define ELEMENT_COUNT_INITIAL_VALUE 0
#define INDEX_OUT_OFFSET 1
#define INDEX_IN_OFFSET 1

#define ADC_QUEUE_EMPTY_RETURN 0
#define POPPED_REPLACEMENT 0

#define FOR_LOOP_START_VALUE 0
#define BUFFER_INITIAL_VALUE 0


//macros for test
#define NUM_ITERATIONS 100

#define NUM_ELEMENTS_IN_QUEUE 10

// This implements a dedicated circular buffer for storing values
// from the ADC until they are read and processed by detector().
// adcBuffer_t is similar to a queue.
typedef struct {
  uint32_t indexIn;               // New values go here.
  uint32_t indexOut;              // Pull old values from here.
  uint32_t elementCount;          // Number of elements in the buffer.
  uint32_t data[ADC_BUFFER_SIZE]; // Values are stored here.
} adcBuffer_t;


//index Out refers to the index corresponding to the oldest value in the queue
//i.e. index out will be some number and the data array at that index number will
//be the oldest data value in the array

//index In refers to the index corresponding to the index that is one past the newest
//value. i.e., if the index of the newest value is 6, the value of indexIn will be 7
//if there are no elements in the queue, indexIn will be the same number as indexOut

// This is the instantiation of adcBuffer.
volatile static adcBuffer_t adcBuffer;

void adcBufferInit();

// returns whether the adc Queue is empty
bool adcQueueIsEmpty();

// our test function for the adc Queue
void adcTest();

// Performs inits for anything in isr.c
void isr_init() {
  transmitter_init();
  buttons_init();
  switches_init();
  lockoutTimer_init();
  hitLedTimer_init();
  trigger_init();
  adcBufferInit();
}

// This function is invoked by the timer interrupt at 100 kHz.
void isr_function() {
  transmitter_tick();
  lockoutTimer_tick();
  hitLedTimer_tick();
  trigger_tick();
  isr_addDataToAdcBuffer(interrupts_getAdcData());
}

// This adds data to the ADC queue. Data are removed from this queue and used by
// the detector.
// owerwrite push
void isr_addDataToAdcBuffer(uint32_t adcData) 
{
  //case the queue is full, so we call the removeDataFromAdcBuffer function
  //before we go on to add the new adc data
  if(((adcBuffer.indexIn + INDEX_IN_OFFSET) % ADC_BUFFER_SIZE) == adcBuffer.indexOut)
  {
    isr_removeDataFromAdcBuffer();
  }
  //we write adcData to indexIn, which will later be incremented to one past
  //this current location
  adcBuffer.data[adcBuffer.indexIn] = adcData;
  //increment the element count. (This cancels out the decrement if we popped
  //before adding this new data in)
  (adcBuffer.elementCount)++;

  //updates indexIn to be one past its original location with also a
  //wraparound check
  adcBuffer.indexIn = (adcBuffer.indexIn + INDEX_IN_OFFSET) % ADC_BUFFER_SIZE;
}

// This removes a value from the ADC buffer.
// this removes the oldest value from the buffer,
//which corresponds to the indexOut variable.
//if there is no data in the queue, this function
//returns zero, and does nothing else
uint32_t isr_removeDataFromAdcBuffer() 
{
  //case the adcQueue is empty, and we return zero and 
  //we do nothing else
  if(adcQueueIsEmpty())
  {
    return ADC_QUEUE_EMPTY_RETURN;
  }
  //case the adcQueue is NOT empty. We set the data value at the oldest slot
  //to zero for convenience. We increment indexOut by one, taking
  //into account the wraparound from the highest value back to the lowest
  //we decrement the number of elements
  else
  {
    //temporary variable that stores the data at the popped point
    //stores it for use later to be returned later
    uint32_t oldData = adcBuffer.data[adcBuffer.indexOut];
    //replaces the data at that point with zero
    adcBuffer.data[adcBuffer.indexOut] = POPPED_REPLACEMENT;
    //increments index Out taking into account the wraparound
    adcBuffer.indexOut = (adcBuffer.indexOut + INDEX_OUT_OFFSET) % ADC_BUFFER_SIZE;
    //decrements the number of elements in the queue by 1
    (adcBuffer.elementCount)--;
    return oldData;
  }
}

// This returns the number of values in the ADC buffer.
uint32_t isr_adcBufferElementCount() 
{ 
  return adcBuffer.elementCount;
}

//adcBuffer Init
void adcBufferInit() 
{
  //sets indexOut to zero
  adcBuffer.indexOut = INDEX_OUT_INITIAL_VALUE;
  //sets indexIn to zero
  adcBuffer.indexIn = INDEX_IN_INITIAL_VALUE;
  //sets number of elements to zero
  adcBuffer.elementCount = ELEMENT_COUNT_INITIAL_VALUE;
  //iterates through each slot in the data array, and initializes
  //those values to be zero. 
  for(uint32_t i = FOR_LOOP_START_VALUE; i < ADC_BUFFER_SIZE; i++)
  {
    adcBuffer.data[i] = BUFFER_INITIAL_VALUE;
  }
}

// returns whether the adc Queue is empty
//it knows this by seeing if indexOut and indexIn
//are equal to each other, that they are at the 
//same value
bool adcQueueIsEmpty() 
{  
  return (adcBuffer.indexOut == adcBuffer.indexIn);
}

// our test function for the adc Queue
void adcTest() 
{
  adcBufferInit();
  // iterates through 100 different times just to check the performance of the
  // circular queue
  for (uint32_t i = FOR_LOOP_START_VALUE; i < NUM_ITERATIONS; i++) {
    isr_addDataToAdcBuffer(i);
    printf("Values in queue: ");
    // iterates through each possible element in the queue, including the
    // possible garbage values of the queue
    for (uint32_t j = FOR_LOOP_START_VALUE; j < NUM_ELEMENTS_IN_QUEUE; j++) {
      printf("%d, ", adcBuffer.data[j]);
    }
    printf("garbage value: %d", adcBuffer.data[adcBuffer.indexIn]);
    printf("\n");
  }
  isr_removeDataFromAdcBuffer();
  //iterates through and prints all
  for(uint8_t i = FOR_LOOP_START_VALUE; i < NUM_ELEMENTS_IN_QUEUE; i++)
  {
    printf("%d, ", adcBuffer.data[i]);
  }
    printf("garbage value: %d", adcBuffer.data[adcBuffer.indexIn]);
    printf(" element count: %d", adcBuffer.elementCount);
    printf("\n"); 
}

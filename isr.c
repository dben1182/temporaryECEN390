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
#include "hitLedTimer.h"
#include "interrupts.h"
#include "lockoutTimer.h"
#include "transmitter.h"
#include "trigger.h"

#define ADC_BUFFER_SIZE 100000
#define INDEX_IN_INITIAL_VALUE 0
#define INDEX_OUT_INITIAL_VALUE 0
#define ELEMENT_COUNT_INITIAL_VALUE 0
#define INDEX_OUT_OFFSET 1
#define INDEX_IN_OFFSET 1

#define FOR_LOOP_START_VALUE 0
#define BUFFER_INITIAL_VALUE 0

// This implements a dedicated circular buffer for storing values
// from the ADC until they are read and processed by detector().
// adcBuffer_t is similar to a queue.
typedef struct {
  uint32_t indexIn;               // New values go here.
  uint32_t indexOut;              // Pull old values from here.
  uint32_t elementCount;          // Number of elements in the buffer.
  uint32_t data[ADC_BUFFER_SIZE]; // Values are stored here.
} adcBuffer_t;

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
void isr_addDataToAdcBuffer(uint32_t adcData) {
  // case the queue is full. we change index out.
  // otherwise, index out stays in the same exact place
  if ((adcBuffer.indexIn + INDEX_IN_OFFSET) % ADC_BUFFER_SIZE == adcBuffer.indexOut) 
  {
    adcBuffer.indexOut = (adcBuffer.indexOut + INDEX_OUT_OFFSET) % ADC_BUFFER_SIZE;
    // element Count is decremented to account for the queue being full
    // and then incremented later
    (adcBuffer.elementCount)--;
  }
  // writes new data to the former indexIn index
  adcBuffer.data[adcBuffer.indexIn] = adcData;
  // increment element Count
  (adcBuffer.elementCount)++;
  // updates indexIn to be an increase of one, with a wraparound
  // possible check
  adcBuffer.indexIn = (adcBuffer.indexIn + INDEX_IN_OFFSET) % ADC_BUFFER_SIZE;
}

// This removes a value from the ADC buffer.
// pop function
uint32_t isr_removeDataFromAdcBuffer() {
  // case the adc queue is empty, and we return a zero
  if (adcQueueIsEmpty()) {
    return 0;
  }
  // otherwise, we do the popping and return the value
  else {
    uint32_t temp = adcBuffer.data[adcBuffer.indexOut];
    // replaces popped value with a zero value
    adcBuffer.data[adcBuffer.indexOut] = 0;
    adcBuffer.indexOut = (adcBuffer.indexOut + INDEX_OUT_OFFSET) % ADC_BUFFER_SIZE;
    //decrements the number of elements in the queue by 1
    (adcBuffer.elementCount)--;
    return temp;
  }
}

// This returns the number of values in the ADC buffer.
uint32_t isr_adcBufferElementCount() { return adcBuffer.elementCount; }

void adcBufferInit() {
  adcBuffer.indexIn = INDEX_IN_INITIAL_VALUE;
  adcBuffer.indexOut = INDEX_OUT_INITIAL_VALUE;
  adcBuffer.elementCount = ELEMENT_COUNT_INITIAL_VALUE;

  // initializes each value in data with zeros to begin with

  for (uint32_t i = FOR_LOOP_START_VALUE; i < ADC_BUFFER_SIZE; i++) {
    adcBuffer.data[i] = BUFFER_INITIAL_VALUE;
  }
}

// returns whether the adc Queue is empty
bool adcQueueIsEmpty() { return (adcBuffer.indexOut == adcBuffer.indexIn); }

// our test function for the adc Queue
void adcTest() {
  adcBufferInit();
  // iterates through 100 different times just to check the performance of the
  // circular queue
  for (uint32_t i = FOR_LOOP_START_VALUE; i < 100; i++) {
    isr_addDataToAdcBuffer(i);
    printf("Values in queue: ");
    // iterates through each possible element in the queue, including the
    // possible garbage values of the queue
    for (uint32_t j = FOR_LOOP_START_VALUE; j < 10; j++) {
      printf("%d, ", adcBuffer.data[j]);
    }
    printf("garbage value: %d", adcBuffer.data[adcBuffer.indexIn]);
    printf("\n");
  }
}
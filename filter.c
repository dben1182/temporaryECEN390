#include "filter.h"
#include "queue.h"
#include "coef.h"

#define NUM_IIR_FILTERS 10
#define NUM_Z_QUEUES NUM_IIR_FILTERS
#define NUM_OUTPUT_QUEUES NUM_IIR_FILTERS


#define ASCII_OFFSET 48
#define Z_QUEUE_NAME_LENGTH 10
#define Z_NAME_OFFSET 7
#define OUTPUT_QUEUE_NAME_LENGTH 15
#define OUTPUT_NAME_OFFSET 12

#define DECIMATION_VALUE 10

#define X_QUEUE_SIZE 81
#define Y_QUEUE_SIZE 11
#define Z_QUEUE_SIZE 10
#define OUTPUT_QUEUE_SIZE 2000

#define FOR_LOOP_START_VALUE 0
#define INITIAL_QUEUE_VALUE 0
#define INITIAL_SUM_VALUE 0
#define INITIAL_POWER_VALUES 0

#define INDEX_OFFSET 1


static queue_t xQueue;
const static queue_size_t xQueue_size = X_QUEUE_SIZE;
const char xQueue_name[] = "xQueue";

static queue_t yQueue;
const static queue_size_t yQueue_size = Y_QUEUE_SIZE;
const char yQueue_name[] = "yQueue";

const static queue_size_t zQueue_size = Z_QUEUE_SIZE;
static queue_t zQueues[NUM_Z_QUEUES];

char zQueueNames[Z_QUEUE_NAME_LENGTH] = "zQueue_";





const static queue_size_t outputQueue_size = OUTPUT_QUEUE_SIZE;
static queue_t outputQueues[NUM_OUTPUT_QUEUES];

char outputQueueNames[OUTPUT_QUEUE_NAME_LENGTH] = "outputQueue_";



double currentPowerValue[NUM_IIR_FILTERS];
double previousPowerValue[NUM_IIR_FILTERS];
double oldestValueLastUsed[NUM_IIR_FILTERS];

//function that initializes arrays that store the power values that will 
//be subtracted for the next iterations's calculation. Initializes them
//all to zero
void init_powerQueues();

//function called at the start that initializes each of queues by calling
//their own respective initialization functions
void filter_init()
{
  init_yQueue();
  init_xQueue();
  init_z_queues();
  init_output_queues();
}

//function that initializes the xQueue function by calling the queue_init function
//and then pushing zeros to the entirety of the queue
void init_xQueue()
{
  queue_init(&xQueue, xQueue_size, xQueue_name);
  //iterates through each slot in the x_queue to fill it completely with zeros
  for(uint16_t i = FOR_LOOP_START_VALUE; i < xQueue_size; i++)
  {
    queue_push(&xQueue, INITIAL_QUEUE_VALUE);
  }
}

//function that initializes the yQueue function by calling the queue_init function
//and then pushing zeros to the entirety of the queue
void init_yQueue()
{
  queue_init(&yQueue, yQueue_size, yQueue_name);
  //iterates through each slot in the y_queue to fill it completely with zeros
  for(uint16_t i = FOR_LOOP_START_VALUE; i < yQueue_size; i++)
  {
    queue_overwritePush(&yQueue, INITIAL_QUEUE_VALUE);
  }
}

void init_z_queues()
{
  //iterates through each of the ten seperate z queues and initialize each
  //of those respective slots to zeros
  for(uint16_t i = FOR_LOOP_START_VALUE; i < NUM_Z_QUEUES; i++)
  {
    //creates unique numbered name for z queues
    zQueueNames[Z_NAME_OFFSET] = i + ASCII_OFFSET;
    queue_init(&(zQueues[i]), zQueue_size, zQueueNames);
    //fill each index with zeros
    for(uint16_t j = FOR_LOOP_START_VALUE; j < zQueue_size; j++)
    {
      queue_overwritePush(&(zQueues[i]), INITIAL_QUEUE_VALUE);
    }
  }
}



void init_output_queues()
{
  //iterates through each of the 10 output queues to initialize them
  for(uint16_t i = FOR_LOOP_START_VALUE; i < NUM_OUTPUT_QUEUES; i++)
  {
    outputQueueNames[OUTPUT_NAME_OFFSET] = i + ASCII_OFFSET;
    queue_init(&(outputQueues[i]), outputQueue_size, outputQueueNames);

    //iterates through each spot and pushes zero onto the queue
    for(uint16_t j = FOR_LOOP_START_VALUE; j < outputQueue_size; j++)
    {
      queue_overwritePush(&(outputQueues[i]), INITIAL_QUEUE_VALUE);
    }

  }
}

//Use this to copy an input into the input queue of the FIR-filter (xQueue).
void filter_addNewInput(double x) 
{ 
  queue_overwritePush(&xQueue, x);
}

void filter_fillQueue(queue_t *q, double fillValue)
{
  for(uint16_t i = FOR_LOOP_START_VALUE; i < queue_size(q); i++)
  {
    queue_overwritePush(q, fillValue);
  }
}


double filter_firFilter()
{
  double y_sum = INITIAL_SUM_VALUE;
  for(uint16_t k = FOR_LOOP_START_VALUE; k < xQueue_size; k++)
  {
    y_sum = y_sum + firCoefficients[i]*queue_readElementAt(&xQueue, xQueue_size - INDEX_OFFSET - k);
  }
  queue_overwritePush(&yQueue, y_sum);
  return y_sum;
}

double filter_iirFilter()(uint16_t filterNumber)
{
  double b_and_y_sum = INITIAL_SUM_VALUE;
  double a_and_z_sum = INITIAL_SUM_VALUE;
  for(uint16_t k = FOR_LOOP_START_VALUE; k < yQueue_size; k++)
  {
    b_and_y_sum = b_and_y_sum + iirBCoefficientConstants[filterNumber]*queue_readElementAt(&yQueue, yQueue_size - INDEX_OFFSET - k);
  }
  for(uint16_t k = FOR_LOOP_START_VALUE; k < zQueue_size; k++)
  {
    a_and_z_sum = a_and_z_sum + iirACoefficientConstants[filterNumber]*queue_readElementAt(&zQueues[filterNumber], zQueue_size - INDEX_OFFSET - k);
  }

  double filter_sum
}




//function that initializes arrays that store the power values that will 
//be subtracted for the next iterations's calculation. Initializes them
//all to zero
void init_powerQueues()
{
  //for loop that iterates and corresponds to each output queue for
  //each IIR Filter
  for(uint16_t i = FOR_LOOP_START_VALUE; i < NUM_IIR_FILTERS; i++)
  {
    currentPowerValue[i] = INITIAL_POWER_VALUES;
    previousPowerValue[i] = INITIAL_POWER_VALUES;
    oldestValueLastUsed[i] = INITIAL_POWER_VALUES;
  }
}
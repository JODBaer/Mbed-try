/*#include "mbed.h"
#include "rtos.h"
 
Queue<uint32_t, 5> queue;
 
DigitalOut myled(LED1);
 
void queue_isr() {
    queue.put((uint32_t*)2);
    myled = !myled;
}
 
void queue_thread(void const *args) {
    while (true) {
        queue.put((uint32_t*)1);
        Thread::wait(1000);
    }
}
 
int main (void) {
    Thread thread(queue_thread);
    
    Ticker ticker;
    ticker.attach(queue_isr, 1.0);
    
    while (true) {
        osEvent evt = queue.get();
        if (evt.status != osEventMessage) {
            printf("queue->get() returned %02x status\n\r", evt.status);
        } else {
            printf("queue->get() returned %d\n\r", evt.value.v);
        }
    }
}
*/
/* Copyright (c) 2018 Arm Limited
*
* SPDX-License-Identifier: Apache-2.07
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "mbed.h"
#include "stdlib.h"
//#include <cstdio>
//#include <queue>

// For SystemView
//#include "MbedSysview.h"
//#include "SysviewMarkers.h"


// two Event-Queues for two threds with diffrent Priority
EventQueue meassureQueue;
EventQueue calculateQueue;

// funktion for the Appliction
void rand_meassure(void);
void blink(DigitalOut &led);

void rise_handler(void);
unsigned long fibonacci(unsigned int n);
//shared resource
unsigned int fibo;
Mutex fibo_mutex;

// Queue from Mbed 
#define QUEUE_MAX 160
MemoryPool<int, QUEUE_MAX> mpool;
Queue<int, QUEUE_MAX> queue;

//Creat Threads so they could live forever, until they terminated.
Thread thread_meassure(osPriorityRealtime, OS_STACK_SIZE, nullptr, "Measure-Thread");
Thread thread_calculate(osPriorityLow1, OS_STACK_SIZE, nullptr, "Calculate-Thread");
//Thread thread_user(osPriorityLow, OS_STACK_SIZE, nullptr, "User-Thread");

// For SystemView
#ifdef TARGET_STM32F4
void sleeplessIdleFunc()
{
    while(true) {}
}
Thread sleeplessIdleThread(osPriorityLow, OS_STACK_SIZE, nullptr, "Idle Thread");
#endif


//Map Input Output
DigitalOut led1(LED1);
DigitalOut led2(LED2);
DigitalOut led3(LED3);
InterruptIn sw(BUTTON1);

// For SystemView
//MbedSysview sysview;


int main(void)
{
    printf("*** start ***\n");

    // For SystemView
    //sysview.begin();

//#ifdef TARGET_STM32F4
//    sleeplessIdleThread.start(&sleeplessIdleFunc);
//#endif


    //Start the Thread with the EventQueues & Buttonhanlder
    thread_meassure.start(callback(&meassureQueue, &EventQueue::dispatch_forever));
    thread_calculate.start(callback(&calculateQueue, &EventQueue::dispatch_forever));
    //thread_user.start(callback(&calculateQueue, &EventQueue::dispatch_forever));

    sw.rise(calculateQueue.event(rise_handler));

    // events are simple callbacks
    meassureQueue.call_every(1, rand_meassure);
    calculateQueue.call_every(10, blink, led1);


    //----------------------------------Stopwatch-Start------------------------
    //auto now = Kernel::Clock::now();
    //-------------------------------------------------------------------------
        fibo_mutex.lock();
        fibo=1000;
        fibo_mutex.unlock();
    //-------------------------------------Stop Stop-Watch-----------------------------
    //auto later = Kernel::Clock::now();

    //calculate millisecs elapsed
    //auto elapsed = later - now;

    //printf("Elapsed ms: %ld \r\n", (uint32_t)elapsed.count());
    //thread_meassure.terminate();

    while(true)
    {
        ThisThread::sleep_for(100);
    }
}


/** 
 * @brief Psoude Meassure Data and put it in the Queue and in the shared resource
 * @param 
 * @return 
 */
enum
{
  minValue = 1, ///< minimal value for random numbers
  maxValue = 20, ///< maximal value for random numbers
};
void rand_meassure (void)
{
    static uint32_t count = 0;
    ++count;
    int* message = mpool.alloc_for(osWaitForever);
    if (message != nullptr)
    {   
        int r = (int)(static_cast<double>(rand()) 
        / (RAND_MAX + 1.0)
        * (maxValue - minValue + 1))
        + minValue; 
        *message = r;
        fibo_mutex.lock();
        fibo=r;
        fibo_mutex.unlock();
        if (!queue.put(message))
        {
            printf("Queue::try_put() error!\n");
        }
    }
    else
    {
        printf("mpool.try_alloc() errror!\n");
    }
}


// Blink Funktion with Led refernce, witch a Led is refered
// And empty the Queue
/** 
 * @brief Blink Funktion with Led refernce, witch a Led is refered
 * @param led the led witch is used to Bilnk
 */
void blink(DigitalOut &led)
{
    osEvent notEmpty;
    int i = 0;
    int32_t data[QUEUE_MAX];

    static int count=0;
    static int result =0;
    do 
    {
        int* elem;
        notEmpty = queue.get(osWaitForever);
        if (notEmpty.status == osEventMessage)
        {
            ++count;
            data[i] = *elem;
            result += data[i]; // summ all reandom meassure up
            ++i;
            if (osOK != mpool.free(elem))
                printf("mpool.free() error!\n");
        }
        else if(i == 0)// no message was in the Queue
        {
            printf("queue.try_get() error!\n");
        }
    } while(notEmpty.status != osOK);
    
    if(count >= 500) // Blink rythmus um das 1000 fache verlangsamen
    {
        led = !led;
        printf("Result: %2i Count:%i Mean: %i \r\n", data[i-1], count, (int)result/count);
        count=0;
        result=0;
    }
}

/** 
 * @brief catch the Button interrupt
 */
void rise_handler(void)
{
    static int count;
    ++count;
    unsigned long out = fibonacci(fibo);
    printf("rise_handler in context %p %i, %i is Fibo: %li\n ", ThisThread::get_id(), count, fibo, out);
    // Toggle LED
    led3 = !led3;
}


/** 
 * @brief calculates the Fibonacci number of n iteratively
 * @param n the nth number of the Fibonacci sequence will be calculated
 * @return Fibonacci number of the argument n
 */
unsigned long fibonacci(unsigned int n)
{
  unsigned long fibMin1 = 1;
  unsigned long fibMin2 = 1;
  unsigned long fib = 1;
  
  if (n == 1 || n == 2)
    return fib;

  for (unsigned int i = 3; i <= n; ++i)
  {
    fib = fibMin2 + fibMin1;
    fibMin2 = fibMin1;
    fibMin1 = fib;
  }
  return fib;
}  


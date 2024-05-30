;
#include "stm32f10x.h"                  // Device header
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "core_cm3.h"


void trigger_pendsv(void);
void init_pendsv_priority(void);
void init_ticktimer_priority(void);
void vPortSetupTimerInterrupt(void);
	
int create_task (
	void *pentry,
	void *param);

	
static inline void disable_interrupts(void) {
    __asm volatile ("CPSID i" : : : "memory");
}

static inline void enable_interrupts(void) {
    __asm volatile ("CPSIE i" : : : "memory");
}

int fputc(int ch, FILE *f)
{
	disable_interrupts();
  ITM_SendChar(ch);
	enable_interrupts();
  return ch;
}


int intrrupt_init(void)
{
	init_pendsv_priority();
	init_ticktimer_priority();

	return  0;
}
void printtask (void);



int thread_test (void *param)
{
	char ch = (char)param;
	int  i = 0;
	printf("\r\nthread run....");

	while(1) {
		printf("%c-%d ", ch, ++i);

	}
}

int task_init(void)
{

	create_task(thread_test, (void *)'a');
	create_task(thread_test, (void *)'b');
	create_task(thread_test, (void *)'c');
	create_task(thread_test, (void *)'d');

	return 0;
}


void start_task1 (void);
int main(void)
{
	
		__asm volatile (
			"MRS R0, MSP\n"
			"MSR PSP, R0\n"       // Set PSP to new_psp
			"MRS R0, CONTROL\n"   // Read CONTROL register
			"ORR R0, R0, #2\n"    // Set bit 1 (SPSEL) to 1 to select PSP
			"MSR CONTROL, R0\n"   // Write back to CONTROL register
	);
			
	printf("hello wrold!\r\n");

	intrrupt_init();

	task_init();

	vPortSetupTimerInterrupt( );

	start_task1();
	while (1) {

	}
}

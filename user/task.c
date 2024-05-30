
#include "stm32f10x.h"                  // Device header
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "core_cm3.h"

struct taskpcb {
	void *pstktop;
	void *pentry;
	void *param;
	char stack[512-12];
};

#define TASK_MAX  5
#define DEFAULT_XPSR   (0x01000000)
#define DEFAULT_LD     (0xFFFFFFFD)
static struct taskpcb TCB[TASK_MAX];
static struct taskpcb NullTask;

static struct taskpcb *tasklist[TASK_MAX];

struct taskpcb *pcurtasktcb;
struct taskpcb *pcanditasktcb;

struct taskpcb * getfreetcb (void);
int regitserTask (struct taskpcb *ptcb);
int create_task (
	void *pentry,
	void *param);


int getCanditask (void)
{
	static int i = -1;

	do {
		if (i < TASK_MAX - 1) i++;
		else i = 0;

		pcanditasktcb = tasklist[i];

	}while (pcanditasktcb == NULL);

	//printf("pcurtasktcb: [%p] = %p  -->%p\r\n", &pcurtasktcb, pcurtasktcb, pcurtasktcb->pstktop);
	
	return  0;
}
struct taskpcb * getfreetcb (void)
{
	int  i;

	for (i = 0; i < TASK_MAX; i++)
	{
		if(TCB[i].pentry == NULL) return (&TCB[i]);

	}
	return  NULL;
}

int regitserTask (struct taskpcb *ptcb)
{
	int  i;
	
	for (i = 0; i < TASK_MAX; i++) {
		if(tasklist[i] == NULL) {
			tasklist[i] = ptcb;
			return  0;
		}
	}
	return -1;
}

int create_task (
	void *pentry,
	void *param)
{
	struct taskpcb *ptcb = NULL;
	long  *pstktop = NULL;

	ptcb = getfreetcb();

	pstktop = (long *)&ptcb->stack[sizeof(ptcb->stack) - 32];
	ptcb->pentry = pentry;

	*(pstktop--) = 0x01000000L;
	*(pstktop--) = (intptr_t) pentry; //pc
	*(pstktop--) = DEFAULT_LD;
	*(pstktop--) = 0x12121212L;
	*(pstktop--) = 0x03030303;
	*(pstktop--) = 0x02020202;
	*(pstktop--) = 0x01010101;
	*(pstktop--) = (intptr_t) param; // r0 param
// save r4 - r11
	*(pstktop--) = 0x11111111;
	*(pstktop--) = 0x10101010;
	*(pstktop--) = 0x09090909;
	*(pstktop--) = 0x08080808;
	*(pstktop--) = 0x77777777;
	*(pstktop--) = 0x66666666;
	*(pstktop--) = 0x55555555;
	*(pstktop) = 0x44444444;

	ptcb->pstktop = pstktop;

printf("task: 0x%p stack: 0x%p\r\n", ptcb, pstktop);
	return  regitserTask(ptcb);
}

void start_task1 (void)
{
	int  *new_psp;
	void *taskentry;
	pcurtasktcb = &NullTask;
	pcanditasktcb = NULL;
	
	NullTask.pstktop = &NullTask.stack[sizeof(NullTask.stack)];
	
	//wait systick schedule
	while(1){}
}


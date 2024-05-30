# 最小任务切换的实现
本例子实现了一个 rtos 最小的任务切换功能，使用 keil 仿真功能，在模拟的 stm32f103 的器件上实现了使用 PendSV 中断切换线程的效果。

## 环境基础
- win11
- keil v5.39
- ArmClang.exe V6.21

## 技术基础
- cortex-m3 架构中，在中断模式下（Handler）使用 MSP，在用户模式（Thread）模式下使用 PSP 作为栈指针。使用 `msr msp, r0` 指令来设置 MSP 寄存器，寄存器的值为栈的高地址。
- cortex-m3 架构中，栈的增长方向为递减，向栈内压入数据，栈指针的地址的数值减小，并且栈的指针指向最新的数据。
- cortex-m3 架构中，在进入中断模式时，硬件会自动将 [r0 r1 r2 r3 r12 r14(LR) r15(pc) xpsr] 一共 8 个寄存器压入栈内，且压入顺序固定
- cortex-m3 架构中，从中断模式返回时，Cortex-M3 处理器支持不同的异常返回方式，这些方式由 LR 寄存器中的特定值指示。当处理器从异常（例如中断或系统调用）返回时，它会检查 LR 寄存器的值以确定返回方式和堆栈指针。常见的异常返回值包括：
>>
    0xFFFFFFF1：返回到特权模式，使用 MSP（Main Stack Pointer）。
    0xFFFFFFF9：返回到特权模式，使用 MSP（这是硬件自动保存的值）。
    0xFFFFFFFD：返回到线程模式，使用 PSP（Process Stack Pointer）。
- cortex-m3 架构中，通常使用 PendSV 中断来切换线程，由于它可以方便通过软件触发，是一个系统级中断，而且使用中我们将其优先级配置为最低，保证其他中断事务处理完成之后才进行任务切换。
- cortex-m3 架构中，有一个 systick 定时器，用它作为系统的时基。


## 实现步骤 
1. 设计使用 systick 作为时基。设置成为一个周期触发的事件，用来检查是否需要切换任务，并触发 PendSV 中断。代码如下，这个配置是来自 freertos，详细内容见 systick.c 文件。 
```c
#define configKERNEL_INTERRUPT_PRIORITY    255
/* Constants required to manipulate the core.  Registers first... */
#define portNVIC_SYSTICK_CTRL_REG             ( *( ( volatile uint32_t * ) 0xe000e010 ) )
#define portNVIC_SYSTICK_LOAD_REG             ( *( ( volatile uint32_t * ) 0xe000e014 ) )
#define portNVIC_SYSTICK_CURRENT_VALUE_REG    ( *( ( volatile uint32_t * ) 0xe000e018 ) )
#define portNVIC_SHPR3_REG                    ( *( ( volatile uint32_t * ) 0xe000ed20 ) )
/* ...then bits in the registers. */
#define portNVIC_SYSTICK_CLK_BIT              ( 1UL << 2UL )
#define portNVIC_SYSTICK_INT_BIT              ( 1UL << 1UL )
#define portNVIC_SYSTICK_ENABLE_BIT           ( 1UL << 0UL )
#define portNVIC_SYSTICK_COUNT_FLAG_BIT       ( 1UL << 16UL )
#define portNVIC_PENDSVCLEAR_BIT              ( 1UL << 27UL )
#define portNVIC_PEND_SYSTICK_SET_BIT         ( 1UL << 26UL )
#define portNVIC_PEND_SYSTICK_CLEAR_BIT       ( 1UL << 25UL )

#define portNVIC_PENDSV_PRI                   ( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 16UL )
#define portNVIC_SYSTICK_PRI                  ( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 24UL )

/* Constants required to check the validity of an interrupt priority. */
#define portFIRST_USER_INTERRUPT_NUMBER       ( 16 )
#define portNVIC_IP_REGISTERS_OFFSET_16       ( 0xE000E3F0 )
#define portAIRCR_REG                         ( *( ( volatile uint32_t * ) 0xE000ED0C ) )
#define portMAX_8_BIT_VALUE                   ( ( uint8_t ) 0xff )
#define portTOP_BIT_OF_BYTE                   ( ( uint8_t ) 0x80 )
#define portMAX_PRIGROUP_BITS                 ( ( uint8_t ) 7 )
#define portPRIORITY_GROUP_MASK               ( 0x07UL << 8UL )
#define portPRIGROUP_SHIFT                    ( 8UL )

/* Masks off all bits but the VECTACTIVE bits in the ICSR register. */
#define portVECTACTIVE_MASK                   ( 0xFFUL )

#define portNVIC_INT_CTRL_REG     ( *( ( volatile uint32_t * ) 0xe000ed04 ) )
#define portNVIC_PENDSVSET_BIT    ( 1UL << 28UL )

static long g_lsystimetick = 0;
int getCanditask (void);

/* Scheduler utilities. */
#define portYIELD()                                 \
{                                                   \
		/* Set a PendSV to request a context switch. */ \
		portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT; \
		__asm( "	dsb");                                \
		__asm( "	isb");                                \
}
	

void trigger_pendsv(void) {
	portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
}

void init_pendsv_priority(void) {
    portNVIC_SHPR3_REG |= portNVIC_PENDSV_PRI;
}
void init_ticktimer_priority(void) {
    portNVIC_SHPR3_REG |= portNVIC_SYSTICK_PRI;
}

#define configSYSTICK_CLOCK_HZ  (74 * 1000 * 100)
#define configTICK_RATE_HZ      (1000) 
#define portMAX_24_BIT_NUMBER                 ( 0xffffffUL )
void vPortSetupTimerInterrupt( void )
{
	int ulTimerCountsForOneTick;
	int xMaximumPossibleSuppressedTicks; 
    /* Calculate the constants required to configure the tick interrupt. */
	{
			ulTimerCountsForOneTick = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ );
			xMaximumPossibleSuppressedTicks = portMAX_24_BIT_NUMBER / ulTimerCountsForOneTick;
			//ulStoppedTimerCompensation = portMISSED_COUNTS_FACTOR / ( configCPU_CLOCK_HZ / configSYSTICK_CLOCK_HZ );
	}

    /* Stop and clear the SysTick. */
    portNVIC_SYSTICK_CTRL_REG = 0UL;
    portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

    /* Configure SysTick to interrupt at the requested rate. */
    portNVIC_SYSTICK_LOAD_REG = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;
    portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT );
}
void nSysTick_Handler (void)
{
	g_lsystimetick++;
	// select candiate task
	getCanditask ();

	trigger_pendsv();
```

2. 创建任务。
需要配置任务的入口地址，任务的栈空间，且必须将栈空间初始化。下面的代码使用简单的方法
```c
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
```

3. 在 `nSysTick_Handler` 里触发 PendSV 中断
```c
void nSysTick_Handler (void) 
{
	trigger_pendsv();
}
```
4. 在 PendSV 里面进行线程切换，需要 2 个变量，一个是当前的任务控制块，用来保存当前的寄存器到它的任务栈。一个是候选的任务控制块，用来恢复寄存器的数据。
```asm
AREA    |.text|, CODE, READONLY
	EXPORT  PendSV_Handler
	IMPORT  pcurtasktcb
	IMPORT  pcanditasktcb
		
PendSV_Handler
; save r4 - r11 ---> stack
	mrs r0, psp
	STMDB r0!, {r4-r11}  ; r0 = new stack top; save to task ctx 
	
	ldr r1, =pcurtasktcb   ; pcurtasktcb = point to current task ctx
	ldr r3, [r1]           ; r3 = *pcurtasktcb
	str r0, [r3]           ; save new stack to 
	
; switch to new task
 	ldr r2, =pcanditasktcb  ; 
	ldr r2, [r2]  ; r2 = *pcanditasktcb
	str r2, [r1]  ; save 
	ldr r2, [r2]
	
	LDMIA r2!, {r4-r11}
	
	; update psp reg
	msr psp, r2
	BX  lr
	
	END
```

## 工程使用方法：
下载 keil v5.39，打开本工程，编译通过。点击仿真按钮进行仿真，可以在控制输出的窗口中看到循环打印。


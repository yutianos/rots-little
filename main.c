

#include "stm32f10x.h"                  // Device header
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "core_cm3.h"

int fputc(int ch, FILE *f)
{
  ITM_SendChar(ch);
  return ch;
}

int main(void)
{
		printf("hello wrold!\r\n");
}
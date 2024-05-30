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

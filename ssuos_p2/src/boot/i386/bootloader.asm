org	0x7c00   

[BITS 16]

START:   
		jmp		BOOT1_LOAD ;BOOT1_LOAD로 점프

BOOT1_LOAD:
	mov     ax, 0x0900 
        mov     es, ax
        mov     bx, 0x0

        mov     ah, 2	
        mov     al, 0x4		
        mov     ch, 0	
        mov     cl, 2	
        mov     dh, 0		
        mov     dl, 0x80
        int     0x13	
        jc      BOOT1_LOAD
		mov		ax, 0xb800
		mov		es, ax
		mov		ax, 0x0

LP:						;loop for clean
	mov		[es:bx], ax
	add		bx, 1
	cmp		bx, 80*25*2
	jne		LP

mov		ax, 0
mov		bx, 0
mov		cx, 0
mov		dx, 0
mov		si, 0			;reset index register

BOOT_MENU:				;show boot menu
	mov		ax, [ssuos_1+si]
	mov		ah, 0x07
	mov		cx, [ssuos_2+si]
	mov		ch, 0x07
	mov		dx,	[ssuos_3+si]
	mov		dh, 0x07
	mov		[es:bx], ax
	mov		[es:bx+0xa0], cx
	mov		[es:bx+0x140], dx
	add		si, 1
	add		bx, 2
	cmp		al, 0
	jne		BOOT_MENU


mov		ax, 0
mov		bx, 0
mov		cx, 0
mov		dx, 0
mov		si, 0
cmp		di, -1
je		CIRCLE_MARK_3
cmp		di, 1
je		CIRCLE_MARK_2
cmp		di, 2
je		CIRCLE_MARK_3

CIRCLE_MARK_1:
	mov		di, 0
	mov		ax, [select+si]
	mov		ah, 0x07
	mov		[es:bx], ax
	add		si, 1
	add		bx, 2
	cmp		al, 0
	je		KEY_INT
	jne		CIRCLE_MARK_1

CIRCLE_MARK_2:
	mov		di, 1
	mov		ax, [select+si]
	mov		ah, 0x07
	mov		[es:bx+0xa0], ax
	add		si, 1
	add		bx, 2
	cmp		al, 0
	je		KEY_INT
	jne		CIRCLE_MARK_2

CIRCLE_MARK_3:
	mov		di, 2
	mov		ax, [select+si]
	mov		ah, 0x07
	mov		[es:bx+0x140], ax
	add		si, 1
	add		bx, 2
	cmp		al, 0
	je		KEY_INT
	jne		CIRCLE_MARK_3

KEY_INT:
	mov		ax, 0
	mov		si, 0
	mov		bx, 0
	int		0x16
	cmp		ax, 0x4800
	je		UP_ARROW
	cmp		ax, 0x5000
	je		DOWN_ARROW
	cmp		ax, 0x1c0d
	je		KERNEL_SELECT
	jmp		KEY_INT

UP_ARROW:
	sub		di, 1
	cmp		di, -1
	je		LP
	cmp		di, 0
	je		LP
	cmp		di, 1
	je		LP
	jmp		KEY_INT

DOWN_ARROW:
	add		di, 1
	cmp		di, 3
	je		LP
	cmp		di, 2
	je		LP
	cmp		di, 1
	je		LP
	jmp		KEY_INT

KERNEL_LOAD:
	mov     ax, 0x1000	
        mov     es, ax		
        mov     bx, 0x0		

        mov     ah, 2		
        mov     al, 0x3f	
        mov     ch, 0		
        mov     cl, 0x6	
        mov     dh, 0     
        mov     dl, 0x80

        int     0x13
        jc      KERNEL_LOAD
jmp		0x0900:0x0000

KERNEL1_LOAD:
	mov		ax, 0x1000
		mov		es, ax
		mov		bx, 0x0

		mov		ah, 2
		mov		al, 0x3f
		mov		ch, 0x9
		mov		cl, 0x2f
		mov		dh, 0xe
		mov		dl, 0x80

		int		0x13
		jc		KERNEL1_LOAD
jmp		0x0900:0x0000

KERNEL2_LOAD:
	mov		ax, 0x1000
		mov		es, ax
		mov		bx, 0x0

		mov		ah, 2
		mov		al, 0x3f
		mov		ch, 0xe
		mov		cl, 0x7
		mov		dh, 0xe
		mov		dl, 0x80

		int 	0x13
		jc		KERNEL2_LOAD
jmp		0x0900:0x0000

KERNEL_SELECT:
	cmp		di, 0
	je		KERNEL_LOAD
	cmp		di, 1
	je		KERNEL1_LOAD
	cmp		di, 2
	je		KERNEL2_LOAD

select db "[O]",0
ssuos_1 db "[ ] SSUOS_1",0
ssuos_2 db "[ ] SSUOS_2",0
ssuos_3 db "[ ] SSUOS_3",0
ssuos_4 db "[ ] SSUOS_4",0
partition_num : resw 1

times   446-($-$$) db 0x00

PTE:
partition1 db 0x80, 0x00, 0x00, 0x00, 0x83, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x3f, 0x0, 0x00, 0x00
partition2 db 0x80, 0x00, 0x00, 0x00, 0x83, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x3f, 0x0, 0x00, 0x00
partition3 db 0x80, 0x00, 0x00, 0x00, 0x83, 0x00, 0x00, 0x00, 0x98, 0x3a, 0x00, 0x00, 0x3f, 0x0, 0x00, 0x00
partition4 db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
times 	510-($-$$) db 0x00
dw	0xaa55

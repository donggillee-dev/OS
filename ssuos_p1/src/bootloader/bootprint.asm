[BITS 16]
[org 0x7C00]

START:   
mov		ax, 0xb800
mov		es, ax
mov		ax, 0x00
mov		bx, 0
mov		cx, 80*25*2

CLS:
	mov		[es:bx], ax
	add		bx, 1
	loop	CLS

mov		bx, 0
mov		si, 0

LP:
mov		al, [msg+si]
mov		ah, 0x07
mov		[es:bx], ax
add		bx, 2
add		si, 1
cmp		al, 0
jne		LP

msg	db	"Hello, LeeDongGil's World", 0

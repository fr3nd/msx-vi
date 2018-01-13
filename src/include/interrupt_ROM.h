
#ifndef  __INTERRUPT_H__
#define  __INTERRUPT_H__

#define  DI               __asm di __endasm
#define  EI               __asm ei __endasm
#define  READ_VDP_STATUS  __asm in a,(#0x99) __endasm

/*
	To write your own ISR:

	void my_isr(void) interrupt {
		DI;
		READ_VDP_STATUS;
		...
		your code goes here
		...
		EI;
	}


	To install:   install_isr(my_isr);
	To uninstall: uninstall_isr();
*/

//extern unsigned int old_isr;

extern void install_isr(void (*isr)(void));
extern void uninstall_isr(void);

#endif

#include <arch/i386/idt.h>
#define IRQ_OFFSET 32

typedef void (irq_handler_t)(idt_reg_stack_t*);

void irq_init();
void irq_register(uint8_t num, irq_handler_t* handler);
void nmi_init();
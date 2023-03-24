#ifndef _MOB_IRQ_H_
#define _MOB_IRQ_H_

void __init mob_irq_init(void);
void __exit mob_irq_close(void);
int mob_irq_trigger(int id);

#endif 

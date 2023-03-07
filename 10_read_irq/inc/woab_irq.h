#ifndef _WOAB_IRQ_FILE_H_
#define _WOAB_IRQ_FILE_H_

int __init woab_irq_init(void);
void __exit woab_irq_close(void);
int woab_irq_trigger(int dev_idx);

#endif /* _WOAB_IRQ_FILE_H_ */
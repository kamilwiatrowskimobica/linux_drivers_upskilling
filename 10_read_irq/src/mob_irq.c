#include <linux/interrupt.h>
#include <linux/irq.h>
#include "mob_debug.h"
#include "mob_irq.h"

#define IRQ_NUM (11u)

static irqreturn_t mob_irq_handler(int irq, void *dev_id);
static void mob_irq_worker(unsigned long);
spinlock_t irq_spinlock;
struct tasklet_struct* tsklt;

static irqreturn_t mob_irq_handler(int irq, void *dev_id)
{
    MOB_PRINT("Shared IRQ: Interrupt Occurred");


	tasklet_schedule(tsklt);
    return IRQ_HANDLED;
}

void __init mob_irq_init(void)
{
    if (request_irq(IRQ_NUM, mob_irq_handler, IRQF_SHARED, "mob_device",
			(void *)(mob_irq_handler))) {
		MOB_PRINT("Can'a init IRQ\n");
		free_irq(IRQ_NUM, (void *)(mob_irq_handler));
		return;
	}

	tsklt = kmalloc(sizeof(struct tasklet_struct), GFP_KERNEL);
	if (tsklt == NULL) {
		MOB_PRINT("Can't init tasklet");
	}
	tasklet_init(tsklt, mob_irq_worker, 0);

	spin_lock_init(&irq_spinlock);
}

int mob_irq_trigger(int dev_idx)
{
	struct irq_desc *desc = NULL;
	unsigned long flags;

	desc = irq_data_to_desc(irq_get_irq_data(IRQ_NUM));
	if (!desc) {
		return -EINVAL;
	}

	spin_lock_irqsave(&irq_spinlock, flags);
	//status_registers |= (1 << dev_idx);
	spin_unlock_irqrestore(&irq_spinlock, flags);

	__this_cpu_write(vector_irq[59], desc);
	asm("int $0x3B"); // Corresponding to irq 11

	return 0;
}

void __exit mob_irq_close(void)
{
	kfree(tsklt);
    free_irq(IRQ_NUM, (void *)(mob_irq_handler));
}

static void mob_irq_worker(unsigned long)
{
	MOB_PRINT("Tasklet spin\n");
}
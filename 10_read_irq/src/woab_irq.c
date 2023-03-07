#include <linux/interrupt.h>
#include <linux/irq.h>
#include <woab_debug.h>

#define IRQ_NO 11

static irqreturn_t woab_irq_handler(int irq, void *dev_id);
static void woab_irq_worker(unsigned long);

static struct tasklet_struct *tasklet;
static int status_registers;
spinlock_t status_spinlock;

static irqreturn_t woab_irq_handler(int irq, void *dev_id)
{
	int status_cpy, i;
	spin_lock(&status_spinlock);
	status_cpy = status_registers;
	spin_unlock(&status_spinlock);
	if (0 == status_registers) {
		WINFO("Interrupt triggered by other device\n");
		return IRQ_NONE;
	}
	for (i = 0; i < sizeof(status_cpy); i++) {
		if (status_cpy & 1 << i) {
			WINFO("Interrupt occurred on device %d\n", i);
			status_cpy &= (~(1 << i));
		}
	}

	// clear bits
	spin_lock(&status_spinlock);
	status_registers = status_cpy;
	spin_unlock(&status_spinlock);

	tasklet_schedule(tasklet);
	return IRQ_HANDLED;
}

static void woab_irq_worker(unsigned long)
{
	WINFO("Do nothig, just demonstrate tasklet\n");
}

void __init woab_irq_init(void)
{
	if (request_irq(IRQ_NO, woab_irq_handler, IRQF_SHARED, "woab_device",
			(void *)(woab_irq_handler))) {
		WERROR("cannot register IRQ ");
		free_irq(IRQ_NO, (void *)(woab_irq_handler));
		return;
	}

	tasklet = kmalloc(sizeof(struct tasklet_struct), GFP_KERNEL);
	if (tasklet == NULL) {
		WERROR("cannot allocate tasklet");
	}
	tasklet_init(tasklet, woab_irq_worker, 0);

	spin_lock_init(&status_spinlock);
}

void __exit woab_irq_close(void)
{
	kfree(tasklet);
	free_irq(IRQ_NO, (void *)(woab_irq_handler));
}

int woab_irq_trigger(int dev_idx)
{
	struct irq_desc *desc = NULL;
	desc = irq_data_to_desc(irq_get_irq_data(11));
	if (!desc) {
		return -EINVAL;
	}

	spin_lock(&status_spinlock);
	status_registers |= (1 << dev_idx);
	spin_unlock(&status_spinlock);

	__this_cpu_write(vector_irq[59], desc);
	asm("int $0x3B"); // Corresponding to irq 11

	return 0;
}
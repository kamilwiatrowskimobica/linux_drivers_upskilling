#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/seq_file.h>

MODULE_LICENSE("GPL");

#define TRACE
#define DEBUG

#define WOAD_CHAR_DELAY (HZ * 1)
#define WOAB_DATA_CHAR 'w'
#define WOAB_TTY_MAJOR 240
#define WOAB_TTY_MINORS 8

#ifdef TRACE
#define WTRACE() printk(KERN_INFO "WOABtty: %s %d\n", __FUNCTION__, __LINE__)
#else
#define WTRACE()
#endif

#ifdef DEBUG
#define WDEBUG(fmt, args...) printk(KERN_DEBUG "WOABtty: " fmt, ##args)
#else
#define WDEBUG(fmt, args...)
#endif

struct woab_serial {
	struct tty_struct *tty;
	int open_count;
	struct mutex mutex;
	struct timer_list timer; // just to simulate there is something on input
};

static void woab_timer(struct timer_list *t);
static int woab_open(struct tty_struct *tty, struct file *file);
static void woab_internal_close(struct woab_serial *woab_tty);
static void woab_close(struct tty_struct *tty, struct file *file);
static int woab_write(struct tty_struct *tty, const unsigned char *buffer,
		      int count);
static unsigned int woab_write_room(struct tty_struct *tty);
static int woab_proc_show(struct seq_file *m, void *v);

static struct woab_serial *woab_ttys[WOAB_TTY_MINORS];
static struct tty_port woab_tty_port[WOAB_TTY_MINORS];
static struct tty_driver *woab_tty_driver;

static const struct tty_operations serial_ops = {
	.open = woab_open,
	.close = woab_close,
	.write = woab_write,
	.write_room = woab_write_room,
	.proc_show = woab_proc_show,
};

static void woab_timer(struct timer_list *t)
{
	struct woab_serial *woab_tty = from_timer(woab_tty, t, timer);
	struct tty_struct *tty;
	struct tty_port *port;
	char data[1] = { WOAB_DATA_CHAR };
	int data_size = 1;
	int i;

	WTRACE();

	if (!woab_tty)
		return;

	tty = woab_tty->tty;
	port = tty->port;

	// write single character from simulated hardware
	for (i = 0; i < data_size; ++i) {
		if (!tty_buffer_request_room(port, 1))
			tty_flip_buffer_push(port);
		tty_insert_flip_char(port, data[i], TTY_NORMAL);
	}
	tty_flip_buffer_push(port);

	// and start timer again so next time we see new character
	woab_tty->timer.expires = jiffies + WOAD_CHAR_DELAY;
	add_timer(&woab_tty->timer);
}

static int woab_open(struct tty_struct *tty, struct file *file)
{
	struct woab_serial *woab_tty;
	int index;

	WTRACE();

	tty->driver_data = NULL;

	index = tty->index;
	woab_tty = woab_ttys[index];
	if (woab_tty == NULL) {
		woab_tty = kmalloc(sizeof(*woab_tty), GFP_KERNEL);
		if (!woab_tty)
			return -ENOMEM;

		mutex_init(&woab_tty->mutex);
		woab_tty->open_count = 0;

		woab_ttys[index] = woab_tty;
	}

	mutex_lock(&woab_tty->mutex);

	tty->driver_data = woab_tty;
	woab_tty->tty = tty;

	woab_tty->open_count += 1;
	if (woab_tty->open_count == 1) {
		// create timer so in jext WOAD_CHAR_DELAY we will see something on input
		timer_setup(&woab_tty->timer, woab_timer, 0);
		woab_tty->timer.expires = jiffies + WOAD_CHAR_DELAY;
		add_timer(&woab_tty->timer);
	}

	mutex_unlock(&woab_tty->mutex);
	return 0;
}

static void woab_internal_close(struct woab_serial *woab_tty)
{
	WTRACE();

	mutex_lock(&woab_tty->mutex);

	if (woab_tty->open_count > 0) {
		woab_tty->open_count -= 1;
		if (woab_tty->open_count == 0) {
			// closed so remove our simulated hardware
			del_timer(&woab_tty->timer);
		}
	}

	mutex_unlock(&woab_tty->mutex);
}

static void woab_close(struct tty_struct *tty, struct file *file)
{
	struct woab_serial *woab_tty = tty->driver_data;

	WTRACE();

	if (woab_tty)
		woab_internal_close(woab_tty);
}

static int woab_write(struct tty_struct *tty, const unsigned char *buffer,
		      int count)
{
	struct woab_serial *woab_tty = tty->driver_data;
	int i;
	int retval = -EINVAL;

	WTRACE();

	if (!woab_tty)
		return -ENODEV;

	mutex_lock(&woab_tty->mutex);

	if (woab_tty->open_count) {
		// write something to demonstrate writting to HW
		WDEBUG("%s function wrote:\n", __FUNCTION__);
		for (i = 0; i < count; ++i)
			WDEBUG("0x%02x\n", buffer[i]);
	}
	mutex_unlock(&woab_tty->mutex);
	return retval;
}

static unsigned int woab_write_room(struct tty_struct *tty)
{
	struct woab_serial *woab_tty = tty->driver_data;
	int room = -EINVAL;

	WTRACE();

	if (!woab_tty)
		return -ENODEV;

	mutex_lock(&woab_tty->mutex);

	if (woab_tty->open_count) {
		room = 255;
	}

	mutex_unlock(&woab_tty->mutex);
	return room;
}

static int woab_proc_show(struct seq_file *m, void *v)
{
	struct woab_serial *woab_tty;
	int i;

	WTRACE();

	seq_printf(m, "tty's used so far:\n");
	for (i = 0; i < WOAB_TTY_MINORS; ++i) {
		woab_tty = woab_ttys[i];
		if (woab_tty == NULL)
			continue;

		seq_printf(m, "%d\n", i);
	}

	return 0;
}

static int __init woab_init(void)
{
	int retval;
	int i;

	WTRACE();

	woab_tty_driver = tty_alloc_driver(WOAB_TTY_MINORS, 0);
	if (!woab_tty_driver)
		return -ENOMEM;

	woab_tty_driver->owner = THIS_MODULE;
	woab_tty_driver->driver_name = "woab_tty";
	woab_tty_driver->name = "wtty";
	woab_tty_driver->major = WOAB_TTY_MAJOR,
	woab_tty_driver->type = TTY_DRIVER_TYPE_SERIAL,
	woab_tty_driver->subtype = SERIAL_TYPE_NORMAL,
	woab_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV,
	woab_tty_driver->init_termios = tty_std_termios;
	woab_tty_driver->init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	tty_set_operations(woab_tty_driver, &serial_ops);

	for (i = 0; i < WOAB_TTY_MINORS; i++) {
		tty_port_init(woab_tty_port + i);
		tty_port_link_device(woab_tty_port + i, woab_tty_driver, i);
	}

	retval = tty_register_driver(woab_tty_driver);
	if (retval) {
		WDEBUG("failed to register tty driver");
		tty_driver_kref_put(woab_tty_driver);
		return retval;
	}

	for (i = 0; i < WOAB_TTY_MINORS; ++i)
		tty_register_device(woab_tty_driver, i, NULL);

	return retval;
}

static void __exit woab_exit(void)
{
	struct woab_serial *woab_tty;
	int i;

	WTRACE();

	for (i = 0; i < WOAB_TTY_MINORS; ++i) {
		tty_unregister_device(woab_tty_driver, i);
		tty_port_destroy(woab_tty_port + i);
	}
	tty_unregister_driver(woab_tty_driver);

	tty_driver_kref_put(woab_tty_driver);

	for (i = 0; i < WOAB_TTY_MINORS; ++i) {
		woab_tty = woab_ttys[i];
		if (woab_tty) {
			while (woab_tty->open_count)
				woab_internal_close(woab_tty);

			del_timer(&woab_tty->timer);
			kfree(woab_tty);
			woab_ttys[i] = NULL;
		}
	}
}

module_init(woab_init);
module_exit(woab_exit);
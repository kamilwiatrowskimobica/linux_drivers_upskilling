#include <linux/init.h>          // Macros used to mark up functions e.g. __init __exit
#include <linux/kernel.h>        // printk
#include <linux/module.h>

#include <linux/tty_driver.h>
#include <linux/slab.h>
#include <linux/tty.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rafal Zielinski");
MODULE_DESCRIPTION("skeleton tty driver");
MODULE_VERSION("0.1");

#define NR_LINES	2
#define NR_DEVICES	2

struct tiny_serial {
	int open_count;
	struct mutex	mutex;
};

static struct tty_driver* my_driver;
static struct tiny_serial *tiny_table[NR_DEVICES];
static struct tty_port tiny_tty_port[NR_DEVICES];

static int dev_open(struct tty_struct *tty, struct file *file){
	int index;
	struct tiny_serial *tiny;
 	
 	index = tty->index;
 	tiny = tiny_table[index];
 	
 	printk(KERN_INFO "rzetty: opening dev: %d\n", index);

	mutex_lock(&tiny->mutex);
	
	++tiny->open_count;
	if (tiny->open_count == 1) {
		/* this is the first time this port is opened */
		/* do any hardware initialization needed here */
	}
	
	mutex_unlock(&tiny->mutex);

	return 0;
}

static void dev_close(struct tty_struct *tty, struct file *file){
	int index;
	struct tiny_serial *tiny;
	
	index = tty->index;
 	tiny = tiny_table[index];
 	
 	printk(KERN_INFO "rzetty: closing dev: %d\n", index);
 	
 	mutex_lock(&tiny->mutex);

	--tiny->open_count;
	if (tiny->open_count <= 0) {
		/* The port is being closed by the last user. */
		/* Do any hardware specific stuff here */
	}
	
	mutex_unlock(&tiny->mutex);
}

static int dev_write(struct tty_struct *tty,
		      const unsigned char *buffer, int count)
{
	struct tiny_serial *tiny;
	int index,i;
	int retval = -EINVAL;
	
	index = tty->index;
 	tiny = tiny_table[index];
 	
 	printk(KERN_INFO "rzetty: writing size %d to dev: %d\n", count, index);

	if (!tiny)
		return -ENODEV;

	mutex_lock(&tiny->mutex);

	if (!tiny->open_count)
		/* port was not opened */
		goto exit;

	/* fake sending the data out a hardware port by
	 * writing it to the kernel debug log.
	 */
	pr_debug("%s - ", __func__);
	for (i = 0; i < count; ++i)
		pr_info("%02x ", buffer[i]);
	pr_info("\n");
	
	retval = count;

exit:
	mutex_unlock(&tiny->mutex);
	return retval;
}

static unsigned int dev_write_room(struct tty_struct *tty){
	struct tiny_serial *tiny;
	int index;
	int room = -EINVAL;
	
	index = tty->index;
 	tiny = tiny_table[index];
 	
	if (!tiny)
		return -ENODEV;

	mutex_lock(&tiny->mutex);

	if (!tiny->open_count) {
		/* port was not opened */
		goto exit;
	}

	/* calculate how much room is left in the device */
	room = 255;
	
	printk(KERN_INFO "rzetty: write room: %d for dev: %d\n", room, index);


exit:
	mutex_unlock(&tiny->mutex);
	return room;
}

#define RELEVANT_IFLAG(iflag) ((iflag) & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))
static void dev_set_termios(struct tty_struct *tty, struct ktermios *old_termios){
	unsigned int cflag;

	cflag = tty->termios.c_cflag;

	/* check that they really want us to change something */
	if (old_termios) {
		if ((cflag == old_termios->c_cflag) &&
		    (RELEVANT_IFLAG(tty->termios.c_iflag) ==
		     RELEVANT_IFLAG(old_termios->c_iflag))) {
			pr_debug(" - nothing to change...\n");
			return;
		}
	}

	/* get the byte size */
	switch (cflag & CSIZE) {
	case CS5:
		pr_debug(" - data bits = 5\n");
		break;
	case CS6:
		pr_debug(" - data bits = 6\n");
		break;
	case CS7:
		pr_debug(" - data bits = 7\n");
		break;
	default:
	case CS8:
		pr_debug(" - data bits = 8\n");
		break;
	}

	/* determine the parity */
	if (cflag & PARENB)
		if (cflag & PARODD)
			pr_debug(" - parity = odd\n");
		else
			pr_debug(" - parity = even\n");
	else
		pr_debug(" - parity = none\n");

	/* figure out the stop bits requested */
	if (cflag & CSTOPB)
		pr_debug(" - stop bits = 2\n");
	else
		pr_debug(" - stop bits = 1\n");

	/* figure out the hardware flow control settings */
	if (cflag & CRTSCTS)
		pr_debug(" - RTS/CTS is enabled\n");
	else
		pr_debug(" - RTS/CTS is disabled\n");

	/* determine software flow control */
	/* if we are implementing XON/XOFF, set the start and
	 * stop character in the device */
	if (I_IXOFF(tty) || I_IXON(tty)) {
		unsigned char stop_char  = STOP_CHAR(tty);
		unsigned char start_char = START_CHAR(tty);

		/* if we are implementing INBOUND XON/XOFF */
		if (I_IXOFF(tty))
			pr_debug(" - INBOUND XON/XOFF is enabled, "
				"XON = %2x, XOFF = %2x", start_char, stop_char);
		else
			pr_debug(" - INBOUND XON/XOFF is disabled");

		/* if we are implementing OUTBOUND XON/XOFF */
		if (I_IXON(tty))
			pr_debug(" - OUTBOUND XON/XOFF is enabled, "
				"XON = %2x, XOFF = %2x", start_char, stop_char);
		else
			pr_debug(" - OUTBOUND XON/XOFF is disabled");
	}

	/* get the baud rate wanted */
	pr_debug(" - baud rate = %d", tty_get_baud_rate(tty));
}

static struct tty_operations serial_ops = {
	.open = dev_open,
	.close = dev_close,
	.write = dev_write,
	.write_room = dev_write_room,
	.set_termios = dev_set_termios,
};

static int __init rzettydrv_init(void){
        
        int result = -ENOMEM;
        int i=0;
        struct tiny_serial *tiny;

	printk(KERN_INFO "rzetty: allocatting tty %d\n", NR_LINES);
	my_driver = tty_alloc_driver(NR_LINES, 0);
	if (IS_ERR_OR_NULL(my_driver)) {
		printk(KERN_INFO "rzetty: error allocatting driver lines: %d\n",i);
		return -ENOMEM;
	}

        //req fields: driver_name, name, type, subtype, init_termios, and ops.
	my_driver->owner = THIS_MODULE;
	my_driver->driver_name = "rzetty";
 	my_driver->name = "rzetty";
 	my_driver->type = TTY_DRIVER_TYPE_SERIAL,
 	my_driver->subtype = SERIAL_TYPE_NORMAL,
 	my_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV,
 	my_driver->init_termios = tty_std_termios;
 	my_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
 	tty_set_operations(my_driver, &serial_ops);

	result = tty_register_driver(my_driver);
	if (result) {
 		printk(KERN_ERR "rzetty: error %i registering tty driver", result);
 		tty_driver_kref_put(my_driver);
 		return result;
	}
	printk(KERN_INFO "rzetty: driver registerred\n");

	for(i =0; i < NR_DEVICES; i++){
		printk(KERN_INFO "rzetty: allocatting tty dev %d\n", i);
		tiny = kmalloc(sizeof(*tiny), GFP_KERNEL);
		if (!tiny)
			return -ENOMEM;

		mutex_init(&tiny->mutex);
		tiny->open_count = 0;
		tiny_table[i] = tiny;
	
		tty_port_init(tiny_tty_port + i);
		tty_port_link_device(tiny_tty_port + i, my_driver, i);
		tty_register_device(my_driver, i, NULL);
	}

	return result;
}

static void __exit rzettydrv_exit(void){
	struct tiny_serial *tiny;
	int i;
	for (i = 0; i < NR_DEVICES; ++i){
		tiny = tiny_table[i];
		if (tiny) {
			tty_port_destroy(tiny_tty_port + i);
			
			kfree(tiny);
			tiny_table[i] = NULL;
		}
		tty_unregister_device(my_driver, i);	
	}
	tty_unregister_driver(my_driver);
	
	tty_driver_kref_put(my_driver);
	
	printk(KERN_INFO "rzetty LKM unloaded!\n");
	return;
}

module_init(rzettydrv_init);
module_exit(rzettydrv_exit);

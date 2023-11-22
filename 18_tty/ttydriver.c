#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/serial.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/wait.h>

#define TTYDRIVER_MINORS 1
#define TTYDRIVER_MAJOR 100
#define RELEVANT_IFLAG(iflag) ((iflag) & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

static struct tty_driver* ttydriver;
static struct serial_data* serial_table[TTYDRIVER_MINORS];
static struct tty_port ttyport[TTYDRIVER_MINORS];

static struct serial_data
{
    struct tty_struct* tty;
    int open_count;
    struct mutex mutex;
    struct timer_list timer;
};

static int ttydev_open(struct tty_struct* tty, struct file* file);
static void ttydev_close(struct tty_struct* tty, struct file* file);
static int ttydev_write(struct tty_struct* tty, const unsigned char* buffer, int count);
static unsigned int ttydev_write_room(struct tty_struct* tty);
static void ttydev_set_termios(struct tty_struct* tty, struct ktermios* old_termios);

static struct tty_operations serial_ops =
{
    .open = ttydev_open,
    .close = ttydev_close,
    .write = ttydev_write,
    .write_room = ttydev_write_room,
    .set_termios = ttydev_set_termios
};

static int ttydev_write(struct tty_struct* tty, const unsigned char* buffer, int count)
{
    printk(KERN_INFO "TtyDriver: Write\n");

    struct serial_data* serial = tty->driver_data;
    int i;

    if (!serial)
        return -ENODEV;
    
    mutex_lock(&serial->mutex);

    if (!serial->open_count)
    {
        mutex_unlock(&serial->mutex);
        return -EINVAL;
    }

    for (i = 0; i < count; ++i)
        printk(KERN_INFO "TtyDriver: %02x\n", buffer[i]);
    
    printk(KERN_INFO "\n");

    mutex_unlock(&serial->mutex);

    return -EINVAL;
}

static unsigned int ttydev_write_room(struct tty_struct* tty)
{
    printk(KERN_INFO "TtyDriver: Write room\n");

    struct serial_data* serial = tty->driver_data;
    int room;

    if (!serial)
        return -ENODEV;
    
    mutex_lock(&serial->mutex);
    serial->open_count ? room = 255 : -EINVAL;
    mutex_unlock(&serial->mutex);

    return room;
}

static void ttydev_set_termios(struct tty_struct* tty, struct ktermios* old_termios)
{
    printk(KERN_INFO "TtyDriver: Set termios\n");

    unsigned int cflag;
    cflag = tty->termios.c_cflag;

    if (old_termios)
    {
        if ((cflag == old_termios->c_cflag) &&
            (RELEVANT_IFLAG(tty->termios.c_iflag) == RELEVANT_IFLAG(old_termios->c_iflag)))
        {
            printk(KERN_INFO "TtyDriver: Line settings not changed\n");
        }
    }
}

static void serial_timer(struct timer_list* tl)
{
    printk(KERN_INFO "TtyDriver: Fake timeout\n");
}

static int ttydev_open(struct tty_struct* tty, struct file* file)
{
    printk(KERN_INFO "TtyDriver: Open\n");

    struct serial_data* serial;
    uint8_t index;

    if (!tty)
        return -ENODEV;

    tty->driver_data = NULL;

    index = tty->index;
    serial = serial_table[index];
    if (!serial)
    {
        serial = kmalloc(sizeof(*serial), GFP_KERNEL);
        if (!serial)
            return -ENOMEM;
        mutex_init(&serial->mutex);
        serial->open_count = 0;
        serial_table[index] = serial;
    }

    mutex_lock(&serial->mutex);

    tty->driver_data = serial;
    serial->tty = tty;

    ++serial->open_count;
    if (serial->open_count == 1)
    {
        timer_setup(&serial->timer, serial_timer, 0);
        serial->timer.expires = jiffies + 5;
        add_timer(&serial->timer);
    }

    mutex_unlock(&serial->mutex);

    return -ENODEV; // CHECK
}

static void ttydev_close(struct tty_struct* tty, struct file* file)
{
    printk(KERN_INFO "TtyDriver: Close\n");

    if (!tty)
        return;

    struct serial_data* serial = tty->driver_data;

    if (!serial)
        return;

    mutex_lock(&serial->mutex);

    if (!serial->open_count)
    {
        mutex_unlock(&serial->mutex);
        return;
    }

    --serial->open_count;

    if (serial->open_count <= 0)
        del_timer(&serial->timer);

    mutex_unlock(&serial->mutex);
}

static int __init ttydev_init(void)
{
    printk(KERN_INFO "TtyDriver: Init\n");

    int i, result;

    ttydriver = tty_alloc_driver(TTYDRIVER_MINORS, 0);
    if (!ttydriver)
        return -ENOMEM;
    
    ttydriver->owner = THIS_MODULE;
    ttydriver->driver_name = "ttydriver"; // driver name under /proc/tty/drivers
    ttydriver->name = "ttydevice"; // node name under /dev
    ttydriver->major = TTYDRIVER_MAJOR;
    ttydriver->type = TTY_DRIVER_TYPE_SERIAL;
    ttydriver->subtype = SERIAL_TYPE_NORMAL;
    ttydriver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
    ttydriver->init_termios = tty_std_termios;
    ttydriver->init_termios.c_iflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    tty_set_operations(ttydriver, &serial_ops);

    for (i = 0; i < TTYDRIVER_MINORS; ++i)
    {
        tty_port_init(ttyport + i);
        tty_port_link_device(ttyport + i, ttydriver, i);
    }

    result = tty_register_driver(ttydriver); // creates sysfs tty files for device
    if (result)
    {
        printk(KERN_INFO "TtyDriver: Driver registration failed\n");
        tty_driver_kref_put(ttydriver);
        return result;
    }

    for (i = 0; i < TTYDRIVER_MINORS; ++i)
        tty_register_device(ttydriver, i, NULL);

    return result;
}

static void __exit ttydev_exit(void)
{
    printk(KERN_INFO "TtyDriver: Exit\n");

    int i;
    for (i = 0; i < TTYDRIVER_MINORS; ++i)
        tty_unregister_device(ttydriver, i);
    tty_unregister_driver(ttydriver);    
}

module_init(ttydev_init);
module_exit(ttydev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A B");
MODULE_DESCRIPTION("Tty driver");
MODULE_VERSION("1.0");

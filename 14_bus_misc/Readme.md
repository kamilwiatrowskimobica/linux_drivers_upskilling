# Linux sysfs demo

## Content

* woab_bus.c - **bus** and **device** implementation
* woab_misc.c - **driver** and **miscdevice** implementation
* Makefile

## Environment

* Windows Subsystem for Linux (WSL) 2.0
* Own kernel compiled to support modules loading

## Compilation

Compile using `make`

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc#
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# make
make -C /home/wa/WSL2-Linux-Kernel/  M=/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc modules
make[1]: Entering directory '/home/wa/WSL2-Linux-Kernel'
  CC [M]  /mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc/woab_bus.o
  CC [M]  /mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc/woab_misc.o
make[2]: Warning: File '/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc/modules.order' has modification time 0.0013 s in the future
  MODPOST /mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc/Module.symvers
make[3]: Warning: File '/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc/woab_bus.mod.c' has modification time 0.012 s in the future
  CC [M]  /mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc/woab_bus.mod.o
  LD [M]  /mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc/woab_bus.ko
  BTF [M] /mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc/woab_bus.ko
  CC [M]  /mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc/woab_misc.mod.o
  LD [M]  /mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc/woab_misc.ko
  BTF [M] /mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc/woab_misc.ko
make[3]: warning:  Clock skew detected.  Your build may be incomplete.
make[2]: warning:  Clock skew detected.  Your build may be incomplete.
make[1]: Leaving directory '/home/wa/WSL2-Linux-Kernel'
```

Clean using `make clean`

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# make clean
make -C /home/wa/WSL2-Linux-Kernel/  M=/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc clean
make[1]: Entering directory '/home/wa/WSL2-Linux-Kernel'
make[1]: Leaving directory '/home/wa/WSL2-Linux-Kernel'
```

## Overview
```
             +-----------------------------------------------------------------------------------------------------------------+
             |                                                                                                                 |
             |                                                                                                                 |
             v                                                                                                                 v
+------------+--------------+         +------------+--------------+         +-------------+-------------+         +---------------------------+
|            |              |         |            |              |         |             |             |         |            |              |
|            |              |         |            |              |         |             |             |         |            |              |
|          Device           |<------->|           Bus             |<------->|          Driver           |-------->|        miscdevice         |
|            |    kernel,   |         |            |              |         |             |    kernel,  |         |            |              |
|   sysfs    |   internal   |         |   sysfs    |    kernel    |         |   kernel    |   internal  |         |  internal  |    kernel    |
+-----+------+------+-------+         +-----+------+------+-------+         +-----+-------+------+------+         +-----+------+------+-------+
      |             |                       |             |                       |              |                      |             |
      +--type       +--dev                  +--add        +--probe                +--probe       +--type                +--dev        +--open
      +--version    +--buffer               +--del        +--remove               +--remove      +--driver              +--misc       +--read
                    +--uevent                             +--match                                                                    +--write
                    +--release                                                                                                        +--release
```

Demo system consists of **device** and a **driver** both attached to **bus** with support of **miscdevice** to communicate with a **device** of given *type* and *version*. 

**Bus** supports *sysfs* *add* and *del* operations which allow to add and remove **devices** from **bus** manually. **Bus** is named *woab*. Moreover **bus** implements standard **bus** operations requred by kernel like *probe*, *remove* and *match*.

**Device** internally contains *type*, *version* and *buffer* which have no meaning other than purpose of this tutorial. *Type* is a string and *version* is integer. *Buffer* is used to simulate read and write operations to **device**. *Type* and *version* properites could be read from 
filesystem while *buffer* is accessible only via **miscdevice** created by **driver** in certain circumstances (*type* and *version* matches). Additionaly **device** store kernel 
related `struct device dev`. *Release* and *uevent* functions are neede by kernel to support *udev* and resource cleanup.

**Driver** handles *probe* function which is used to determine if **device** is supported by **driver**. This step is typically required because not all **devices** attached to **bus** have to be supported by particular **driver**. For demo purposes *type* for **device** and **driver** have to match and *version* of **device** has to be less or equal 1. *Remove* functions is used once **device** is unbinded from **driver**. *Type* is internal field used to match with **devices**. Additionally **driver** store kernel related `struct device_driver driver`. **Driver** is named *woab_misc*.

**Miscdevice** is created by **driver** for **devices** matching requirements with regular *open*, *read*, *write* and *release* support. Once **driver** added **miscdevice** communication with **device** shall be possible via created **miscdevice**. 

Goal is to add **device** of *type* and *version* which **driver** supports and then communicate with **device** using **misdevice** created via **driver**.

## Bus implementation

### Bus core

To implement **bus** core `struct bus_type` has to be filled
```
static int woab_bus_match(struct device *dev, struct device_driver *driver);
static int woab_bus_probe(struct device *dev);
static void woab_bus_remove(struct device *dev);

struct bus_type woab_bus_type = {
        .name   = "woab",
        .match  = woab_bus_match,
        .probe  = woab_bus_probe,
        .remove = woab_bus_remove,
};
```

and registered on module init `bus_register(&woab_bus_type)`. With this implementation **bus** is registered in the system but there is no possibility to add **devices** manually using *sysfs*.

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# insmod woab_bus.ko
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/bus/woab/
total 0
drwxr-xr-x 2 root root    0 Mar 24 07:27 devices
drwxr-xr-x 2 root root    0 Mar 24 07:27 drivers
-rw-r--r-- 1 root root 4096 Mar 24 07:27 drivers_autoprobe
--w------- 1 root root 4096 Mar 24 07:27 drivers_probe
--w------- 1 root root 4096 Mar 24 07:27 uevent
```

### Bus add / del support

To support *add* and *del* operations it is needed to define them inside a `struct bus_attribute` with assosiated handlers

```
static ssize_t woab_del_store(struct bus_type *bt, const char *buf, size_t count);
static ssize_t woab_add_store(struct bus_type *bt, const char *buf, size_t count);

struct bus_attribute bus_attr_add = {
        .attr.name = "add", 
        .attr.mode = 0644,
        .show = NULL,
        .store = woab_add_store,
};

struct bus_attribute bus_attr_del = {
        .attr.name = "del", 
        .attr.mode = 0644,
        .show = NULL,
        .store = woab_del_store,
};
```

Once defined needs to be grouped together

```
static struct attribute *woab_bus_attrs[] = {
        &bus_attr_add.attr,
        &bus_attr_del.attr,
        NULL
};

ATTRIBUTE_GROUPS(woab_bus);
```

Finally `woab_bus_groups` could be used in **bus** definition

```
struct bus_type woab_bus_type = {
        .name   = "woab",
        .match  = woab_bus_match,
        .probe  = woab_bus_probe,
        .remove = woab_bus_remove,
        .bus_groups = woab_bus_groups,
};
```

## Device implementation

### Device kernel support

**Device** needs to implement two functions and fill `struct device_type`

```
static int woab_dev_uevent(struct device *dev, struct kobj_uevent_env *env);
static void woab_dev_release(struct device *dev);

struct device_type woab_device_type = {
        .uevent  = woab_dev_uevent,
        .release = woab_dev_release,
};
```

*release* method is used to free resources and *uevent* to support *udev*.

### Device version and type sysfs file support

To support *version* and *type* files on filesystem handler functions and atributes has to be defined:
```
static ssize_t woab_type_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t woab_version_show(struct device *dev, struct device_attribute *attr, char *buf);

DEVICE_ATTR(type, 0444, woab_type_show, NULL);
DEVICE_ATTR(version, 0444, woab_version_show, NULL);

static struct attribute *woab_dev_attrs[] = {
        &dev_attr_type.attr,
        &dev_attr_version.attr,
        NULL
};

ATTRIBUTE_GROUPS(woab_dev);
```

Finally `woab_dev_groups` could be used in **device** kernel definition

```
struct device_type woab_device_type = {
        .groups  = woab_dev_groups,
        .uevent  = woab_dev_uevent,
        .release = woab_dev_release,
};
```

## Connecting device and bus

### Assosiating device with bus

Above implementation is not complete because **bus** does not know how to *add* **device**. For this reason helper function was created `static int woab_bus_add_device(const char *name, const char *type, int version)` which:
* allocates memory for **device** of exact **device type** `struct woab_device`
* fill **device** related information (*type* and *version* from commandline) 
* connects **device** to the **bus** `woab_dev->dev.bus = &woab_bus_type`
* tells **device** to be **device of given type** `woab_dev->dev.type = &woab_device_type`
* assigns **device** name `dev_set_name(&woab_dev->dev, "%s", name)`
* register **device** in the linux `return device_register(&woab_dev->dev)`

Please note that **bus** can implement mutiple *add* functions for various **devices**, that is why current **device** needs to be connected with exact **device type** - both, allocated structure and supported filesystem operations. **Device type** is a structure in code `struct device_type` which describes **device** in linux and has nothing common with *type* used by **driver** to match **devices**. 

Corresponding `static int woab_bus_del_device(const char *name)` functions was implemented to:
* unregister **device** `device_unregister(dev)`
* reduce reference count `put_device(dev)`

It is worth noting that resources assositated with **device** are released in **device** related function `static void woab_dev_release(struct device *dev)`.

## Bus and device testing

```
+------------+--------------+         +------------+--------------+
|            |              |         |            |              |
|            |              |         |            |              |
|          Device           |<------->|           Bus             |
|            |    kernel,   |         |            |              |
|   sysfs    |   internal   |         |   sysfs    |    kernel    |
+-----+------+------+-------+         +-----+------+------+-------+
      |             |                       |             |        
      +--type       +--dev                  +--add        +--probe 
      +--version    +--buffer               +--del        +--remove
                    +--uevent                             +--match 
                    +--release                                     
```

At this point **bus** is able to handle *adding* / *deleting* devices of `struct woab_device` type.

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# insmod woab_bus.ko
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/bus/woab/
total 0
-rw-r--r-- 1 root root 4096 Mar 24 07:43 add
-rw-r--r-- 1 root root 4096 Mar 24 07:43 del
drwxr-xr-x 2 root root    0 Mar 24 07:43 devices
drwxr-xr-x 2 root root    0 Mar 24 07:43 drivers
-rw-r--r-- 1 root root 4096 Mar 24 07:43 drivers_autoprobe
--w------- 1 root root 4096 Mar 24 07:43 drivers_probe
--w------- 1 root root 4096 Mar 24 07:43 uevent
```

*Adding* **devices**

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# echo "dev1 type_a 1" > /sys/bus/woab/add
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# echo "dev2 type_b 2" > /sys/bus/woab/add
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/bus/woab/devices/
total 0
lrwxrwxrwx 1 root root 0 Mar 24 07:44 dev1 -> ../../../devices/dev1
lrwxrwxrwx 1 root root 0 Mar 24 07:44 dev2 -> ../../../devices/dev2
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/devices/ | grep dev
drwxr-xr-x  2 root root 0 Mar 24 07:44 dev1
drwxr-xr-x  2 root root 0 Mar 24 07:44 dev2
```

Checking properties
```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /sys/devices/dev1/version
1
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /sys/devices/dev1/type
type_a
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /sys/devices/dev2/version
2
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /sys/devices/dev2/type
type_b
```

*Deleting* **devices**
```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# echo "dev1" > /sys/bus/woab/del
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/bus/woab/devices/
total 0
lrwxrwxrwx 1 root root 0 Mar 24 07:44 dev2 -> ../../../devices/dev2
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/devices/ | grep dev
drwxr-xr-x  2 root root 0 Mar 24 07:44 dev2
```

## Driver support

### Implementing miscdevice

Support for **miscdevice** is quite simple. It is very similar to character device. *Open*, *read*, *write* and *release* functions has to be implemented and connected in `struct file_operations` structure to fulfill linux requirements.

```
static int woab_misc_open(struct inode *inode, struct file *file);
static int woab_misc_release(struct inode *inode, struct file *file);
static ssize_t woab_misc_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset);
static ssize_t woab_misc_write(struct file *file, const char __user *user_buffer, size_t size, loff_t *offset);

struct file_operations woab_misc_fops = {
	.owner = THIS_MODULE,
	.open = woab_misc_open,
	.read = woab_misc_read,
	.write = woab_misc_write,
	.release = woab_misc_release,
};
```

### Implementing driver

**Driver** is described by following `struct woab_driver`
```
int woab_misc_probe(struct woab_device *dev);
void woab_misc_remove(struct woab_device *dev);

struct woab_driver woab_misc_driver = {
	.type = "misc",
	.probe = woab_misc_probe,
	.remove = woab_misc_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "woab_misc",
	},
};
```

*Type* is used for internal usage to match with **device**. *Probe* and *remove* functions are exeuted once **bus** matches **device** *type* with a *type* defined in **driver**.

### Connecting device, driver and miscdevice

**Driver's** internal structure to connect **miscdevice** with **device** is represented by `struct woab_device`

```
struct woab_misc_device {
	struct miscdevice misc;
	struct woab_device *dev;
};
```

Is used to support **device** of `struct woab_device` of *type* which match **driver** *type* and *version* with previousily prepared implementation of `struct miscdevice` handled by `struct file_operations woab_misc_fops`. In **driver** *probe* function for **devices** matching *version* we need to allocate memory for `struct woab_misc_device` and connects **device** with **miscdevice**.

```
	woab_md = kzalloc(sizeof(*woab_md), GFP_KERNEL);

	woab_md->misc.minor = MISC_DYNAMIC_MINOR;
	snprintf(buf, sizeof(buf), "woab-misc-%d", woab_misc_count++);
	woab_md->misc.name = kstrdup(buf, GFP_KERNEL);
	woab_md->misc.parent = &dev->dev;
	woab_md->misc.fops = &woab_misc_fops;
	woab_md->dev = dev;
	dev_set_drvdata(&dev->dev, woab_md);

	ret = misc_register(&woab_md->misc);
```

Above step leads to creating connection between **device** and **miscdevice** once *probe* function is executed without errors.

```
             +-----------------------------------------------------------------------------------------------------------------+
             |                                                                                                                 |
             |                                                                                                                 |
             v                                                                                                                 v
+------------+--------------+                                               +-------------+-------------+         +---------------------------+
|            |              |                                               |             |             |         |            |              |
|            |              |                                               |             |             |         |            |              |
|          Device           |                                               |          Driver           |-------->|        miscdevice         |
|            |    kernel,   |                                               |             |    kernel,  |         |            |              |
|   sysfs    |   internal   |                                               |   kernel    |   internal  |         |  internal  |    kernel    |
+-----+------+------+-------+                                               +-----+-------+------+------+         +-----+------+------+-------+
      |             |                                                             |              |                      |             |
      +--type       +--dev                                                        +--probe       +--type                +--dev        +--open
      +--version    +--buffer                                                     +--remove      +--driver              +--misc       +--read
                    +--uevent                                                                                                         +--write
                    +--release                                                                                                        +--release
```

## Connecting bus with driver

At this point two separate subsystems are implemented as two different modules. **Bus** + **Device**

```
+------------+--------------+         +------------+--------------+
|            |              |         |            |              |
|            |              |         |            |              |
|          Device           |<------->|           Bus             |
|            |    kernel,   |         |            |              |
|   sysfs    |   internal   |         |   sysfs    |    kernel    |
+-----+------+------+-------+         +-----+------+------+-------+
      |             |                       |             |        
      +--type       +--dev                  +--add        +--probe 
      +--version    +--buffer               +--del        +--remove
                    +--uevent                             +--match 
                    +--release                                     
```

And **Driver** + **miscdevice** + **device**

```
             +-----------------------------------------------------------------------------------------------------------------+
             |                                                                                                                 |
             |                                                                                                                 |
             v                                                                                                                 v
+------------+--------------+                                               +-------------+-------------+         +---------------------------+
|            |              |                                               |             |             |         |            |              |
|            |              |                                               |             |             |         |            |              |
|          Device           |                                               |          Driver           |-------->|        miscdevice         |
|            |    kernel,   |                                               |             |    kernel,  |         |            |              |
|   sysfs    |   internal   |                                               |   kernel    |   internal  |         |  internal  |    kernel    |
+-----+------+------+-------+                                               +-----+-------+------+------+         +-----+------+------+-------+
      |             |                                                             |              |                      |             |
      +--type       +--dev                                                        +--probe       +--type                +--dev        +--open
      +--version    +--buffer                                                     +--remove      +--driver              +--misc       +--read
                    +--uevent                                                                                                         +--write
                    +--release                                                                                                        +--release
```

To connect them togerther **Bus** has to implement own *register* function

```
int woab_register_driver(struct woab_driver *drv);
void woab_unregister_driver(struct woab_driver *drv);
```

And exports them outside module

```
EXPORT_SYMBOL(woab_register_driver);
EXPORT_SYMBOL(woab_unregister_driver);
```

As a final step **Driver** has to connect to **bus** calling them during initialisation:

```
err = woab_register_driver(&woab_misc_driver);
```

Unregister on module exit

```
woab_unregister_driver(&woab_misc_driver);
```

Attaching **driver** to **bus** is implemented via

```
drv->driver.bus = &woab_bus_type;
ret = driver_register(&drv->driver);
```

Simply **bus** is assigned to **driver's** bus pointer and then **driver** is registered inside linux to give full picture

```
             +-----------------------------------------------------------------------------------------------------------------+
             |                                                                                                                 |
             |                                                                                                                 |
             v                                                                                                                 v
+------------+--------------+         +------------+--------------+         +-------------+-------------+         +---------------------------+
|            |              |         |            |              |         |             |             |         |            |              |
|            |              |         |            |              |         |             |             |         |            |              |
|          Device           |<------->|           Bus             |<------->|          Driver           |-------->|        miscdevice         |
|            |    kernel,   |         |            |              |         |             |    kernel,  |         |            |              |
|   sysfs    |   internal   |         |   sysfs    |    kernel    |         |   kernel    |   internal  |         |  internal  |    kernel    |
+-----+------+------+-------+         +-----+------+------+-------+         +-----+-------+------+------+         +-----+------+------+-------+
      |             |                       |             |                       |              |                      |             |
      +--type       +--dev                  +--add        +--probe                +--probe       +--type                +--dev        +--open
      +--version    +--buffer               +--del        +--remove               +--remove      +--driver              +--misc       +--read
                    +--uevent                             +--match                                                                    +--write
                    +--release                                                                                                        +--release
```

## Usecase

### Add various devices

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# insmod woab_bus.ko
```

**Bus** module loaded

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# echo "dev1 type_a 1" > /sys/bus/woab/add
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# echo "dev2 type_b 2" > /sys/bus/woab/add
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# echo "dev3 misc 3" > /sys/bus/woab/add
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# echo "dev4 misc 1" > /sys/bus/woab/add
```

Four **devices** added

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/bus/woab/devices/
total 0
lrwxrwxrwx 1 root root 0 Mar 24 12:32 dev1 -> ../../../devices/dev1
lrwxrwxrwx 1 root root 0 Mar 24 12:32 dev2 -> ../../../devices/dev2
lrwxrwxrwx 1 root root 0 Mar 24 12:32 dev3 -> ../../../devices/dev3
lrwxrwxrwx 1 root root 0 Mar 24 12:32 dev4 -> ../../../devices/dev4
```

**Devices** visible under *linux bus* subsystem.

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/devices/dev*
/sys/devices/dev1:
total 0
lrwxrwxrwx 1 root root    0 Mar 24 12:38 subsystem -> ../../bus/woab
-r--r--r-- 1 root root 4096 Mar 24 12:36 type
-rw-r--r-- 1 root root 4096 Mar 24 12:38 uevent
-r--r--r-- 1 root root 4096 Mar 24 12:36 version

/sys/devices/dev2:
total 0
lrwxrwxrwx 1 root root    0 Mar 24 12:38 subsystem -> ../../bus/woab
-r--r--r-- 1 root root 4096 Mar 24 12:36 type
-rw-r--r-- 1 root root 4096 Mar 24 12:38 uevent
-r--r--r-- 1 root root 4096 Mar 24 12:36 version

/sys/devices/dev3:
total 0
lrwxrwxrwx 1 root root    0 Mar 24 12:38 subsystem -> ../../bus/woab
-r--r--r-- 1 root root 4096 Mar 24 12:36 type
-rw-r--r-- 1 root root 4096 Mar 24 12:38 uevent
-r--r--r-- 1 root root 4096 Mar 24 12:36 version

/sys/devices/dev4:
total 0
lrwxrwxrwx 1 root root    0 Mar 24 12:38 subsystem -> ../../bus/woab
-r--r--r-- 1 root root 4096 Mar 24 12:36 type
-rw-r--r-- 1 root root 4096 Mar 24 12:38 uevent
-r--r--r-- 1 root root 4096 Mar 24 12:36 version
```

**Devices** visible under *linux device* subsystem.

### Read type and version using sysfs

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /sys/devices/dev1/version
1
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /sys/devices/dev1/type
type_a
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /sys/devices/dev2/version
2
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /sys/devices/dev2/type
type_b
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /sys/devices/dev3/version
3
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /sys/devices/dev3/type
misc
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /sys/devices/dev4/version
1
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /sys/devices/dev4/type
misc
```

Note that only last **device** *dev4* matches **driver** requirements: *type* misc and *version* less or equal 1.

### Load driver

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# insmod woab_misc.ko
```

**Driver** loaded correctly

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/bus/woab/drivers/
total 0
drwxr-xr-x 2 root root 0 Mar 24 12:41 woab_misc
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/bus/woab/drivers/woab_misc/
total 0
--w------- 1 root root 4096 Mar 24 12:41 bind
lrwxrwxrwx 1 root root    0 Mar 24 12:41 dev4 -> ../../../../devices/dev4
lrwxrwxrwx 1 root root    0 Mar 24 12:41 module -> ../../../../module/woab_misc
--w------- 1 root root 4096 Mar 24 12:41 uevent
--w------- 1 root root 4096 Mar 24 12:41 unbind
```

**Driver** *woab_misc* was correctly loaded and attached to **bus** *woab*. **Device** *dev4* was correctly *probed* by **driver**.

### Validate miscdevice

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /dev/woab-misc-*
crw------- 1 root root 10, 125 Mar 24 12:40 /dev/woab-misc-0
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# echo XYZ > /dev/woab-misc-0
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /dev/woab-misc-0
XYZ
```

**Miscdevice** succesfully wrote and read "XYZ" string. Communication with **device** works.

### Add new supported device

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# echo "dev5 misc 1" > /sys/bus/woab/add
```

**Device** *dev5* added

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/bus/woab/drivers/woab_misc/
total 0
--w------- 1 root root 4096 Mar 24 12:41 bind
lrwxrwxrwx 1 root root    0 Mar 24 12:41 dev4 -> ../../../../devices/dev4
lrwxrwxrwx 1 root root    0 Mar 24 12:45 dev5 -> ../../../../devices/dev5
lrwxrwxrwx 1 root root    0 Mar 24 12:41 module -> ../../../../module/woab_misc
--w------- 1 root root 4096 Mar 24 12:41 uevent
--w------- 1 root root 4096 Mar 24 12:41 unbind
```

**Device** *dev5* visible in **bus** subsystem.
```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /dev/woab-misc-*
crw------- 1 root root 10, 125 Mar 24 12:40 /dev/woab-misc-0
crw------- 1 root root 10, 124 Mar 24 12:45 /dev/woab-misc-1
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# echo ABC > /dev/woab-misc-1
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /dev/woab-misc-1
ABC
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /dev/woab-misc-0
XYZ
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc#
```

When new data wrote into **device** via assosiated **miscdevice** previously written data stays untached.

### Unbind device

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# echo "dev5" > /sys/bus/woab/drivers/woab_misc/unbind
```

**Device** *dev5* unbinded

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/bus/woab/drivers/woab_misc/
total 0
--w------- 1 root root 4096 Mar 24 12:41 bind
lrwxrwxrwx 1 root root    0 Mar 24 12:41 dev4 -> ../../../../devices/dev4
lrwxrwxrwx 1 root root    0 Mar 24 12:41 module -> ../../../../module/woab_misc
--w------- 1 root root 4096 Mar 24 12:41 uevent
--w------- 1 root root 4096 Mar 24 12:48 unbind
```

**Device** *dev5* is not visible under **driver** *woab_misc* subsystem.

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /dev/woab-misc-*
crw------- 1 root root 10, 125 Mar 24 12:40 /dev/woab-misc-0
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# cat /dev/woab-misc-0
XYZ
```

Assosiated *woab-misc-1* file was correctly removed.

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/bus/woab/devices/
total 0
lrwxrwxrwx 1 root root 0 Mar 24 12:32 dev1 -> ../../../devices/dev1
lrwxrwxrwx 1 root root 0 Mar 24 12:32 dev2 -> ../../../devices/dev2
lrwxrwxrwx 1 root root 0 Mar 24 12:32 dev3 -> ../../../devices/dev3
lrwxrwxrwx 1 root root 0 Mar 24 12:32 dev4 -> ../../../devices/dev4
lrwxrwxrwx 1 root root 0 Mar 24 12:48 dev5 -> ../../../devices/dev5
```

**Device** still exists within *woab* **bus**.

### Remove device

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# echo "dev5" > /sys/bus/woab/del
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# echo "dev4" > /sys/bus/woab/del
```

**Devices** *dev4* and *dev5* removed

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/bus/woab/drivers/
total 0
drwxr-xr-x 2 root root 0 Mar 24 12:41 woab_misc
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/bus/woab/drivers/woab_misc/
total 0
--w------- 1 root root 4096 Mar 24 12:41 bind
lrwxrwxrwx 1 root root    0 Mar 24 12:41 module -> ../../../../module/woab_misc
--w------- 1 root root 4096 Mar 24 12:41 uevent
--w------- 1 root root 4096 Mar 24 12:48 unbind
```

*woab_misc* still loaded but no **devices** connected.

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /dev/woab-misc-*
ls: cannot access '/dev/woab-misc-*': No such file or directory
```

No artifacts in /dev/ directory.

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/devices/dev*
/sys/devices/dev1:
total 0
lrwxrwxrwx 1 root root    0 Mar 24 12:38 subsystem -> ../../bus/woab
-r--r--r-- 1 root root 4096 Mar 24 12:36 type
-rw-r--r-- 1 root root 4096 Mar 24 12:38 uevent
-r--r--r-- 1 root root 4096 Mar 24 12:36 version

/sys/devices/dev2:
total 0
lrwxrwxrwx 1 root root    0 Mar 24 12:38 subsystem -> ../../bus/woab
-r--r--r-- 1 root root 4096 Mar 24 12:36 type
-rw-r--r-- 1 root root 4096 Mar 24 12:38 uevent
-r--r--r-- 1 root root 4096 Mar 24 12:36 version

/sys/devices/dev3:
total 0
lrwxrwxrwx 1 root root    0 Mar 24 12:38 subsystem -> ../../bus/woab
-r--r--r-- 1 root root 4096 Mar 24 12:36 type
-rw-r--r-- 1 root root 4096 Mar 24 12:38 uevent
-r--r--r-- 1 root root 4096 Mar 24 12:36 version
```

Remaining **devicess** untached.

### Remove bus

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# rmmod woab_bus
rmmod: ERROR: Module woab_bus is in use by: woab_misc
```

**Bus** can not be unloaded until **driver** is still loaded.

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# lsmod
Module                  Size  Used by
woab_misc              16384  0
woab_bus               16384  1 woab_misc
```

Unload **driver**

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# rmmod woab_misc
```

**Driver** unloaded

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# lsmod
Module                  Size  Used by
woab_bus               16384  0
```

Unload **bus**

```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# rmmod woab_bus
```

Validate artifacts
```
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/devices/dev*
ls: cannot access '/sys/devices/dev*': No such file or directory
root@9077BG3:/mnt/c/Users/woab/Desktop/Linux/linux_drivers_upskilling/14_bus_misc# ls -l /sys/bus/woab/
ls: cannot access '/sys/bus/woab/': No such file or directory
```

No remaining artifacts

## Details

For more details please compile with `TRACE` and `DEBUG` support in *woab.h*

```
#define DEBUG
#define TRACE
```

and analyze output of *dmesg*
<br>
<br>
<br>
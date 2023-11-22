#ifndef __MOBBUS_H__
#define __MOBBUS_H__

struct mob_device;

struct mob_driver
{
    const char* type;
    struct device_driver driver;
    int (*probe) (struct mob_device* dev);
    void (*remove) (struct mob_device* dev);
};

struct mob_device
{
    char* name;
    const char* type;
    struct mob_drver* driver;
    struct device dev;
};

int mob_register_driver(struct mob_driver* driver);
void mob_unregister_driver(struct mob_driver* driver);

#endif //__MOBBUS_H__
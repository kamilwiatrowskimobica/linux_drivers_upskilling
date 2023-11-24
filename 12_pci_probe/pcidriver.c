#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>

// get vendorId and deviceId for a given pci device like:
// cat /sys/bus/pci/devices/0000:00:14:0/vendor
// cat /sys/bus/pci/devices/0000:00:14:0/device
// check driver binded to the device:
// ls -l /sys/bus/pci/devices/0000\:00\:14.0/driver
// /sys/bus/pci/devices/0000:00:14.0/driver -> ../../../bus/pci/drivers/xhci_hcd
// unbind driver:
// cd /sys/bus/pci/drivers/xhci_hcd
// sudo bash -c "echo 0000:00:14.0 > unbind"
// bind custom driver:
// cd /sys/bus/pci/drivers/pcidriver
// sudo bash -c "echo 0000:00:14.0 > bind"
// check dmesg
// do the reverse to restore original driver

static struct pci_device_id devices[] =
{
    { PCI_DEVICE(0x8086, 0xA36D), },  // VENDOR_ID, DEVICE_ID
    { 0, }
};

MODULE_DEVICE_TABLE(pci, devices);

static void print_config_reg(struct pci_dev* dev)
{
    u16 word;

    pci_read_config_word(dev, PCI_VENDOR_ID, &word);
    printk(KERN_INFO "VendorId = 0x%04X\n", word);

    pci_read_config_word(dev, PCI_DEVICE_ID, &word);
    printk(KERN_INFO "DeviceId = 0x%04X\n", word);
}

static int probe(struct pci_dev* dev, const struct pci_device_id* id)
{
    if (pci_enable_device(dev))
    {
        dev_err(&dev->dev, "can't enable PCI device\n");
        return -ENODEV;
    }

    print_config_reg(dev);
    printk(KERN_INFO "Successfully probed 0x%04X 0x%04X\n", dev->vendor, dev->device);
    
    return 0;
}

static void remove(struct pci_dev* dev)
{
    printk(KERN_INFO "Successfully removed 0x%04X 0x%04X\n", dev->vendor, dev->device);
}

static struct pci_driver pci_driver =
{
    .name = "pcidriver",
    .id_table = devices,
    .probe = probe,
    .remove = remove
};

static int __init pcidev_init(void)
{
    return pci_register_driver(&pci_driver);
}

static void __exit pcidev_exit(void)
{
    pci_unregister_driver(&pci_driver);
}

MODULE_LICENSE("GPL");

module_init(pcidev_init);
module_exit(pcidev_exit);

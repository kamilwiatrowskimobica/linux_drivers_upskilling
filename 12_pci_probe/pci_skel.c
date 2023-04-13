#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>

/**
 * Check the vendor and device id for a given pci device,
 * in my case get it from: "/sys/bus/pci/devices/0000:00:14.0".
 * Get the driver binded for the device:
 * ls -l /sys/bus/pci/devices/0000\:00\:14.0/driver
 * /sys/bus/pci/devices/0000:00:14.0/driver -> ../../../bus/pci/drivers/xhci_hcd
 * 
 * Go to the driver and unbind it from the device:
 * cd /sys/bus/pci/drivers/xhci_hcd
 * sudo bash -c "echo 0000:00:14.0 > unbind"
 * 
 * Bind the custom driver to the device:
 * cd /sys/bus/pci/drivers/pci_skel
 * sudo bash -c "echo 0000:00:14.0 > bind"
 * 
 * Check dmesg or syslog for the logs from `print_config_reg`.
 * 
 * Do the reverse operation to restore the original driver.
 * Note: It will cause the given device to stop functioning,
 * a usb keyboard in my case, it was restored after binding back
 * the original driver.
 * 
 * https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-bus-pci
*/

// {vendor, device}
static struct pci_device_id ids[] = {
	// { PCI_DEVICE(PCI_ANY_ID, PCI_ANY_ID), },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0xa36d), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, ids);

static void print_config_reg(struct pci_dev *dev)
{
	u8 byte;
	u16 word;
	u32 dword;
	
	printk(KERN_INFO "Some config registers:\n");
	pci_read_config_word(dev, PCI_VENDOR_ID, &word);
	printk(KERN_INFO "VendorId = 0x%04X\n", word);
	
	pci_read_config_word(dev, PCI_DEVICE_ID, &word);
	printk(KERN_INFO "DeviceId = 0x%04X\n", word);
	
	pci_read_config_word(dev, PCI_COMMAND, &word);
	printk(KERN_INFO "Command = 0x%04X\n", word);
	
	pci_read_config_word(dev, PCI_STATUS, &word);
	printk(KERN_INFO "Status = 0x%04X\n", word);
	
	pci_read_config_byte(dev, PCI_REVISION_ID, &byte);
	printk(KERN_INFO "Revision = 0x%02X\n", byte);
	
	pci_read_config_dword(dev, PCI_CLASS_REVISION, &dword);
	printk(KERN_INFO "Class = 0x%06X\n", dword >> 8);
	
	pci_read_config_byte(dev, PCI_CACHE_LINE_SIZE, &byte);
	printk(KERN_INFO "Cache line = 0x%02X\n", byte);
	
	pci_read_config_byte(dev, PCI_LATENCY_TIMER, &byte);
	printk(KERN_INFO "Latency timer = 0x%02X\n", byte);

	pci_read_config_byte(dev, PCI_HEADER_TYPE, &byte);
	printk(KERN_INFO "header type = 0x%02X\n", byte);

	pci_read_config_byte(dev, PCI_BIST, &byte);
	printk(KERN_INFO "bist = 0x%02X\n", byte);

	pci_read_config_dword(dev, PCI_BASE_ADDRESS_0, &dword);
	printk(KERN_INFO "Base address 0 = 0x%08X\n", dword);
	
	pci_read_config_dword(dev, PCI_BASE_ADDRESS_1, &dword);
	printk(KERN_INFO "Base address 1 = 0x%08X\n", dword);
	
	pci_read_config_dword(dev, PCI_BASE_ADDRESS_2, &dword);
	printk(KERN_INFO "Base address 2 = 0x%08X\n", dword);
	
	pci_read_config_dword(dev, PCI_BASE_ADDRESS_3, &dword);
	printk(KERN_INFO "Base address 3 = 0x%08X\n", dword);
	
	pci_read_config_dword(dev, PCI_BASE_ADDRESS_4, &dword);
	printk(KERN_INFO "Base address 4 = 0x%08X\n", dword);
	
	pci_read_config_dword(dev, PCI_BASE_ADDRESS_5, &dword);
	printk(KERN_INFO "Base address 5 = 0x%08X\n", dword);
	
	pci_read_config_dword(dev, PCI_CARDBUS_CIS, &dword);
	printk(KERN_INFO "Card bus CIS = 0x%08X\n", dword);
	
	pci_read_config_word(dev, PCI_SUBSYSTEM_VENDOR_ID, &word);
	printk(KERN_INFO "Subsystem VendorId = 0x%04X\n", word);
	
	pci_read_config_word(dev, PCI_SUBSYSTEM_ID, &word);
	printk(KERN_INFO "Subsystem DeviceId = 0x%04X\n", word);

	pci_read_config_dword(dev, PCI_ROM_ADDRESS, &dword);
	printk(KERN_INFO "Rom address = 0x%08X\n", dword);

	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &byte);
	printk(KERN_INFO "Interrupt line = 0x%02X\n", byte);

	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &byte);
	printk(KERN_INFO "Interrupt pin = 0x%02X\n", byte);

	pci_read_config_byte(dev, PCI_MIN_GNT, &byte);
	printk(KERN_INFO "Min GNT = 0x%02X\n", byte);
	
	pci_read_config_byte(dev, PCI_MAX_LAT, &byte);
	printk(KERN_INFO "Max LAT = 0x%02X\n", byte);
}

static int probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	/* Do probing type stuff here.  
	 * Like calling request_region();
	 */

	if(pci_enable_device(dev)) {
		dev_err(&dev->dev, "can't enable PCI device\n");
		return -ENODEV;
	}

	print_config_reg(dev);

	printk(KERN_INFO "Successfully probed 0x%04X 0x%04X\n", dev->vendor, dev->device);
	return 0;
}

static void remove(struct pci_dev *dev)
{
	/* clean up any allocated resources and stuff here.
	 * like call release_region();
	 */
	 printk(KERN_INFO "Successfully removed 0x%04X 0x%04X\n", dev->vendor, dev->device);
}

static struct pci_driver pci_driver = {
	.name = "pci_skel",
	.id_table = ids,
	.probe = probe,
	.remove = remove,
};

static int __init pci_skel_init(void)
{
	return pci_register_driver(&pci_driver);;
}

static void __exit pci_skel_exit(void)
{
	pci_unregister_driver(&pci_driver);
}

MODULE_LICENSE("GPL");

module_init(pci_skel_init);
module_exit(pci_skel_exit);

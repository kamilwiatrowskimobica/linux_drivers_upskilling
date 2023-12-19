#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>


static struct pci_device_id ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x9a17), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, ids);

static int probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	/* Do probing type stuff here.  
	 * Like calling request_region();
	 */
	 
	 u16 word;
	 
	if(pci_enable_device(dev)) {
		dev_err(&dev->dev, "can't enable PCI device\n");
		return -ENODEV;
	}
	
	pci_read_config_word(dev, PCI_VENDOR_ID, &word);
	printk(KERN_INFO "VendorId = 0x%04X\n", word);
	
	pci_read_config_word(dev, PCI_DEVICE_ID, &word);
	printk(KERN_INFO "DeviceId = 0x%04X\n", word);

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
	.name = "pci_probe",
	.id_table = ids,
	.probe = probe,
	.remove = remove,
};

static int __init pci_skel_init(void)
{
	return pci_register_driver(&pci_driver);
}

static void __exit pci_skel_exit(void)
{
	pci_unregister_driver(&pci_driver);
}

MODULE_LICENSE("GPL");

module_init(pci_skel_init);
module_exit(pci_skel_exit);

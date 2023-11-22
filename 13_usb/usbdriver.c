#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>

#define USB_VENDOR_ID 0x47F
#define USB_PRODUCT_ID 0x430A

static int usb_probe(struct usb_interface* interface, const struct usb_device_id* id);
static void usb_disconnect(struct usb_interface* interface);
static void print_usb_interface_descriptor(struct usb_interface_descriptor i);
static void print_usb_endpoint_descriptor(struct usb_endpoint_descriptor e);

const struct usb_device_id usb_table[] =
{
    { USB_DEVICE(USB_VENDOR_ID, USB_PRODUCT_ID) },
    { }
};

static struct usb_driver usb_driver =
{
    .name = "usb_driver",
    .probe = usb_probe,
    .disconnect = usb_disconnect,
    .id_table = usb_table
};

module_usb_driver(usb_driver); // module_init and module_exit all in one

static int usb_probe(struct usb_interface* interface, const struct usb_device_id* id)
{
    unsigned int i;
    unsigned int endpoints_count;

    struct usb_host_interface* iface_desc = interface->cur_altsetting;
    dev_info(&interface->dev, "USB Driver probed: vendorID : 0x%02x,\t"
                              "productID : 0x%02x\n", id->idVendor, id->idProduct);

    endpoints_count = iface_desc->desc.bNumEndpoints;

    print_usb_interface_descriptor(iface_desc->desc);

    for (i = 0; i < endpoints_count; i++)
    {
        print_usb_endpoint_descriptor(iface_desc->endpoint[i].desc);
    }

    return 0;
}

static void usb_disconnect(struct usb_interface* interface)
{
    dev_info(&interface->dev, "USB Driver disconnected\n");
}

static void print_usb_interface_descriptor(struct usb_interface_descriptor i)
{
    pr_info("USB INTERFACE DESCRIPTOR:\n");
    pr_info("---------------------------------------\n");
    pr_info("bLength: 0x%x\n", i.bLength);
    pr_info("bDescriptorType: 0x%x\n", i.bDescriptorType);
    pr_info("bInterfaceNumber: 0x%x\n", i.bInterfaceNumber);
    pr_info("bAlternateSetting: 0x%x\n", i.bAlternateSetting);
    pr_info("bNumEndpoints: 0x%x\n", i.bNumEndpoints);
    pr_info("bInterfaceClass: 0x%x\n", i.bInterfaceClass);
    pr_info("bInterfaceSubClass: 0x%x\n", i.bInterfaceSubClass);
    pr_info("bInterfaceProtocol: 0x%x\n", i.bInterfaceProtocol);
    pr_info("iInterface: 0x%x\n", i.iInterface);
    pr_info("\n");
}

static void print_usb_endpoint_descriptor(struct usb_endpoint_descriptor e)
{
    pr_info("USB ENDPOINT DESCRIPTOR:\n");
    pr_info("---------------------------------------\n");
    pr_info("bLength: 0x%x\n", e.bLength);
    pr_info("bDescriptorType: 0x%x\n", e.bDescriptorType);
    pr_info("bEndPointAddress: 0x%x\n", e.bEndpointAddress);
    pr_info("bmAttributes: 0x%x\n", e.bmAttributes);
    pr_info("wMaxPacketSize: 0x%x\n", e.wMaxPacketSize);
    pr_info("bInterval: 0x%x\n", e.bInterval);
    pr_info("\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A B");
MODULE_DESCRIPTION("Usb driver");
MODULE_VERSION("1.0");

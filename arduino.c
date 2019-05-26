#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pablo Rodriguez Quesada");
MODULE_DESCRIPTION("An Arduino Serial Module");
MODULE_VERSION("0.01");

#define BULK_EP_OUT 0x04   //address where arduino device allow write
#define BULK_EP_IN 0x83	//address where arduino device allow read

static DEFINE_MUTEX(fs_mutex); // Defining a mutex
#define VENDOR_ID	0x2341
#define PRODUCT_ID	0x0043
#define MINOR_BASE	192
#define DEVICE_NAME "Arduino Uno R3"

/* Prototypes for device functions */
static void device_disconnect(struct usb_interface *interface);
static int device_probe(struct usb_interface *interface, const struct usb_device_id *id);
/* Prototypes for device fops functions */
static void device_delete(struct kref *kref );
static int device_open(struct inode *inode, struct file *file );
static int device_release(struct inode *inode, struct file *file );
static ssize_t device_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos);
static void device_write_bulk_callback(struct urb *urb );
static ssize_t device_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos);

/* Prototypes for device functions */

/* table of devices that work with this driver */
static struct usb_device_id id_table[] = {
	{ USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
	{ },
};
MODULE_DEVICE_TABLE(usb, id_table);


struct arduino {
	struct usb_device *	udev;			// the usb device
	struct usb_interface *	interface;		// the interface for this device
	unsigned char *		bulk_in_buffer;		// the buffer to receive data
	size_t			bulk_in_size;		// the size of the receive buffer
	__u8			bulk_in_endpointAddr;	// the address of the bulk in endpoint
	__u8			bulk_out_endpointAddr;	// the address of the bulk out endpoint
	struct kref		kref;
};

#define to_device_dev(d) container_of(d, struct arduino, kref)

static struct usb_driver arduino = {
 .name = "arduino",
 .probe = device_probe,
 .disconnect = device_disconnect,
 .id_table = id_table
};

/*
*
*  *  Devices are represented as file structure in the kernel.
*   */
static struct file_operations device_fops = {
.owner =	THIS_MODULE,
.read =		device_read,
.write =	device_write,
.open =		device_open,
.release =	device_release,
};

static struct usb_class_driver device_class = {
	.name = "ardu%d",
	.fops = &device_fops,
	.minor_base = MINOR_BASE,

};


/*
	*******FILE OPERATIONS******
*/
static void device_delete(struct kref *kref )  {
	struct arduino *dev = to_device_dev(kref);
    printk (KERN_INFO "arduino: Deleting device %d", dev->udev->devnum);
	usb_put_dev(dev->udev);
	kfree (dev->bulk_in_buffer);
	kfree (dev);
}


static int device_open(struct inode *inode, struct file *file )  {
	struct arduino *dev;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;

	subminor = iminor(inode);

	interface = usb_find_interface(&arduino, subminor);
	if (!interface) {
		printk(KERN_INFO "arduino: %s - error, can't find device for minor %d",
				     __FUNCTION__, subminor);
		retval = -ENODEV;
		return retval;
	}

	dev = usb_get_intfdata(interface);
	if (!dev) {
		retval = -ENODEV;
		return retval;
	}

	kref_get(&dev->kref);
	file->private_data = dev;

	return retval;
}	

static int device_release(struct inode *inode, struct file *file )  {
	struct arduino *dev;

	dev = (struct arduino *) file->private_data;
	if (dev == NULL)
		return -ENODEV;

	kref_put(&dev->kref, device_delete);
	return 0;
}

static ssize_t device_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos) {
	int count_int = (int) count;
	struct arduino *dev;
	int retval = 0;

	dev = (struct arduino*) file->private_data;

	retval = usb_bulk_msg(dev->udev,
	      usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
	      dev->bulk_in_buffer,
	      min(dev->bulk_in_size, count),
	      &count_int, HZ*10);

	if (!retval) {
		if (copy_to_user(buffer, dev->bulk_in_buffer, count))
				retval = -EFAULT;
		else
			retval = count;
	}

	printk(KERN_INFO "arduino: Error reading retval=%d\n",retval);
	return retval;
}

static void device_write_bulk_callback(struct urb *urb )  {
	if (urb->status &&
	!(urb->status == -ENOENT ||
	urb->status == -ECONNRESET ||
	urb->status == -ESHUTDOWN)) {
		printk(KERN_INFO "arduino: %s - nonzero write bulk status received: %d\n",
		__FUNCTION__, urb->status);
	}

	usb_free_coherent(urb->dev, urb->transfer_buffer_length,
	urb->transfer_buffer, urb->transfer_dma);
}

static ssize_t device_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos) {

	struct arduino *dev;
	int retval = 0;
	struct urb *urb = NULL;
	char *buf = NULL;

	dev = (struct arduino *) file->private_data;

	if (count == 0)
		goto exit;

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		retval = -ENOMEM;
		goto error;
	}
	buf =  usb_alloc_coherent(dev->udev, count, GFP_KERNEL, &urb->transfer_dma);
	if (!buf) {
		retval = -ENOMEM;
		goto error;
	}
	if (copy_from_user(buf, user_buffer, count)) {
		retval = -EFAULT;
		goto error;
	}

	usb_fill_bulk_urb(urb, dev->udev, usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
	buf, count, device_write_bulk_callback, dev);
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	retval = usb_submit_urb(urb, GFP_KERNEL);
	if (retval) {
		printk(KERN_INFO "arduino: %s - failed submitting write usb, error %d", __FUNCTION__, retval);
		goto error;
	}

	printk(KERN_INFO "arduino: successful write");
	usb_free_urb(urb);

	exit:
	return count;

	error:
	usb_free_coherent(dev->udev, count, buf, urb->transfer_dma);
	usb_free_urb(urb);
	kfree(buf);
	return retval;
}

/*
	******USB OPERATIONS********
*/
static int device_probe(struct usb_interface *interface, const struct usb_device_id *id){
	struct arduino *dev = NULL;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	size_t buffer_size;
	int i;
	int retval = -ENOMEM;

	dev = kmalloc(sizeof(struct arduino), GFP_KERNEL);
	if (dev == NULL) {
		printk(KERN_INFO "arduino: Out of memory\n");
		goto error;
	}
	memset(dev, 0x00, sizeof (*dev));
	kref_init(&dev->kref);

	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;
    printk(KERN_INFO "arduino: New device connected device number %d \n",dev->udev->devnum);
	iface_desc = interface->cur_altsetting;
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;

		if (!dev->bulk_in_endpointAddr &&
		(endpoint->bEndpointAddress & USB_DIR_IN) &&
		((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
		== USB_ENDPOINT_XFER_BULK)) {
			buffer_size = endpoint->wMaxPacketSize;
			dev->bulk_in_size = buffer_size;
			dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
			dev->bulk_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
			if (!dev->bulk_in_buffer) {
				printk(KERN_INFO "arduino: %d Could not allocate bulk_in_buffer\n",dev->udev->devnum);
				goto error;
			}
		}

		if (!dev->bulk_out_endpointAddr &&!(endpoint->bEndpointAddress & USB_DIR_IN) &&((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
		== USB_ENDPOINT_XFER_BULK)) {
			dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
		}
	}
	if (!(dev->bulk_in_endpointAddr && dev->bulk_out_endpointAddr)) {
		printk(KERN_INFO "arduino: %d Could not find both bulk-in and bulk-out endpoints\n",dev->udev->devnum);
		goto error;
	}
    else {
        printk(KERN_INFO "arduino: %d bulk-in: %x bulk-out %x\n",dev->udev->devnum, dev->bulk_in_endpointAddr, dev->bulk_out_endpointAddr);
		
    }

	usb_set_intfdata(interface, dev);

	retval = usb_register_dev(interface, &device_class);
	if (retval) {
		printk(KERN_INFO "arduino: %d Not able to get a minor for this device.\n",dev->udev->devnum);
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	printk(KERN_INFO "arduino: %d device now attached to /dev/ardu%d\n", dev->udev->devnum, interface->minor);
	return 0;

	error:
	printk(KERN_INFO "arduino: %d Exiting from error\n", dev->udev->devnum);
	if (dev)    kref_put(&dev->kref, device_delete);
	return retval;
}
/* Called when a process closes our device */
static void device_disconnect(struct usb_interface *interface) {
	struct arduino *dev;
	int minor = interface->minor;

	mutex_lock(&fs_mutex);

	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	usb_deregister_dev(interface, &device_class);

	mutex_unlock(&fs_mutex);

	kref_put(&dev->kref, device_delete);

	printk(KERN_INFO "arduino: /dev/ardu%d now disconnected\n", minor);
}
static int __init device_init(void) {
	int res;

	res = usb_register(&arduino);
	if (res )
		printk(KERN_INFO "arduino: usb_register failed. Error number %d\n", res);

	printk(KERN_INFO "arduino: driver registered\n");
	return res;
}
static void __exit device_exit(void) {
 /* Remember — we have to clean up after ourselves. Unregister the character device. */
	printk(KERN_INFO "arduino: driver deregistered\n");
	usb_deregister(&arduino);
}
/* Register module functions */
module_init(device_init);
module_exit(device_exit);

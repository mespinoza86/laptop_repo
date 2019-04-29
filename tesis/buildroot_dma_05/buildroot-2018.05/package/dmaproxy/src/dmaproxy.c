/* DMA Proxy
 *
 * This module is designed to be a small example of a DMA device driver that is
 * a client to the DMA Engine using the AXI DMA driver. It serves as a proxy for
 * kernel space DMA control to a user space application.
 *
 * A zero copy scheme is provided by allowing user space to mmap a kernel allocated
 * memory region into user space, referred to as a proxy channel interface. The
 * ioctl function is provided to start a DMA transfer which then blocks until the
 * transfer is complete. No input arguments are being used in the ioctl function.
 *
 * There is an associated user space application, dma_proxy_test.c, and dma_proxy.h
 * that work with this device driver.
 *
 * The hardware design was tested with an AXI DMA without scatter gather and
 * with the transmit channel looped back to the receive channel.
 *
 */

#include <linux/dmaengine.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>

#define CACHED_BUFFERS

/*
	PARAMETROS DE CONFIGURACION
*/

// DEFINIR LA CANTIDAD DE ÍNDICES DEL ARREGLO DEL BUFFER
#define TEST_SIZE (32)
// DEFINIR LOS PUERTOS DEL DMA AL DRIVER (DMA DEVICE): Configurable en el device tree
#define DMA_TX "dmaproxy_tx"
#define DMA_RX "dmaproxy_rx"
// DEFINIR EL PREFIJO DE PUERTOS DEL USERSPACE AL DRIVER: Se usan los sufijos _tx y _rx
#define DRIVER_NAME 		"dma_proxy"
// DEFINIR 
#define CHANNEL_COUNT 		2
#define ERROR 			-1
#define NOT_LAST_CHANNEL 	0
#define LAST_CHANNEL 		1

//#define INTERNAL_TEST

/* The following module parameter controls where the allocated interface memory area is cached or not
 * such that both can be illustrated.  Add cached_buffers=1 to the command line insert of the module
 * to cause the allocated memory to be cached.
 */
static unsigned cached_buffers = 0;
module_param(cached_buffers, int, S_IRUGO);

MODULE_LICENSE("GPL");

// ------------------------------------- DMA CONNECTING PART -------------------------

struct dma_proxy_channel_interface {
	unsigned char buffer[TEST_SIZE];
	enum proxy_status { PROXY_NO_ERROR = 0, PROXY_BUSY = 1, PROXY_TIMEOUT = 2, PROXY_ERROR = 3 } status;
	unsigned int length;
};

static struct dma_chan *tx_chan;
static struct dma_chan *rx_chan;
static struct completion tx_cmp;
static struct completion rx_cmp;
static dma_addr_t tx_dma_handle;
static dma_addr_t rx_dma_handle;
static dma_cookie_t tx_cookie;
static dma_cookie_t rx_cookie;
#define WAIT 	1
#define NO_WAIT 0

/* The following data structure represents a single channel of DMA, transmit or receive in the case
 * when using AXI DMA.  It contains all the data to be maintained for the channel.
 */
struct dma_proxy_channel {
	struct dma_proxy_channel_interface *interface_p;	/* user to kernel space interface */
	dma_addr_t interface_phys_addr;

	struct device *proxy_device_p;				/* character device support */
	struct device *dma_device_p;
	dev_t dev_node;
	struct cdev cdev;
	struct class *class_p;

	struct dma_chan *channel_p;				/* dma support */
	struct completion cmp;
	dma_cookie_t cookie;
	dma_addr_t dma_handle;
	u32 direction;						/* DMA_MEM_TO_DEV or DMA_DEV_TO_MEM */
};

// ------------------------------------- DMA CONNECTING PART -------------------------

/* Allocate the channels for this example statically rather than dynamically for simplicity.
 */
static struct dma_proxy_channel channels[CHANNEL_COUNT];

static const struct of_device_id rfx_axidmatest_of_ids[] = {    // Implementado 
	{ .compatible = "xlnx,axi-dma-test-1.00.a",},
	{}
};

static int xilinx_axidmatest_probe(struct platform_device *pdev)
{

	int err;
	dma_cap_mask_t mask;
    
    /* Step 1, zero out the capability mask then initialize
	 * it for a slave channel that is private
	 */
	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE | DMA_PRIVATE, mask);
    

    /* Step 2, request the transmit and receive channels for the AXI DMA
	 * from the DMA engine
	 */

    //    struct dma_chan *tx_chan, *rx_chan;

    
	tx_chan = dma_request_slave_channel(&pdev->dev, DMA_TX);
	if (IS_ERR(tx_chan)) {
		pr_err("xilinx_dmatest: No Tx channel\n");
		return PTR_ERR(tx_chan);
	}

	rx_chan = dma_request_slave_channel(&pdev->dev, DMA_RX);
	if (IS_ERR(rx_chan)) {
		err = PTR_ERR(rx_chan);
		pr_err("xilinx_dmatest: No Rx channel\n");
		goto free_tx;
	}

	// CHANGES TO COUPLE TEST SCRIPT WITH DMA MAPPED
	channels[1].channel_p  = rx_chan;
	channels[0].channel_p  = tx_chan;
	channels[0].dma_device_p = &(channels[0].channel_p->dev->device);
	channels[1].dma_device_p = &(channels[1].channel_p->dev->device);
    
	return 0;

free_rx:
	dma_release_channel(rx_chan);				// Implementado
free_tx:
	dma_release_channel(tx_chan);				// Implementado

	return 0;
}

static void axidma_sync_callback(void *completion)
{
	/* Step 9, indicate the DMA transaction completed to allow the other
	 * thread of control to finish processing
	 */ 

	complete(completion);

}

static dma_cookie_t axidma_prep_buffer(struct dma_chan *chan, dma_addr_t buf, size_t len, 
					enum dma_transfer_direction dir, struct completion *cmp) 
{
	enum dma_ctrl_flags flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;
	struct dma_async_tx_descriptor *chan_desc;
	dma_cookie_t cookie;

	/* Step 5, create a buffer (channel)  descriptor for the buffer since only a  
	 * single buffer is being used for this transfer
	 */

	chan_desc = dmaengine_prep_slave_single(chan, buf, len, dir, flags);

	/* Make sure the operation was completed successfully
	 */
	if (!chan_desc) {
		printk(KERN_ERR "dmaengine_prep_slave_single error\n");
		cookie = -EBUSY;
	} else {
		chan_desc->callback = axidma_sync_callback;
		chan_desc->callback_param = cmp;

		/* Step 6, submit the transaction to the DMA engine so that it's queued
		 * up to be processed later and get a cookie to track it's status
		 */

		cookie = dmaengine_submit(chan_desc);
	
	}
	return cookie;
}

static int xilinx_axidmatest_remove(struct platform_device *pdev)
{
	struct dmatest_chan *dtc, *_dtc;
	struct dma_chan *chan;

    /* Step 12, release the DMA channels back to the DMA engine
	 */
    
    dma_release_channel(tx_chan);			
    dma_release_channel(rx_chan);			
    
//	}
	return 0;
}

static struct platform_driver rfx_axidmatest_driver = {
	.driver = {
		.name = "rfx-axidma",
		.owner = THIS_MODULE,
		.of_match_table = rfx_axidmatest_of_ids,                   
	},
	.probe = xilinx_axidmatest_probe,                             
	.remove = xilinx_axidmatest_remove,                           
};

// ------------------------------------- END OF DMA CONNECTING PART -------------------------



/* Handle a callback and indicate the DMA transfer is complete to another
 * thread of control
 */
static void sync_callback(void *completion)
{
	/* Indicate the DMA transaction completed to allow the other
	 * thread of control to finish processing
	 */
	complete(completion);
}

/* Prepare a DMA buffer to be used in a DMA transaction, submit it to the DMA engine
 * to queued and return a cookie that can be used to track that status of the
 * transaction
 */
static dma_cookie_t start_transfer(struct dma_proxy_channel *pchannel_p)
{
	enum dma_ctrl_flags flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;
	struct dma_async_tx_descriptor *chan_desc;
	dma_cookie_t cookie;

	struct dma_proxy_channel_interface *interface_p = pchannel_p->interface_p;

	/* Create a buffer (channel)  descriptor for the buffer since only a
	 * single buffer is being used for this transfer
	 */
	chan_desc = dmaengine_prep_slave_single(pchannel_p->channel_p, pchannel_p->dma_handle,interface_p->length,pchannel_p->direction,flags);
	

	/* Make sure the operation was completed successfully
	 */
	if (!chan_desc) {
		printk(KERN_ERR "dmaengine_prep_slave_single error\n");
		cookie = -EBUSY;
	} else {
		chan_desc->callback = sync_callback;
		chan_desc->callback_param = &pchannel_p->cmp;

		/* Initialize the completion for the transfer and before using it
		 * then submit the transaction to the DMA engine so that it's queued
		 * up to be processed later and get a cookie to track it's status
		 */
		init_completion(&pchannel_p->cmp);
		cookie = dmaengine_submit(chan_desc);

		/* Start the DMA transaction which was previously queued up in the DMA engine
		 */
		dma_async_issue_pending(pchannel_p->channel_p);
	}
	return cookie;
}

/* Wait for a DMA transfer that was previously submitted to the DMA engine
 * wait for it complete, timeout or have an error
 */
static void wait_for_transfer(struct dma_proxy_channel *pchannel_p)
{
	unsigned long timeout = msecs_to_jiffies(3000);
	enum dma_status status;

	pchannel_p->interface_p->status = PROXY_BUSY;

	/* Wait for the transaction to complete, or timeout, or get an error
	 */
	timeout = wait_for_completion_timeout(&pchannel_p->cmp, timeout);
	status = dma_async_is_tx_complete(pchannel_p->channel_p, pchannel_p->cookie, NULL, NULL);

	if (timeout == 0)  {
		pchannel_p->interface_p->status  = PROXY_TIMEOUT;
		printk(KERN_ERR "DMA timed out\n");
	} else if (status != DMA_COMPLETE) {
		pchannel_p->interface_p->status = PROXY_ERROR;
		printk(KERN_ERR "DMA returned completion callback status of: %s\n",
			   status == DMA_ERROR ? "error" : "in progress");
	} else
		pchannel_p->interface_p->status = PROXY_NO_ERROR;

	if (cached_buffers) {

		/* Cached buffers need to be unmapped after the transfer is done so that the CPU
		 * can see the new data when being received. This is done in this function to
		 * allow this function to be called from another thread of control also (non-blocking).
		 */
		u32 map_direction;
		if (pchannel_p->direction == DMA_MEM_TO_DEV)
			map_direction = DMA_TO_DEVICE;
		else
			map_direction = DMA_FROM_DEVICE;

		dma_unmap_single(pchannel_p->dma_device_p, pchannel_p->dma_handle,
						 pchannel_p->interface_p->length,
						 map_direction);
	}
}

/* For debug only, print out the channel details
 */
void print_channel(struct dma_proxy_channel *pchannel_p)
{
	struct dma_proxy_channel_interface *interface_p = pchannel_p->interface_p;

	printk("length = %d ", interface_p->length);
	if (pchannel_p->direction == DMA_MEM_TO_DEV)
		printk("tx direction ");
	else
		printk("rx direction ");
}

/* Setup the DMA transfer for the channel by taking care of any cache operations
 * and the start it.
 */
static void transfer(struct dma_proxy_channel *pchannel_p)
{
	struct dma_proxy_channel_interface *interface_p = pchannel_p->interface_p;
	u32 map_direction;

	print_channel(pchannel_p);

	if (cached_buffers) {

		/* Cached buffers need to be handled before starting the transfer so that
		 * any cached data is pushed to memory.
		 */
		if (pchannel_p->direction == DMA_MEM_TO_DEV)
			map_direction = DMA_TO_DEVICE;
		else
			map_direction = DMA_FROM_DEVICE;

		pchannel_p->dma_handle = dma_map_single(pchannel_p->dma_device_p,interface_p->buffer,interface_p->length,map_direction);

	} else {

		/* The physical address of the buffer in the interface is needed for the dma transfer
		 * as the buffer may not be the first data in the interface
		 */
		u32 offset = (u32)&interface_p->buffer - (u32)interface_p;
		pchannel_p->dma_handle = (dma_addr_t)(pchannel_p->interface_phys_addr + offset);
	}


	/* Start the DMA transfer and make sure there were not any errors
	 */
	printk(KERN_INFO "Starting DMA transfers\n");
	pchannel_p->cookie = start_transfer(pchannel_p);

	if (dma_submit_error(pchannel_p->cookie)) {
		printk(KERN_ERR "xdma_prep_buffer error\n");
		return;
	}

	wait_for_transfer(pchannel_p);
}

// ----------------------------------------------------- TEST SCRIPT --------------------------------------------
static void axidma_start_transfer(struct dma_chan *chan, struct completion *cmp, 
					dma_cookie_t cookie, int wait)
{
	unsigned long timeout = msecs_to_jiffies(3000);
	enum dma_status status;

	/* Step 7, initialize the completion before using it and then start the 
	 * DMA transaction which was previously queued up in the DMA engine
	 */

	init_completion(cmp);
	dma_async_issue_pending(chan);

	if (wait) {
		printk("Waiting for DMA to complete...\n");

		/* Step 8, wait for the transaction to complete, timeout, or get
		 * get an error
		 */

		timeout = wait_for_completion_timeout(cmp, timeout);
		status = dma_async_is_tx_complete(chan, cookie, NULL, NULL);

		/* Determine if the transaction completed without a timeout and
		 * withtout any errors
		 */
		if (timeout == 0)  {
			printk(KERN_ERR "DMA timed out\n");
		} else if (status != DMA_COMPLETE) {
			printk(KERN_ERR "DMA returned completion callback status of: %s\n",
			       status == DMA_ERROR ? "error" : "in progress");
		}
	}
}


/* The following functions are designed to test the driver from within the device
 * driver without any user space.
 */
#ifdef INTERNAL_TEST
static void tx_test(struct work_struct *unused)
{
	//transfer(&channels[0]);
}

int outgoing[TEST_SIZE];

static void test(void)
{
	int i;

	// Filling outgoing buffer
	printk("Filling Outgoing Buffer... \n");
	for (i = 0; i < TEST_SIZE; i++)
	{
		outgoing[i] = i;
		printk("With: %d\n", outgoing[i]);
	}

	int dma_length = sizeof(outgoing); // Size message
	

	/* Step 3, allocate cached memory for the transmit and receive buffers to use for DMA
	 * zeroing the destination buffer
	 */

	int *src_dma_buffer = kzalloc(dma_length, GFP_KERNEL);
	int *dest_dma_buffer = kzalloc(dma_length, GFP_KERNEL);

	memcpy(src_dma_buffer, outgoing, sizeof(outgoing));

	// Checking outgoing buffer
	printk("Reading Outgoing Buffer... \n");
	for (i = 0; i < TEST_SIZE; i++)
	{
		src_dma_buffer[i] = i;
		printk("With: %d\n", src_dma_buffer[i]);
	}
	
	/* Initialize the source buffer with known data to allow the destination buffer to
	 * be checked for success
	 */

	/* Step 4, since the CPU is done with the buffers, transfer ownership to the DMA and don't
	 * touch the buffers til the DMA is done, transferring ownership may involve cache operations
	 */

	tx_dma_handle = dma_map_single(tx_chan->device->dev, src_dma_buffer, dma_length, DMA_TO_DEVICE);	
	rx_dma_handle = dma_map_single(rx_chan->device->dev, dest_dma_buffer, dma_length, DMA_FROM_DEVICE);	
	
	/* Prepare the DMA buffers and the DMA transactions to be performed and make sure there was not
	 * any errors
	 */
	rx_cookie = axidma_prep_buffer(rx_chan, rx_dma_handle, dma_length, DMA_DEV_TO_MEM, &rx_cmp);
	tx_cookie = axidma_prep_buffer(tx_chan, tx_dma_handle, dma_length, DMA_MEM_TO_DEV, &tx_cmp);

	if (dma_submit_error(rx_cookie) || dma_submit_error(tx_cookie)) {
		printk(KERN_ERR "xdma_prep_buffer error\n");
		return;
	}

	printk(KERN_INFO "Starting DMA transfers\n");

	/* Start both DMA transfers and wait for them to complete
	 */
	axidma_start_transfer(rx_chan, &rx_cmp, rx_cookie, NO_WAIT);
	axidma_start_transfer(tx_chan, &tx_cmp, tx_cookie, WAIT);

	/* Step 10, the DMA is done with the buffers so transfer ownership back to the CPU so that
	 * any cache operations needed are done
	 */

	dma_unmap_single(rx_chan->device->dev, rx_dma_handle, dma_length, DMA_FROM_DEVICE);	
	dma_unmap_single(tx_chan->device->dev, tx_dma_handle, dma_length, DMA_TO_DEVICE);

	/* Verify the data in the destination buffer matches the source buffer 
	 */
	for (i = 0; i < TEST_SIZE; i++) {
	//printk(KERN_INFO "Got %d\texpected %d\n",*(int *)(&dest_dma_buffer[i*4]),*(int *)(&src_dma_buffer[i*4]));
		if (dest_dma_buffer[i] != src_dma_buffer[i]) {
			printk(KERN_INFO "DMA transfer failure");
			break;	
		}
	}


	printk("On Incoming Buffer... \n");
	for (i = 0; i < 32; i++)
	{
		printk("Data received: %d - Expected: %d\n",dest_dma_buffer[i],src_dma_buffer[i]);
	}

	printk(KERN_INFO "DMA bytes sent: %d\n", dma_length);

	/* Step 11, free the buffers used for DMA back to the kernel */
	kfree(src_dma_buffer);
	kfree(dest_dma_buffer);	
}
#endif

/* Map the memory for the channel interface into user space such that user space can
 * access it taking into account if the memory is not cached.
 */

//  ----------------------------------------------------- END OF TEST SCRIPT --------------------------------------------
static int mmap(struct file *file_p, struct vm_area_struct *vma)
{
	printk(KERN_INFO "DMAPROXY_MODULE: mmap was used\n");
	struct dma_proxy_channel *pchannel_p = (struct dma_proxy_channel *)file_p->private_data;

	/* The virtual address to map into is good, but the page frame will not be good since
	 * user space passes a physical address of 0, so get the physical address of the buffer
	 * that was allocated and convert to a page frame number.
	 */
	if (cached_buffers) {
		if (remap_pfn_range(vma, vma->vm_start,
							virt_to_phys((void *)pchannel_p->interface_p)>>PAGE_SHIFT,
							vma->vm_end - vma->vm_start, vma->vm_page_prot))
			return -EAGAIN;
		return 0;
	} else

		/* Step 3, use the DMA utility to map the DMA memory into space so that the
		 * user space application can use it
		 */
		return dma_common_mmap(pchannel_p->dma_device_p, vma,
							   pchannel_p->interface_p, pchannel_p->interface_phys_addr,
							   vma->vm_end - vma->vm_start);
}

/* Open the device file and set up the data pointer to the proxy channel data for the
 * proxy channel such that the ioctl function can access the data structure later.
 */
static int local_open(struct inode *ino, struct file *file)
{
	printk(KERN_INFO "DMAPROXY_MODULE: File Open\n");
	file->private_data = container_of(ino->i_cdev, struct dma_proxy_channel, cdev);
	
	return 0;
}

/* Close the file and there's nothing to do for it
 */
static int release(struct inode *ino, struct file *file)
{
	return 0;
}

/* Perform I/O control to start a DMA transfer.
 */
static long ioctl(struct file *file, unsigned int unused , unsigned long arg)
{
	printk(KERN_INFO "DMAPROXY_MODULE: ioctl was used\n");
	struct dma_proxy_channel *pchannel_p = (struct dma_proxy_channel *)file->private_data;

	dev_dbg(pchannel_p->dma_device_p, "ioctl\n");

	/* Step 2, call the transfer function for the channel to start the DMA and wait
	 * for it to finish (blocking in the function).
	 */
	transfer(pchannel_p);

	return 0;
}
static struct file_operations dm_fops = {
	.owner    = THIS_MODULE,
	.open     = local_open,
	.release  = release,
	.unlocked_ioctl = ioctl,
	.mmap	= mmap
};


/* Initialize the driver to be a character device such that is responds to
 * file operations.
 */
static int cdevice_init(struct dma_proxy_channel *pchannel_p, char *name)
{
	int rc;
	char device_name[32] = DRIVER_NAME;
	static struct class *local_class_p = NULL;

	/* Allocate a character device from the kernel for this
	 * driver
	 */
	rc = alloc_chrdev_region(&pchannel_p->dev_node, 0, 1, DRIVER_NAME);

	if (rc) {
		dev_err(pchannel_p->dma_device_p, "unable to get a char device number\n");
		return rc;
	}

	/* Initialize the ter device data structure before
	 * registering the character device with the kernel
	 */
	cdev_init(&pchannel_p->cdev, &dm_fops);
	pchannel_p->cdev.owner = THIS_MODULE;
	rc = cdev_add(&pchannel_p->cdev, pchannel_p->dev_node, 1);

	if (rc) {
		dev_err(pchannel_p->dma_device_p, "unable to add char device\n");
		goto init_error1;
	}

	/* Only one class in sysfs is to be created for multiple channels,
	 * create the device in sysfs which will allow the device node
	 * in /dev to be created
	 */
	if (!local_class_p) {
		local_class_p = class_create(THIS_MODULE, DRIVER_NAME);

		if (IS_ERR(pchannel_p->dma_device_p->class)) {
			dev_err(pchannel_p->dma_device_p, "unable to create class\n");
			rc = ERROR;
			goto init_error2;
		}
	}
	pchannel_p->class_p = local_class_p;

	/* Create the device node in /dev so the device is accessible
	 * as a character device
	 */
	strcat(device_name, name);
	pchannel_p->proxy_device_p = device_create(pchannel_p->class_p, NULL,
											   pchannel_p->dev_node, NULL, device_name);

	if (IS_ERR(pchannel_p->proxy_device_p)) {
		dev_err(pchannel_p->dma_device_p, "unable to create the device\n");
		goto init_error3;
	}

	return 0;

init_error3:
	class_destroy(pchannel_p->class_p);

init_error2:
	cdev_del(&pchannel_p->cdev);

init_error1:
	unregister_chrdev_region(pchannel_p->dev_node, 1);
	return rc;
}

/* Exit the character device by freeing up the resources that it created and
 * disconnecting itself from the kernel.
 */
static void cdevice_exit(struct dma_proxy_channel *pchannel_p, int last_channel)
{
	/* Take everything down in the reverse order
	 * from how it was created for the char device
	 */
	if (pchannel_p->proxy_device_p) {
		device_destroy(pchannel_p->class_p, pchannel_p->dev_node);
		if (last_channel)
			class_destroy(pchannel_p->class_p);

		cdev_del(&pchannel_p->cdev);
		unregister_chrdev_region(pchannel_p->dev_node, 1);
	}
}

/* Create a DMA channel by getting a DMA channel from the DMA Engine and then setting
 * up the channel as a character device to allow user space control.
 */
static int create_channel(struct dma_proxy_channel *pchannel_p, char *name, u32 direction)
{
	int rc;
	dma_cap_mask_t mask;



	/* Initialize the character device for the dma proxy channel
	 */
	rc = cdevice_init(pchannel_p, name);
	if (rc) {
		return rc;
	}

	pchannel_p->direction = direction;

	/* Allocate memory for the proxy channel interface for the channel as either
	 * cached or non-cache depending on input parameter. Use the managed
	 * device memory when possible but right now there's a bug that's not understood
	 * when using devm_kzalloc rather than kzalloc, so stay with kzalloc.
	 */
	if (cached_buffers) {
		pchannel_p->interface_p = (struct dma_proxy_channel_interface *)
			kzalloc(sizeof(struct dma_proxy_channel_interface),
					GFP_KERNEL);
		printk(KERN_INFO "Allocating cached memory at 0x%08X\n",
			   (unsigned int)pchannel_p->interface_p);
	} else {

		/* Step 1, set dma memory allocation for the channel so that all of memory
		 * is available for allocation, and then allocate the uncached memory (DMA)
		 * for the channel interface
		 */
		dma_set_coherent_mask(pchannel_p->proxy_device_p, 0xFFFFFFFF);
		pchannel_p->interface_p = (struct dma_proxy_channel_interface *)
			dmam_alloc_coherent(pchannel_p->proxy_device_p,
								sizeof(struct dma_proxy_channel_interface),
								&pchannel_p->interface_phys_addr, GFP_KERNEL);
		printk(KERN_INFO "Allocating uncached memory at 0x%08X\n",
			   (unsigned int)pchannel_p->interface_p);
	}
	if (!pchannel_p->interface_p) {
		dev_err(pchannel_p->dma_device_p, "DMA allocation error\n");
		return ERROR;
	}
	return 0;
}

/* Initialize the dma proxy device driver module.
 */
static int __init dma_proxy_init(void)
{
	int rc;

	printk(KERN_INFO "dma_proxy module initialized\n");
	// Register DMA
	int plat = platform_driver_register(&rfx_axidmatest_driver);

	/* Create 2 channels, the first is a transmit channel
	 * the second is the receive channel.
	 */
	printk(KERN_INFO "Proceding with the rest...\n");
	rc = create_channel(&channels[0], "_tx", DMA_MEM_TO_DEV);

	if (rc) {
		return rc;
	}

	rc = create_channel(&channels[1], "_rx", DMA_DEV_TO_MEM);
	if (rc) {
		return rc;
	}

	
#ifdef INTERNAL_TEST
	// Test it
	test();
#endif

	return 0;
}

/* Exit the dma proxy device driver module.
 */
static void __exit dma_proxy_exit(void)
{
	int i;

	printk(KERN_INFO "dma_proxy module exited\n");

	/* Take care of the char device infrastructure for each
	 * channel except for the last channel. Handle the last
	 * channel seperately.
	 */
	for (i = 0; i < CHANNEL_COUNT - 1; i++) {
		if (channels[i].proxy_device_p)
			cdevice_exit(&channels[i], NOT_LAST_CHANNEL);
	}
	cdevice_exit(&channels[i], LAST_CHANNEL);

	/* Take care of the DMA channels and the any buffers allocated
	 * for the DMA transfers. The DMA buffers are using managed
	 * memory such that it's automatically done.
	 */
	for (i = 0; i < CHANNEL_COUNT; i++) {
		if (channels[i].channel_p)
			dma_release_channel(channels[i].channel_p);

		if (channels[i].interface_p && cached_buffers)
			kfree((void *)channels[i].interface_p);
	}
	platform_driver_unregister(&rfx_axidmatest_driver);
}

module_init(dma_proxy_init);
module_exit(dma_proxy_exit);
MODULE_LICENSE("GPL");


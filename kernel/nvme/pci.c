#define PCI_CLASS_NVME      0x010802
#define PCI_CLASS_NVME_MASK 0xffffff

/* Define a filter for selecting devices we are interested in */
static const struct pci_device_id id_table[] =
{
	{ PCI_DEVICE_CLASS(PCI_CLASS_NVME, PCI_CLASS_NVME_MASK) },
	{ 0 }
};


/* Reference to the first character device */
static dev_t	dev_first;


/* Device class */
static struct class*	dev_class;


/* List of controller devices */
static struct list	ctrl_list;


/* List of mapped host memory */
static struct list	host_list;


/* List of mapped device memory */
static struct list	device_list;

/* Number of devices */
static int	num_ctrls = 8;
module_param(num_ctrls, int, 0);
MODULE_PARM_DESC(num_ctrls, "Number of controller devices");

static int	curr_ctrls = 0;


static int mmap_registers(struct file* file, struct vm_area_struct* vma)
{
	struct ctrl*	ctrl = NULL;

	ctrl = ctrl_find_by_inode(&ctrl_list, file->f_inode);
	if (ctrl == NULL)
	{
		printk(KERN_CRIT "Unknown controller reference\n");
		return -EBADF;
	}

	if (vma->vm_end - vma->vm_start > pci_resource_len(ctrl->pdev, 0))
	{
		printk(KERN_WARNING "Invalid range size\n");
		return -EINVAL;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	return vm_iomap_memory(vma, pci_resource_start(ctrl->pdev, 0), vma->vm_end - vma->vm_start);
}

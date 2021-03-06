//included files for part 2 
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

MODULE_LICENSE("Dual BSD/GPL");

#define BUF_LEN 100

// static struct used for initialization
static struct proc_dir_entry* proc_entry;

//static variables used for 
static char msg[BUF_LEN];
static int procfs_buf_len = 0;

//ints used for tracking the previous times
static int previous_time = 0;
static int previous_nsec = 0;

//initializing the proc
static ssize_t procfile_read(struct file* file, char * ubuf, size_t count, loff_t *ppos)
{

//variables used for the proc
	
	long int current_time;
	long int current_nsec;
	long int s_diff;
	long int ns_diff;

	printk(KERN_INFO "proc_read\n");

// function used to trac the cuurent time is sec  and nsec
    current_time  = current_kernel_time().tv_sec;
	current_nsec = current_kernel_time().tv_nsec;

//for proc buffer
	procfs_buf_len = 0;
	if (*ppos > 0 || count < procfs_buf_len)
		return 0;

// if else series used to track current and previous time
	if(!previous_time) {
		previous_time = current_time;
		previous_nsec = current_nsec;
		procfs_buf_len += sprintf(msg, "Current Time = %ld.%ld\n", current_time, current_nsec);
	} else {
		procfs_buf_len += sprintf(msg, "Current Time = %ld.%ld\n", current_time, current_nsec);
		s_diff = current_time - previous_time;
		ns_diff = current_nsec - previous_nsec;
		if(ns_diff < 0) {
			s_diff -= 1;
			ns_diff += 1000000000;
		}
		procfs_buf_len += sprintf(msg + procfs_buf_len, "Elapsed Time = %ld.%ld\n", s_diff, ns_diff);
		previous_time = current_time;
		previous_nsec = current_nsec;
	}

//used for error detection
	if (copy_to_user(ubuf, msg, procfs_buf_len))
		return -EFAULT;


	*ppos = procfs_buf_len;

	printk(KERN_INFO "gave to user %s\n", msg);

	return procfs_buf_len;
}

// struct used to write to the proc file 
static ssize_t procfile_write(struct file* file, const char * ubuf, size_t count, loff_t* ppos)
{
	printk(KERN_INFO "proc_write\n");

	if (count > BUF_LEN)
		procfs_buf_len = BUF_LEN;
	else
		procfs_buf_len = count;

	copy_from_user(msg, ubuf, procfs_buf_len);

	printk(KERN_INFO "got from user: %s\n", msg);

	return procfs_buf_len;
}


static struct file_operations procfile_fops = {
	.owner = THIS_MODULE,
	.read = procfile_read,
	.write = procfile_write,
};

// static timer initializer
static int timer_init(void) {
	proc_entry = proc_create("timer", 0666, NULL, &procfile_fops);

	if (proc_entry == NULL)
		return -ENOMEM;

	return 0;
}

// module initializer implements timer_init
module_init(timer_init);

static void timer_exit(void) {
	proc_remove(proc_entry);
	return;
}
module_exit(timer_exit);






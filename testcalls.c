#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>

/* System call stub */
long (*STUB_start_elevator)(int) = NULL;
long (*STUB_issue_request)(int) = NULL;
long (*STUB_stop_elevator)(int) = NULL;
EXPORT_SYMBOL(STUB_start_elevator);
EXPORT_SYMBOL(STUB_issue_request);
EXPORT_SYMBOL(STUB_stop_elevator);

/* System call wrapper */
SYSCALL_DEFINE1(start_elevator, int, test_int) {
	printk(KERN_NOTICE "Inside SYSCALL_DEFINE1 block. %s: Your int is %d\n", __FUNCTION__, test_int);
	if (STUB_start_elevator != NULL)
		return STUB_start_elevator(test_int);
	else
		return -ENOSYS;
}

SYSCALL_DEFINE2(issue_request, int, test_int) {
	printk(KERN_NOTICE "Inside SYSCALL_DEFINE1 block. %s: Your int is %d\n", __FUNCTION__, test_int);
	if (STUB_issue_request != NULL)
		return STUB_issue_request(test_int);
	else
		return -ENOSYS;
}

SYSCALL_DEFINE3(stop_elevator, int, test_int) {
	printk(KERN_NOTICE "Inside SYSCALL_DEFINE1 block. %s: Your int is %d\n", __FUNCTION__, test_int);
	if (STUB_stop_elevator != NULL)
		return STUB_stop_elevator(test_int);
	else
		return -ENOSYS;
}

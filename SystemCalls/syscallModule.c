#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/linkage.h>
MODULE_LICENSE("GPL");

extern long (*STUB_start_elevator)(void);
extern long (*STUB_issue_request)(int,int,int);
extern long (*STUB_stop_elevator)(void);

long start_elevator_handler(void) {
	printk(KERN_NOTICE "%s\n", __FUNCTION__);
	return 1;
}
long issue_request_handler(int start_floor, int destination_floor, int type) {
	printk(KERN_NOTICE "%s\n", __FUNCTION__);
	return 1;
}
long stop_elevator_handler(void) {
	printk(KERN_NOTICE "%s\n", __FUNCTION__);
	return 1;
}

static int hello_init(void) {
	STUB_start_elevator = start_elevator_handler;
	STUB_issue_request = issue_request_handler;
	STUB_stop_elevator = stop_elevator_handler;
	return 0;
}
module_init(hello_init);

static void hello_exit(void) {
	STUB_start_elevator = NULL;
	STUB_issue_request = NULL;
	STUB_stop_elevator = NULL;
}
module_exit(hello_exit);

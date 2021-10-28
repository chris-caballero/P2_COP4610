#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/mutex.h>


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Adds all sorts of people to a list and gets stats with proc");

#define ENTRY_NAME "passenger_list"
#define ENTRY_SIZE 1000
#define NUM_FLOORS 10
#define PERMS 0644
#define PARENT NULL
static struct file_operations fops;

#define DAILY_WORKER 0
#define MAINTENANCE_PERSON 1
#define MAIL_CARRIER 2

#define NUM_PERSON_TYPES 3
#define MAX_WEIGHT 1000

#define OFFLINE 0
#define IDLE 1
#define LOADING 2
#define UP 3
#define DOWN 4


static char *message;
static int read_p;


typedef struct {
    int size;
    int height;
    int up;
    int down;
    struct list_head list;
} Floor;

struct {
    int total_height;
    int total_tally;
    int total_serviced; 
    Floor **floors;

    struct mutex mutex;
} building;

struct thread_elevator elevator;




/***************************************************************/

int print_floor(Floor* floor) {
    struct list_head *temp;
    Passenger *p;
    char *buf;

    buf = kmalloc(sizeof(char) * 100, __GFP_RECLAIM);
	if (buf == NULL) {
		printk(KERN_WARNING "print_building null buffer");
		return -ENOMEM;
	}
    if(floor == NULL) {
        return -1;
    }
    if(elevator.current_floor == floor->height) {
        sprintf(buf, "[*] Floor %d: %d ", floor->height + 1, floor->size);
    } else {
        sprintf(buf, "[ ] Floor %d: %d ", floor->height + 1, floor->size);
    }
    strcat(message, buf);

    list_for_each(temp, &floor->list) {
        p = list_entry(temp, Passenger, list);
        sprintf(buf, "%s ", p->name);
        strcat(message, buf);
    }
    strcat(message, "\n");

    kfree(buf);
    return 0;
}

int print_building(void) {
    Floor *f;
    int i;

    for(i = NUM_FLOORS-1; i >= 0; i--) {
        f = building.floors[i];
        if(print_floor(f) == -1) {
            return -1;
        }
    }

    return 0;
}

int print_stats(void) {
    char *buf;

    buf = kmalloc(sizeof(char) * 100, __GFP_RECLAIM);
	if (buf == NULL) {
		printk(KERN_WARNING "print_building null buffer");
		return -ENOMEM;
	}
    strcpy(message, "");
    switch(elevator.state) {
        case OFFLINE:
            sprintf(buf, "Elevator State: OFFLINE\n");
            break;
        case IDLE:
            sprintf(buf, "Elevator State: IDLE\n");
            break;
        case LOADING:
            sprintf(buf, "Elevator State: LOADING\n");
            break;
        case UP:
            sprintf(buf, "Elevator State: UP\n");
            break;
        case DOWN:
            sprintf(buf, "Elevator State: DOWN\n");
            break;
        default:
            return -1;
    }
    strcat(message, buf);
    sprintf(buf, "Current floor: %d\n", elevator.current_floor+1); strcat(message, buf);
    sprintf(buf, "Current weight: %d\n", elevator.total_weight); strcat(message, buf);
    sprintf(buf, "Elevator status: %d D, %d M, %d C\n", elevator.num_workers, elevator.num_maintenance, elevator.num_carriers);
    strcat(message, buf);
    sprintf(buf, "Number of passengers: %d\n", elevator.size); strcat(message, buf);
    sprintf(buf, "Number of passengers waiting: %d\n", building.total_tally - elevator.size);
    strcat(message, buf);
    sprintf(buf, "Number of passengers serviced: %d\n", building.total_serviced); strcat(message, buf);
    kfree(buf);

    strcat(message, "\n");

    if(print_building() == -1) {
        return-1;
    }

    return 0;
}

int delete_floors(void) {
    Floor *f;
    int i;

    for(i = 0; i < NUM_FLOORS; i++) {
        f = building.floors[i];
        kfree(f);
    }

    return 0;
}

/********************************************************************/

int elevator_proc_open(struct inode *sp_inode, struct file *sp_file) {
    int res = 0;
	read_p = 1;
	message = kmalloc(sizeof(char) * ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	if (message == NULL) {
		printk(KERN_WARNING "elevator_proc_open");
		return -ENOMEM;
	}
    if(mutex_lock_interruptible(&elevator.mutex) == 0) {
        if(mutex_lock_interruptible(&building.mutex) == 0) {
            res = print_stats();
            mutex_unlock(&building.mutex);
        } 
        mutex_unlock(&elevator.mutex);  
    }
    return res;
}

ssize_t elevator_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset) {
	int len = strlen(message);
	
	read_p = !read_p;
	if (read_p)
		return 0;
		
	copy_to_user(buf, message, len);
	return len;
}

int elevator_proc_release(struct inode *sp_inode, struct file *sp_file) {
	kfree(message);
	return 0;
}

/********************************************************************/

static int elevator_init(void) {
    int i, start, end;
	fops.open = elevator_proc_open;
	fops.read = elevator_proc_read;
	fops.release = elevator_proc_release;
	
	if (!proc_create(ENTRY_NAME, PERMS, NULL, &fops)) {
		printk(KERN_WARNING "elevator_init\n");
		remove_proc_entry(ENTRY_NAME, NULL);
		return -ENOMEM;
	}

    mutex_init(&building.mutex);
    building.total_serviced = 0;
    building.total_tally = 0;
    building.total_height = NUM_FLOORS;
    building.floors = kmalloc_array(NUM_FLOORS, sizeof(Floor*), __GFP_RECLAIM);
    for(i = 0; i < NUM_FLOORS; i++) {
        Floor *f = kmalloc(sizeof(Floor)*1, __GFP_RECLAIM);
        INIT_LIST_HEAD(&f->list);
        f->size = 0;
        f->height = i;
        f->up = 0;
        f->down = 0;
        building.floors[i] = f;
    }

    thread_init_elevator(&elevator); 
    if(IS_ERR(elevator.kthread)) {
        printk(KERN_WARNING "error spawning thread");
		remove_proc_entry(ENTRY_NAME, NULL);
		return PTR_ERR(elevator.kthread);
    }   

    for(i = 0; i < 10; i++) {
        do {
            start = get_random_int() % NUM_FLOORS;
            end = get_random_int() % NUM_FLOORS;
        } while(start == end);
        add_passenger(start, end, get_random_int() % NUM_PERSON_TYPES); 
    }
    
    return 0;
}
module_init(elevator_init);

static void elevator_exit(void) {
    //first we need to let elevator finish
    if(mutex_lock_interruptible(&building.mutex) == 0) {
        delete_floors();
        mutex_unlock(&building.mutex);
    }
    
    kthread_stop(elevator.kthread);
	remove_proc_entry(ENTRY_NAME, NULL);
    mutex_destroy(&elevator.mutex);
    printk(KERN_NOTICE "Removing /proc/%s\n", ENTRY_NAME);
}
module_exit(elevator_exit);

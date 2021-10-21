#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/random.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Adds all sorts of people to a list and gets stats with proc");

#define ENTRY_NAME "passenger_list"
#define NUM_FLOORS 3
#define PERMS 0644
#define PARENT NULL
static struct file_operations fops;

#define DAILY_WORKER 0
#define MAINTENANCE_PERSON 1
#define MAIL_CARRIER 2

#define NUM_PERSON_TYPES 3
#define MAX_WEIGHT 1000

static char *message;
static int read_p;

/*
struct {
    int total_height;
    struct list_head list;
} building;
*/
struct {
    int size;
    int height;
    struct list_head list;
} floor;

typedef struct {
    int start_floor;
    int destination_floor;
    int type;
    int weight;
	const char *name;
	struct list_head list;
} Passenger;

//create passenger and add to floor[start_floor]
int create_passenger(int start_floor, int destination_floor, int type) {
    
    Passenger *new_passenger;
    new_passenger = kmalloc(sizeof(Passenger)*1, __GFP_RECLAIM);
    int weight = 150;

    if(new_passenger == NULL) {
        return -ENOMEM;
    }

    switch(type) {
        case DAILY_WORKER:
            new_passenger->name = "D";
            break;
        case MAINTENANCE_PERSON:
            new_passenger->name = "M";
            new_passenger->weight += 20;
            break;
        case MAIL_CARRIER:
            new_passenger->name = "C";
            new_passenger->weight += 75;
            break;
        default;
            return -1;
    }

    new_passenger->start_floor = start_floor;
    new_passenger->destination_floor = destination_floor;
    new_passenger->type = type;
    new_passenger->weight = weight;

    list_add_tail(&new_passenger->list, &floor.list);
    f.size += 1;

    return 0;
}

int delete_passengers(void) {
    list_head *temp;
    list_head *dummy
    Passenger *p;

    list_for_each_safe(temp, dummy, &floor.list) {
        p = list_entry(temp, Passenger, list);
        list_del(temp);
        kfree(p);
    }
    return 0;
}

int print_floor(void) {
    struct list_head *temp;
    Passenger *p;

    if(p == NULL) {
        return -ENOMOM;
    }

    sprintf(buf, "[ ] Floor %d: %d ", floor.height + 1, floor.size);

    list_for_each(temp, &floor.list) {
        p = list_entry(temp, Passenger, list);
        sprintf(buf, "%s ", p->name);
    }
    strcat(message, buf);
    strcat(message, "\n");

    kfree(buf);
    return 0;
}


/********************************************************************/

int elevator_proc_open(struct inode *sp_inode, struct file *sp_file) {
	read_p = 1;
	message = kmalloc(sizeof(char) * ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	if (message == NULL) {
		printk(KERN_WARNING "elevator_proc_open");
		return -ENOMEM;
	}
	create_passenger(0, 1, get_random_int() % NUM_PERSON_TYPES);
	//add_elevator(get_random_int() % NUM_elevator_TYPES);
	return print_floor();
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
	fops.open = elevator_proc_open;
	fops.read = elevator_proc_read;
	fops.release = elevator_proc_release;
	
	if (!proc_create(ENTRY_NAME, PERMS, NULL, &fops)) {
		printk(KERN_WARNING "elevator_init\n");
		remove_proc_entry(ENTRY_NAME, NULL);
		return -ENOMEM;
	}

	floor.size = 0;
    floor.height = 0;
    INIT_LIST_HEAD(&floor.list);
	
	return 0;
}
module_init(elevator_init);

static void elevator_exit(void) {
    delete_passengers();
	remove_proc_entry(ENTRY_NAME, NULL);
}
module_exit(elevator_exit);
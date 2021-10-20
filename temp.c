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

static char *message;
static int read_p;





//building or floor_list is a structure with a height = num floors in the building's list
//each element of the list is a floor struct with a give length and a list of passengers
struct {
    int total_height;
    struct list_head list;
} building;

struct {
	int size;
    //if needed direction is established with current_floor (< | > | ==) destination floor
    int current_floor;
    int destination_floor;
	int total_weight;
	struct list_head list;
} elevator;

typedef struct floor {
    int size;
    struct list_head list;
};

typedef struct passenger {
    int start_floor;
    int destination_floor;
    int type;
    int weight;
	const char *name;
	struct list_head list;
} Passenger;

#define DAILY_WORKER 0
#define MAINTENANCE_PERSON 1
#define MAIL_CARRIER 2

#define NUM_PERSON_TYPES 3
#define MAX_WEIGHT 1000

//need to make it add to the list corresponding to that start floor
//need list of lists, iterate to find the one to add this person to
int add_person_to_list(int start_floor, int destination_floor, int type, int weight = 150) {
    if(persons.size >= persons.capacity) {
        return 0;
    }
    
    Passenger *new_passenger;
    new_passenger = kmalloc(sizeof(Passenger)*1, __GFP_RECLAIM);

    if(new_passenger == NULL) {
        return -ENOMEM;
    }

    switch(type) {
        case DAILY_WORKER:
            new_passenger->name = "D";
            break;
        case MAINTENANCE_PERSON:
            new_passenger->name = "M";
            break;
        case MAIL_CARRIER:
            new_passenger->name = "C";
            break;
        default;
            return -1;
    }
    new_passenger->start_floor = start_floor;
    new_passenger->destination_floor = destination_floor;
    new_passenger->type = type;
    new_passenger->weight = weight;

    list_add_tail(&new_passenger->list, &passengers->list);

    passengers.size += 1;

    return 0;
}
//this is addition of one person to the elevator
//assumes we feed this into the elevator correctly
int add_person_to_elevator(Passenger *current_passenger) {
    if(elevator.total_weight + current_passenger->weight > MAX_WEIGHT) {
        return -1;
    }
    elevator.total_weight += current_passenger->weight;
    list_add_tail(&current_passenger->list, &elevator->list)
    return 0;
}
//should add a whole floor to the elevator until capacity
int add_people_to_elevator(floorList current_floor) {
    struct list_head *temp;
    Passenger *current_passenger;

    list_for_each(temp, &current_floor.list) {
        current_passenger = list_entry(temp, Passenger, list);
        if(add_person_to_elevator(current_passenger) == -1) {
            return -1;
        }
    }
    return 0;
}

//must define floorList
//prints a single floor
int print_floor(char *buf, floorList floor) {
    struct list_head *temp;
    Passenger *current_passenger;

    //if floor is floor where elevator is at:
        //sprintf(buf, "[ ] Floor %d: %d ", floor.index, floor.size);
    //else:
        //sprintf(buf, "[*] Floor %d: %d ", floor.index, floor.size);
    list_for_each(temp, &floor.list) {
        current_passenger = list_entry(temp, Passenger, list);
        sprintf(buf, "%s ", current_passenger->name);
    }
    strcat(message, buf);
    strcat(message, "\n");

    kfree(buf);
    return 0;
}
int print_floors(void) {
    int i;
    Passenger *p;
	struct list_head *temp;

	char *buf = kmalloc(sizeof(char) * 100, __GFP_RECLAIM);
	if (buf == NULL) {
		printk(KERN_WARNING "print_elevators");
		return -ENOMEM;
	}

    strcpy(message, "");

    //either store in reverse order, or reverse iterate
    //for(auto floor : floorList.reverse)
        //print_floor(buf, floor);
}
void delete_passengers(int type) {
//make delete from floor and delete from elevator
}

/********************************************************************/

int elevator_proc_open(struct inode *sp_inode, struct file *sp_file) {
	read_p = 1;
	message = kmalloc(sizeof(char) * ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	if (message == NULL) {
		printk(KERN_WARNING "elevator_proc_open");
		return -ENOMEM;
	}
	
	//add_elevator(get_random_int() % NUM_elevator_TYPES);
	return print_elevators();
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

	elevators.size = 0;
	elevators.total_weight = 0;
    //set the floor sizes as well
        //floorList.size = 3;
	INIT_LIST_HEAD(&elevators.list);
	
	return 0;
}
module_init(elevator_init);

static void elevator_exit(void) {
    //need to properly define delete files
	delete_from_floors(DAILY_WORKER);
	delete_from_floors(MAINTENANCE_PERSON); 
	delete_from_floors(MAIL_CARRIER);
    //for these as well
    delete_from_elevators(DAILY_WORKER);
	delete_from_elevators(MAINTENANCE_PERSON); 
	delete_from_elevators(MAIL_CARRIER);

	remove_proc_entry(ENTRY_NAME, NULL);
}
module_exit(elevator_exit);
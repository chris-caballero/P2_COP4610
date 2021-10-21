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
	int total_weight;
    int direction;
	struct list_head list;
} elevator;

typedef struct floor {
    int size;
    int height;
    struct list_head list;
} Floor;

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

int min(int a, int b) { return (a <= b) ? a : b; }

//need to make it add to the list corresponding to that start floor
//need list of lists, iterate to find the one to add this person to
int new_passenger(int start_floor, int destination_floor, int type, int weight = 150) {
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

    return 0;
}
int add_passenger_to_floor(Passenger *p, Floor *f) {
    //exceptions
    if(p == NULL) {
        return -1;
    } else if(f == NULL) {
        return -1;
    } else if(p->start_floor != f->height) {
        return -1;
    }
        
    list_add_tail(&p->list, &floor->list);
    f.size += 1;

    return 0;
}
int add_passenger_to_building(Passenger *p) {
    if(p == NULL) {
        return -1;
    }
    struct list_head *temp;
    Floor *current_floor;
    current_floor = kmalloc(sizeof(Floor)*1, __GFP_RECLAIM);
    if(current_floor == NULL) {
        return -ENOMEM;
    }
    list_for_each(temp, &building.list) {
        current_floor = list_entry(temp, Floor, list);
        if(current_floor->height == p->start_floor) {
            if(add_passenger_to_floor(p, current_floor) == -1) {
                return -1;
            }
        }
    }

    return 0;
}

void new_building(void) {
    building.total_height = NUM_FLOORS;
    Floor *current_floor;
    current_floor = kmalloc(sizeof(Floor)*1, __GFP_RECLAIM);
    if(current_floor == NULL) {
        return -ENOMEM
    }
    for(int i = 0; i < NUM_FLOORS; i++) {
        current_floor->size = 0;
        current_floor->height = i;
        list_add_tail(&current_floor->list, &building->list);
    }
}

void new_elevator(void) {
	elevator.size = 0;
    elevator.current_floor = 0;
	elevator.total_weight = 0;
    elevator.direction = 0;
}
//this is addition of one person to the elevator
int add_person_to_elevator(Passenger *current_passenger) {
    if(current_passenger == NULL) {
        return -1;
    } else if(elevator.total_weight + current_passenger->weight > MAX_WEIGHT) {
        return -1;
    }
    elevator.total_weight += current_passenger->weight;
    list_add_tail(&current_passenger->list, &elevator->list)
    return 0;
}
//should add a whole floor to the elevator until capacity
//also remove person from floor list 
int load_elevator(Floor *current_floor) {
    if(current_floor->height != elevator.current_floor) {
        return -1;
    }
    struct list_head *temp;
    Passenger *current_passenger;
    current_passenger = kmalloc(sizeof(Passenger)*1, __GFP_RECLAIM);
    if(current_passenger == NULL) {
        return -ENOMEM;
    }

    list_for_each(temp, &current_floor->list) {
        current_passenger = list_entry(temp, Passenger, list);
        if(add_person_to_elevator(current_passenger) == -1) {
            return -1;
        }
        elevator.size += 1;
        //remove from floor
        list_del(current_passenger);
        kfree(current_passenger);
    }
    return 0;
}
//take people off the elevator at their destination
int unload_elevator(Floor *current_floor) {
    if(current_floor->height != elevator.current_floor) {
        return -1;
    }
    struct list_head *temp;
    p = kmalloc(sizeof(Passenger)*1, __GFP_RECLAIM);
    if(p == NULL) {
        return -ENOMEM;
    }
    list_for_each(temp, &elevator.list) {
        Passenger *p;
        p = list_entry(temp, Passenger, list);
        if(p->destination_floor == current_floor->height) {
            list_del(p);
            kfree(p);
            elevator.size -= 1;
        }
    }
    return 0;
}
int unload_floor(Floor *current_floor) {
    struct list_head *temp;
    Passenger *p;
    p = kmalloc(sizeof(Passenger)*1, __GFP_RECLAIM);
    if(p == NULL) {
        return -ENOMEM;
    }
    list_for_each(temp, &current_floor->list) {
        p = list_entry(temp, Passenger, list);
        list_del(p);
        kfree(p);
        floor.size -= 1;
    }
    return 0;
}

void destroy_building(void) {
    struct list_head *temp;
    Floor *f;
    f = kmalloc(sizeof(Floor)*1, __GFP_RECLAIM);
    if(f == NULL) {
        return -ENOMEM;
    }
    //not sure about this either
    //should unload the elevator?
    while(take_to_destinations() != 1) {
        continue;
    }
    //destroy the floors
    list_for_each(temp, &building.list) {
        f = list_entry(temp, Floor, list);
        unload_floor(f);
        list_del(f);
        kfree(f);
    }
}
//need to make get_passengers
int take_to_destinations(void) {
    struct list_head *temp;
    int closest_floor = NUM_FLOORS;

    if(elevator.direction >= 0) {
        elevator.direction = 1;
        list_for_each(temp, &elevator.list) {
            Passenger *p = list_entry(temp, Passenger, list);
            closest_floor = min(closest_floor, p->destination_floor);
        }
        elevator.current_floor = closest_floor;
        list_for_each(temp, &building.list) {
            Floor *current_floor = list_entry(temp, Floor, list);
            if(current_floor->height == closest_floor) {
                unload_elevator(current_floor);
                break;
            }
        }
        if(elevator.size == 0) {
            elevator.direction = -1;
            return 1;
        }
    } else {
        closest_floor = 0;
        elevator.current_floor = closest_floor;
        //not sure about this reference
        temp = &building.list;
        Floor *current_floor = list_entry(temp, Floor, list);
        if(current_floor == NULL) {
            return -1;
        }
        //if(current_floor->height == closest_floor) ?
        //should be the right one
        unload_elevator(current_floor);
    }
    return 0;
}

//prints a single floor
int print_floor(char *buf, Floor *current_floor) {
    struct list_head *temp;
    Passenger *current_passenger;

    if(current_passenger == NULL) {
        return -ENOMOM;
    }


    if(current_floor->height == elevator.current_floor) {
        sprintf(buf, "[ ] Floor %d: %d ", current_floor->height + 1, current_floor->size);
    } else {
        sprintf(buf, "[*] Floor %d: %d ", current_floor->height + 1, current_floor->size);
    }
    list_for_each(temp, &current_floor->list) {
        current_passenger = list_entry(temp, Passenger, list);
        sprintf(buf, "%s ", current_passenger->name);
    }
    strcat(message, buf);
    strcat(message, "\n");

    kfree(buf);
    return 0;
}
int print_building(void) {
	struct list_head *temp;
    Floor *current_floor;

	char *buf = kmalloc(sizeof(char) * 100, __GFP_RECLAIM);
	if (buf == NULL) {
		printk(KERN_WARNING "print_building null buffer");
		return -ENOMEM;
	} else if(current_floor == NULL) {
        printk(KERN_WARNING "print_building current_floor");
		return -ENOMEM;
    }

    strcpy(message, "");

    //not positive how this works but I think this should work?
    list_for_each_entry_reverse(temp, &building.list, list) {
        current_floor = list_entry(temp, Floor, list);
        print_floor(buf, current_floor);
    }

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
	return print_building();
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

	new_elevator();
    new_building();
    //set the floor sizes as well
        //floorList.size = 3;
	INIT_LIST_HEAD(&elevator.list);
    INIT_LIST_HEAD(&building.list);
	
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

    //removes elevator passengers and floor passengers
    destroy_building();
    //how to free elevator and building??? 

	remove_proc_entry(ENTRY_NAME, NULL);
}
module_exit(elevator_exit);
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
#define NUM_FLOORS 3
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
int total_workers;
int total_maintenance;
int total_carriers;
int total_tally;

typedef struct {
    int size;
    int height;
    int up;
    int down;
    struct list_head list;
} Floor;

struct {
    int total_height;
    Floor **floors;
    //struct list_head list;
} building;

struct thread_parameter {
	int size;
    int state;
    int direction;
    int num_workers;
    int num_maintenance;
    int num_carriers;
    int current_floor;
	int total_weight;
    struct list_head list;

    struct task_struct *kthread;
    struct mutex mutex;
};


typedef struct {
    int start_floor;
    int destination_floor;
    int type;
    int weight;
	const char *name;
	struct list_head list;
} Passenger;

int waitingPassengers;      //total_tally - elevator.size
int travelingPassengers;    //elevator.size

struct thread_parameter *elevator;

int load_elevator(struct thread_parameter *elevator);
int unload_elevator(struct thread_parameter *elevator);
int get_next_stop(void);

int get_closest_above(struct thread_parameter *elevator);
int get_closest_below(struct thread_parameter *elevator);

int thread_run(void *data) {
    int i;
    int next_stop = -1;
    struct thread_parameter *param = data;

    while(!kthread_should_stop()) {
        if(mutex_lock_interruptible(&param->mutex) == 0) {
            switch(param->state) {
                case OFFLINE:
                    if(total_tally > 0) {
                        for(i = 0; i < NUM_FLOORS; i++) {
                            if(building.floors[i]->size() > 0) {
                                next_stop = i;
                                param->state = UP;
                                break;
                            }
                        }
                        if(next_stop == param->current_floor) {
                            param->direction = -1;
                            param->state = LOADING;
                        } else {
                            param->state = UP;
                            ++param->current_floor;
                        }
                    } else {
                        param->state = IDLE;
                    }
                    break;
                
                case UP:
                    ssleep(1);
                    param->direction = 1;
                    if(param->current_floor == next_stop || building[param->current_floor].up) {
                        param->state = LOADING;
                    } 
                    if(param->current_floor == NUM_FLOORS-1) {
                        param->state = DOWN;
                    } else {
                        param->current_floor++;
                    }
                    break;

                case DOWN:
                    ssleep(1);
                    param->direction = -1;
                    if(param->current_floor == next_stop || building[param->current_floor].down) {
                        param->state = LOADING;
                    } else if(param->current_floor == 0) {
                        param->state = UP;
                    } else {
                        param->current_floor--;
                    }
                    break;

                case LOADING:
                    ssleep(2);
                    int uld_res = unload_elevator(&param);
                    int ld_res = load_elevator(&param);

                    if(elevator.size() == 0) {
                        if(total_tally == 0) {
                            param->state = IDLE;
                        } else {
                            next_stop = get_next_stop();
                            if(next_stop == -1) {
                                param->state = IDLE
                            } else if(next_stop > param->current_floor) {
                                param->state = UP;
                                ++param->current_floor;
                            } else {
                                param->state = DOWN;
                                --param->current_floor;
                            }
                        }
                    } else {
                        if(param->direction == 1) {
                            next_stop = get_closest_above();
                            param->state = UP;
                            ++param->current_floor;
                        } else {
                            next_stop = get_closest_below();
                            param->state = DOWN;
                            --param->current_floor;
                        }
                    }
                    break;
                
                case IDLE:
                    if(total_tally == 0) {
                        //wait
                    } else {
                        next_stop = get_next_stop();
                        if(next_stop > param->current_floor) {
                            param->state = UP;
                            ++param->current_floor;
                        } else if(next_stop < param->current_floor) {
                            param->state = DOWN;
                            --param->current_floor;
                        } else {
                            if(next_stop == NUM_FLOORS - 1) {
                                param->direction = 1;
                            } else if(next_stop == 0) {
                                param->direction = -1;
                            }
                            param->state = LOADING;
                        }
                    }
                    break;

                default:
                    return -1;
            }
            mutex_unlock(&param->mutex);
        }
    }
    return 0;
}

void thread_init_parameter(struct thread_parameter *param) {
	static int size = 0;
    static int state = OFFLINE;
    static int current_floor = 0;
    static int direction = 0;
    static int total_weight = 0;
    static int num_workers = 0;
    static int num_maintenance = 0;
    static int num_carriers = 0;

    param->size = size;
    param->state = state;
    param->current_floor = current_floor;
    param->direction = direction;
    param->total_weight = total_weight;
    param->num_workers = num_workers;
    param->num_maintenance = num_maintenance;
    param->num_carriers = num_carriers;
    INIT_LIST_HEAD(&param->list);
    mutex_init(&param->mutex);
    param->kthread = kthread_run(thread_run, param, "thread elevator\n");
}

/***************************************************************/

int get_next_stop(void) {
    int i;
    Floor *f;
    

    for(i = NUM_FLOORS - 1; i >= 0; i--) {
        if(building.floors[i]->size > 0) {
            return i;
        }
    }
    return -1;
}

int load_elevator(Floor *f) {
    //move list is elevator
	struct list_head *temp;
	struct list_head *dummy;
    Passenger *p;

    list_for_each_safe(temp, dummy, &f->list) {
        p = list_entry(temp, Passenger, list);
        if(p->weight + elevator.total_weight > MAX_WEIGHT) {
            break;
        } else {
            list_move_tail(temp, &elevator.list);
            f->size -= 1;
            elevator.total_weight += p->weight;
            elevator.size += 1;
            switch(p->type) {
                case DAILY_WORKER:
                    elevator.num_workers += 1;
                    break;
                case MAINTENANCE_PERSON:
                    elevator.num_maintenance += 1;
                    break;
                case MAIL_CARRIER:
                    elevator.num_carriers += 1;
                    break;
                default:
                    return -1;
            }
        }
    }
    return 0;
}

int unload_elevator(Floor *f) {
    struct list_head *temp;
	struct list_head *dummy;
    Passenger *p;

    list_for_each_safe(temp, dummy, &elevator.list) {
        p = list_entry(temp, Passenger, list);
        if(p->destination_floor == f->height) {
            list_del(temp);
            kfree(p);
        }
    }
    return 0;
}
//create passenger and add to floor[start_floor]
int add_passenger(int start_floor, int destination_floor, int type) {
    int i;
    //struct list_head * temp;
    Passenger *new_passenger;

    curr_floor = kmalloc(sizeof(Floor)*1, __GFP_RECLAIM);
    new_passenger = kmalloc(sizeof(Passenger)*1, __GFP_RECLAIM);
    if(new_passenger == NULL || curr_floor == NULL) {
        return -ENOMEM;
    }

    new_passenger->weight = 150;
    switch(type) {
        case DAILY_WORKER:
            new_passenger->name = "D";
            total_workers += 1;
            break;
        case MAINTENANCE_PERSON:
            new_passenger->name = "M";
            new_passenger->weight += 20;
            total_maintenance += 1;
            break;
        case MAIL_CARRIER:
            new_passenger->name = "C";
            new_passenger->weight += 75;
            total_carriers += 1;
            break;
        default:
            return -1;
    }

    new_passenger->start_floor = start_floor;
    new_passenger->destination_floor = destination_floor;
    new_passenger->type = type;

    &building.floors[start_floor]->up = destination_floor > start_floor;
    &building.floors[start_floor]->down = destination_floor < start_floor;

    list_add_tail(&new_passenger->list, &building.floors[start_floor]->list);
    &building.floors[start_floor]->size += 1;

    return 0;
}

int print_floor(Floor* floor) {
    struct list_head *temp;
    Passenger *p;
    char *buf;

    buf = kmalloc(sizeof(char) * 100, __GFP_RECLAIM);
    p = kmalloc(sizeof(Passenger)*1, __GFP_RECLAIM);
	if (buf == NULL || p == NULL) {
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
        if(p == NULL) {
            return -1;
        }
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
    Passenger *p;

    buf = kmalloc(sizeof(char) * 100, __GFP_RECLAIM);
    p = kmalloc(sizeof(Passenger)*1, __GFP_RECLAIM);
	if (buf == NULL || p == NULL) {
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
    sprintf(buf, "Number of passengers waiting: %d\n", total_workers + total_maintenance + total_carriers - elevator.size);
    strcat(message, buf);
    sprintf(buf, "Number of passengers serviced: %d\n", total_tally); strcat(message, buf);
    kfree(buf);

    strcat(message, "\n");

    if(print_building() == -1) {
        return -1;
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
    //int i;
	read_p = 1;
	message = kmalloc(sizeof(char) * ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	if (message == NULL) {
		printk(KERN_WARNING "elevator_proc_open");
		return -ENOMEM;
	}
    // for(i = 0; i < 3; i++) {
    //     add_passenger(get_random_int() % NUM_FLOORS, get_random_int() % NUM_FLOORS, get_random_int() % NUM_PERSON_TYPES);
    // }
	return print_stats();
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

    total_workers = 0;
    total_maintenance = 0;
    total_carriers = 0;
    total_tally = 0;

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

    thread_init_parameter(&elevator); 
    if(IS_ERR(elevator.kthread)) {
        printk(KERN_WARNING "error spawning thread");
		remove_proc_entry(ENTRY_NAME, NULL);
		return PTR_ERR(elevator.kthread);
    }   

    for(i = 0; i < 10; i++) {
        start = get_random_int() % NUM_FLOORS;
        end =  get_random_int() % NUM_FLOORS;
        if(start == 0) {
            while(end == 0) {
                end =  get_random_int() % NUM_FLOORS;
            }
        } else {
            end = 0;
        }
        add_passenger(start, end, get_random_int() % NUM_PERSON_TYPES);
    }
    return 0;
}
module_init(elevator_init);

static void elevator_exit(void) {
    delete_floors();
    kthread_stop(elevator.kthread);
	remove_proc_entry(ENTRY_NAME, NULL);
    mutex_destroy(&elevator.mutex);
    printk(KERN_NOTICE "Removing /proc/%s\n", ENTRY_NAME);
}
module_exit(elevator_exit);

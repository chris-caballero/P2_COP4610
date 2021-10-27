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
int total_tally;
int total_serviced;

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

struct thread_elevator {
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


struct thread_passenger {
    int start_floor;
    int destination_floor;
    int type;
    int weight;
	const char *name;
	struct list_head list;

    struct task_struct *kthread;
    struct mutex mutex;
};


condition waiting_passengers;

struct thread_elevator elevator;

int load_elevator(elevator *elevator);
int unload_elevator(elevator *elevator);
int get_next_stop(void);

int get_closest_above(elevator *elevator);
int get_closest_below(elevator *elevator);

int add_passenger(struct thread_passenger *new_passenger, int start_floor, int destination_floor, int type);

void thread_run_passenger(void *data) {
    struct thread_elevator *param = data;

    if(mutex_lock_interruptible(&param->mutex) == 0) {
        do {
            start = get_random_int() % NUM_FLOORS;
            end =  get_random_int() % NUM_FLOORS;
        } while(start == end);
        add_passenger(param, start, end, get_random_int() % NUM_PERSON_TYPES);
    }
    mutex_unlock(&param->mutex);
    while(!kthread_should_stop()) {
        ssleep(2);
    }
}
int add_passenger(struct thread_passenger *new_passenger, int start_floor, int destination_floor, int type) {
    if(start_floor == destination_floor) {
        return -1;
    }

    new_passenger->weight = 150;
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
        default:
            return -1;
    }

    new_passenger->start_floor = start_floor;
    new_passenger->destination_floor = destination_floor;
    new_passenger->type = type;

    //if anyone on the floor is going up, up is true, same for down
    building.floors[start_floor]->up = (building.floors[start_floor]->up || destination_floor > start_floor);
    building.floors[start_floor]->down = (building.floors[start_floor]->down || destination_floor < start_floor);

    list_add_tail(&new_passenger->list, &building.floors[start_floor]->list);
    building.floors[start_floor]->size += 1;

    total_tally++;

    return 0;
}

void thread_init_thread_passenger(struct thread elevator *param) {
	static int start_floor = 0;
    static int destination_floor = OFFLINE;
    static int type = 0;
    static int weight = 0;
    static const char *name = "";
    static condition serviced = 0;

    param->start_floor = start_floor;
    param->destination_floor = destination_floor;
    param->type = type;
    param->weight = weight;
    param->name = name;
    param->serviced = 0;
    INIT_LIST_HEAD(&param->list);
    mutex_init(&param->mutex);
    param->kthread = kthread_run(thread_run_elevator, param, "thread elevator\n");
}

/************ Thread Elevator************/
int thread_run_elevator(void *data) {
    int i;
    int next_stop = -1;
    struct thread_elevator *param = data;

    while(!kthread_should_stop()) {
        if(mutex_lock_interruptible(&param->mutex) == 0) {
            switch(param->state) {
                case OFFLINE:
                    ssleep(5);
                    if(total_tally > 0) {
                        for(i = 0; i < NUM_FLOORS; i++) {
                            if(building.floors[i]->size > 0) {
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
                            param->current_floor += 1;
                        }
                    } else {
                        param->state = IDLE;
                    }
                    break;
                
                case UP:
                    ssleep(1);
                    param->direction = 1;
                    if(param->current_floor == next_stop) {
                        param->state = LOADING;
                    } else if(building.floors[param->current_floor]->up && MAX_WEIGHT - param->total_weight > 150) {
                        param->state = LOADING;
                    } else if(param->current_floor == NUM_FLOORS-1) {
                        param->state = DOWN;
                    } else {
                        param->current_floor += 1;
                    }
                    break;

                case DOWN:
                    ssleep(1);
                    param->direction = -1;
                    if(param->current_floor == next_stop || building.floors[param->current_floor]->down) {
                        param->state = LOADING;
                    } else if(param->current_floor == 0) {
                        param->state = UP;
                    } else {
                        param->current_floor -= 1;
                    }
                    break;

                case LOADING:
                    ssleep(2);
                    unload_elevator(param);
                    load_elevator(param);
                    if(param->size == 0) {
                        if(total_tally == 0) {
                            param->state = IDLE;
                        } else {
                            next_stop = get_next_stop();
                            if(next_stop == -1) {
                                param->state = IDLE;
                            } else if(next_stop > param->current_floor) {
                                param->state = UP;
                                param->current_floor += 1;
                            } else if(next_stop < param->current_floor) {
                                param->state = DOWN;
                                param->current_floor -= 1;
                            } else {
                                param->direction *= -1;
                                param->state = LOADING;
                            }
                        }
                    } else {
                        if(param->direction == 1) {
                            next_stop = get_closest_above(param);
                            param->state = UP;
                            param->current_floor += 1;
                        } else {
                            next_stop = get_closest_below(param);
                            param->state = DOWN;
                            param->current_floor -= 1;
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
                            param->current_floor += 1;
                        } else if(next_stop < param->current_floor) {
                            param->state = DOWN;
                            param->current_floor -= 1;
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

void thread_init_elevator(struct thread_elevator *param) {
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
    param->kthread = kthread_run(thread_run_elevator, param, "thread elevator\n");
}

/************ Calculating Destinations ************/
int get_next_stop(void) {
    int i;
    for(i = NUM_FLOORS - 1; i >= 0; i--) {
        if(building.floors[i]->size > 0) {
            return i;
        }
    }
    return -1;
}

int get_closest_above(struct thread_elevator *elevator) {
    struct list_head *temp;
    struct list_head *dummy;
    struct thread_passenger *p;
    int closest = NUM_FLOORS;

    //relies on all destinations lying above the current floor

    list_for_each_safe(temp, dummy, &elevator->list) {
        p = list_entry(temp, passenger, list);
        closest = min(closest, p->destination_floor);
    }
    return closest;
}

int get_closest_below(struct thread_elevator *elevator) {
    struct list_head *temp;
    struct list_head *dummy;
    struct thread_passenger *p;
    int closest = -1;

    //relies on all destinations lying above the current floor

    list_for_each_safe(temp, dummy, &elevator->list) {
        p = list_entry(temp, passenger, list);
        closest = max(closest, p->destination_floor);
    }
    return closest;
}

/************ Loading Actions ************/

int load_elevator(struct thread_elevator *elevator) {
    struct list_head *temp;
    struct list_head *dummy;
    struct thread_passenger *p;

    if(&building.floors[elevator->current_floor]->size == 0) {
        return 0;
    }

    if(elevator->current_floor == NUM_FLOORS-1 || elevator->current_floor == 0) {
        elevator->direction *= -1;
    }
    list_for_each_safe(temp, dummy, &building.floors[elevator->current_floor]->list) {
        p = list_entry(temp, passenger, list);
        if(elevator->total_weight + p->weight > MAX_WEIGHT) {
            continue;
        }
        if(elevator->direction == 1) {
            if(p->destination_floor > elevator->current_floor) {
                list_move_tail(temp, &elevator->list);
                elevator->size += 1;
                building.floors[elevator->current_floor]->size -= 1;
                elevator->total_weight += p->weight;
                switch(p->type) {
                    case DAILY_WORKER:
                        elevator->num_workers += 1;
                        break;
                    case MAINTENANCE_PERSON:
                        elevator->num_maintenance += 1;
                        break;
                    case MAIL_CARRIER:
                        elevator->num_carriers += 1;
                        break;
                }
            }
        } else {
            if(p->destination_floor < elevator->current_floor) {
                list_move_tail(temp, &elevator->list);
                elevator->size += 1;
                building.floors[elevator->current_floor]->size -= 1;
                elevator->total_weight += p->weight;
                switch(p->type) {
                    case DAILY_WORKER:
                        elevator->num_workers += 1;
                        break;
                    case MAINTENANCE_PERSON:
                        elevator->num_maintenance += 1;
                        break;
                    case MAIL_CARRIER:
                        elevator->num_carriers += 1;
                        break;
                }
            }
        }   
    }
    if(elevator->direction == 1) {
        building.floors[elevator->current_floor]->up = 0;
    } else {
        building.floors[elevator->current_floor]->down = 0;
    }

    return 0;
}

int unload_elevator(struct thread_elevator *elevator) {
    struct list_head *temp;
    struct list_head *dummy;
    struct thread_passenger *p;

    if(elevator->size == 0) {
        return 0;
    }

    list_for_each_safe(temp, dummy, &elevator->list) {
        p = list_entry(temp, passenger, list);
        if(p->destination_floor == elevator->current_floor) {
            list_del(temp);
            kthread_stop(p->kthread);
            elevator->size -= 1;
            elevator->total_weight -= p->weight;
            total_tally -= 1;
            total_serviced += 1;
            switch(p->type) {
                case DAILY_WORKER:
                    elevator->num_workers -= 1;
                    break;
                case MAINTENANCE_PERSON:
                    elevator->num_maintenance -= 1;
                    break;
                case MAIL_CARRIER:
                    elevator->num_carriers -= 1;
                    break;
            }
        }
    }
    return 0;
}


/***************************************************************/


//create passenger and add to floor[start_floor]


int print_floor(Floor* floor) {
    struct list_head *temp;
    passenger *p;
    char *buf;

    buf = kmalloc(sizeof(char) * 100, __GFP_RECLAIM);
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

    list_for_each_safe(temp, &floor->list) {
        p = list_entry(temp, passenger, list);
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
    sprintf(buf, "Number of passengers waiting: %d\n", total_tally - elevator.size);
    strcat(message, buf);
    sprintf(buf, "Number of passengers serviced: %d\n", total_serviced); strcat(message, buf);
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

    total_serviced = 0;
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

    thread_init_elevator(&elevator); 
    if(IS_ERR(elevator.kthread)) {
        printk(KERN_WARNING "error spawning thread");
		remove_proc_entry(ENTRY_NAME, NULL);
		return PTR_ERR(elevator.kthread);
    }   

    for(i = 0; i < 50; i++) {
        start = get_random_int() % NUM_FLOORS;
        end =  get_random_int() % NUM_FLOORS;
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

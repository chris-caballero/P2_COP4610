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

#define ENTRY_NAME "elevator"
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

// extern int errno;

/************************** Building/Floor structs and functions **************************/

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
    //struct list_head list;
} building;

int print_stats(void);
int print_building(void);
int print_floor(Floor* floor);
int delete_floors(void);

/************************** Elevator struct and functions **************************/

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
struct thread_elevator elevator;

int shutdown_signal;

int load_elevator(struct thread_elevator *elevator);
int unload_elevator(struct thread_elevator *elevator);
int get_next_stop(void);
int get_closest_above(struct thread_elevator *elevator);
int get_closest_below(struct thread_elevator *elevator);

/************************** Passenger struct and functions **************************/

typedef struct {
    int start_floor;
    int destination_floor;
    int type;
    int weight;
	const char *name;
	struct list_head list;
} Passenger;

int add_passenger(int start_floor, int destination_floor, int type);


/************************** System Call Handlers **************************/

extern long (*STUB_start_elevator)(void);
extern long (*STUB_issue_request)(int,int,int);
extern long (*STUB_stop_elevator)(void);

long start_elevator(void) {
    shutdown_signal = 0;
	if(IS_ERR(elevator.kthread)) {
        return -ENOMEM;
    }
	
	if(elevator.state != OFFLINE) {
		return 1;
	}
	elevator.state = IDLE;
	return 0;
}
long issue_request(int start_floor, int destination_floor, int type) {
	if(start_floor < 0 || start_floor >= NUM_FLOORS || destination_floor < 0 || destination_floor >= NUM_FLOORS) {
		return 1;
	}
	add_passenger(start_floor, destination_floor, type);

	return 0;
}
long stop_elevator(void) {
	if(shutdown_signal == 1) {
		return 1;
	}
	shutdown_signal = 1;
	return 0;
}


/************************** Elevator Definitions **************************/

//elevator thread running process
int thread_run_elevator(void *data) {
    struct thread_elevator *param = data;
	int state = param->state, prev_state = state, next_stop = -1;

    //loop these instructions while the module is open (since close is when we stop the thread)
    while(!kthread_should_stop()) {
        //lock shared data being accessed by the elevator (such as building, and elevator info)
        //switch statement outlines each state the elevator can be in a given timestep 
        if(mutex_lock_interruptible(&elevator.mutex) == 0) {
            state = param->state;
            mutex_unlock(&elevator.mutex);
        }
        switch(state) {
            //the elevator is just starting up
            case OFFLINE:
				ssleep(1);
				//elevator will be turned on externally
				//poll every second to see if elevator in IDLE state yet
                break;
            //moving up in the elevator
            case UP:
                //if return from IDLE or OFFLINE we need to check the current floor 
                //for people going in same direction as the elevator
                if(prev_state == LOADING || prev_state == UP) {
                    //elevator must wait for one second before transitioning between floors
                    ssleep(1);
                    if(mutex_lock_interruptible(&elevator.mutex) == 0) {
                        param->current_floor += 1;
                        //set elevator direction to 1 (we do a SCAN up)
                        param->direction = 1;
                        mutex_unlock(&elevator.mutex);
                    }
                    param->direction = 1;
                }
                if(mutex_lock_interruptible(&elevator.mutex) == 0) {
                    if(mutex_lock_interruptible(&building.mutex) == 0) {
                        if(param->current_floor == next_stop) {
                            //if the elevator is at a destination floor for someone in the elevator,
                            //load the elevator
                            param->state = LOADING;
                        } else if(building.floors[param->current_floor]->up && MAX_WEIGHT - param->total_weight > 150) {
							//if we are not shutting the elevator down, pick people up from floors
							if(shutdown_signal == 0) {
								//if the current floor has up = 1 (essentially a button indicator),
								//elevator can quickly check if anyone is going in the same direction
								if(param->current_floor == 0) {
									param->direction = -1;
								}
								param->state = LOADING;
							}
							//otherwise we are just planning on dropping people off so continue

                        } else if(param->current_floor == NUM_FLOORS-1) {
                            //if the current floor is the top floor, go down (just to prevent infinite loops, may not run)
                            param->state = DOWN;
                        }
                        mutex_unlock(&building.mutex);
                    }
                    mutex_unlock(&elevator.mutex);
                }
                prev_state = UP;
                break;
            //moving down in elevator
            //essentially the opposite of moving UP as described above
            case DOWN:
                if(prev_state == LOADING || prev_state == DOWN) {
                    ssleep(1);
                    if(mutex_lock_interruptible(&elevator.mutex) == 0) {
                        param->current_floor -= 1;
                        param->direction = -1;
                        mutex_unlock(&elevator.mutex);
                    }
                }
                if(mutex_lock_interruptible(&elevator.mutex) == 0) {
                    if(mutex_lock_interruptible(&building.mutex) == 0) {
                        if(param->current_floor == next_stop) {
                            param->state = LOADING;
                        } else if (building.floors[param->current_floor]->down && MAX_WEIGHT - param->total_weight > 150) {
							if(shutdown_signal == 0) {
								if(param->current_floor == NUM_FLOORS-1) {
									param->direction = 1;
								}
								param->state = LOADING;
							}
                        } else if(param->current_floor == 0) {
                            param->state = UP;
                        }
                        mutex_unlock(&building.mutex);
                    }
                    mutex_unlock(&elevator.mutex);
                }

                prev_state = DOWN;
                break;
            //loading and unloading the elevator
            case LOADING:
                ssleep(2);
                if(mutex_lock_interruptible(&elevator.mutex) == 0) {
                    //calls function to unload the elevator, see unload_elevator(struct thread_elevator *elevator);
                    unload_elevator(param);
                    //calls function to load the elevator, see load_elevator(struct thread_elevator *elevator);
					//if shutdown signal is sent, we don't load the elevator anymore
					if(shutdown_signal == 0) {
						load_elevator(param);
					}
                    //if the elevator is empty
                    if(param->size == 0) {
						//if there is no one in the elevator and a shutdown signal has been sent, shutdown
						if(shutdown_signal == 1) {
							param->state = OFFLINE;
						} else {
							//if there is no one in the building, go into IDLE waiting state
							if(building.total_tally == 0) {
								param->state = IDLE;
							} else {
								//if there is no one in the elevator, but someone in the building
								//find the next floor using "int get_next_stop(void)"
								next_stop = get_next_stop();

								if(next_stop == -1) {
									//won't happen, but just in case, go into idle state if you dont find a next stop
									param->state = IDLE;
								} else if(next_stop > param->current_floor) {
									//next floor to pick people up at is above us, go up
									param->state = UP;
								} else if(next_stop < param->current_floor) {
									//next floor to pick people up at is below us, go down
									param->state = DOWN;
								} else {
									//this is an outlier case
									//when the next_stop is the same as the current_floor, we are at a pivot
									//we need to change direction from here as if we are at the top floor or the lobby
									param->direction *= -1;
									param->state = LOADING;
								}
							}
						}
					} else {
                        //elevator has passengers so find the closest floor above or below using:
                        //get_closest_above(struct thread_elevator *elevator), //going up
                        //get_closest_below(struct thread_elevator *elevator); //going down 
                        if(param->direction == 1) {
                            next_stop = get_closest_above(param);
                            param->state = UP;
                        } else {
                            next_stop = get_closest_below(param);
                            param->state = DOWN;
                        }
                    }
                    mutex_unlock(&elevator.mutex);
                }
                prev_state = LOADING;
                break;
            //waiting state
            case IDLE:
                if(shutdown_signal == 1) {
                    if(mutex_lock_interruptible(&elevator.mutex) == 0) {
                        param->state = OFFLINE;
                        mutex_unlock(&elevator.mutex);
                    }
                } else if (mutex_lock_interruptible(&building.mutex) == 0) {
                    if(building.total_tally > 0) {
                        mutex_unlock(&building.mutex);
                        //tally > 0 shows we have a waiting passenger
                        //get the next stop and go up and down as in "case LOADING"
                        next_stop = get_next_stop();

                        if(next_stop > param->current_floor) {
                            param->state = UP;
                        } else if(next_stop < param->current_floor) {
                            param->state = DOWN;
                        } else {
                            //current floor is the next stop, so account for loading direction flip
                            if(next_stop == NUM_FLOORS - 1) {
                                param->direction = 1;
                            } else if(next_stop == 0) {
                                param->direction = -1;
                            }
                            param->state = LOADING;
                        }
                    } else {
                        mutex_unlock(&building.mutex);
                        //poll tally after 1 seconds
                        //maybe not good as it will make it difficult to read
                        ssleep(1);
                    }
                }
                prev_state = IDLE;
                break;

            default:
                return -1;
        }
    }
    return 0;
}
void thread_init_elevator(struct thread_elevator *param) {
    //initialize all thread parameters (elevator parameters)
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
    //initialize the list of passengers for the elevator
    INIT_LIST_HEAD(&param->list);
    mutex_init(&param->mutex);
    //run the elevator thread process
    param->kthread = kthread_run(thread_run_elevator, param, "thread elevator\n");
}
//find the next stop 
int get_next_stop(void) {
    int i;
    //SCAN from top floor down to the lobby
    for(i = NUM_FLOORS - 1; i >= 0; i--) {
        if(mutex_lock_interruptible(&building.mutex) == 0) {
            if(building.floors[i]->size > 0) {
                mutex_unlock(&building.mutex);
                //highest floor with people is next stop
                //we will be picking up and dropping off people on the way up if possible
                return i;
            }
            mutex_unlock(&building.mutex);
        }
    }
    return -1;
}
int get_closest_above(struct thread_elevator *elevator) {
    struct list_head *temp;
    struct list_head *dummy;
    Passenger *p;
    int closest = NUM_FLOORS;

    //relies on all destinations lying above the current floor
        //this condition is met in our implementation of "case LOADING" 
        //since everyone we pick up is going in the same direction
    //find the min destination_floor among all passengers
    list_for_each_safe(temp, dummy, &elevator->list) {
        p = list_entry(temp, Passenger, list);
        closest = min(closest, p->destination_floor);
    }
    return closest;
}
int get_closest_below(struct thread_elevator *elevator) {
    struct list_head *temp;
    struct list_head *dummy;
    Passenger *p;
    int closest = -1;

    //relies on all destinations lying below the current floor
    //works opposite to get_closest_above
    list_for_each_safe(temp, dummy, &elevator->list) {
        p = list_entry(temp, Passenger, list);
        closest = max(closest, p->destination_floor);
    }
    return closest;
}
int load_elevator(struct thread_elevator *elevator) {
    struct list_head *temp;
    struct list_head *dummy;
    Passenger *p;

    if(mutex_lock_interruptible(&building.mutex) == 0) {
        //if the floor is empty, no need to load
        if(&building.floors[elevator->current_floor]->size == 0) {
            mutex_unlock(&building.mutex);
            return 0;
        }
        //if we are at an extrema (top floor or lobby), we change direction
        //elevator and passengers stay withing the bounds of the building
        //exit if no one on this floor going in same direction
        if(elevator->current_floor == NUM_FLOORS-1 || elevator->current_floor == 0) {
            elevator->direction *= -1;
        }

        list_for_each_safe(temp, dummy, &building.floors[elevator->current_floor]->list) {
            p = list_entry(temp, Passenger, list);
            if(MAX_WEIGHT - elevator->total_weight <= 150) {
                break;
            } else if(elevator->total_weight + p->weight > MAX_WEIGHT) {
                //weight capacity exceeded, skip this passenger
                continue;
            }
            //elevator moving up
            if(elevator->direction == 1) {
                //if passenger wants to go up
                if(p->destination_floor > elevator->current_floor) {
                    //remove the passenger from the list and add it to the elevator
                    list_move_tail(temp, &elevator->list);
                    //elevator has one more passenger
                    elevator->size += 1;
                    //elevator weight increases by passenger weight
                    elevator->total_weight += p->weight;
                    //increment the respective type counter
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
                        default:
                            mutex_unlock(&building.mutex);
                            return -1;
                    }
                    //floor has one less passenger
                    building.floors[elevator->current_floor]->size -= 1;
                }
            } else {
                //elevator moving down
                //if passenger wants to go down
                if(p->destination_floor < elevator->current_floor) {
                    //procedure to load identical to when elevator is moving up
                    list_move_tail(temp, &elevator->list);
                    elevator->size += 1;
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
                        default:
                            mutex_unlock(&building.mutex);
                            return -1;
                    }
                    building.floors[elevator->current_floor]->size -= 1;
                }
            }   
        }
        //the elevator has processed this floor, 
        //floor indicator reset to 0

        if(elevator->direction == 1) {
            building.floors[elevator->current_floor]->up = 0;
        } else {
            building.floors[elevator->current_floor]->down = 0;
        }
        mutex_unlock(&building.mutex);
    }


    return 0;
}
int unload_elevator(struct thread_elevator *elevator) {
    struct list_head *temp;
    struct list_head *dummy;
    Passenger *p;
    //elevator is empty so there is no one to unload
    if(elevator->size == 0) {
        return 0;
    }

    list_for_each_safe(temp, dummy, &elevator->list) {
        p = list_entry(temp, Passenger, list);
        //if the passenger wants to get off at this floor
        if(p->destination_floor == elevator->current_floor) {
            //remove from elevator list
            list_del(temp);
            //update elevator size, weight, and type tracker
            elevator->size -= 1;
            elevator->total_weight -= p->weight;
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
            if(mutex_lock_interruptible(&building.mutex) == 0) {
                //update the tally, one less person in scope
                building.total_tally -= 1;
                //increment the serviced tracker, one more person has left scope
                building.total_serviced += 1;
                mutex_unlock(&building.mutex);
            }
            kfree(p);
        }
    }
    return 0;
}

/************************** Passenger Definitions **************************/

int add_passenger(int start_floor, int destination_floor, int type) {
    Passenger *new_passenger;

    new_passenger = kmalloc(sizeof(Passenger)*1, __GFP_RECLAIM);
    if(new_passenger == NULL) {
        return -ENOMEM;
    }

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
    INIT_LIST_HEAD(&new_passenger->list);

    if(mutex_lock_interruptible(&building.mutex) == 0) {
        //if anyone on the floor is going up, up is true, same for down
        building.floors[start_floor]->up = (building.floors[start_floor]->up || destination_floor > start_floor);
        building.floors[start_floor]->down = (building.floors[start_floor]->down || destination_floor < start_floor);

        list_add_tail(&new_passenger->list, &building.floors[start_floor]->list);
        building.floors[start_floor]->size += 1;

        building.total_tally++;
        mutex_unlock(&building.mutex);
    }
    return 0;
}

/************************** Printing/Utility Definitions **************************/

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

/************************** Proc Functions **************************/

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

/************************** Kernel Module Init/Exit **************************/

static int elevator_init(void) {
    int i;
	fops.open = elevator_proc_open;
	fops.read = elevator_proc_read;
	fops.release = elevator_proc_release;

	STUB_start_elevator = start_elevator;
	STUB_issue_request = issue_request;
	STUB_stop_elevator = stop_elevator;

    shutdown_signal = 0;
	
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
    
    return 0;
}
module_init(elevator_init);

static void elevator_exit(void) {
	int s;
    //first we need to let elevator finish
	shutdown_signal = 1;
	do {
		ssleep(1);
	} while(elevator.state != OFFLINE);


    if(mutex_lock_interruptible(&building.mutex) == 0) {
        delete_floors();
        mutex_unlock(&building.mutex);
    }
    
    kthread_stop(elevator.kthread);
	remove_proc_entry(ENTRY_NAME, NULL);
    mutex_destroy(&elevator.mutex);

	STUB_start_elevator = NULL;
	STUB_issue_request = NULL;
	STUB_stop_elevator = NULL;

	printk(KERN_NOTICE "Removing /proc/%s\n", ENTRY_NAME);
}
module_exit(elevator_exit);
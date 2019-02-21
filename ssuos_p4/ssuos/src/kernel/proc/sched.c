#include <list.h>
#include <proc/sched.h>
#include <mem/malloc.h>
#include <proc/proc.h>
#include <proc/switch.h>
#include <interrupt.h>
#include <device/console.h>

extern struct list level_que[QUE_LV_MAX];
extern struct list plist;
extern struct list slist;
extern struct process procs[PROC_NUM_MAX];
extern struct process *idle_process;
extern int handling;

struct process *latest;

struct process* get_next_proc(struct list *rlist_target);
void proc_que_levelup(struct process *cur);
void proc_que_leveldown(struct process *cur);
struct process* sched_find_que(void);

int scheduling;
/*
	linux multilevelfeedback queue scheduler
	level 1 que policy is FCFS(First Come, First Served)
	level 2 que policy is RR(Round Robin).
*/

//sched_find_que find the next process from the highest queue that has proccesses.
struct process* sched_find_que(void) {
	int i,j;
	struct process * result = NULL;
	 
	proc_wake();
	for(i = 1; i < 3; i++) {
		if(!list_empty(&level_que[i])) {
			result = get_next_proc(&level_que[i]);
			return result;
		}
	}
		/*TODO :check the queue whether it is empty or not  
		 and find each queue for the next process.
		*/
		

}

struct process* get_next_proc(struct list *rlist_target) {
	struct list_elem *e;

	for(e = list_begin (rlist_target); e != list_end (rlist_target);
		e = list_next (e))
	{
		struct process* p = list_entry(e, struct process, elem_stat);

		if(p->state == PROC_RUN)
			return p;
	}
	return NULL;
}

void schedule(void)
{

	struct process *cur;
	struct process *next;
	struct process *tmp;
	struct list_elem *ele;
	int i = 0, printed = 0, j = 0;

	scheduling = 1;	
	cur = cur_process;
	/*TODO : if current process is idle_process(pid 0), schedule() choose the next process (not pid 0).
	when context switching, you can use switch_process().  
	if current process is not idle_process, schedule() choose the idle process(pid 0).
	complete the schedule() code below.
	*/
	for(i = 1; i < 3; i++) {
		if(list_empty(&level_que[i])) {
			j++;
		}
	}
	if(j == 2){
		return;
	}
	if ((cur -> pid) != 0) {
		if(cur->que_level == 1 && cur->time_slice >= TIMER_MAX) {
			printk("proc%d change the queue(1->2)\n", cur_process->pid);
			list_remove(&cur_process->elem_stat);
			list_push_back(&level_que[2], &cur_process->elem_stat);
			cur_process->que_level = 2;
			cur_process = idle_process;
		}
		else if(cur->que_level == 2 && cur->time_slice >= TIMER_MAX*2) {
			list_remove(&cur_process->elem_stat);
			list_push_back(&level_que[2], &cur_process->elem_stat);
			cur_process = idle_process;
		}
		else
			;
		cur_process = idle_process;
		intr_disable();
		switch_process(cur, idle_process);
		intr_enable();
		scheduling = 0;
		return;
	}

		switch (latest -> que_level){
			
		}

	proc_wake(); //wake up the processes 
	
	//print the info of all 10 proc.
	for (ele = list_begin(&plist); ele != list_end(&plist); ele = list_next(ele)) {
			intr_disable();
		tmp = list_entry (ele, struct process, elem_all);
		if ((tmp -> state == PROC_ZOMBIE) || 
			//(tmp -> state == PROC_BLOCK) || 
			//(tmp -> state == PROC_STOP) ||
					(tmp -> pid == 0)) 	continue;
			if (!printed) {	
				printk("#= %2d t=%3d u=%3d ", tmp -> pid, tmp -> time_slice, tmp -> time_used);
				printk("q=%3d\n", tmp->que_level);
				printed = 1;			
			}
			else {
				printk(", #=%2d t=%3d u=%3d ", tmp -> pid, tmp -> time_slice, tmp->time_used);
				printk("q=%3d\n", tmp->que_level);
				}
			intr_enable();
	}

	if (printed)
		printk("\n");

	if ((next = sched_find_que()) != NULL) {
		printk("Selected process : %d\n", next -> pid);
		next->time_slice = 0;
		cur_process = next;
		scheduling = 0;
		intr_disable();
		switch_process(cur, next);
		intr_enable();
		return;
	}
	scheduling = 0;
	return;
}

void proc_que_levelup(struct process *cur)
{
	/*TODO : change the queue lv2 to queue lv1.*/
}

void proc_que_leveldown(struct process *cur)
{
	/*TODO : change the queue lv1 to queue lv2.*/
}

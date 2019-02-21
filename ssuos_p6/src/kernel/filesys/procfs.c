#include <filesys/procfs.h>
#include <filesys/vnode.h>
#include <proc/proc.h>
#include <list.h>
#include <string.h>
#include <ssulib.h>

extern struct list p_list;
extern struct process *cur_process;

struct vnode *init_procfs(struct vnode *mnt_vnode)
{
	mnt_vnode->v_op.ls = proc_process_ls;
	mnt_vnode->v_op.mkdir = NULL;
	mnt_vnode->v_op.cd = proc_process_cd;

	return mnt_vnode;
}

int proc_process_ls()
{
	int result = 0;
	struct list_elem *e;

	printk(". .. ");
	for(e = list_begin (&p_list); e != list_end (&p_list); e = list_next (e))
	{
		struct process* p = list_entry(e, struct process, elem_all);

		printk("%d ", p->pid);
	}
	printk("\n");

	return result;
}

int proc_process_cd(char *dirname)
{
	int result = 0;
	struct list_elem *e;
	struct vnode * child = vnode_alloc();
	struct vnode *cwd = cur_process->cwd;

	memcpy(child->v_name, dirname, LEN_VNODE_NAME);
	child->v_op.ls = proc_process_info_ls;
	child->v_op.cd = proc_process_info_cd;
	child->v_op.cat = proc_process_info_cat;
	child->type = DIR_TYPE;
	child->v_parent = cur_process->cwd;
	list_push_back(&cwd->childlist, &child->elem);

	for(e = list_begin(&cwd->childlist); e != list_end(&cwd->childlist); e = list_next(e)) {
		child = list_entry(e, struct vnode, elem);

		if(strcmp(child->v_name, dirname) == 0) {
			if(child->type == DIR_TYPE) {
				cur_process->cwd = child;
			}else {
				printk("%s is not a directory\n", dirname);
			}
			return result;
		}
	}
	vnode_free(child);
	printk("%s is not a directory\n", dirname);
	return result;
}

int proc_process_info_ls()
{
	int result = 0;

	printk(". .. ");
	printk("cwd root time stack");

	printk("\n");

	return result;
}

int proc_process_info_cd(char *dirname)
{	
	int result = 0;
	struct list_elem *e;
	struct vnode * child = vnode_alloc();
	struct vnode *cwd = cur_process->cwd;
	
	if(strcmp(dirname, "cwd") != 0 && strcmp(dirname, "root") != 0) {
		printk("cd operation error!\n");
		return result;
	}

	memcpy(child->v_name, dirname, LEN_VNODE_NAME);
	child->v_op.ls = proc_link_ls;
	child->v_op.cd = NULL;
	child->v_op.cat = NULL;
	child->type = DIR_TYPE;
	child->v_parent = cur_process->cwd;
	list_push_back(&cwd->childlist, &child->elem);

	for(e = list_begin(&cwd->childlist); e != list_end(&cwd->childlist); e = list_next(e)) {
		child = list_entry(e, struct vnode, elem);

		if(strcmp(child->v_name, dirname) == 0) {
			if(child->type == DIR_TYPE) {
				cur_process->cwd = child;
			}else {
				printk("%s is not a directory\n", dirname);
			}
			return result;
		}
	}
	vnode_free(child);
	printk("%s is not a directory\n", dirname);
	return result;

}

int proc_process_info_cat(char *filename)
{
	int result = 0;
	struct list_elem *e;
	struct vnode *cwd = cur_process->cwd;
	struct process *p;
	
	if(strcmp(filename, "stack") != 0 && strcmp(filename ,"time") != 0) {
		printk("cat operation error!\n");
		return result;
	}
	for(e = list_begin (&p_list); e != list_end (&p_list); e = list_next (e))
	{
		p = list_entry(e, struct process, elem_all);
		if(p->pid == cwd->v_name[0]-'0') {
			break;
		}
	}
	if(strcmp(filename, "stack") == 0) {
		printk("stack : %x\n", p->stack);
	}
	else if(strcmp(filename, "time") == 0) {
		printk("time_used : %lu\n", p->time_used);
	}
	else
		;

	return result;

}

int proc_link_ls()
{
	int result = 0;
	struct list_elem *e;
	struct vnode *cwd = cur_process->cwd->v_parent;
	struct vnode *child;
	
	printk(". .. ");
	for(e = list_begin (&p_list); e != list_end (&p_list); e = list_next (e))
	{
		struct process* p = list_entry(e, struct process, elem_all);
		if(p->pid == cwd->v_name[0]-'0') {
			if(strcmp(cur_process->cwd->v_name, "root") == 0)
				cwd = p->rootdir;
			else if(strcmp(cur_process->cwd->v_name, "cwd") ==0)
				cwd = p->cwd;
			else ;
			break;
		}
	}
	for(e = list_begin(&cwd->childlist); e != list_end(&cwd->childlist); e = list_next(e)) {
		child = list_entry(e, struct vnode, elem);
		printk("%s ", child->v_name);
	}
	printk("\n");

	return result;
}

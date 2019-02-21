#include <list.h>
#include <proc/sched.h>
#include <mem/malloc.h>
#include <proc/proc.h>
#include <ssulib.h>
#include <string.h>
#include <interrupt.h>
#include <proc/sched.h>
#include <device/console.h>
#include <device/io.h>
#include <syscall.h>
#include <mem/paging.h>
#include <mem/palloc.h>
#include <filesys/fs.h>
#include <filesys/file.h>

#define STACK_SIZE	512
#define PROC_NUM_MAX 16

struct list p_list;		// All Porcess List
struct list r_list;		// Run Porcess List
struct list s_list;		// Sleep Process List
struct list d_list;		// Deleted Process List

struct process procs[PROC_NUM_MAX];
struct process *cur_process;
int pid_num_max;


uint32_t process_stack_ofs;

//values for pid
static int lock_pid_simple; //1 : lock, 0 : unlock
static int lately_pid;		//init vaule = -1

bool more_prio(const struct list_elem *a, const struct list_elem *b,void *aux);
bool less_time_sleep(const struct list_elem *a, const struct list_elem *b,void *aux);
pid_t getValidPid(int *idx);

void proc_start(void);				//프로세스 시작시 실행
void proc_end(void);				//프로세스 종료시 실행

void init_proc()
{
	process_stack_ofs = offsetof (struct process, stack);

	lock_pid_simple = 0;
	lately_pid = -1;

	list_init(&p_list);
	list_init(&r_list);
	list_init(&s_list);
	list_init(&d_list);

	int i;
	for (i = 0; i < PROC_NUM_MAX; i++)
	{
		memset (&procs[i], 0, 77);
		procs[i].pid = i;
		procs[i].state = PROC_UNUSED;
		procs[i].parent = NULL;
	}

	pid_t pid = getValidPid(&i);
    cur_process = &procs[i];

    cur_process->pid = pid;
    cur_process->parent = NULL;
    cur_process->state = PROC_RUN;
	cur_process->priority = 0;
	cur_process->stack = 0;
	cur_process->pd = (void*)read_cr3();

	list_push_back(&p_list, &cur_process->elem_all);
	list_push_back(&r_list, &cur_process->elem_stat);
}

pid_t getValidPid(int *idx) {

	pid_t pid = -1;
	int i;

	while(lock_pid_simple)
		;

	lock_pid_simple++;

	// find unuse process pid and return it
	for(i = 0; i < PROC_NUM_MAX; i++)
	{
		int tmp = i + lately_pid + 1;// % PROC_NUM_MAX;
		if(procs[tmp % PROC_NUM_MAX].state == PROC_UNUSED) { // find out valid state;
			pid = lately_pid + 1;
			*idx = tmp % PROC_NUM_MAX;
			break;
		}
	}

	if(pid != -1)
	{
		lately_pid = pid;	
	}

	lock_pid_simple = 0;

	return pid;
}

pid_t proc_create(proc_func func, struct proc_option *opt, void* aux, void* aux2)
{
	struct process *p;
	int idx;

	enum intr_level old_level = intr_disable();

	pid_t pid = getValidPid(&idx);
	p = &procs[idx];

	p->pid = pid;
	p->state = PROC_RUN;

	if(opt != NULL)
		p->priority = opt->priority;   
	else
		p->priority = (unsigned char)0;

	p->time_used = 0;
	p->time_sched= 0;
	p->parent = cur_process;
	p->simple_lock = 0;
	p->child_pid = -1;
	p->pd = pd_create(pid);
	
	int i;
	for(i=0;i<NR_FILEDES;i++)
		p->file[i] = NULL;

	//init stack
    int *top = (int*)palloc_get_page();
	int stack = (int)top;
	top = (int*)stack + STACK_SIZE - 1;

	*(--top) = (int)aux2;		//argument for func
	*(--top) = (int)aux; 
	*(--top) = (int)proc_end;	//return address from func
	*(--top) = (int)func;		//return address from proc_start
	*(--top) = (int)proc_start; //return address from switch_process


	*(--top) = (int)((int*)stack + STACK_SIZE - 1); //ebp
	*(--top) = 1; //eax
	*(--top) = 2; //ebx
	*(--top) = 3; //ecx
	*(--top) = 4; //edx
	*(--top) = 5; //esi
	*(--top) = 6; //edi

	p->stack = top;
	p->elem_all.prev = NULL;
	p->elem_all.next = NULL;
	p->elem_stat.prev = NULL;
	p->elem_stat.next = NULL;

	/* P7 */
	p->rootdir = p->parent->rootdir;
	p->cwd = p->parent->cwd;

	list_push_back(&p_list, &p->elem_all);
	list_push_back(&r_list, &p->elem_stat);

	intr_set_level (old_level);
	return p->pid;
}

void* getEIP()
{
    return __builtin_return_address(0);
}

void  proc_start(void)
{
	intr_enable ();
	return;
}

void proc_free(void)
{
	uint32_t pt = *(uint32_t*)cur_process->pd;
	cur_process->parent->child_pid = cur_process->pid;
	cur_process->parent->simple_lock = 0;

	list_remove(&cur_process->elem_stat);

	cur_process->state = PROC_ZOMBIE;	//change state
	list_push_back(&d_list, &cur_process->elem_stat);

}

void proc_end(void)
{
	proc_free();
	schedule();
	printk("never reach\n");
	return;	//never reach
}

void proc_wake(void)
{
	struct process* p;
	unsigned long long t = get_ticks();

    while(!list_empty(&s_list))
	{
		p = list_entry(list_front(&s_list), struct process, elem_stat);
		if(p->time_sleep > t)
			break;
		//proc_unblock(p);
		p->state = PROC_RUN;
		list_remove(&p->elem_stat);
	}
}

void proc_sleep(unsigned ticks)
{
	unsigned long cur_ticks = get_ticks();
	cur_process->time_sleep =  ticks + cur_ticks;
	cur_process->state = PROC_STOP;
	list_insert_ordered(&s_list, &cur_process->elem_stat,
			less_time_sleep, NULL);
	schedule();
}

void proc_block(void)
{
	cur_process->state = PROC_BLOCK;
	schedule();	
}

void proc_unblock(struct process* proc)
{
	enum intr_level old_level;

	old_level = intr_disable();

	list_push_back(&r_list, &proc->elem_stat);
	proc->state = PROC_RUN;

	intr_set_level(old_level);
}     

bool less_time_sleep(const struct list_elem *a, const struct list_elem *b,void *aux)
{
	struct process *p1 = list_entry(a, struct process, elem_stat);
	struct process *p2 = list_entry(b, struct process, elem_stat);

    return p1->time_sleep < p2->time_sleep;
}

bool more_prio(const struct list_elem *a, const struct list_elem *b,void *aux)
{
	struct process *p1 = list_entry(a, struct process, elem_stat);
	struct process *p2 = list_entry(b, struct process, elem_stat);
    return p1->priority > p2->priority;
}


void kernel1_proc(void* aux)
{
	cur_process -> priority = 200;
	while(1)
	{
		schedule();
	}
}

void kernel2_proc(void* aux)
{
	cur_process -> priority = 200;
	while(1)
	{
		schedule();
	}
}

void ps_proc(void* aux)
{
	int i;
	for(i = 0; i<PROC_NUM_MAX; i++)
	{
		struct process *p = &procs[i];

		if(p->state == PROC_UNUSED)
			continue;

		printk("pid %d ppid ", p->pid);

		if(p->parent != NULL)
			printk("%d", p->parent->pid);
		else
			printk("non");

		printk(" state %d prio %d using time %d sched time %d\n",
				p->state, p->priority, p->time_used, p->time_sched);

	}
	exit(1);
}

extern const char* VERSION;
extern const char* AUTHOR;
extern const char* MODIFIER;
void uname_proc(void* aux)
{
	printk("SSUOS %s\nmade by %s\nmodefied by %s\n", VERSION, AUTHOR, MODIFIER);	

}

void print_pid(void* aux) {

	while(1) {
		printk("pid = %d ", cur_process->pid);
		printk("prio = %d ", cur_process->priority);
		printk("time = %d ", cur_process->time_slice);
		printk("ticks = %d ", get_ticks());
		printk("in %s\n", aux);

#define SLEEP_FREQ 100
		proc_sleep(cur_process->pid * cur_process->pid * SLEEP_FREQ);
	}
}

void open_proc(void *aux)
{
	char* name = (char*)aux;
	open(name,0);
}

void write_proc(void *aux)
{
	char *name = (char *)aux;
	int fd;
	fd = open(name,O_WRONLY);
	write(fd,"ssuos:oslab",11);
}

void ls_proc(void *aux)
{
	list_segment(cur_process->cwd);
}

void cat_proc(void *aux)
{
	char buf[21] = {0};
	int fd;
	int length;
	fd = open((char *)aux,O_RDONLY);
	if (fd < 0) return;
	length = read(fd, buf, 20);
	printk("%s\n",buf);
}

void lseek_proc(void *aux, void *filename)
{
	char buf[BUFSIZ] = {0};
	int fd;
	if(*(char *)filename == 0 && *(char *)aux != 0) { //filename과 aux에 들어온값을 확인해 제대로 매핑해주기 위한 코드
		if(*(char *)aux == '-') { //이 경우는 ex) test -option 이런식으로 사용한것이기에 오류메세지
			printk("You used wrong syntax\n");
			return;
		} else {//그렇지 않은경우에는 aux와 filename을 swap해줌
			void *test;
			test = aux;
			aux = filename;
			filename = test; }
	} else if(*(char *)filename != 0 && *(char *)aux != 0) { //filename과 aux에 둘다 값이 들어온경우
		if(*(char *)filename == '-') { //이 경우는 ex) test filename -option 이런식으로 사용한 것이기에 오류메세지
			printk("You used wrong syntax\n");
			return;
		} else {//그 외의 경우에는 test -option filename이렇게 들어온경우
			 if(strcmp(aux,"-e") == 0) { //e옵션들어왔을시 e_opt변수 1로, 옵션이 들어왔다는 opt_int도 1로
				 e_opt = 1;
				 opt_int = 1;
			 }
			 else if(strcmp(aux, "-a") == 0) {
				a_opt = 1;
				opt_int = 1;
			 }
			 else if(strcmp(aux, "-re") == 0) {
				 re_opt = 1;
				 opt_int = 1;
			 }
			 else if(strcmp(aux, "-c") == 0) {
				 c_opt = 1;
				 opt_int = 1;
			 }
			 else;
			 int res = e_opt + a_opt + re_opt + c_opt;
			 if(res > 1) {//옵션이 동시에 여러개가 들어온경우 예외처리
				 printk("You used too much options!\n");
				 return;
			 }
			 if(opt_int == 0) {//등록되어있지않은 옵션을 사용한경우에 예외처리
				 printk("You used wrong option!\n");
				 return;
			 }
		}
	} else;

	fd = open(filename, O_RDWR);
	if (fd < 0 )return;
	if(opt_int == 0) { //기본적으로 test a.txt를 하면 시행되는 코드이다. 이 코드는 option test를 하기전에 필수적으로 선행되어야되는 코드
		write(fd , "ssuos ",6);
		//if you add lseek() system call , remove the '//'
		printk ("%d\n", lseek(fd, -3, SEEK_CUR));  
		write(fd, "world",5);
		lseek(fd, 0, SEEK_SET);
		read(fd , buf, 8);
		printk("%s\n", buf);
		lseek(fd, -8, SEEK_END);
		read(fd, buf, 8);
		printk("%s\n", buf);
		test = 1; //옵션 테스트 선행조건  test a.txt 필수수행
	}
	//ssuworld 가 정확히 출력되어야 함
	// 옵션에 대한 시나리오 및 검증할 코드 아래에 추가
	// 각 옵션에 대해 파일 크기 및 내용이 정확하게 채워지는지 보여야 함
	/*   option  */
	if(opt_int != 0) {
		if(test == 0) { //test a.txt가 수행되지 않았을 경우에는 해당 메시지 출력
			printk("Must already used \"test a.txt\" before option test\n");
		}
		else { //test a.txt가 제대로 기수행됐고 option test들어가는 경우
			printk("-----option test-----\n");
			if(e_opt) {//e옵션이 들어온 경우
				printk("-----e_option on!-----\n");
				lseek(fd, -2, SEEK_END);//파일의 끝에서 -2만큼 offset이동
				printk("cur offset : %d\n", lseek(fd, 0, SEEK_CUR)); //현재 offset의 위치를 보여준다
				printk("mod offset : %d\n", lseek(fd, 5, SEEK_CUR)); //e옵션 테스트하기위함, 현재 offset에서 파일의 끝을 초과해서 탐색시도 후 offset 출력해준다
				lseek(fd, 0, SEEK_SET);//파일에 옵션대로 채워졌는지 확인하기위함
				read(fd, buf, 11);
				printk("%s\n", buf);
				printk("mod offset : %d\n", lseek(fd, 2, SEEK_END));//다시 SEEK_END부터 2정도 더 초과해서 탐색시도해본다 후에 이동된 offset출력
				lseek(fd, 0, SEEK_SET);//파일에 옵션대로 채워졌는지 확인하기위함
				read(fd, buf, 13);
				printk("%s\n", buf);
				printk("Please restart to test other option\n");//e옵션 사용후 다른옵션 임의대로 사용해서 테스트 할수 있는 경우를 방지하기위해 시나리오를 각 옵션별로 만들어놓음
				//이에 따라 각 옵션 테스트 1회 후 shutdown 후 make -> make run수행해줘서 다시 처음부터 다른 옵션 테스트 수행
			}else if(a_opt){//a옵션이 들어온 경우
				printk("-----a_option on!-----\n");
				printk("cur offset : %d\n", lseek(fd, 0, SEEK_CUR));//일단은 현재 offset을 출력해준다 0이 나와야정상 -> file을 open해줬기때문
				printk("mod offset : %d\n", lseek(fd, 2, SEEK_CUR));//현재 offset에서 파일의 끝쪽으로 2만큼 이동 -> offset은 2가 더 이동되야하고 파일의 첫부분에 00이 들어가야함
				lseek(fd, 0, SEEK_SET);//파일에 옵션대로 채워졌는지 확인하기위함
				read(fd, buf, 10);
				printk("%s\n", buf);
				printk("cur offset : %d\n", lseek(fd, 0, SEEK_END));//이번에는 파일의 끝으로 offset이동
				printk("mod offset : %d\n", lseek(fd, -2, SEEK_CUR));//파일의 끝으로부터 -2만큼 이동 -> -2만큼 이동했으므로 offset은 -2가 되며 파일의 끝부분에 00이 들어가야함
				lseek(fd, 0, SEEK_SET);//파일에 옵션대로 채워졌는지 확인하기위함
				read(fd, buf, 12);
				printk("%s\n", buf);
				printk("Please restart to test other option\n");
			}else if(re_opt) {//re옵션 들어온 경우
				printk("-----re_option on!-----\n");
				lseek(fd, -2, SEEK_END);//일단 파일의 끝에서 -2만큼 이동한다 이경우 파일의 범위를 초과하지 않기에 그냥 제대로 수행됨 -> offset 6
				printk("cur offset : %d\n", lseek(fd, 0, SEEK_CUR));//현재 offset출력해줌
				printk("mod offset : %d\n", lseek(fd, -8, SEEK_CUR));//파일의 앞부분 범위 초과해본다 -> 파일의 앞부분에 00이 들어가져야하며 offset은 0으로
				lseek(fd, 0, SEEK_SET); //파일에 옵션대로 채워졌는지 확인하기위함
				read(fd, buf, 10);
				printk("%s\n", buf);
				printk("mod offset : %d\n", lseek(fd, -2, SEEK_SET));//offset을 처음으로 해주고 파일의 앞부분으로 한번 더 초과 위와동일하다
				read(fd, buf, 12);//파일에 옵션대로 채워졌는지 확인하기 위함
				printk("%s\n", buf);
				printk("Please restart to test other option\n");
			}else {//c옵션이 들어온 경우
				printk("-----c_option on!-----\n");
				printk("cur offset : %d\n", lseek(fd, 0, SEEK_END));//파일의 끝으로 offset이동
				printk("mod offset : %d\n", lseek(fd, 3, SEEK_CUR));//EOF넘어선 경우
				printk("mod offset : %d\n", lseek(fd, -5, SEEK_CUR));//파일의 시작부분 넘어선 경우
				//c옵션의 경우 출력을 해줄게 offset밖에 없기에 다시 껐다가 테스트 해볼 필요가없어 위와같은 문구 미작성
			}
		}
	}
	e_opt = 0; a_opt = 0; re_opt = 0; c_opt = 0; opt_int = 0;//옵션처리를 위한 전역변수들
}


typedef struct
{
	char* cmd;
	unsigned char type;	//0 : 직접실행, 1 : fork 함수실행
	void* func;
} CmdList;

void shell_proc(void* aux)
{
	//<<<HW>>>
	CmdList cmdlist[] = {
		{"shutdown", 0, shutdown}
		//{"ps", 1, ps_proc},
		//{"uname", 1, uname_proc}
		//{"write", 1, write_proc}
		,{"ls", 1, ls_proc}
		,{"touch",1,open_proc}
		,{"test",1, lseek_proc}
		,{"cat",1,cat_proc}
	};
#define CMDNUM 5
#define TOKNUM 3
	char buf[BUFSIZ];
	char token[TOKNUM][BUFSIZ];
	int token_num;
	struct direntry cwde;
	int fd;
	while(1)
	{
		proc_func *func;
		int i,j, len;
		/* cwd 출력*/
		if(cur_process->cwd == cur_process->rootdir)
			printk("~");
		else
			printk("%s", cwde.de_name);

		printk("> ");

		for(i=0;i<BUFSIZ;i++) 
		{
			buf[i] = 0;
			for(j=0;j<TOKNUM;j++)
				token[j][i] = 0;
		}
		
		while(getkbd(buf,BUFSIZ))
		{
			; 
		}
		
		for(i=0;buf[i] != '\n'; i++); 
		for(i--; buf[i] == ' '; i--)
			buf[i] = 0;

//		for( i = 0 ; buf[i] == ' ' ; i++)
//			buf[i] = 0;

		token_num = getToken(buf,token,TOKNUM);


		if( strcmp(token[0], "exit") == 0)
			break;

		if( strncmp(token[0], "list", BUFSIZ) == 0)
		{
			for(i = 0; i < CMDNUM; i++)
				printk("%s\n", cmdlist[i].cmd);
			continue;
		}

		if( strncmp(token[0], "cd", BUFSIZ) == 0)
		{
			if(change_dir(cur_process->cwd, token[1]) == 0)
			{
				get_curde(cur_process->cwd, &cwde);
			}
			continue;
		}
		
		for(i = 0; i < CMDNUM; i++)
		{
			if( strncmp(cmdlist[i].cmd, token[0], BUFSIZ) == 0)
				break;
		}

		if(i == CMDNUM)
		{
			printk("Unknown command %s\n", buf);
			continue;
		}

		if(cmdlist[i].type == 0)
		{
			void (*func)(void*);
			func = cmdlist[i].func;
			func((void*)0x9);
		}
		else if(cmdlist[i].type == 1)
		{
			cur_process->simple_lock = 1;
			int pid = fork(cmdlist[i].func, token[1], token[2]);

			while(cur_process->simple_lock)
				;
		}
		else
		{
			printk("Unknown type\n");
			continue;
		}
	}
}

void login_proc(void* aux)
{
	int i,fd;
	char id[30];
	char password[30];
	char buf[30];
	char buf2[] = "ssuos:oslab";

	cur_process -> priority = 100;
	
	//fd = open("passwd",O_RDWR);
	//write(fd,buf2,11);
	//fd = open("passwd",O_RDWR);
	//read(fd,buf,30);	
	while(1)
	{
		for(i=0;i<30;i++) {
			id[i] = 0;
			password[i] = 0;
		}
		printk("id : ");
		while(getkbd(id,BUFSIZ));
	    
		printk("password : ");
	    while(getkbd(password,BUFSIZ));
		
		if(id[6] != 0 || strncmp(id,buf2,5) != 0) {printk("%s\n",id); continue;}
		if(password[6] != 0 || strncmp(password,buf2+6,5) != 0) {printk("%s\n",password); continue;}
		shell_proc(NULL);
	}

}

void idle(void* aux)
{
	proc_create(kernel1_proc, NULL, NULL, NULL);
	proc_create(kernel2_proc, NULL, NULL, NULL);
	proc_create(login_proc, NULL, NULL, NULL);

	while(1) {  
		if(cur_process->pid != 0) {
			printk("error : idle process's pid != 0\n", cur_process->pid);
			while(1);
		}

		while( !list_empty(&d_list) )
		{
			struct list_elem *e = list_pop_front(&d_list);
			struct process *p = list_entry(e, struct process, elem_stat);
			p->state = PROC_UNUSED;
			list_remove( &p->elem_all);
		}

		schedule();     
	}
}

void proc_print_data()
{
	int a, b, c, d, bp, si, di, sp;

	//eax ebx ecx edx
	__asm__ __volatile("mov %%eax ,%0": "=m"(a));

	__asm__ __volatile("mov %ebx ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(b));
	
	__asm__ __volatile("mov %ecx ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(c));
	
	__asm__ __volatile("mov %edx ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(d));
	
	//ebp esi edi esp
	__asm__ __volatile("mov %ebp ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(bp));

	__asm__ __volatile("mov %esi ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(si));

	__asm__ __volatile("mov %edi ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(di));

	__asm__ __volatile("mov %esp ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(sp));

	printk(	"\neax %o ebx %o ecx %o edx %o"\
			"\nebp %o esi %o edi %o esp %o\n"\
			, a, b, c, d, bp, si, di, sp);
}

void hexDump (void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    if (len == 0) {
        printk("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printk("  NEGATIVE LENGTH: %i\n",len);
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printk ("  %s\n", buff);

            // Output the offset.
            printk ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printk (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printk ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printk ("  %s\n", buff);
}



#include <proc/sched.h>
#include <proc/proc.h>
#include <device/device.h>
#include <interrupt.h>
#include <device/kbd.h>
#include <filesys/file.h>
#include <syscall.h>

pid_t do_fork(proc_func func, void* aux1, void* aux2)
{
	pid_t pid;
	struct proc_option opt;

	opt.priority = cur_process-> priority;
	pid = proc_create(func, &opt, aux1, aux2);

	return pid;
}

void do_exit(int status)
{
	cur_process->exit_status = status; 	//종료 상태 저장
	proc_free();						//프로세스 자원 해제
	do_sched_on_return();				//인터럽트 종료시 스케줄링
}

pid_t do_wait(int *status)
{
	while(cur_process->child_pid != -1)
		schedule();
	//SSUMUST : schedule 제거.

	int pid = cur_process->child_pid;
	cur_process->child_pid = -1;

	extern struct process procs[];
	procs[pid].state = PROC_UNUSED;

	if(!status)
		*status = procs[pid].exit_status;

	return pid;
}

void do_shutdown(void)
{
	dev_shutdown();
	return;
}

int do_ssuread(void)
{
	return kbd_read_char();
}

int do_open(const char *pathname, int flags)
{
	struct inode *inode;
	struct ssufile **file_cursor = cur_process->file;
	int fd;

	for(fd = 0; fd < NR_FILEDES; fd++)
		if(file_cursor[fd] == NULL) break;

	if (fd == NR_FILEDES)
		return -1;

	if ( (inode = inode_open(pathname)) == NULL)
		return -1;
	
	if (inode->sn_type == SSU_TYPE_DIR)
		return -1;

	fd = file_open(inode,flags,0);
	
	return fd;
}

int do_read(int fd, char *buf, int len)
{
	return generic_read(fd, (void *)buf, len);
}
int do_write(int fd, const char *buf, int len)
{
	return generic_write(fd, (void *)buf, len);
}
int do_lseek(int fd, int offset, int whence) { //SEEK_SET 0, SEEK_CUR 1, SEEK_END -1
	struct ssufile *cursor = cur_process->file[fd];
	uint16_t *pos = &(cur_process->file[fd]->pos);
	int filesize = cursor->inode->sn_size;
	//int *pos = (int *)pos1;
	char buf[BUFSIZ] = {0,};
	int i;

	if(cursor == NULL) { //파일의 정보를 못가져오면 종료
		return -1;
	}
	switch(whence) { //들어온 whence별로 경우 나누어진다
		case 0://SEEK_SET인 경우
			*pos = (uint16_t)0; //일단 해당 file의 offset을 0으로 초기화해준다
			if(opt_int) { //옵션이 들어온경우
				if(e_opt) { //e옵션의 경우
					if(offset > 0) {//offset이 양수인경우에
						*pos += offset;//파일의 offset에 offset더해줌
						if(*pos > filesize){//offset이 filesize보다 초과한경우 e옵션 명세대로 처리
							int end_off = *pos - filesize;
							*pos -= end_off;
							for(i = 0; i < end_off; i++)
								write(fd, "0", 1);//초과한만큼 0으로 채워준다
						}
					}
					else { //offset이 음수이거나 양수인경우
						if(*pos < -offset)//e옵션은 파일의 앞부분으로 초과하는 것에 대해서 처리해주는 옵션이 아니므로 종료
							return -1;
						else//offset이 0일수도 있으므로 해당경우는 종료가 아니라 처리진행
							*pos += offset;
					}
				}else if(a_opt) {//a옵션의 경우
					read(fd, buf, filesize);//일단 offset이 파일의 시작점으로 가있기에 파일내용을 다 가져와 buf에 저장
					buf[filesize] = '\0';
					*pos = 0;//다시 파일의 offset은 0으로 초기화해줌
					if(offset < 0){ //offset이 음수인경우
						for(i = 0; i < -offset; i++)//파일의 초반부분에 0입력
							write(fd, "0", 1);
						write(fd, buf, filesize);//그 후에 초기에 있던 파일내용 입력해줌
						*pos = 0;//offset이 음수기에 SEEK_SET이므로 *pos를 0으로 초기화
					}
					else{//offset이 양수인경우
						for(i = 0; i < offset; i++)
							write(fd, "0", 1);//0입력
						write(fd, buf, filesize);//파일내용 입력
						*pos -= filesize;//현재 위치에서 써준 파일내용만큼 offset이동
					}
				}else if(re_opt) { //re옵션이 들어온경우
					if(offset < 0) {//offset이 음수인경우
						read(fd, buf, filesize);//buf에 파일내용저장
						buf[filesize] = '\0';
						*pos = 0;
						for(i = 0; i < -offset; i++)
							write(fd, "0", 1);//offset절대값만큼 0입력
						write(fd, buf, filesize);//다시 buf내용써주고 
						*pos = 0;//offset초기화
					}
					else {//offset이 양수인경우
						if(offset > filesize)//EOF넘어가면 바로 종료
							return -1;
						else//그렇지 않은경우에는 그대로 더해줌
							*pos += offset;
					}
				}else {//c옵션의 경우
					if(offset < 0) { //offset 음수인경우
						for(i = 0; i < -offset; i++) { //파일의 시작점만나게되면 *pos를 바로 EOF로 세팅
							if(*pos == 0)
								*pos = filesize;
							*pos -= 1;
						}
					}
					else {//offset 양수인경우
						for(i = 0; i < offset; i++) {
							if(*pos == filesize)//EOF만나게되면 *pos를 바로 파일의 시작점으로 세팅
								*pos = 0;
							*pos += 1;
						}
					}
				}
			}
			else {//옵션이 들어오지 않은 경우
				if(offset < 0 && *pos < -offset)
					return -1;
				else if(filesize - *pos < offset)
					return -1;
				else
					*pos += (uint16_t)offset;
			}
			break;
		case 1://SEEK_CUR인 경우
			if(opt_int) {//옵션이 들어오면
				if(e_opt) {//e옵션이 들어온경우
					if(offset < 0 && *pos < -offset) {//offset이 음수, 현재 *pos가 이동시킬 offset보다 더 작은경우에는 종료
						return -1;
					}
					else if(offset < 0 && *pos > -offset)//그렇지 않은 경우에는 그대로 더해줌
						*pos += (uint16_t)offset;
					else if(offset > 0) {//offset이 양수인경우에
						*pos += offset;//일단 *pos에 offset만큼 더해줌
						if(*pos > filesize) {//EOF초과시 옵션처리
							int end_off = *pos - filesize;
							*pos = filesize;
							for(i = 0 ; i < end_off; i++)
								write(fd, "0", 1);
						}
					}else;
				}else if(a_opt) {//a옵션이 들어온경우
					int tmp = filesize - *pos;//현재 *pos와 파일끝까지의 차이를 구해놓는다
					if(offset < 0) {//offset이 음수인경우
						read(fd, buf, tmp);//tmp만큼 현재 *pos부터 파일내용읽어서 저장
						buf[tmp] = '\0';
						*pos -= tmp;//읽기전으로 *pos 되돌려놓는다
						for(i = 0; i < -offset; i++) {//0입력
							write(fd, "0", 1);
						}
						write(fd, buf, tmp);//읽은내용 다시입력
						*pos -= tmp;//write전으로 *pos돌려놓음
						*pos += offset;//음수이므로 offset도 전으로
					}
					else {//양수의 경우 음수와는 다르게 마지막에 offset만큼돌리는 작업을 하지않는다
						read(fd, buf, tmp);
						buf[tmp] = '\0';
						*pos -= tmp;
						for(i = 0; i < offset; i++)
							write(fd, "0", 1);
						write(fd, buf, tmp);
						*pos -= tmp;
					}
				}else if(re_opt) {//re옵션의 경우
					if(offset < 0 && *pos < -offset) {//파일의 처음부분 넘어가서 접근하는 경우
						int tmp = *pos;//*pos값의 경우 uint16_t이기에 음수를 표현불가 임의의 int형 tmp변수에 값 넣어놓고
						tmp += offset;
						*pos = 0;
						read(fd, buf, filesize);//일단 파일 전체내용 다 읽어들여옴
						buf[filesize] = '\0';
						*pos = 0;//다시 초기화
						for(i = 0; i < -tmp; i++)//tmp는 음수일 것이므로 tmp만큼 0입력
							write(fd, "0", 1);
						write(fd, buf, filesize);//다시 파일의 내용을 뒤에 덧붙여줌
						*pos = 0;
					}
					else if(offset > 0 && *pos + offset > filesize)//EOF를 넘어가는 경우에는 바로 종료
						return -1;
					else//그외의 경우에는 그냥 offset값 더해준다
						*pos += (uint16_t)offset;

				}else {//c옵션의 경우
					if(offset < 0) {
						for(i = 0; i < -offset; i++) {
							if(*pos == 0)
								*pos = filesize;
							*pos -= 1;
						}
					}
					else {
						for(i = 0; i < offset; i++) {
							if(*pos == filesize)
								*pos = 0;
							*pos += 1;
						}
					}
				}
			}
			else {//옵션이 들어오지 않은경우
				if(offset < 0 && *pos < -offset) {
					return -1;
				}
				else if(*pos > cursor->inode->sn_size)
					return -1;
				else
					*pos += (uint16_t)offset;
			}
			break;
		case -1://SEEK_END일때
			*pos = filesize;
			if(opt_int) {
				if(e_opt) {//e옵션의 경우
					if(offset < 0) {
						if(-offset > filesize)
							return -1;
						else
							*pos += offset;
					}
					else {
						for(i = 0; i < offset; i++)
							write(fd, "0", 1);
					}
				}else if(a_opt) {//a옵션의 경우
					if(offset < 0) {
						for(i = 0; i < -offset; i++)
							write(fd, "0", 1);
						*pos += offset;
					}
					else {
						for(i = 0; i < offset; i++)
							write(fd, "0", 1);
					}
				}else if(re_opt) {//re옵션의 경우
					if(offset > 0)
						return -1;
					else {
						if(-offset < filesize)
							*pos += offset;
						else {
							int tmp = filesize + offset;

							*pos = 0;
							read(fd, buf, filesize);
							buf[filesize] = '\0';
							*pos = 0;
							for(i = 0; i < -tmp; i++)
								write(fd, "0", 1);
							write(fd, buf, filesize);
							*pos = 0;
						}
					}
				}else {//c옵션의 경우
					if(offset < 0) {
						for(i = 0; i < -offset; i++) {
							if(*pos == 0)
								*pos = filesize;
							*pos -= 1;
						}
					}else {
						for(i = 0; i < offset; i++) {
							if(*pos == filesize)
								*pos = 0;
							*pos += 1;
						}
					}
				}
			}
			else {//옵션이 들어오지 않은경우
				if(offset > 0)
					return -1;
				else if(offset < 0 && filesize < -offset)
					return -1;
				else
					*pos += (uint16_t)offset;
			}
			break;
		default:
			break;
	}
	return *pos;
}

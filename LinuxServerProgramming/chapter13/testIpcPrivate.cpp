#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

union semun
{
				int val;
				struct semid_ds *buf;
				unsigned short int *array;
				struct seminfo *__buf;
};

//op为-1时执行p操作(进入),为1时执行v操作(离开)
void pv(int semId, int op)
{
				struct sembuf semb;
				semb.sem_num=0;
				semb.sem_op=op;
				semb.sem_flg=SEM_UNDO;
				semop(semId, &semb, 1);
}

int main(int argc, char *argv[])
{
				int semId=semget(IPC_PRIVATE, 1, 0666);//IPC_CREATE表明,无论信号量是否存在,semget都将创建信号量.父子进程都可访问该信号量
				union semun semUn;
				semUn.val=1;
				semctl(semId, 0, SETVAL, semUn);

				pid_t id=fork();
				if(id<0)
				{
								return 1;
				}
				else if(id==0)
				{
								printf("child try to get binary sem\n");
								pv(semId, -1);
								printf("child get the sem and would release it after 5 seconds\n");
								sleep(5);
								pv(semId, 1);
								exit(0);
				}
				else
				{
								printf("parent try to get binary sem\n");
								pv(semId, -1);
								printf("parent get the sem and would release it after 5 seconds\n");
								sleep(5);
								pv(semId, 1);
				}
				waitpid(id, NULL, 0);
				semctl(semId, 0, IPC_RMID, semUn);//删除信号量
				return 0;
}

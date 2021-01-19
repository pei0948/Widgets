#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define USER_LIMIT 5
#define BUF_SIZE 1024
#define FD_LIMIT 65535
#define MAX_EVENT_NUM 1024
#define PROCESS_LIMIT 65535

struct ClientData
{
				sockaddr_in address;
				int conFd;
				pid_t pid;
				int pipeFd[2];
};

static const char *shmName="/myShm";//共享内存对象
int sigPipeFd[2];
int epollFd;
int listenFd;
int shmFd;
char* shareMem=0;

ClientData* users=nullptr;

int* subProcess=nullptr;//子进程和客户连接映射关系表,用进程PID索引即可找到相应用户的连接数据的编号

int userCnt=0;//当前客户数量
bool stopChild=false;

int setNonblock(int fd)
{
				int oldOption=fcntl(fd, F_GETFL);
				int newOption=oldOption|O_NONBLOCK;
				fcntl(fd, F_SETFL, newOption);
				return oldOption;
}

void addFd(int epollFd, int fd)
{
				epoll_event event;
				event.data.fd=fd;
				event.events=EPOLLIN|EPOLLET;
				epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);
				setNonblock(fd);
}

void sigHandler(int sig)
{
				int saveErrno=errno;
				int msg=sig;
				send(sigPipeFd[1], (char*)&msg, 1, 0);
				errno=saveErrno;//保证函数的可重入性
}

void addSig(int sig, void (*handler)(int), bool restart=true)
{
				struct sigaction sa;
				bzero(&sa, sizeof(sa));
				sa.sa_handler=handler;
				if(restart)
								sa.sa_flags|=SA_RESTART;
				sigfillset(&sa.sa_mask);
				assert(sigaction(sig, &sa, NULL)!=-1);
}

void delResource()
{
				close(sigPipeFd[1]);
				close(sigPipeFd[0]);
				close(listenFd);
				close(epollFd);
				shm_unlink(shmName);
				delete []users;
				delete []subProcess;
}

//停止子进程
void childTerm(int sig)
{
				stopChild=true;
}

//子进程运行函数
int runChild(int idx, ClientData* users, char *shareMem)
{
				epoll_event events[MAX_EVENT_NUM];

				//子进程利用I/O复用技术同时监听客户连接socket和与父进程通信管道
				int childEpollFd=epoll_create(5);
				assert(childEpollFd!=-1);
				int conFd=users[idx].conFd;
				addFd(childEpollFd, conFd);

				int pipeFd=users[idx].pipeFd[1];
				addFd(childEpollFd, pipeFd);

				addSig(SIGTERM, childTerm, false);

				int res;
				while(!stopChild)
				{
								int cnt=epoll_wait(childEpollFd, events, MAX_EVENT_NUM, -1);
								if(cnt<0 && errno!=EINTR)
								{
												printf("epoll fails\n");
												break;
								}
								for(int i=0; i<cnt; ++i)
								{
												int sockFd=events[i].data.fd;
												if(sockFd==conFd && (events[i].events&EPOLLIN))
												{
																memset(shareMem+idx*BUF_SIZE, '\0', BUF_SIZE);
																res=recv(conFd, shareMem+idx*BUF_SIZE, BUF_SIZE-1, 0);
																if(res<0)
																{
																				if(errno!=EAGAIN)
																				{
																								stopChild=true;
																				}
																}
																else if(res==0)
																{
																				stopChild=true;
																}
																else
																{
																				send(pipeFd, (char*)&idx, sizeof(idx), 0);
																}
												}
												else if(sockFd==pipeFd && (events[i].events&EPOLLIN))
												{
																int client=0;
																res=recv(sockFd, (char*)&client, sizeof(client), 0);
																if(res<0)
																{
																				if(errno!=EAGAIN)
																								stopChild=true;
																}
																else if(res==0)
																{
																				stopChild=true;
																}
																else
																{
																				send(conFd, shareMem+client*BUF_SIZE, BUF_SIZE, 0);
																}
												}
												else
												{
																continue;
												}
								}
				}
				close(conFd);
				close(pipeFd);
				close(childEpollFd);
				return 0;
}

int main(int argc, char* argv[])
{
				if(argc<3)
				{
								printf("ip port\n");
								return 0;
				}
				const char* ip=argv[1];
				int port=atoi(argv[2]);

				sockaddr_in serverAddr;
				bzero(&serverAddr, sizeof(serverAddr));
				serverAddr.sin_family=PF_INET;
				serverAddr.sin_port=htons(port);
				inet_pton(PF_INET, ip, &serverAddr.sin_addr);
				
				listenFd=socket(PF_INET, SOCK_STREAM, 0);
				assert(listenFd>=0);

				int res=bind(listenFd, (sockaddr*)&serverAddr, sizeof(serverAddr));
				assert(res!=-1);

				res=listen(listenFd, 5);
				assert(res!=-1);

				userCnt=0;
				users=new ClientData[USER_LIMIT+1];
				subProcess=new int[PROCESS_LIMIT+1];
				for(int i=0; i<=PROCESS_LIMIT; ++i)
								subProcess[i]=-1;
				epoll_event events[MAX_EVENT_NUM];
				epollFd=epoll_create(5);
				assert(epollFd!=-1);
				addFd(epollFd, listenFd);

				res=socketpair(PF_UNIX, SOCK_STREAM, 0, sigPipeFd);
				assert(res!=-1);
				setNonblock(sigPipeFd[1]);
				addFd(epollFd, sigPipeFd[0]);

				addSig(SIGCHLD, sigHandler);
				addSig(SIGTERM, sigHandler);
				addSig(SIGINT, sigHandler);
				addSig(SIGPIPE, SIG_IGN);
				
				bool stopServer=false;
				bool terminate=false;

				//创建共享内存,作为客户socket连接的读缓存
				shmFd=shm_open(shmName, O_CREAT|O_RDWR, 0666);
				assert(shmFd!=-1);
				res=ftruncate(shmFd, USER_LIMIT*BUF_SIZE);
				assert(res!=-1);

				shareMem=(char*)mmap(NULL, USER_LIMIT*BUF_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, shmFd, 0);
				assert(shareMem!=MAP_FAILED);
				close(shmFd);

				while(!stopServer)
				{
								int cnt=epoll_wait(epollFd, events, MAX_EVENT_NUM, -1);
								if((cnt<0) && (errno!=EINTR))
								{
												printf("epoll fails\n");
												break;
								}
								for(int i=0; i<cnt; ++i)
								{
												int sockFd=events[i].data.fd;
												if(sockFd==listenFd)
												{
																sockaddr_in clientAddr;
																bzero(&clientAddr, sizeof(clientAddr));
																socklen_t clientAddrLen=sizeof(clientAddr);
																int conFd=accept(listenFd, (sockaddr*)&clientAddr, &clientAddrLen);

																if(conFd<0)
																{
																				printf("error is %d\n", errno);
																				continue;
																}
																if(userCnt>=USER_LIMIT)
																{
																				const char* info="too many users\n";
																				printf("%s", info);
																				send(conFd, info, strlen(info), 0);
																				continue;
																}
																users[userCnt].address=clientAddr;
																users[userCnt].conFd=conFd;
																res=socketpair(PF_UNIX, SOCK_STREAM, 0, users[userCnt].pipeFd);
																assert(res!=-1);
																pid_t pid=fork();
																if(pid<0)
																{
																				close(conFd);
																				continue;
																}
																else if(pid==0)
																{
																				close(epollFd);
																				close(listenFd);
																				close(users[userCnt].pipeFd[0]);
																				close(sigPipeFd[0]);
																				close(sigPipeFd[1]);
																				runChild(userCnt, users, shareMem);
																				munmap((void*)shareMem, USER_LIMIT*BUF_SIZE);
																				exit(0);
																}
																else
																{
																				close(conFd);
																				close(users[userCnt].pipeFd[1]);
																				addFd(epollFd, users[userCnt].pipeFd[0]);
																				users[userCnt].pid=pid;
																				
																				subProcess[pid]=userCnt;
																				++userCnt;
																}

												}
												else if((sockFd==sigPipeFd[0]) && (events[i].events&EPOLLIN))
												{
																int sig;
																char signals[1024];
																res=recv(sigPipeFd[0], signals, sizeof(signals), 0);
																if(res==-1)
																{
																				continue;
																}
																else if(res==0)
																{
																				continue;
																}
																else
																{
																				for(int i=0; i<res; ++i)
																				{
																								switch(signals[i])
																								{
																												//子进程退出,表示某个客户端关闭了连接
																												case SIGCHLD:
																																pid_t pid;
																																int stat;
																																while((pid=waitpid(-1, &stat, WNOHANG))>0)
																																{
																																				int delUser=subProcess[pid];
																																				subProcess[pid]=-1;
																																				if(delUser<0 || delUser>USER_LIMIT)
																																								continue;
																																				epoll_ctl(epollFd, EPOLL_CTL_DEL, users[delUser].pipeFd[0], 0);
																																				close(users[delUser].pipeFd[0]);
																																				--userCnt;
																																				users[delUser]=users[userCnt];
																																				subProcess[users[delUser].pid]=delUser;
																																}
																																if(terminate && userCnt==0)
																																				stopServer=true;
																																break;
																												case SIGTERM:
																												case SIGINT:
																																printf("kill all the child now\n");
																																if(userCnt==0)
																																{
																																				stopServer=true;
																																				break;
																																}
																																for(int i=0; i<userCnt; ++i)
																																{
																																				int pid=users[i].pid;
																																				kill(pid, SIGTERM);
																																}
																																terminate=true;
																																break;
																												default:
																																break;
																								}
																				}
																}
												}
												else if(events[i].events & EPOLLIN)
												{
																int child=0;
																res=recv(sockFd, (char*) &child, sizeof(child), 0);
																printf("read data from child accross pipe\n");
																if(res==-1)
																{
																				continue;
																}
																else if(res==0)
																{
																				continue;
																}
																else
																{
																				for(int j=0; j<userCnt; ++j)
																				{
																								if(users[j].pipeFd[0]!=sockFd)
																								{
																												printf("send data to child accross pipe\n");
																												send(users[j].pipeFd[0], (char*)&child, sizeof(child), 0);
																								}
																				}
																}
												}
								}
				}
				delResource();
				return 0;
}

#ifndef PROCESSPOOL_H
#define PROCESSPOOL_H

#include <sys/types.h>
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
#include <sys/wait.h>
#include <sys/stat.h>

static int sigPipeFd[2];//信号管道

static int setNonblock(int fd)
{
				int oldOption=fcntl(fd, F_GETFL);
				int newOption=oldOption|O_NONBLOCK;
				fcntl(fd, F_SETFL, newOption);
				return oldOption;
}

static void addFd(int epollFd, int fd)
{
				epoll_event event;
				event.data.fd=fd;
				event.events=EPOLLIN | EPOLLET;
				epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);
				setNonblock(fd);
}

static void removeFd(int epollFd, int fd)
{
				epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, 0);
				close(fd);
}

static void sigHandler(int sig)
{
				int saveErrno=errno;
				int msg=sig;
				send(sigPipeFd[1], (char*)&msg, 1, 0);
				errno=saveErrno;
}

static void addSig(int sig, void(handler)(int), bool restart=true)
{
				struct sigaction sa;
				bzero(&sa, sizeof(sa));
				sa.sa_handler=handler;
				if(restart)
								sa.sa_flags|=SA_RESTART;
				sigfillset(&sa.sa_mask);
				assert(sigaction(sig, &sa, NULL)!=-1);
}

//进程类
struct Process
{
				Process():mPid(-1){}

				pid_t mPid;
				int mPipeFd[2];
};

//进程池类.模板参数为处理逻辑任务的类
template <typename T>
class ProcessPool
{
				private:
								ProcessPool(int listenFd, int processNum=8);
				public:
								//单例模式
								static ProcessPool<T>* create(int listenFd, int processNum=8)
								{
												if(mInstance==nullptr)
												{
																mInstance=new ProcessPool<T>(listenFd, processNum);
												}
												return mInstance;
								}
								virtual ~ProcessPool()
								{
												delete []mSubProcess;
								}

								//启动进程池
								void run();
				private:
								void setSigPipe();
								void runParent();
								void runChild();
				private:
								static const int MAX_PROCESS_NUM=16;//进程池中的最大子进程数量
								static const int USER_PER_PROCESS=65535;//每个进程能够处理的客户数量
								static const int MAX_EVENT_NUM=10000;//epoll最多能处理的事件数
								int mProcessNum;//进程池中的进程总数
								int mIdx;//子进程在池中的序号,从0开始
								int mEpollFd;//指向进程内核事件表
								int mListenFd;//监听socket
								int mStop;//子进程通过mStop判断是否停止
								Process* mSubProcess;//进程数组
								static ProcessPool<T>* mInstance;//进程池实例
};

template <typename T>
ProcessPool<T>* ProcessPool<T>::mInstance=nullptr;


template <typename T>
ProcessPool<T>::ProcessPool(int listenFd, int processNum):mListenFd(listenFd), mProcessNum(processNum), mIdx(-1), mStop(false)
{
				assert((processNum>0 && processNum<=MAX_PROCESS_NUM));

				mSubProcess=new Process[processNum];
				assert(mSubProcess!=nullptr);

				for(int i=0; i<processNum; ++i)
				{
								int res=socketpair(PF_UNIX, SOCK_STREAM, 0, mSubProcess[i].mPipeFd);
								assert(res==0);

								mSubProcess[i].mPid=fork();
								assert(mSubProcess[i].mPid>=0);

								if(mSubProcess[i].mPid>0)
								{
												close(mSubProcess[i].mPipeFd[1]);
												continue;
								}
								else
								{
												close(mSubProcess[i].mPipeFd[0]);
												mIdx=i;
												break;
								}
				}
}

template <typename T>
void ProcessPool<T>::setSigPipe()
{
				mEpollFd=epoll_create(5);
				assert(mEpollFd!=-1);

				int res=socketpair(PF_UNIX, SOCK_STREAM, 0, sigPipeFd);
				assert(res==0);

				setNonblock(sigPipeFd[1]);
				addFd(mEpollFd, sigPipeFd[0]);

				addSig(SIGCHLD, sigHandler);
				addSig(SIGTERM, sigHandler);
				addSig(SIGINT, sigHandler);
				addSig(SIGPIPE, SIG_IGN);
}

template <typename T>
void ProcessPool<T>::run()
{
				if(mIdx==-1)//父进程中mIdx为-1,子进程中mIdx不等于-1
								runParent();
				else
								runChild();
}

template <typename T>
void ProcessPool<T>::runChild()
{
				setSigPipe();

				int pipeFd=mSubProcess[mIdx].mPipeFd[1];
				addFd(mEpollFd, pipeFd);

				T* users=new T[USER_PER_PROCESS];
				assert(users!=nullptr);

				epoll_event events[MAX_EVENT_NUM];

				int cnt=0;//mEpollFd上的事件数
				int res=-1;

				while(!mStop)
				{
								cnt=epoll_wait(mEpollFd, events, MAX_EVENT_NUM, -1);
								if(cnt<0 && errno!=EINTR)
								{
												printf("epoll fails\n");
												break;
								}
								for(int i=0; i<cnt; ++i)
								{
												int sockFd=events[i].data.fd;
												if(sockFd==pipeFd && (events[i].events & EPOLLIN))
												{
																int client=0;
																res=recv(sockFd, (char*)&client, sizeof(client), 0);
																if((res<0 && (errno!=EAGAIN)) || res==0)
																				continue;
																sockaddr_in clientAddr;
																bzero(&clientAddr, sizeof(clientAddr));
																socklen_t clientAddrLen=sizeof(clientAddr);
																int conFd=accept(mListenFd, (sockaddr*)&clientAddr, &clientAddrLen);
																if(conFd<0)
																{
																				printf("errno is %d\n", errno);
																				continue;
																}
																addFd(mEpollFd, conFd);
																
																//模板类需要实现init方法,以初始化一个客户连接
																users[conFd].init(mEpollFd, conFd, clientAddr);
												}
												else if(sockFd==sigPipeFd[0] && (events[i].events & EPOLLIN))
												{
																int sig;
																char signals[1024];
																memset(signals, '\0', 1024);
																res=recv(sockFd, signals, 1023, 0);
																if(res<=0)
																				continue;
																for(int i=0; i<res; ++i)
																{
																				switch(signals[i])
																				{
																								case SIGCHLD:
																												pid_t pid;
																												int stat;
																												while((pid=waitpid(-1, &stat, WNOHANG))>0)
																																continue;
																												break;
																								case SIGTERM:
																								case SIGINT:
																												mStop=true;
																												break;
																								default:
																												break;
																				}
																}
												}
												else if(events[i].events & EPOLLIN)
												{
																users[sockFd].process();
												}
												else
												{
																continue;
												}
								}
				}
				
				delete []users;
				users=nullptr;
				close(pipeFd);
				close(mEpollFd);
}

template <typename T>
void ProcessPool<T>::runParent()
{
				setSigPipe();

				addFd(mEpollFd, mListenFd);

				epoll_event events[MAX_EVENT_NUM];
				
				int subProcessCnt=0;
				int newConn=1;
				int cnt=0;
				int res=-1;

				while(!mStop)
				{
								cnt=epoll_wait(mEpollFd, events, MAX_EVENT_NUM, -1);
								if(cnt<0 && errno!=EINTR)
								{
												printf("epoll fails\n");
												break;
								}
								for(int i=0; i<cnt; ++i)
								{
												int sockFd=events[i].data.fd;
												if(sockFd==mListenFd)//Round Robin方式分配子进程
												{
																int i=subProcessCnt;
																do
																{
																				if(mSubProcess[i].mPid!=-1)
																								break;
																				i=(i+1)%mProcessNum;
																}while(i!=subProcessCnt);
																if(mSubProcess[i].mPid==-1)
																{
																				mStop=true;
																				break;
																}
																subProcessCnt=(i+1)%mProcessNum;
																send(mSubProcess[i].mPipeFd[0], (char*)&newConn, sizeof(newConn), 0);
																printf("send request to child %d\n", i);
												}
												else if(sockFd==sigPipeFd[0] && (events[i].events & EPOLLIN))
												{
																int sig;
																char signals[1024];
																memset(signals, '\0', 1024);
																res=recv(sockFd, signals, 1023, 0);
																if(res<0)
																				continue;
																for(int i=0; i<res; ++i)
																{
																				switch(signals[i])
																				{
																								case SIGCHLD:
																												pid_t pid;
																												int stat;
																												while((pid=waitpid(-1, &stat, WNOHANG))>0)
																												{
																																for(int i=0; i<mProcessNum; ++i)
																																{
																																				if(mSubProcess[i].mPid==pid)
																																				{
																																								printf("child %d leaves\n", pid);
																																								close(mSubProcess[i].mPipeFd[0]);
																																								mSubProcess[i].mPid=-1;
																																								break;
																																				}
																																}
																												}
																												mStop=true;
																												for(int i=0; i<mProcessNum; ++i)
																												{
																																if(mSubProcess[i].mPid!=-1)
																																{
																																				mStop=false;
																																				break;
																																}
																												}
																												break;
																								case SIGTERM:
																								case SIGINT:
																												printf("kill all the child\n");
																												for(int i=0; i<mProcessNum; ++i)
																												{
																																int pid=mSubProcess[i].mPid;
																																if(pid!=-1)
																																				kill(pid, SIGTERM);
																												}
																												break;
																								default:
																												break;
																				}
																}
												}
												else
												{
																continue;
												}
								}
				}
				close(mEpollFd);
}

#endif

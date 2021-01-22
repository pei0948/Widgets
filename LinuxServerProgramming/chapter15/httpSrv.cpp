#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <cassert>
#include <sys/epoll.h>

#include "../chapter14/Locker.hpp"
#include "ThreadPool.hpp"
#include "HttpConn.hpp"

#define MAX_FD 65536
#define MAX_EVENT_NUM 10000

extern void addFd(int epollFd, int fd, bool oneShot);
extern void removeFd(int epollFd, int fd);

void addSig(int sig, void (handler)(int), bool restart=true)
{
				struct sigaction sa;
				bzero(&sa, sizeof(sa));
				sa.sa_handler=handler;
				if(restart)
								sa.sa_flags|=SA_RESTART;
				sigfillset(&sa.sa_mask);
				assert(sigaction(sig, &sa, NULL)!=-1);
}

void showError(int conFd, const char* info)
{
				printf("%s", info);
				send(conFd, info, strlen(info), 0);
				close(conFd);
}

int main(int argc, char* argv[])
{
				if(argc<=2)
				{
								printf("ip port\n");
								return 1;
				}
				const char* ip=argv[1];
				int port=atoi(argv[2]);

				addSig(SIGPIPE, SIG_IGN);//忽略SIGPIPE信号

				//创建线程池
				ThreadPool<HttpConn> *pool=nullptr;
				try
				{
								pool=new ThreadPool<HttpConn>();
				}
				catch(...)
				{
								return 1;
				}
				HttpConn* users=new HttpConn[MAX_FD];
				assert(users!=nullptr);

				sockaddr_in serverAddr;
				bzero(&serverAddr, sizeof(serverAddr));
				serverAddr.sin_family=PF_INET;
				serverAddr.sin_port=htons(port);
				inet_pton(PF_INET, ip, &serverAddr.sin_addr);

				int listenSock=socket(PF_INET, SOCK_STREAM, 0);
				assert(listenSock>=0);

				int res=bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
				assert(res!=-1);

				res=listen(listenSock, 5);
				assert(res!=-1);

				int epollFd=epoll_create(5);
				assert(epollFd!=-1);
				addFd(epollFd, listenSock, false);

				epoll_event events[MAX_EVENT_NUM];

				while(true)
				{
								int cnt=epoll_wait(epollFd, events, MAX_EVENT_NUM, -1);
								if(cnt<0 && errno!=EINTR)
								{
												printf("epoll fails\n");
												break;
								}
								for(int i=0; i<cnt; ++i)
								{
												int sockFd=events[i].data.fd;
												if(sockFd==listenSock)
												{
																sockaddr_in clientAddr;
																bzero(&clientAddr, sizeof(clientAddr));
																socklen_t clientAddrLen=sizeof(clientAddr);
																int conFd=accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
																if(conFd<0)
																{
																				printf("errno is %d\n", conFd);
																				continue;
																}
																if(HttpConn::mUserCnt>=MAX_FD)
																{
																				showError(conFd, "Internal server busy");
																				continue;
																}
																users[conFd].init(conFd, clientAddr);
												}
												else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
												{
																users[sockFd].closeConn();
												}
												else if(events[i].events & EPOLLIN)
												{
																if(users[sockFd].read())
																{
																				pool->append(users+sockFd);
																}
																else
																{
																				users[sockFd].closeConn();
																}
												}
												else if(events[i].events & EPOLLOUT)
												{
																printf("ready to write\n");
																if(!users[sockFd].write())
																				users[sockFd].closeConn();
												}
												else
												{}
								}
				}

				close(epollFd);
				close(listenSock);
				delete []users;
				delete pool;
				return 0;
}

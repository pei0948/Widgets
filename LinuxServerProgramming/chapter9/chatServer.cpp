#define _GNU_SOURCE 1

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

#define USER_LIMIT 5 //最大用户数量
#define BUF_SIZE 64 //读缓冲区大小
#define FD_LIMIT 65535 //文件描述数量限制

struct clientData
{
				sockaddr_in address;
				char *writeBuf;
				char buf[BUF_SIZE];
};
int setnonblock(int fd)
{
				int oldOption=fcntl(fd, F_GETFL);
				int newOption=(oldOption | O_NONBLOCK);
				fcntl(fd, F_SETFL,newOption);
				return oldOption;
}

int main(int argc, char *argv[])
{
				if(argc<3)
				{
								printf("ip port\n");
								return 1;
				}
				const char *ip=argv[1];
				int port=atoi(argv[2]);

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

				clientData *users=new clientData[FD_LIMIT];

				pollfd fds[USER_LIMIT+1];
				
				int userCnt=0;
				for(int i=1; i<=USER_LIMIT; ++i)
				{
								fds[i].fd=-1;
								fds[i].events=0;
				}
				fds[0].fd=listenSock;
				fds[0].events=POLLIN | POLLERR;
				fds[0].revents=0;

				while(1)
				{
								res=poll(fds, userCnt+1, -1);
								if(res<0)
								{
												printf("poll fails\n");
												break;
								}
								for(int i=0; i<=userCnt; ++i)
								{
												if(fds[i].fd==listenSock && (fds[i].revents & POLLIN))
												{
																sockaddr_in clientAddr;
																bzero(&clientAddr, sizeof(clientAddr));
																socklen_t clientAddrLen=sizeof(clientAddr);
																int confd=accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
																if(confd<0)
																{
																				printf("error is %d\n", errno);
																				continue;
																}
																if(userCnt>=USER_LIMIT)
																{
																				const char *info="too many users\n";
																				printf("%s\n", info);
																				send(confd, info, strlen(info), 0);
																				continue;
																}

																++userCnt;
																users[confd].address=clientAddr;
																setnonblock(confd);
																fds[userCnt].fd=confd;
																fds[userCnt].events=POLLIN | POLLRDHUP | POLLERR;
																fds[userCnt].revents=0;
																printf("comes a new user, now have %d \n", userCnt);
												}
												else if(fds[i].revents & POLLERR)
												{
																printf("get an error from %d\n", fds[i].fd);
																char errors[100];
																memset(errors, '\0', 100);
																socklen_t len=sizeof(errors);
																if(getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &len)<0)
																{
																				printf("get socket option failed\n");
																}
																continue;
												}
												else if(fds[i].revents & POLLRDHUP)//客户端关闭连接
												{
																users[fds[i].fd]=users[fds[userCnt].fd];
																close(fds[i].fd);
																fds[i]=fds[userCnt];
																--i;
																--userCnt;
																printf("a client leave\n");
												}
												else if(fds[i].revents & POLLIN)
												{
																int confd=fds[i].fd;
																memset(users[confd].buf, '\0', BUF_SIZE);
																res=recv(confd, users[confd].buf, BUF_SIZE-1, 0);
																printf("get %d bytes: %s from %d\n", res, users[confd].buf, confd);
																if(res<0)
																{
																				if(errno!=EAGAIN)
																				{
																								close(confd);
																								users[fds[i].fd]=users[fds[userCnt].fd];
																								fds[i]=fds[userCnt];
																								--i;
																								--userCnt;
																				}
																}
																else if(res==0)
																{
																}
																else
																{
																				for(int j=1; j<=userCnt; ++j)
																				{
																								if(fds[j].fd==confd)
																								{
																												continue;
																								}
																								fds[j].events|=(~POLLIN);
																								fds[j].events|=(POLLOUT);
																								users[fds[j].fd].writeBuf=users[confd].buf;
																				}
																}
												}
												else if(fds[i].revents & POLLOUT)
												{
																int confd=fds[i].fd;
																if(!users[confd].writeBuf)
																{
																				continue;
																}
																res=send(confd, users[confd].writeBuf, strlen(users[confd].writeBuf), 0);
																users[confd].writeBuf=NULL;
																fds[i].events |= (~POLLOUT);
																fds[i].revents |= (POLLIN);
												}
								}
				}
				delete []users;
				close(listenSock);
				return 0;
}

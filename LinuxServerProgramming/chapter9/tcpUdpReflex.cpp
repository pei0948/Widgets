#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <pthread.h>

#define MAX_EVENT_NUMBER 1024
#define TCP_BUF_SIZE 512
#define UDP_BUF_SIZE 1024

int setnonblock(int fd)
{
				int oldOption=fcntl(fd, F_GETFL);
				int newOption=oldOption | O_NONBLOCK;
				fcntl(fd, F_SETFL, newOption);
				return oldOption;
}
void addfd(int epollfd, int fd)
{
				epoll_event event;
				event.data.fd=fd;
				event.events=EPOLLIN | EPOLLET;
				epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
				setnonblock(fd);
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
				int udpSock=socket(PF_INET, SOCK_DGRAM, 0);
				assert(udpSock>=0);

				int res=bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
				assert(res!=-1);
				res=bind(udpSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
				assert(res!=-1);

				res=listen(listenSock, 5);
				assert(res!=-1);

				epoll_event events[MAX_EVENT_NUMBER];
				int epollfd=epoll_create(5);
				assert(epollfd!=-1);

				addfd(epollfd, listenSock);
				addfd(epollfd, udpSock);

				while(1)
				{
								int cnt=epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
								if(cnt<0)
								{
												printf("epoll fails\n");
												break;
								}
								for(int i=0; i<cnt; ++i)
								{
												int sockfd=events[i].data.fd;
												if(sockfd==listenSock)
												{
																sockaddr_in clientAddr;
																bzero(&clientAddr, sizeof(clientAddr));
																socklen_t clientAddrLen=sizeof(clientAddr);
																int confd=accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
																assert(confd>=0);
																addfd(epollfd, confd);
												}
												else if(sockfd==udpSock)
												{
																char buf[UDP_BUF_SIZE];
																memset(buf, '\0', UDP_BUF_SIZE-1);

																sockaddr_in clientAddr;
																bzero(&clientAddr, sizeof(clientAddr));
																socklen_t clientAddrLen=sizeof(clientAddr);
																res=recvfrom(udpSock, buf, UDP_BUF_SIZE-1, 0, (sockaddr*)&clientAddr, &clientAddrLen);

																if(res>0)
																{
																				sendto(udpSock, buf, UDP_BUF_SIZE-1, 0, (sockaddr*)&clientAddr, clientAddrLen);
																}
												}
												else if(events[i].events & EPOLLIN)
												{
																char buf[TCP_BUF_SIZE];
																while(1)
																{
																				memset(buf, '\0', TCP_BUF_SIZE-1);
																				res=recv(sockfd, buf, TCP_BUF_SIZE-1, 0);
																				if(res<0)
																				{
																								if(errno == EAGAIN || errno == EWOULDBLOCK)
																								{
																												break;
																								}
																								close(sockfd);
																								break;
																				}
																				else if(res==0)
																				{
																								close(sockfd);
																				}
																				else
																				{
																								send(sockfd, buf, TCP_BUF_SIZE-1, 0);
																				}
																}
												}
												else
												{
																printf("something else happend\n");
												}
								}
				}
				close(listenSock);
				return 0;
}

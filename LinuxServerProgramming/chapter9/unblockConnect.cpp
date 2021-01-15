#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

#define BUF_SIZE 1023

int setnonblock(int fd)
{
				int oldOption=fcntl(fd, F_GETFL);
				int newOption=oldOption | O_NONBLOCK;
				fcntl(fd, F_SETFL, newOption);
				return oldOption;
}

int unblockConnect(const char *ip, int port, int time)
{
				sockaddr_in serverAddr;
				serverAddr.sin_family=PF_INET;
				serverAddr.sin_port=htons(port);
				inet_pton(PF_INET, ip, &serverAddr.sin_addr);

				int sockfd=socket(PF_INET, SOCK_STREAM, 0);
				assert(sockfd>=0);
				int fdopt=setnonblock(sockfd);

				int res=connect(sockfd, (sockaddr*)&serverAddr, sizeof(serverAddr));
				if(res==0)
				{
								printf("connection success\n");
								fcntl(sockfd, F_SETFL, fdopt);
								return sockfd;
				}
				if(errno!=EINPROGRESS)//如果没有立即建立连接,只有errno是EINPROGRESS时,连接还在进行,否则出错返回
				{
								printf("unblock connect not support\n");
								return -1;
				}

				fd_set readfds;
				fd_set writefds;
				timeval timeout;

				FD_ZERO(&readfds);
				FD_SET(sockfd, &writefds);

				timeout.tv_sec=time;
				timeout.tv_sec=0;

				res=select(sockfd+1, NULL, &writefds, NULL, &timeout);
				if(res<=0)
				{
								printf("connection timeout\n");
								close(sockfd);
								return -1;
				}

				if(!FD_ISSET(sockfd, &writefds))
				{
								printf("no events on sockfd found\n");
								close(sockfd);
								return -1;
				}
				int error=0;
				socklen_t errorLen=sizeof(error);
				if(getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &errorLen)<0)//获取并清除sockfd上的错误
				{
								printf("get socket option failed\n");
								close(sockfd);
								return -1;
				}
				if(error!=0)//表示出错
				{
								printf("connection failed after select with the error %d\n", error);
								close(sockfd);
								return -1;
				}

				printf("connection ready after select with the socket %d\n", sockfd);
				fcntl(sockfd, F_SETFL, fdopt);
				return sockfd;
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

				int sockfd=unblockConnect(ip, port, 10);
				if(sockfd<0)
								return 1;
				char buf[1024];
				memset(buf, 'a', 1024);
				buf[1023]='\0';
				int res=send(sockfd, buf, 1023, 0);
				printf("send %d bytes: %s\n", res, buf);
				close(sockfd);
				return 0;
}

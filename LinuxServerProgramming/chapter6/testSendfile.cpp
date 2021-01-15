#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

int main(int argc, char *argv[])
{
				if(argc<4)
				{
								printf("ip port file\n");
								return 0;
				}
				const char *ip=argv[1];
				int port=atoi(argv[2]);
				const char *fileName=argv[3];

				int fd=open(fileName, O_RDONLY);
				assert(fd>0);
				struct stat fileStat;
				fstat(fd, &fileStat);

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

				sockaddr_in clientAddr;
				bzero(&clientAddr, sizeof(clientAddr));
				socklen_t clientAddrLen=sizeof(clientAddr);

				int confd=accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
				if(confd<0)
				{
								printf("error is %d", errno);
				}
				else
				{
								sendfile(confd, fd, NULL, fileStat.st_size);
								close(fd);
				}
				close(listenSock);
				return 0;
}

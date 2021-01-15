#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>

#define BUF_SIZE 1024

static const char* statusLine[2]={"200 OK", "500 Internal server error"};

int main(int argc, char *argv[])
{
				if(argc<=3)
				{
								printf("ip port filename\n");
								return 1;
				}
				const char *ip=argv[1];
				int port=atoi(argv[2]);
				const char *fileName=argv[3];

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
								close(listenSock);
								return 1;
				}
				char header[BUF_SIZE];
				memset(header, '\0', BUF_SIZE);
				char *buf;
				struct stat fileStat;
				bool valid=true;
				int len=0;
				if(stat(fileName, &fileStat)<0)
				{
								close(confd);
								close(listenSock);
								return 1;
				}
				if(S_ISDIR(fileStat.st_mode))
				{
								valid=false;
				}
				else if(fileStat.st_mode & S_IROTH)
				{
								int fd=open(fileName, O_RDONLY);
								buf=new char[fileStat.st_size+1];
								memset(buf, '\0', fileStat.st_size+1);
								if(read(fd, buf, fileStat.st_size)<0)
								{
												valid=false;
								}
				}
				else
				{
								valid=false;
				}
				if(valid)
				{
								res=snprintf(header, BUF_SIZE-1, "%s %s\r\n", "HTTP/1.1", statusLine[0]);
								len+=res;
								res=snprintf(header+len, BUF_SIZE-len-1, "Content-Length: %d\r\n", (int)fileStat.st_size);
								len+=res;
								res=snprintf(header+len, BUF_SIZE-len-1, "%s", "\r\n");

								iovec iv[2];
								iv[0].iov_base=header;
								iv[0].iov_len=strlen(header);
								iv[1].iov_base=buf;
								iv[1].iov_len=fileStat.st_size;
								res=writev(confd, iv, 2);
				}
				else
				{
								res=snprintf(header, BUF_SIZE-1, "%s %s\r\n", "HTTP/1.1", statusLine[1]);
								len+=res;
								res=snprintf(header+len, BUF_SIZE-len-1, "%s", "\r\n");
								send(confd, header, strlen(header), 0);
				}
				close(confd);
				delete []buf;
				close(listenSock);
				return 0;
}

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
				if(argc<2)
				{
								printf("filename\n");
								return 1;
				}
				const char *fileName=argv[1];

				int fd=open(fileName, O_CREAT|O_WRONLY|O_TRUNC, 0666);
				int pipStd[2];
				int res=pipe(pipStd);
				assert(res!=-1);

				int pipFile[2];
				res=pipe(pipFile);
				assert(res!=-1);

				res=splice(STDIN_FILENO, NULL, pipStd[1], NULL, 32768, SPLICE_F_MORE|SPLICE_F_MOVE);
				assert(res!=-1);

				res=tee(pipStd[0], pipFile[1], 32768, SPLICE_F_NONBLOCK);
				assert(res!=-1);

				res=splice(pipFile[0], NULL, STDOUT_FILENO, NULL, 32768, SPLICE_F_MORE|SPLICE_F_MOVE);
				assert(res!=-1);

				res=splice(pipStd[0], NULL, fd, NULL, 32768, SPLICE_F_MORE|SPLICE_F_MOVE);
				assert(res!=-1);

				close(fd);
				close(pipStd[0]);
				close(pipStd[1]);
				close(pipFile[0]);
				close(pipFile[1]);
				return 0;
}

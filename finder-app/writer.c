#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
	const char* writefile;
	const char* writestr;
	int fd;
	ssize_t len;
	ssize_t written;

	openlog("writer", LOG_PID, LOG_USER);
	if(argc !=3)
	{
		syslog(LOG_ERR, "Invalid number of args:%d", argc-1);
		closelog();
		return 1;
	}
	writefile= argv[1];
	writestr= argv[2];
	len= (ssize_t)strlen(writestr);

	fd= open(writefile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if(fd == -1)
	{
		syslog(LOG_ERR, "Couldn't open file: %s", writefile);
		closelog();
		return 1;
	}

	written= write( fd, writestr, (size_t)len);
	if(written != len)
	{
		syslog(LOG_ERR, "Write Failed:%s", writefile);
		closelog();
		return 1;
	}

	if(close(fd) != 0)
	{
		syslog(LOG_ERR , "Unable to close file:%s", writefile);
		closelog();
		return 1;
	}
	syslog(LOG_USER,"Write Successful");
	closelog();
	return 0;
}


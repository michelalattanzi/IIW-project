#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


int main (int argc, char *argv[])
{
char buff[1024];
if( NULL!=getcwd(buff,sizeof(buff)) )
	puts(buff);
else
	perror("getcwd() error");
return 0;
}



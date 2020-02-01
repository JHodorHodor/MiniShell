#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <limits.h>
#include <dirent.h>

#include "builtins.h"

int echo(char*[]);
int lexit(char*[]);
int lcd(char*[]);
int lkill(char*[]);
int lls(char*[]);
int undefined(char *[]);

void error_des(char*);

builtin_pair builtins_table[]={
	{"exit",	&lexit},
	{"lecho",	&echo},
	{"lcd",		&lcd},
	{"lkill",	&lkill},
	{"lls",		&lls},
	{NULL,NULL}
};

int 
echo( char * argv[])
{
	int i = 1;
	if (argv[i]) printf("%s", argv[i++]);
	while  (argv[i])
		printf(" %s", argv[i++]);

	printf("\n");
	fflush(stdout);
	return 0;
}

int
lexit(char * argv[])
{
	int ret;
	if(!argv[1]){
		exit(0);
	} else if(!argv[2]){
		exit(atoi(argv[1]));
	} else {
		error_des(argv[0]);
		return 1;
	}
	return 0;
}

int
lcd(char * argv[])
{
	int ret;
	if(!argv[1]){
		ret = chdir(getenv("HOME"));
	} else if(!argv[2]){
		ret = chdir(argv[1]);
	} else {
		ret = 1;
	}
	if(ret)
		error_des(argv[0]);
	return ret;
}

int
lkill(char * argv[])
{
	int ret, pid;
	int signal = SIGTERM;
	char * pEnd;
	char * sEnd;

	if(!argv[1]){
		error_des(argv[0]);
		return -1;
	} else if(argv[2]){
		signal = strtol(argv[1],&sEnd,10) * (-1);
		pid = strtol(argv[2],&pEnd,10);	

		if(signal == 0 || *sEnd != 0 || *pEnd != 0){
			error_des(argv[0]);
			return -1;
		}
	} else if(!argv[2]){
		pid = strtol(argv[1],&pEnd,10);

		if(*pEnd != 0){
			error_des(argv[0]);
			return -1;
		}
	} else {
		error_des(argv[0]);
		return -1;
	}

	ret = kill(pid,signal);

	if(ret)
		error_des(argv[0]);
	return ret;
}

int
lls(char * argv[])
{
	int ret;
	if(argv[1]){
		ret = 1;
	} else {
		char cwd[PATH_MAX];
		getcwd(cwd,PATH_MAX);

		DIR *dir;
    	struct dirent *file;
		dir = opendir(cwd);
		while((file = readdir(dir)) != NULL){
			if(file->d_name[0] != '.'){
				write(1,file->d_name,strlen(file->d_name));
				write(1,"\n",1);
			}
		}
		ret = closedir(dir);

	}
	if(ret)
		error_des(argv[0]);
	return ret;
}


int 
undefined(char * argv[])
{
	fprintf(stderr, "Command %s undefined.\n", argv[0]);
	return BUILTIN_ERROR;
}

void 
error_des(char* name)
{
	write(2,"Builtin ",strlen("Builtin "));
	write(2,name,strlen(name));
	write(2," error.\n",strlen(" error.\n"));
}

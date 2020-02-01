#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "builtins.h"

#define MAX_BG_NOTES_CAPACITY 2000

volatile int children_counter = 0; //only fg children
pid_t fg_children[MAX_LINE_LENGTH / 2]; //list of fg processes, that we need to wait for (ends with NULL)

bool bg_exit_or_kill[MAX_BG_NOTES_CAPACITY]; //exit = true, killed = false
int bg_exit_status[MAX_BG_NOTES_CAPACITY];
pid_t bg_pids[MAX_BG_NOTES_CAPACITY];
int bg_index = 0;

bool is_fg_process(pid_t pid){
	int i = 0;
	while(fg_children[i] != -1){
		if(fg_children[i++] == pid)
			return true;
	}
	return false;
}

void sigchild_handler(int sig_number){
	while(1){ //multiple children can be ready
		int exit_status;
		//don't block on waitpid
		int pid = waitpid(-1, &exit_status, WNOHANG);

		if(pid == -1 || pid == 0) 
			return;
		if(is_fg_process(pid)){
			children_counter--;
		} else { //bg process - make note
			if (WIFEXITED(exit_status)) {
				bg_exit_or_kill[bg_index] = true;
				bg_exit_status[bg_index] = WEXITSTATUS(exit_status);
				bg_pids[bg_index++] = pid;
            } else if (WIFSIGNALED(exit_status)) {
				bg_exit_or_kill[bg_index] = false;
				bg_exit_status[bg_index] = WIFSIGNALED(exit_status);
				bg_pids[bg_index++] = pid;
            }
		}
	}
}

void syntax_error(){
	write(2, SYNTAX_ERROR_STR, strlen(SYNTAX_ERROR_STR));
	write(2, "\n", 1);
}

void insert_args(command * com, char ** _argv){
	int i = 0;
	if(com -> args == NULL){
		_argv[0] = NULL;
		return;
	}

	argseq * begin = com -> args;
	argseq * tmp = begin;
	do {
		_argv[i++] = tmp -> arg;
		tmp = tmp -> next;
	} while(tmp != begin);
	_argv[i] = NULL;
}

void insert_redirs(command * com, redir ** _argv){
	int i = 0;
	if(com -> redirs == NULL){
		_argv[0] = NULL;
		return;
	}

	redirseq * begin = com -> redirs;
	redirseq * tmp = begin;
	do {
		_argv[i++] = tmp -> r;
		tmp = tmp -> next;
	} while(tmp != begin);
	_argv[i] = NULL;
}

int is_correct_pipeline(commandseq* commands){ //0 - OK, 1 - empty command in simple pipeline 2 - NOT OK
	if(commands == NULL) exit(1);
	commandseq * c_it = commands;
	if(c_it -> next == c_it && c_it -> com == NULL) return 1;
	do {
		if(c_it -> com == NULL) return 2;
		c_it = c_it -> next;
	} while(c_it != commands);
	return 0;
}

int
main(int argc, char *argv[])
{
	sigset_t emask;
	sigemptyset(&emask);
	sigset_t sigchild_mask;
	sigemptyset(&sigchild_mask);
	sigaddset(&sigchild_mask, SIGCHLD);
	//SIGCHILD is blocked on outputing notes and during updating fg info
	//is unblocked during read and while parent is waiting for fg children

	//SIGCHLD
	struct sigaction act;
	act.sa_handler = sigchild_handler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGCHLD, &act, NULL);

	//SIGINT
	struct sigaction _act;
	_act.sa_handler = SIG_IGN;
	_act.sa_flags = 0;
	sigemptyset(&_act.sa_mask);
	struct sigaction old_sigint_handler;
	sigaction(SIGINT, &_act, &old_sigint_handler);
	//SIGINT is blocked always, but should affect children

	fg_children[0] = -1;
	char buf[MAX_LINE_LENGTH + 1];
	
	struct stat info;
	fstat(0,&info);

	while (1){
		
		sigprocmask(SIG_BLOCK, &sigchild_mask, NULL);

		if(S_ISCHR(info.st_mode)){ //console
			if(bg_index > 0){
				for(int i = 0; i < bg_index; i++){ //print notes on bck processes
					write(2, "Background process ", 19);
					int tmp = bg_pids[i];
					char str[10];
					sprintf(str, "%d", tmp);
					write(2, str, strlen(str));
					write(2, " terminated. ", 13);
					if(bg_exit_or_kill[i]) 
						write(2, "(exited with status ", 20);
					else
						write(2, "(killed by signal ", 18);
					tmp = bg_exit_status[i];

					sprintf(str, "%d", tmp);
					write(2, str, strlen(str));
					write(2, ")\n", 2);
				}
				bg_index = 0;
			}
			write(1, PROMPT_STR, strlen(PROMPT_STR));	
		}
		
		sigprocmask(SIG_UNBLOCK, &sigchild_mask, NULL);

		int command_lenght = -1;
		while(command_lenght == -1){
			command_lenght = read(0,buf,MAX_LINE_LENGTH); //try to read
			if(command_lenght == -1){
				if(errno != EINTR) exit(1); //read may be interrupted
			} else if(command_lenght == 0) exit(0);
		}
		
		if(S_ISCHR(info.st_mode)){ //console - technical reasons
			buf[command_lenght] = 0;
		}	

		char* start = buf;
		char* end;
		bool too_long = false;

//---read input
		while(command_lenght > 0){
			end = memchr(start,'\n',command_lenght);
			if(end != NULL){
				*end = 0;
				command_lenght--;
				if(too_long){
					syntax_error();
					command_lenght = command_lenght - (end - start);
					start = end + 1;
					too_long = false;
					continue;
				}
			} else {
				if(too_long) command_lenght = 0;
				memmove(buf, start, command_lenght);
				start = buf;

				int the_rest = -1;
				while(the_rest == -1){
					the_rest = read(0, start +  command_lenght, MAX_LINE_LENGTH - command_lenght);	
					if(the_rest == -1){
						if(errno != EINTR) exit(1); //read may be interrupted
					} else if(the_rest == 0) exit(0);
				}

				command_lenght += the_rest;
				if(memchr(start,'\n',command_lenght) == NULL 
					&& start + command_lenght == buf + MAX_LINE_LENGTH){
					too_long = true;
				}
				continue;
			}

//---check if there is any command to execute
			if(end - start > 0 && *(start) != '#'){ //nie wiem jak wykryć komentarze inaczej			
				struct pipelineseq * pipelineseq;
				pipelineseq = parseline(start);
				if(pipelineseq == NULL) {
					syntax_error();
					command_lenght = 0;
					continue;
				}
				//printparsedline(pipelineseq);
				struct pipelineseq * pipelineseq_it = pipelineseq;


				do { //pipelineseq

					sigprocmask(SIG_BLOCK, &sigchild_mask, NULL);

					struct pipeline * pipeline = pipelineseq_it -> pipeline;
					struct commandseq * commands = pipeline -> commands;
					int cor_status = is_correct_pipeline(commands);
					if(cor_status == 2){
						syntax_error();
						continue;
					} else if(cor_status == 1){
						continue;
					}
					struct commandseq * commands_it = commands;

					int pipefd_prev[2]; pipefd_prev[0] = -1;
					int pipefd_next[2]; pipefd_next[0] = -1;

					int fg_index = 0;
					do{ //commandseq

						struct command * com = commands_it -> com;

//---pipelines
						pipefd_prev[0] = pipefd_next[0];
						pipefd_prev[1] = pipefd_next[1];
						if(commands_it -> next != commands){ //not the last one
							if(pipe(pipefd_next) < 0) exit(1);
						}


//---build arrays
						char* _argv[MAX_LINE_LENGTH];
						redir* _redirargv[MAX_LINE_LENGTH];
						insert_args(com,_argv);
						insert_redirs(com,_redirargv);


//---check if shell command
						int shell_command_index = -1;
						if(commands_it -> next == commands_it){ //trivial pipeline
							for(int i = 0; builtins_table[i].name != NULL; i++){
								if(strcmp(builtins_table[i].name, _argv[0]) == 0)
									shell_command_index = i;
							}
						}

//---execute (first: builtin commands)
						if(shell_command_index != -1){ //shell command
							(builtins_table[shell_command_index].fun)(_argv);
						} else { //command in child process
							int error = 0;
							pid_t child_pid = fork();

							if(child_pid == -1){
								write(2,"Failed to create a new process.\n",32);
							} else if(!child_pid){

//---CHILD:
								sigaction(SIGINT, &old_sigint_handler, NULL);
								if(pipeline -> flags == INBACKGROUND)
									setsid(); //leave group
								sigprocmask(SIG_UNBLOCK, &sigchild_mask, NULL);

								if(commands_it != commands){ //not first ---> IN
									if(close(0) < 0) exit(1);
									if(dup2(pipefd_prev[0],0) < 0) exit(1);
									if(close(pipefd_prev[0]) < 0) exit(1);
									if(close(pipefd_prev[1]) < 0) exit(1);

								}
								if(commands_it -> next != commands){ //not last ---> OUT
									if(close(1) < 0) exit(1);
									if(dup2(pipefd_next[1],1) < 0) exit(1);
									if(close(pipefd_next[0]) < 0) exit(1);
									if(close(pipefd_next[1]) < 0) exit(1);
								}

//---redirections
								int i = 0;
								while(_redirargv[i] != NULL){
									int redir_flag = _redirargv[i] -> flags;
									int flags;
									if(IS_RIN(redir_flag)){
										if(close(0) < 0) exit(1);
										flags = O_RDONLY;
									} else if(IS_RAPPEND(redir_flag)){
										if(close(1) < 0) exit(1);
										flags = O_WRONLY | O_APPEND | O_CREAT;
									} else if(IS_ROUT(redir_flag)) {
										if(close(1) < 0) exit(1);
										flags = O_WRONLY | O_TRUNC | O_CREAT;
									} else {
										exit(1);
									}
									char * file = _redirargv[i] -> filename;
									if ( open(file, flags, 0644) < 0){
										write(2, file, strlen(file));
										if(errno == ENOENT)
											write(2,": no such file or directory\n",28);
										else if(errno == EACCES)
											write(2,": permission denied\n",20);
										exit(1);
									}
									i++;
								}

//---execute if success
								char* name = _argv[0];
								if(execvp(name, _argv) < 0){
									write(2, name, strlen(name));
									if(errno == ENOENT)
										write(2,": no such file or directory\n",28);
									else if(errno == EACCES)
										write(2,": permission denied\n",20);
									else
										write(2,": exec error\n",13);
									exit(EXEC_FAILURE);
								}
							} else {

//---PARENT:	

								if(pipeline -> flags != INBACKGROUND){
									fg_children[fg_index++] = child_pid;
									fg_children[fg_index] = -1;
									children_counter++;
								}

								if(pipefd_prev[0] >= 0){
									if(close(pipefd_prev[0]) < 0) exit(1);
									if(close(pipefd_prev[1]) < 0) exit(1);
								}
							}
						}
						commands_it = commands_it -> next;
					} while(commands_it != commands);
					
					sigprocmask(SIG_UNBLOCK, &sigchild_mask, NULL);

					while(children_counter > 0)
						sigsuspend(&emask);
					

					pipelineseq_it = pipelineseq_it -> next;
				} while(pipelineseq_it != pipelineseq);
			}
			
			command_lenght = command_lenght - (end - start);
			start = end + 1;
			
		}
	}



	return 0;

	/*ln = parseline("ls -las | grep k | wc ; echo abc > f1 ;  cat < f2 ; echo abc >> f3\n");
	printparsedline(ln);
	printf("\n");
	com = pickfirstcommand(ln);
	printcommand(com,1);

	ln = parseline("sleep 3 &");
	printparsedline(ln);
	printf("\n");
	
	ln = parseline("echo  & abc >> f3\n");
	printparsedline(ln);
	printf("\n");
	com = pickfirstcommand(ln);
	printcommand(com,1);*/
}


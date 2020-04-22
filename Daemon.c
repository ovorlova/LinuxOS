#include <stdio.h> 
#include <string.h> 
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <resolv.h>
#include <errno.h>
#include <syslog.h>
#include <stdlib.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdlib.h>

int flag = 0;
int flagToDie = 0;
sem_t semaphore;

void sigalrm_handler(int signum){ // обработаем сигнал SIGALRM - его и будем ловить во время работы
	flag = 1;
	return;
}

void sigterm_handler(int signum){ // обработаем сигнал SIGTERM для graceful-завершения программы
	flagToDie = 1;
	return;
}

int Daemon(char* filename) {
	signal(SIGALRM, sigalrm_handler); 
	signal(SIGTERM, sigterm_handler);
	openlog("NewSemaphore", LOG_PID, LOG_DAEMON); // будем писать в syslog сообщения, помеченные "NewDaemon", и выводить PID
	
	while(1){
		pause();
		if (flag == 1){ // поймали сигнал
			syslog(LOG_NOTICE, "Alarm signal catched"); 
			
			char * buf = NULL;
			size_t buf_size = 0;
			
			FILE *fp = fopen("cmd.txt", "r");
			char ** commands[1000];
			int cnt_commands = 0;
			while(getline(&buf, &buf_size, fp)>=0){
				char** command = (char**)malloc(256);
				char* pch = strtok (buf, " "); // вместо split
 
				int cnt_str = 0;
				while (pch != NULL) 
				{
					if (pch[0] == '\n')
						continue;
					command[cnt_str] = pch;
					if (command[cnt_str][strlen(pch)-1] == '\n') // если в команду попал символ '\n'
						command[cnt_str][strlen(pch)-1] = '\0';
					cnt_str++;
					pch = strtok(NULL, " ");
				}
				command[cnt_str] = NULL;
				commands[cnt_commands++] = command;
				buf_size = 0;
			}
			flag = 0;
			fclose(fp);
			
			sem_init(&semaphore, 0, 1);
			
			
			for (int i = 0; i < cnt_commands; i++){ // обработка комманд
				pid_t parpid;
				if((parpid=fork()) == 0){ // дочерний процесс исполнит команду и завершится
				
					sem_wait(&semaphore); 
					
					char file[20];
					sprintf(file, "%d%s",  i+1, "_output.txt");
					char log_message[100];
					sprintf(log_message, "%s%s%s", "Daemon created file ", file, "\n");
					
					int logFile = open("log.txt", O_CREAT|O_RDWR, S_IRWXU);
					lseek(logFile, 0, SEEK_END);
					int retWrite = write(logFile, log_message, strlen(log_message));
					if (retWrite == -1){ 
						char error[100];
						sprintf(error, "%s%d%s", "Can't write ", i+1, " command\n");
						syslog(LOG_ERR, error);
					}
					close(logFile);
					
					close(1);
					int fileOut = open(file, O_CREAT|O_RDWR, S_IRWXU);
					dup2(fileOut, 1); 
					
					sem_post(&semaphore);
					execv(commands[i][0], commands[i]);
				}
			}
		}
		if (flagToDie == 1){ // сигнал завершения процесса
			sem_destroy(&semaphore);
			syslog(LOG_NOTICE, "Daemon is killed");
			closelog(); // закрываем дескриптор
			exit(0);
		}
	}

}

int main(int argc, char* argv[])
{
	  pid_t parpid;
	  if((parpid=fork())<0) // не получается создать дочерний процесс
		{               
		 printf("\ncan't fork"); 
		 exit(1); // ошибка
		}
	  else if (parpid!=0){  exit(0); } // если родитель, выходим
	  char* filename = argv[1];
	  setsid();  // перевод нашего дочернего процесса в новую сесию
	  Daemon(filename);  // вызов демона
	  return 0;
}

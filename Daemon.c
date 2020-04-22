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
	
	sem_t semaphore;
	sem_init(&semaphore, 0, 1);
	

	while(1){
		pause();
		if (flag == 1){ 
			syslog(LOG_NOTICE, "Alarm signal catched"); 
			
			char buf[1000] = "";
            int fd = open(filename, O_CREAT|O_RDWR, S_IRWXU);
            read(fd, buf, sizeof(buf));
            close(fd);
            flag = 0;
            
			char* commands[1000];
			int cnt_commands = 0;
			
			char* pch = strtok(buf, "\n");			
			while(pch != NULL) {
				commands[cnt_commands] = pch;
				commands[cnt_commands++][strlen(pch)] = '\0';
				pch = strtok(NULL, "\n");
			} 

			commands[cnt_commands] = NULL;

			for (int i = 0; i < cnt_commands; i++){ // обработка комманд
				pid_t parpid;
				if((parpid=fork()) == 0){ // дочерний процесс исполнит команду и завершится
					
					int cnt_str = 0;
					char* pch = strtok (commands[i], " ");
					char* command[100];
					while (pch != NULL) 
					{
						command[cnt_str++] = pch;
						pch = strtok(NULL, " ");
					}
					command[cnt_str] = NULL;
					int wait = sem_wait(&semaphore);
					if (wait == -1){ 
						char error[100];
						sprintf(error, "%s%d%s", "Can't implement ", i+1, " command\n");
						syslog(LOG_ERR, error);
					}
					else{
					
						char log_message[100];
						sprintf(log_message, "%s%d%s", "Daemon implements command ", i+1, "\n");
						
						int logFile = open("log.txt", O_CREAT|O_RDWR, S_IRWXU);
						lseek(logFile, 0, SEEK_END);
						write(logFile, log_message, strlen(log_message));
						close(logFile);
						
						close(1);
						int fileOut = open("output.txt", O_CREAT|O_RDWR|O_APPEND, S_IRWXU); 
						dup2(fileOut, 1); 
						
						sem_post(&semaphore);
						execv(command[0], command);
					}
					
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

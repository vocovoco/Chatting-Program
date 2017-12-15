/*
	작성자: vocovoco
	컴파일 명령어: gcc mproc_server.c -o mproc_server
	실행 방법: ./mproc_server <Port#>
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024
struct shared_use{
	int clnt_num;
	int read_clnt_num;
	pid_t write_clnt;
	char message[BUF_SIZE];
};

void* shared_memory = (void*)0;
struct shared_use *shared_space;
int shmid, running;

void error_handling(char *message);
void read_childproc(int sig);

int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;

	pid_t pid;
	struct sigaction act;
	socklen_t adr_sz;
	int str_len, state;
	char buf[BUF_SIZE];

	shmid = shmget((key_t)5555, sizeof(struct shared_use), 0666|IPC_CREAT);
	if(shmid == -1)
	{
		puts("shmget() error");
		exit(1);
	}
	shared_memory = shmat(shmid, (void*)0, 0);
	if(shared_memory == (void*)-1)
		error_handling("shmat() error");
	shared_space = (struct shared_use*)shared_memory;
	shared_space->clnt_num = 0;
	shared_space->read_clnt_num = shared_space->clnt_num - 1;
	strcpy(shared_space->message, "null");

	if(argc != 2){
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	act.sa_handler = read_childproc;
	sigemptyset(&act.sa_mask);
	act.sa_flags=0;

	sigaddset(&act.sa_mask,SIGINT);
	sigaddset(&act.sa_mask,SIGCHLD);

	state=sigaction(SIGINT, &act, 0);
	state=sigaction(SIGCHLD, &act, 0);
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));

	if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1)
		error_handling("bind() error");
	if(listen(serv_sock, 5) == -1)
		error_handling("listen() error");

	while(1)
	{
		adr_sz = sizeof(clnt_adr);
		clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
		if(clnt_sock == -1)
			continue;
		shmctl(shmid, SHM_LOCK, 0);
		shared_space->clnt_num++;
		shared_space->read_clnt_num++;
		shmctl(shmid, SHM_UNLOCK, 0);
		//printf("%d\n", shared_space->clnt_num);

		pid = fork();
		if(pid == -1)
		{
			close(clnt_sock);
			continue;
		}
		if(pid == 0)
		{
			printf("new client %s:%d connected...\n", inet_ntoa(clnt_adr.sin_addr), getpid());
			close(serv_sock);

			pid = fork();
			if(pid == 0)
			{
				while((str_len=read(clnt_sock, buf, BUF_SIZE)) != 0)
				{
					while(1){
						shmctl(shmid, SHM_LOCK, 0);					
						if(shared_space->read_clnt_num >= (shared_space->clnt_num - 1)){
							shared_space->read_clnt_num--;
							//puts("--------------------");
							break;
						}
						shmctl(shmid, SHM_UNLOCK, 0);
					}
					
					shared_space->write_clnt = getppid();
					strcpy(shared_space->message, buf);
					shmctl(shmid, SHM_UNLOCK, 0);
				}
				close(clnt_sock);//shutdown(clnt_sock, SHUT_RDWR);
				return 0;
			}

			running = 1;
			while(running)
			{
				if(strcmp(shared_space->message,"null")&&(getpid() != shared_space->write_clnt))
				{
					shmctl(shmid, SHM_LOCK, 0);
					//printf("%d, %d\n", getpid(), shared_space->write_clnt);
					//printf("%d\n", shared_space->read_clnt_num);
					if((shared_space->read_clnt_num--) == 0){
						//printf("%d, %d\n", shared_space->clnt_num, shared_space->read_clnt_num);
						sprintf(buf, "%d", (int)shared_space->write_clnt);
						strcat(buf, ": ");
						strcat(buf, shared_space->message);
						write(clnt_sock, buf, strlen(buf));
						
						strcpy(shared_space->message, "null");
						//printf("1: %d\n", getpid());
						shared_space->read_clnt_num = shared_space->clnt_num - 1;
						shmctl(shmid, SHM_UNLOCK, 0);
					}
					else
					{
						sprintf(buf, "%d", (int)shared_space->write_clnt);
						strcat(buf, ": ");
						strcat(buf, shared_space->message);
						write(clnt_sock, buf, strlen(buf));
						//printf("2: %d, %d\n", getpid(), shared_space->read_clnt_num);
						shmctl(shmid, SHM_UNLOCK, 0);

						while(strcmp(shared_space->message, "null"));
					}
				}
				else if(shared_space->clnt_num == 1)
					strcpy(shared_space->message, "null");
			}

			printf("client %s:%d disconnected...\n", inet_ntoa(clnt_adr.sin_addr), getpid());
			shmctl(shmid, SHM_LOCK, 0);
			shared_space->clnt_num--;
			shmctl(shmid, SHM_UNLOCK, 0);
			//printf("%d\n", shared_space->clnt_num);
			close(clnt_sock);
			return 0;
			
		}
		else
			close(clnt_sock);
	}
	close(serv_sock);
	return 0;
}

void read_childproc(int sig)
{
	pid_t pid;
	int status;

	switch(sig) {
		case SIGCHLD:
			pid = waitpid(-1, &status, WNOHANG);
			if(pid > 0)
				running = 0;
			break;
		case SIGINT:
			shmdt(shared_memory);
			shmctl(shmid, IPC_RMID, 0);
			exit(1);
			break;
	}
}
void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('/n', stderr);
	raise(SIGINT);
}
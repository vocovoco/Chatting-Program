/*
	작성자: vocovoco
	컴파일 명령어: gcc iompx_client.c -o iompx_client
	실행 방법: ./iompx_client <Server IP> <Server Port#> <chatting nickname>
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1004
#define NAME_SIZE 20

char buf[BUF_SIZE];
char name[NAME_SIZE] = "[DEFAULT]";

void error_handling(char *message);
void read_routine(int sock, char *buf);
void write_routine(int sock, char *buf);

int main(int argc, char *argv[])
{
	int sock;
	pid_t pid;

	struct sockaddr_in serv_adr;
	if(argc != 4){
		printf("Usage : %s <IP> <port> <name>\n", argv[0]);
		exit(1);
	}

	sprintf(name, "[%s]", argv[3]);

	sock = socket(PF_INET, SOCK_STREAM, 0);
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_adr.sin_port = htons(atoi(argv[2]));

	if(connect(sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
		error_handling("connect() error!");

	pid = fork();
	if(pid == 0)
		write_routine(sock, buf);
	else
		read_routine(sock, buf);

	close(sock);
	return 0;
}

void read_routine(int sock, char *buf)
{
	while(1)
	{
		int str_len = read(sock, buf, NAME_SIZE + BUF_SIZE);
		if(str_len == 0)
			return;

		buf[str_len] = 0;
		puts(buf);
	}
}
void write_routine(int sock, char *buf)
{
	char name_msg[NAME_SIZE + BUF_SIZE];

	while(1)
	{
		fgets(buf, BUF_SIZE, stdin);
		if(!strcmp(buf, "q\n") || !strcmp(buf, "Q\n"))
		{
			shutdown(sock, SHUT_WR);
			return;
		}
		sprintf(name_msg, "%s: %s", name, buf);
		write(sock, name_msg, strlen(buf)+1);
	}
}
void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}
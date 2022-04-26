#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// SMTP protocol
#define BUFLEN 3000
#define MAX_CLIENTS 5

#define DIE(assertion, call_description)  \
	do                                    \
	{                                     \
		if (assertion)                    \
		{                                 \
			fprintf(stderr, "(%s, %d): ", \
					__FILE__, __LINE__);  \
			perror(call_description);     \
			exit(EXIT_FAILURE);           \
		}                                 \
	} while (0)

int wait_for_respond(int socket, char *expecting)
{
	char buffer[BUFLEN];
	int bytes_received = recv(socket, buffer, BUFLEN, 0);
	DIE(bytes_received < 0, "greeting");

	printf("Received: %s\n", buffer);
	if (strcmp(buffer, expecting) == 0)
	{
		return 1;
	}
	return 0;
}

void send_PROT(int socket, char *expecting)
{
	char buffer[BUFLEN];
	memset(buffer, 0, sizeof(buffer));
	strcpy(buffer, expecting);
	int n = send(socket, buffer, strlen(buffer), 0);
	DIE(n < 0, "send");
}


void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	fd_set read_fds; // multimea de citire folosita in select()
	fd_set tmp_fds;	 // multime folosita temporar
	int fdmax;		 // valoare maxima fd din multimea read_fds

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	if (argc < 3)
	{
		usage(argv[0]);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;
	FD_SET(STDIN_FILENO, &read_fds);

	// list of protocols
	int generate = 0;
	int HELO = 0; // 0 - not done, 1 - done
	int MAIL = 0; // 0 - not done, 1 - done
	int RCPT = 0; // 0 - not done, 1 - done
	int DATA = 0; // 0 - not done, 1 - done
	int QUIT = 0; // 0 - not done, 1 - done
	while (1)
	{
		tmp_fds = read_fds;

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if (FD_ISSET(sockfd, &tmp_fds))
		{
			if (wait_for_respond(sockfd, "220") == 1)
			{
				send_PROT(sockfd, "HELO");
			}

			if (wait_for_respond(sockfd, "250 helo") == 1)
			{
				char mesg[BUFLEN];
				strcpy(mesg, "MAIL");
				strcat(mesg, " ");
				strcat(mesg, argv[3]);
				strcat(mesg, " ");
				strcat(mesg, argv[2]);
				strcat(mesg, " ");
				strcat(mesg, argv[1]);
				send_PROT(sockfd, mesg);
			}

			if (wait_for_respond(sockfd, "250 ok") == 1)
			{
				send_PROT(sockfd, "RCPT");
			}

			if (wait_for_respond(sockfd, "250 accept") == 1)
			{
				send_PROT(sockfd, "DATA");
			}
			
			if (wait_for_respond(sockfd, "354") == 1)
			{
			}
		}
	}
	close(sockfd);
	return 0;
}
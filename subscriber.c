#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	int HELO = 0; // 0 - not done, 1 - done
	int MAIL = 0; // 0 - not done, 1 - done
	int RCPT = 0; // 0 - not done, 1 - done
	int DATA = 0; // 0 - not done, 1 - done
	int QUIT = 0; // 0 - not done, 1 - done

	char *id_client = argv[1];
	char *ip_server = argv[2];
	char *port = argv[3];

	fd_set read_fds; // multimea de citire folosita in select()
	fd_set tmp_fds;	 // multime folosita temporar
	int fdmax;		 // valoare maxima fd din multimea read_fds

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

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

	while (1)
	{
		tmp_fds = read_fds;

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if (FD_ISSET(STDIN_FILENO, &tmp_fds))
		{
			// if (HELO == 1)
			// {
			// 	if (MAIL == 1)
			// 	{
			// 		if (RCPT == 1)
			// 		{
			// 			if (DATA == 1)
			// 			{
			// 				// se citeste de la stdin
			// 				memset(buffer, 0, sizeof(buffer));
			// 				n = read(0, buffer, sizeof(buffer) - 1);
			// 				DIE(n < 0, "read");

			// 				if (strncmp(buffer, "exit", 4) == 0)
			// 				{
			// 					break;
			// 				}

			// 				// se trimite mesaj la server
			// 				n = send(sockfd, buffer, strlen(buffer), 0);
			// 				DIE(n < 0, "send");
			// 			}
			// 		}
			// 	}
			// }
			// se citeste de la stdin
			memset(buffer, 0, sizeof(buffer));
			n = read(0, buffer, sizeof(buffer) - 1);
			DIE(n < 0, "read");

			if (strncmp(buffer, "exit", 4) == 0)
			{
				break;
			}
		}
		else
		{
			memset(buffer, 0, sizeof(buffer));
			// se primeste mesaj la server
			n = recv(sockfd, buffer, BUFLEN, 0);
			if (strcmp(buffer, "220") == 0 && HELO == 0)
			{
				memset(buffer, 0, sizeof(buffer));
				strcpy(buffer, "HELO");
				n = send(sockfd, buffer, strlen(buffer), 0);
				DIE(n < 0, "send");
				HELO = 1;
			}
			if (strcmp(buffer, "250") == 0 && HELO == 1)
			{
				memset(buffer, 0, sizeof(buffer));
				strcpy(buffer, "MAIL ");
				strcat(buffer, id_client);
				strcat(buffer, " ");
				strcat(buffer, ip_server);
				strcat(buffer, " ");
				strcat(buffer, port);
				n = send(sockfd, buffer, strlen(buffer), 0);
				DIE(n < 0, "send");
				MAIL = 1;
			}

			DIE(n < 0, "recv");
			printf("Received: %s", buffer);
		}
	}

	close(sockfd);

	return 0;
}

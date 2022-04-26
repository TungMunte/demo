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

typedef struct
{
	unsigned int type_value;
	int socket_of_client;
	int SF;
	int generate;
	int HELO; // 0 - not done, 1 - done
	int MAIL; // 0 - not done, 1 - done
	int RCPT; // 0 - not done, 1 - done
	int DATA; // 0 - not done, 1 - done
	int QUIT; // 0 - not done, 1 - done
	char *id_client;
	char *ip;
	char *port;
	char *topic;
	char *content;
} Client;

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

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret;
	socklen_t clilen;

	fd_set read_fds; // multimea de citire folosita in select()
	fd_set tmp_fds;	 // multime folosita temporar
	int fdmax;		 // valoare maxima fd din multimea read_fds

	if (argc < 2)
	{
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(sockfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;

	int j = 0;
	int connfd[MAX_CLIENTS];
	// create info clients
	Client *client = (Client *)malloc(sizeof(Client) * MAX_CLIENTS);
	for (size_t i = 0; i < MAX_CLIENTS; i++)
	{
		client[i].generate = 0;
		client[i].HELO = 0;
		client[i].DATA = 0;
		client[i].MAIL = 0;
		client[i].RCPT = 0;
		client[i].QUIT = 0;
		client[i].id_client = (char *)malloc(sizeof(char) * 5);
		client[i].ip = (char *)malloc(sizeof(char) * 15);
		client[i].port = (char *)malloc(sizeof(char) * 15);
		client[i].topic = (char *)malloc(sizeof(char) * 50);
		client[i].content = (char *)malloc(sizeof(char) * 1500);
	}

	int over = 0;
	while (1)
	{
		tmp_fds = read_fds;

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		for (i = 0; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &tmp_fds))
			{
				if (i == sockfd)
				{
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					clilen = sizeof(cli_addr);
					newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
					DIE(newsockfd < 0, "accept");

					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax)
					{
						fdmax = newsockfd;
					}

					//printf("Noua conexiune de la %s, port %d, socket client %d\n",
					//	   inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockfd);

					memset(buffer, 0, BUFLEN);
					strcpy(buffer, "220");
					n = send(newsockfd, buffer, BUFLEN, 0);
					DIE(n < 0, "send");
					connfd[j] = newsockfd;
					client[j].socket_of_client = newsockfd;
					client[j].generate = 1;
					j++;
				}
				else
				{
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(buffer) - 1, 0);
					DIE(n < 0, "recv");

					if (n == 0)
					{
						// conexiunea s-a inchis
						// printf("Socket-ul client %d a inchis conexiunea\n", i);
						close(i);

						// se scoate din multimea de citire socketul inchis
						FD_CLR(i, &read_fds);
					}
					else
					{
						// printf("S-a primit de la clientul de pe socketul %d mesajul: %s\n", i, buffer);

						for (int l = 0; l < j; l++)
						{
							if (connfd[l] != sockfd)
							{
								if (client[l].generate == 1 && client[l].HELO == 0 && client[l].MAIL == 0 && client[l].RCPT == 0 && client[l].DATA == 0 && client[l].QUIT == 0 && strcmp(buffer, "HELO") == 0)
								{
									strcpy(buffer, "250 helo");
									send(connfd[l], buffer, BUFLEN, 0);
									client[l].HELO = 1;
								}
								if (client[l].generate == 1 && client[l].HELO == 1 && client[l].MAIL == 0 && client[l].RCPT == 0 && client[l].DATA == 0 && client[l].QUIT == 0 && strstr(buffer, "MAIL") != NULL)
								{
									char *token;
									const char s[2] = " ";
									token = strtok(buffer, s);
									int count = 0;
									while (token != NULL)
									{
										if (count == 1)
											strcpy(client[l].port, token);
										if (count == 2)
											strcpy(client[l].ip, token);
										if (count == 3)
											strcpy(client[l].id_client, token);
										token = strtok(NULL, s);
										count++;
									}
									printf("New client %s connected from %s:%s\n", client[l].id_client, client[l].ip, client[l].port);
									strcpy(buffer, "250 ok");
									send(connfd[l], buffer, BUFLEN, 0);
									client[l].MAIL = 1;
								}
								if (client[l].generate == 1 && client[l].HELO == 1 && client[l].MAIL == 1 && client[l].RCPT == 0 && client[l].DATA == 0 && client[l].QUIT == 0 && strcmp(buffer, "RCPT") == 0)
								{
									strcpy(buffer, "250 accept");
									send(connfd[l], buffer, BUFLEN, 0);
									client[l].RCPT = 1;
								}
								if (client[l].generate == 1 && client[l].HELO == 1 && client[l].MAIL == 1 && client[l].RCPT == 1 && client[l].DATA == 0 && client[l].QUIT == 0 && strcmp(buffer, "DATA") == 0)
								{
									strcpy(buffer, "354");
									send(connfd[l], buffer, BUFLEN, 0);
									client[l].DATA = 1;
								}
							}
						}
					}
				}
			}
		}
	}
	for (size_t i = 0; i < MAX_CLIENTS; i++)
	{
		close(client[i].socket_of_client);
	}

	close(newsockfd);
	close(sockfd);

	return 0;
}

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
    char *id_client;
    char *ip;
    char *port;
    int SF;
    int generate;
    int HELO; // 0 - not done, 1 - done
    int MAIL; // 0 - not done, 1 - done
    int RCPT; // 0 - not done, 1 - done
    int DATA; // 0 - not done, 1 - done
    int QUIT; // 0 - not done, 1 - done
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

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno;
    char buffer[BUFLEN];
    struct sockaddr_in serv_addr, cli_addr;
    int n, i, ret;
    socklen_t clilen;

    Client *client = (Client *)malloc(sizeof(Client) * MAX_CLIENTS);
    for (size_t i = 0; i < MAX_CLIENTS; i++)
    {
        client[i].generate = 0;
        client[i].HELO = 0;
        client[i].MAIL = 0;
        client[i].RCPT = 0;
        client[i].DATA = 0;
        client[i].QUIT = 0;
        client[i].id_client = (char *)malloc(sizeof(char) * 15);
        client[i].ip = (char *)malloc(sizeof(char) * 15);
        client[i].port = (char *)malloc(sizeof(char) * 15);
        client[i].topic = (char *)malloc(sizeof(char) * 50);
        client[i].content = (char *)malloc(sizeof(char) * 1500);
    }

    fd_set read_fds;
    fd_set tmp_fds;
    int fdmax;

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

    FD_SET(sockfd, &read_fds);
    fdmax = sockfd;

    int j = 0;
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
                    client[j].socket_of_client = newsockfd;

                    // memset(buffer, 0, BUFLEN);
                    // strcpy(buffer, "220");
                    // send(client[j].socket_of_client, buffer, BUFLEN, 0);
                    // client[j].generate = 1;
                    // j++;
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
                        close(i);

                        // se scoate din multimea de citire socketul inchis
                        FD_CLR(i, &read_fds);
                    }
                    else
                    {
                        for (int l = 0; l < j; l++)
                        {
                            if (client[l].socket_of_client != i)
                            {
                                // if (strcmp(buffer, "HELO") == 0 && client[l].generate == 1)
                                // {
                                //     client[l].HELO = 1;
                                //     strcpy(buffer, "250 hello");
                                //     send(client[l].socket_of_client, buffer, BUFLEN, 0);
                                // }
                                // else if (strncmp(buffer, "MAIL", 4) == 0 && client[l].HELO == 1)
                                // {
                                //     const char s[2] = " ";
                                //     client[l].MAIL = 1;
                                //     char *token = strtok(buffer, s);
                                //     strcpy(client[l].id_client, token);
                                //     token = strtok(NULL, s);
                                //     strcpy(client[l].ip, token);
                                //     token = strtok(NULL, s);
                                //     strcpy(client[l].port, token);

                                //     printf("New client %s connected from %s:%s\n", client[l].id_client, client[l].port, client[l].ip);
                                //     strcpy(buffer, "250 ok");
                                //     send(client[l].socket_of_client, buffer, BUFLEN, 0);
                                // }
                            }
                        }
                    }
                }
            }
        }
    }
    for (size_t i = 0; i < j; i++)
    {
        close(client[i].socket_of_client);
        FD_CLR(client[i].socket_of_client, &read_fds);
    }

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>

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
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sockfd, n, ret;
    struct sockaddr_in serv_addr;
    char buffer[BUFLEN];

    fd_set read_fds; // multimea de citire folosita in select()
    fd_set tmp_fds;  // multime folosita temporar
    int fdmax;       // valoare maxima fd din multimea read_fds

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

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;

    int condition_to_stop = 0;
    int ready_to_connect = 0;
    while (1)
    {
        tmp_fds = read_fds;

        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, &tv);
        DIE(ret < 0, "select");
        if (FD_ISSET(STDIN_FILENO, &tmp_fds))
        {
            // se citeste de la stdin
            n = read(0, buffer, sizeof(buffer) - 1);
            DIE(n < 0, "read");

            if (strncmp(buffer, "exit", 4) == 0)
            {
                break;
            }
            else if (strncmp(buffer, "subscribe", 9) == 0)
            {
                printf("Subscribed to topic.\n");
                n = send(sockfd, buffer, strlen(buffer), 0);
                DIE(n < 0, "send");
            }
            else if (strncmp(buffer, "unsubscribe", 11) == 0)
            {
                char *token;
                const char s[2] = " ";
                token = strtok(buffer, s);
                token = strtok(NULL, s);
                char *topic;
                strcpy(topic, token);
                printf("Unsubscribed from %s\n", topic);
                n = send(sockfd, buffer, strlen(buffer), 0);
                DIE(n < 0, "send");
            }
        }
        if (FD_ISSET(sockfd, &tmp_fds))
        {
            char buffer[BUFLEN];
            int bytes_received = recv(sockfd, buffer, BUFLEN, 0);
            DIE(bytes_received < 0, "recv udp");

            if (bytes_received == 0)
            {
                condition_to_stop++;
            }
            if (condition_to_stop == 10)
            {
                break;
            }
            if (ready_to_connect == 1 && bytes_received > 0)
            {
                printf("%s\n");
            }
            else if (strcmp(buffer, "exit") == 0)
            {
                break;
            }
            else if (strcmp(buffer, "220") == 0 && ready_to_connect == 0)
            {
                send_PROT(sockfd, "HELO");
            }
            else if (strcmp(buffer, "250 helo") == 0 && ready_to_connect == 0)
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
            else if (strcmp(buffer, "250 ok") == 0 && ready_to_connect == 0)
            {
                send_PROT(sockfd, "RCPT");
            }
            else if (strcmp(buffer, "250 accept") == 0 && ready_to_connect == 0)
            {
                send_PROT(sockfd, "DATA");
            }
            else if (strcmp(buffer, "354") == 0 && ready_to_connect == 0)
            {
                ready_to_connect = 1;
            }
            
        }
    }
    close(STDIN_FILENO);
    close(sockfd);
    return 0;
}
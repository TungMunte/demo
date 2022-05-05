#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <netdb.h>
#include <math.h>

// SMTP protocol
#define BUFLEN 3000
#define MAX_CLIENTS 5

typedef struct
{
    char topic[50];
    char SF[2];
} Message;

typedef struct
{
    int socket_of_client;
    int SF;
    int online; // when client is online, the same id_client is not allowed to connect to server | 1 = on , 0 = off
    int disconnect;
    int generate;
    int HELO; // 0 - not done, 1 - done
    int MAIL; // 0 - not done, 1 - done
    int RCPT; // 0 - not done, 1 - done
    int DATA; // 0 - not done, 1 - done
    int QUIT; // 0 - not done, 1 - done
    char id_client[3];
    char ip[15];
    char port[10];
    int number_of_sub;
    int number_of_unsub;
    Message sub[16];
    Message unsub[16];
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

void process_payload_and_send(int socket, char *read, char *ip_udp_client, char *port_udp_client)
{
    char *message = (char *)malloc(sizeof(char) * BUFLEN);
    char *topic = (char *)malloc(sizeof(char) * 50);
    for (size_t i = 0; i < 50; i++)
    {
        topic[i] = read[i];
    }

    char *value = (char *)malloc(sizeof(char) * 1500);
    for (size_t i = 51; i < 1551; i++)
    {
        value[i - 51] = read[i];
    }
    if ((int)read[50] == 3)
    {
        snprintf(message, BUFLEN, "%s:%s - %s - %s - %s", ip_udp_client, port_udp_client, topic, "STRING", value);
        send(socket, message, BUFLEN, 0);
    }
    else if ((int)read[50] == 2)
    {
        uint8_t byte_sign = (uint8_t)value[0];
        uint8_t byte_1 = (uint8_t)value[1];
        uint8_t byte_2 = (uint8_t)value[2];
        uint8_t byte_3 = (uint8_t)value[3];
        uint8_t byte_4 = (uint8_t)value[4];
        uint8_t byte_5 = (uint8_t)value[5];

        uint64_t sum = 0;
        sum = byte_1 * pow(2, 24) + byte_2 * pow(2, 16) + byte_3 * pow(2, 8) + byte_4;
        int divider = pow(10, byte_5);
        double new_sum = (double)sum / divider;

        int charsNeeded = 1 + snprintf(NULL, 0, "%.*f", byte_5, new_sum);
        char *demo = malloc(charsNeeded);
        snprintf(demo, charsNeeded, "%.*f", byte_5, new_sum);

        if (byte_sign == 1)
        {
            snprintf(message, BUFLEN, "%s:%s - %s - %s - -%s", ip_udp_client, port_udp_client, topic, "FLOAT", demo);
        }
        else
        {
            snprintf(message, BUFLEN, "%s:%s - %s - %s - %s", ip_udp_client, port_udp_client, topic, "FLOAT", demo);
        }
        send(socket, message, BUFLEN, 0);
    }
    else if ((int)read[50] == 1)
    {
        uint8_t byte_0 = (uint8_t)value[0];
        uint8_t byte_1 = (uint8_t)value[1];

        uint64_t sum = 0;
        sum = byte_0 * pow(2, 8) + byte_1;

        double new_sum = (double)sum / (double)100;

        int charsNeeded = 1 + snprintf(NULL, 0, "%.2f", new_sum);
        char *demo = malloc(charsNeeded);
        snprintf(demo, charsNeeded, "%.2f", new_sum);

        snprintf(message, BUFLEN, "%s:%s - %s - %s - %s", ip_udp_client, port_udp_client, topic, "SHORT_REAL", demo);
        send(socket, message, BUFLEN, 0);
    }
    else if ((int)read[50] == 0)
    {
        uint8_t byte_sign = (uint8_t)value[0];
        uint8_t byte_1 = (uint8_t)value[1];
        uint8_t byte_2 = (uint8_t)value[2];
        uint8_t byte_3 = (uint8_t)value[3];
        uint8_t byte_4 = (uint8_t)value[4];
        uint64_t sum = 0;
        sum = byte_1 * pow(2, 24) + byte_2 * pow(2, 16) + byte_3 * pow(2, 8) + byte_4;

        int charsNeeded = 1 + snprintf(NULL, 0, "%ld", sum);
        char *demo = malloc(charsNeeded);
        snprintf(demo, charsNeeded, "%ld", sum);

        if (byte_sign == 1)
        {
            snprintf(message, BUFLEN, "%s:%s - %s - %s - -%s", ip_udp_client, port_udp_client, topic, "INT", demo);
        }
        else
        {
            snprintf(message, BUFLEN, "%s:%s - %s - %s - %s", ip_udp_client, port_udp_client, topic, "INT", demo);
        }
        send(socket, message, BUFLEN, 0);
    }
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sockfd, newsockfd, portno;
    char buffer[BUFLEN];
    struct sockaddr_in serv_addr, cli_addr;
    int n, i, ret;
    socklen_t clilen;

    fd_set read_fds; // multimea de citire folosita in select()
    fd_set tmp_fds;  // multime folosita temporar
    int fdmax;       // valoare maxima fd din multimea read_fds

    Client client[MAX_CLIENTS];
    for (size_t i = 0; i < MAX_CLIENTS; i++)
    {
        client[i].disconnect = 0;
        client[i].generate = 0;
        client[i].online = 0;
        client[i].HELO = 0;
        client[i].DATA = 0;
        client[i].MAIL = 0;
        client[i].RCPT = 0;
        client[i].QUIT = 0;
        client[i].number_of_sub = 0;
        client[i].number_of_unsub = 0;
    }

    // for TCP client
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

    // UDP setup
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(0, argv[1], &hints, &bind_address);

    int socket_udp;
    socket_udp = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    DIE(socket_udp < 0, "socket_listen");

    ret = bind(socket_udp, bind_address->ai_addr, bind_address->ai_addrlen);
    DIE(ret < 0, "bind udp");
    freeaddrinfo(bind_address);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000;
    setsockopt(socket_udp, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
    FD_SET(sockfd, &read_fds);
    FD_SET(socket_udp, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    fdmax = sockfd;
    if (socket_udp > fdmax)
    {
        fdmax = socket_udp;
    }
    fdmax = socket_udp;
    int j = 0;
    int connfd[MAX_CLIENTS];

    int over = 0;
    while (1)
    {
        tmp_fds = read_fds;

        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, &tv);
        DIE(ret < 0, "select");

        for (i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &tmp_fds))
            {
                if (i == STDIN_FILENO)
                {
                    // se citeste de la stdin
                    memset(buffer, 0, sizeof(buffer));
                    n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
                    DIE(n < 0, "read");

                    if (strncmp(buffer, "exit", 4) == 0)
                    {
                        for (size_t demo = 0; demo < MAX_CLIENTS; demo++)
                        {
                            send(connfd[demo], buffer, BUFLEN, 0);
                            close(connfd[demo]);
                            FD_CLR(connfd[demo], &read_fds);
                        }
                        over = 1;
                        break;
                    }
                }
                else if (i == sockfd)
                {
                    clilen = sizeof(cli_addr);
                    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
                    DIE(newsockfd < 0, "accept");

                    FD_SET(newsockfd, &read_fds);
                    if (newsockfd > fdmax)
                    {
                        fdmax = newsockfd;
                    }

                    int check_overlap_socket = 0;
                    int position_of_overlap_socket = 0;
                    for (size_t count = 0; count < j; count++)
                    {
                        if (newsockfd == client[count].socket_of_client)
                        {
                            position_of_overlap_socket = count;
                            check_overlap_socket = 1;
                        }
                    }

                    memset(buffer, 0, BUFLEN);
                    strcpy(buffer, "220");
                    n = send(newsockfd, buffer, BUFLEN, 0);
                    DIE(n < 0, "send");
                    connfd[j] = newsockfd;
                    client[j].socket_of_client = newsockfd;
                    client[j].generate = 1;
                    client[j].online = 1;
                    j++;
                }
                else if (i == socket_udp)
                {
                    struct sockaddr_in client_address;
                    socklen_t client_len = sizeof(client_address);
                    char read[1551];
                    struct addrinfo demo_addr;
                    int bytes_received = recvfrom(socket_udp, read, 1551, 0, (struct sockaddr *)&client_address, &client_len);
                    if (bytes_received > 0)
                    {
                        char *ip_udp_client = (char *)malloc(sizeof(char) * 20);
                        ip_udp_client = inet_ntoa((struct in_addr)(client_address.sin_addr));

                        int len = snprintf(NULL, 0, "%d", client_address.sin_port);
                        char *port_udp_client = malloc(len + 1);
                        snprintf(port_udp_client, len + 1, "%d", ntohs(client_address.sin_port));

                        for (size_t current_client = 0; current_client < j; current_client++)
                        {
                            // check client online to send
                            if (client[current_client].online == 1)
                            {
                                int topic_ubsub = 0;
                                // check topic unsub
                                for (size_t current_topic = 0; current_topic < client[current_client].number_of_unsub; current_topic++)
                                {
                                    if (strstr(read, client[current_client].unsub[current_topic].topic) != NULL)
                                    {
                                        topic_ubsub = 1;
                                        break;
                                    }
                                }
                                if (topic_ubsub == 0)
                                {
                                    for (size_t current_topic = 0; current_topic < client[current_client].number_of_sub; current_topic++)
                                    {
                                        if (strstr(read, client[current_client].sub[current_topic].topic) != NULL)
                                        {
                                            process_payload_and_send(client[current_client].socket_of_client, read, ip_udp_client, port_udp_client);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                else
                {
                    memset(buffer, 0, BUFLEN);
                    n = recv(i, buffer, sizeof(buffer) - 1, 0);
                    DIE(n < 0, "recv");
                    char *temp_store;
                    const char delim[2] = " ";
                    int count = 0;
                    char topic[50];
                    char SF[2];

                    if (n == 0)
                    {
                        for (size_t l = 0; l < j; l++)
                        {
                            if (i == client[l].socket_of_client)
                            {
                                client[l].generate = 0;
                                client[l].HELO = 0;
                                client[l].DATA = 0;
                                client[l].MAIL = 0;
                                client[l].RCPT = 0;
                                client[l].QUIT = 0;
                                client[l].disconnect = 1;
                                client[l].online = 0;
                                printf("Client %s disconnected.\n", client[l].id_client);
                            }
                        }
                        close(i);
                        FD_CLR(i, &read_fds);
                    }
                    else
                    {
                        for (int l = 0; l < j; l++)
                        {
                            if (client[l].socket_of_client != sockfd)
                            {
                                if (client[l].generate == 1 && client[l].HELO == 0 && client[l].MAIL == 0 && client[l].RCPT == 0 && client[l].DATA == 0 && client[l].QUIT == 0 && strcmp(buffer, "HELO") == 0)
                                {
                                    strcpy(buffer, "250 helo");
                                    send(client[l].socket_of_client, buffer, BUFLEN, 0);
                                    client[l].HELO = 1;
                                }
                                else if (client[l].generate == 1 && client[l].HELO == 1 && client[l].MAIL == 0 && client[l].RCPT == 0 && client[l].DATA == 0 && client[l].QUIT == 0 && strstr(buffer, "MAIL") != NULL)
                                {
                                    char *token;
                                    const char s[2] = " ";
                                    token = strtok(buffer, s);
                                    int count = 0;
                                    char *id = (char *)malloc(sizeof(char) * 2);
                                    char *port = (char *)malloc(sizeof(char) * 10);
                                    char *ip = (char *)malloc(sizeof(char) * 15);
                                    while (token != NULL)
                                    {
                                        if (count == 1)
                                        {
                                            strcpy(port, token);
                                        }
                                        if (count == 2)
                                        {
                                            strcpy(ip, token);
                                        }
                                        if (count == 3)
                                        {
                                            strcpy(id, token);
                                        }
                                        token = strtok(NULL, s);
                                        count++;
                                    }
                                    int check_same_id = 0;
                                    int check_reconnect = 0;
                                    if (l == j - 1)
                                    {
                                        for (size_t count = 0; count < j - 1; count++)
                                        {
                                            if (strcmp(client[count].id_client, id) == 0 && client[count].online == 1)
                                            {
                                                client[l].generate = 0;
                                                client[l].HELO = 0;
                                                client[l].DATA = 0;
                                                client[l].MAIL = 0;
                                                client[l].RCPT = 0;
                                                client[l].QUIT = 0;
                                                check_same_id = 1;

                                                strcpy(buffer, "exit");
                                                send(client[l].socket_of_client, buffer, BUFLEN, 0);
                                                close(client[l].socket_of_client);
                                                FD_CLR(client[l].socket_of_client, &read_fds);

                                                fprintf(stderr , "Client %s already connected.\n", client[count].id_client);

                                
                                            }
                                            else if (strcmp(client[count].id_client, id) == 0 && client[count].online == 0 && client[count].disconnect == 1)
                                            {
                                                client[count].online = 1;
                                                client[count].disconnect = 0;
                                                client[count].generate = 1;
                                                client[count].HELO = 1;
                                                client[count].DATA = 1;
                                                client[count].MAIL = 1;
                                                client[count].RCPT = 0;
                                                client[count].QUIT = 0;

                                                client[count].socket_of_client = client[l].socket_of_client;
                                                check_same_id = 0;
                                                j--;
                                            }
                                        }
                                    }

                                    if (check_same_id == 0)
                                    {
                                        snprintf(client[l].id_client, 3, "%s", id);
                                        snprintf(client[l].ip, 15, "%s", ip);
                                        snprintf(client[l].port, 10, "%s", port);
                                        printf("New client %s connected from %s:%s\n", client[l].id_client, client[l].ip, client[l].port);
                                        strcpy(buffer, "250 ok");
                                        send(client[l].socket_of_client, buffer, BUFLEN, 0);
                                        client[l].MAIL = 1;
                                    }
                                    else
                                    {
                                        j--;
                                    }
                                }
                                else if (client[l].generate == 1 && client[l].HELO == 1 && client[l].MAIL == 1 && client[l].RCPT == 0 && client[l].DATA == 0 && client[l].QUIT == 0 && strcmp(buffer, "RCPT") == 0)
                                {
                                    strcpy(buffer, "250 accept");
                                    send(client[l].socket_of_client, buffer, BUFLEN, 0);
                                    client[l].RCPT = 1;
                                }
                                else if (client[l].generate == 1 && client[l].HELO == 1 && client[l].MAIL == 1 && client[l].RCPT == 1 && client[l].DATA == 0 && client[l].QUIT == 0 && strcmp(buffer, "DATA") == 0)
                                {
                                    strcpy(buffer, "354");
                                    send(client[l].socket_of_client, buffer, BUFLEN, 0);
                                    client[l].DATA = 1;
                                }
                                else if (client[l].generate == 1 && client[l].HELO == 1 && client[l].MAIL == 1 && client[l].RCPT == 1 && client[l].DATA == 1 && client[l].QUIT == 0)
                                {
                                    if (strstr(buffer, "subscribe ") != NULL)
                                    {
                                        temp_store = strtok(buffer, delim);
                                        int count = 0;

                                        while (temp_store != NULL)
                                        {
                                            if (count == 1)
                                            {
                                                strcpy(topic, temp_store);
                                            }
                                            if (count == 2)
                                            {
                                                strcpy(SF, temp_store);
                                            }
                                            temp_store = strtok(NULL, delim);
                                            count++;
                                        }

                                        int topic_already_exit = 0;
                                        for (size_t current_index = 0; current_index < client[l].number_of_sub; current_index++)
                                        {
                                            if (strcmp(topic, client[l].sub[current_index].topic) == 0)
                                            {
                                                topic_already_exit = 1;
                                                break;
                                            }
                                        }

                                        if (topic_already_exit == 0)
                                        {
                                            int index = client[l].number_of_sub;
                                            strcpy(client[l].sub[index].topic, topic);
                                            strcpy(client[l].sub[index].SF, SF);
                                            client[l].number_of_sub++;
                                        }
                                    }
                                    else if (strstr(buffer, "unsubscribe ") != NULL)
                                    {
                                        temp_store = strtok(buffer, delim);

                                        while (temp_store != NULL)
                                        {
                                            if (count == 1)
                                            {
                                                strcpy(topic, temp_store);
                                            }
                                            temp_store = strtok(NULL, delim);
                                            count++;
                                        }

                                        int topic_already_exit = 0;
                                        for (size_t current_index = 0; current_index < client[l].number_of_unsub; current_index++)
                                        {
                                            if (strcmp(topic, client[l].unsub[current_index].topic) == 0)
                                            {
                                                topic_already_exit = 1;
                                                break;
                                            }
                                        }

                                        if (topic_already_exit == 0)
                                        {
                                            int index = client[l].number_of_unsub;
                                            strcpy(client[l].unsub[index].topic, topic);
                                            client[l].number_of_unsub++;
                                        }

                                        for (size_t current_index = 0; current_index < client[l].number_of_sub; current_index++)
                                        {
                                            if (strcmp(topic, client[l].sub[current_index].topic) == 0)
                                            {
                                                strcpy(client[l].sub[current_index].topic, "unsub");
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        if (over == 1)
        {
            break;
        }
    }
    for (size_t i = 0; i < MAX_CLIENTS; i++)
    {
        close(client[i].socket_of_client);
        FD_CLR(client[i].socket_of_client, &read_fds);
    }

    close(socket_udp);
    close(newsockfd);
    close(sockfd);
    return 0;
}

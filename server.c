#define _XOPEN_SOURCE 500
#include <sys/inotify.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>

#define ever ;;

#define USERS 0
#define MEMORY 1
#define PROCESSES 2
#define LOAD 3
#define INOTIFY 4
#define FEATURES 5
#define NAMESZ 256
#define BUF_LEN (64 * (sizeof(struct inotify_event) + 256))
#define MSG_LEN (BUF_LEN+NAMESZ+256)
#define ADDR_LEN 20
#define MAXCLIENTS 100
#define error(msg) {perror(msg); exit(1);}

struct msg {
    char name[NAMESZ];
    char text[MSG_LEN];
    char features[FEATURES];
};

void free_client(int i);

int sock_fd;
int biggest;
int available = MAXCLIENTS;
fd_set master;
char port[10];
char names[MAXCLIENTS][NAMESZ];
int fds[MAXCLIENTS];
char addresses[MAXCLIENTS][ADDR_LEN];
char last_status[MAXCLIENTS][MSG_LEN];
char features[MAXCLIENTS][FEATURES];

void *get_addr(struct sockaddr *sa) {
    return &(((struct sockaddr_in *) sa)->sin_addr);
}

void *th_stdin(void *nvm) {
    //int numbytes;
    //struct msg buf;
    //strcpy(buf.name,client_name);
    //while(fgets(buf.text, TXTSZ, stdin) != NULL) {
    //strip newline
    //    char *pos;
    //    if ((pos=strchr(buf.text, '\n')) != NULL)*pos = '\0';
    //numbytes = send(sockfd,&buf,sizeof(struct msg),0);
    //    if(numbytes <= 0)error("disconnected");
    //}
    pthread_exit(NULL);
}

void conn() {
    int rv;
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // AF_INET -> force IPv4, AF_UNSPEC -> v4/v6
    hints.ai_socktype = SOCK_STREAM; //stream
    hints.ai_flags = AI_PASSIVE; // use any of host's network addresses

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) //get possible socket addresses
    error("getaddrinfo");

    for (p = servinfo; p != NULL; p = p->ai_next) {  //bind to first available
        if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }
        if (bind(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sock_fd);
            perror("bind");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "error: failed to bind socket\n");
        exit(1);
    }
    freeaddrinfo(servinfo);
    if (listen(sock_fd, 5)) error("listen");
}

void handle_reg() {
    //someone wants to connect!
    struct sockaddr remoteaddr;
    socklen_t addrlen = sizeof remoteaddr;
    int newfd = accept(sock_fd, &remoteaddr, &addrlen);
    if (newfd == -1) error("accept");
    if (available--) {
        FD_SET(newfd, &master); //add to "selected"
        if (newfd > biggest)biggest = newfd;
        int i;
        for (i = 0; i < MAXCLIENTS && fds[i] != -1; i++); //find id
        inet_ntop(AF_INET, get_addr(&remoteaddr), addresses[i], ADDR_LEN);
        fds[i] = newfd;
        strcpy(last_status[i], "Up\n");
    }
    else {
        close(newfd);
        printf("warning: no client slots available");
    }
}

void handle_msg(int fd) { //sth waiting
    struct msg buf;
    ssize_t nbytes;
    if ((nbytes = recv(fd, &buf, sizeof(struct msg), 0)) == -1) error("recv");
    int i;
    for (i = 0; i < MAXCLIENTS && fds[i] != fd; i++); //find id
    if (nbytes <= 0) {
        //disconnected
        close(fd);
        FD_CLR(fd, &master);
        strcpy(last_status[i], "Down\n");
        for (int f = 0; i < FEATURES; i++)features[i][f] = 0;
    }
    else {
        //received client msg
        strcpy(names[i], buf.name);
        strcpy(last_status[i], buf.text);
        strcpy(features[i], buf.features);
    }
}

void quit(int nvm) {
    for (int i = 0; i <= biggest; i++)
        if (FD_ISSET(i, &master))close(i);
    exit(0);
}

void init() {
    for (int i = 0; i < MAXCLIENTS; i++)free_client(i);
}

void free_client(int i) {
    fds[i] = -1;
    strcpy(names[i], "");
    strcpy(last_status[i], "");
    strcpy(addresses[i], "");
    for (int f = 0; f < FEATURES; f++)features[i][f] = 0;
    available++;
}
int main(int argc, char *argv[]) {
    struct timeval mtv, tv;
    mtv.tv_sec = 2;
    mtv.tv_usec = 0;
    //parse args
    strcpy(port, argv[1]);
    signal(SIGINT, quit);
    printf("Ninety per cent of most magic merely consists of knowing one extra fact.\n");
    init();
    conn();
    FD_ZERO(&master);
    FD_SET(sock_fd, &master);
    biggest = sock_fd;
    int on_next = 0;
    fd_set readfds;
    if (argc != 2) {
        printf("usage: %s port\n", argv[0]);
        return 0;
    }
    for (ever) {
        readfds = master;
        tv = mtv;
        select(biggest + 1, &readfds, NULL, NULL, &tv);
        for (int i = 0; i <= biggest; i++)
            if (FD_ISSET(i, &readfds)) {
                if (i == sock_fd)handle_reg();
                else handle_msg(i);
            }
        if (!on_next && tv.tv_sec != 0)on_next = 1;
        else {
            system("clear");
            for (int i = 0; i < MAXCLIENTS; i++) {
                if (fds[i] != -1) {
                    printf("\n%s %s %s", addresses[i], names[i], last_status[i]);
                    if (!strcmp(last_status[i], "Down\n"))free_client(i);
                }
            }
            on_next = 0;
        }
    }

    return 0;
}

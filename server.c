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
#include <termios.h>

#define ever ;;

#define USERS 0
#define MEMORY 1
#define PROCESSES 2
#define LOAD 3
#define INOTIFY 4
#define FEATURES 5
#define NAMESZ 256
#define BUF_LEN (32 * (sizeof(struct inotify_event) + 256))
#define MSG_LEN (BUF_LEN+NAMESZ+256)
#define ADDR_LEN 20
#define MAXCLIENTS 100
#define error(msg) {perror(msg); exit(1);}

struct msg {
    char name[NAMESZ];
    char text[MSG_LEN];
    char features[FEATURES];
};

struct fmsg {
    char feature;
    char path[BUF_LEN];
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
static struct termios oldt;

void *get_addr(struct sockaddr *sa) {
    return &(((struct sockaddr_in *) sa)->sin_addr);
}

void conn() {
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // AF_INET -> force IPv4, AF_UNSPEC -> v4/v6
    hints.ai_socktype = SOCK_STREAM; //stream
    hints.ai_flags = AI_PASSIVE; // use any of host's network addresses

    if ((getaddrinfo(NULL, port, &hints, &servinfo)) != 0) //get possible socket addresses
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
        for (int f = 0; f < FEATURES; f++)features[i][f] = buf.features[f];
    }
}

void set_stdin() {
    static struct termios newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON); //no stdin buffering to return, select asap
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

void restore_stdin() {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

void quit() {
    for (int i = 0; i <= biggest; i++)
        if (FD_ISSET(i, &master))close(i);
    restore_stdin();
    exit(0);
}

void init() {
    for (int i = 0; i < MAXCLIENTS; i++)free_client(i);
    set_stdin();
}

void free_client(int i) {
    fds[i] = -1;
    strcpy(names[i], "");
    strcpy(last_status[i], "");
    strcpy(addresses[i], "");
    for (int f = 0; f < FEATURES; f++)features[i][f] = 0;
    available++;
}

void handle_stdin() {
    char command[10];
    getchar();
    system("clear");
    printf("\nEnter client number: ");
    if (!fgets(command, 10, stdin))return;
    int i = atoi(command);
    if (i < 0 || i >= MAXCLIENTS || fds[i] == -1)return;
    printf("\nAvailable features (%d): \n", i);
    if (features[i][USERS])printf("u: Number of users\n");
    if (features[i][MEMORY])printf("m: Memory\n");
    if (features[i][PROCESSES])printf("p: Running processes\n");
    if (features[i][LOAD])printf("l: Load avg\n");
    if (features[i][INOTIFY])printf("i: Directory changes\n");
    int c = getchar();
    struct fmsg buf;
    strcpy(buf.path, "");
    switch (c) {
        case 'u':
            buf.feature = USERS;
            break;
        case 'm':
            buf.feature = MEMORY;
            break;
        case 'p':
            buf.feature = PROCESSES;
            break;
        case 'l':
            buf.feature = LOAD;
            break;
        case 'i':
            buf.feature = INOTIFY;
            printf("\nEnter directory path: ");
            if (!fgets(buf.path, BUF_LEN, stdin))return;
            char *pos;
            if ((pos = strchr(buf.path, '\n')) != NULL)*pos = '\0'; //strip newline
            break;
        default:
            return;
    }
    send(fds[i], &buf, sizeof(buf), 0);
}

int main(int argc, char *argv[]) {
    struct timeval mtv, tv;
    mtv.tv_sec = 2;
    mtv.tv_usec = 0;
    //parse args
    strcpy(port, argv[1]);
    quit();
    printf("Ninety per cent of most magic merely consists of knowing one extra fact.\n");
    init();
    conn();
    FD_ZERO(&master);
    FD_SET(sock_fd, &master);
    FD_SET(STDIN_FILENO, &master);
    biggest = sock_fd;
    volatile int on_next = 0;
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
                else if (i == STDIN_FILENO)handle_stdin();
                else handle_msg(i);
            }
        if (!on_next && tv.tv_sec != 0)on_next = 1;
        else {
            system("clear");
            printf("Press any key to set client features.\n");
            for (int i = 0; i < MAXCLIENTS; i++) {
                if (fds[i] != -1) {
                    printf("\n%d. %s %s %s", i, addresses[i], names[i], last_status[i]);
                    if (!strcmp(last_status[i], "Down\n"))free_client(i);
                }
            }
            on_next = 0;
        }
    }

    return 0;
}

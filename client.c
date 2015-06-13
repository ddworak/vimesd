#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <utmp.h>
#include <dirent.h>
#include <string.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <ctype.h>
#include <netdb.h>
#include <arpa/inet.h>

#define ever ;;

#define USERS 0
#define MEMORY 1
#define PROCESSES 2
#define LOAD 3
#define INOTIFY 4
#define FEATURES 5
#define NAMESZ 256
#define BUF_LEN 4096
#define MSG_LEN (BUF_LEN+NAMESZ+256)
#define error(msg) {perror(msg); exit(1);}

char features[FEATURES];
char subscribed[FEATURES];

int inotify_fd, sock_fd;
char name[NAMESZ];

struct msg {
    char name[NAMESZ];
    char text[MSG_LEN];
    char features[FEATURES];
};

struct fmsg {
    char feature;
    char path[BUF_LEN];
};

//format info from inotify_event structure
char *strinotify(struct inotify_event* i) {
    char *info = (char *) malloc(512);
    strcpy(info, "");
    if (i->len > 0)strcat(info, i->name);
    strcat(info, " change: ");
    if (i->mask & IN_ACCESS) strcat(info, "IN_ACCESS");
    if (i->mask & IN_ATTRIB) strcat(info, "IN_ATTRIB");
    if (i->mask & IN_CLOSE_NOWRITE) strcat(info, "IN_CLOSE_NOWRITE");
    if (i->mask & IN_CLOSE_WRITE) strcat(info, "IN_CLOSE_WRITE");
    if (i->mask & IN_CREATE) strcat(info, "IN_CREATE");
    if (i->mask & IN_DELETE) strcat(info, "IN_DELETE");
    if (i->mask & IN_DELETE_SELF) strcat(info, "IN_DELETE_SELF");
    if (i->mask & IN_IGNORED) strcat(info, "IN_IGNORED");
    if (i->mask & IN_ISDIR) strcat(info, "IN_ISDIR");
    if (i->mask & IN_MODIFY) strcat(info, "IN_MODIFY");
    if (i->mask & IN_MOVE_SELF) strcat(info, "IN_MOVE_SELF");
    if (i->mask & IN_MOVED_FROM) strcat(info, "IN_MOVED_FROM");
    if (i->mask & IN_MOVED_TO) strcat(info, "IN_MOVED_TO");
    if (i->mask & IN_OPEN) strcat(info, "IN_OPEN");
    if (i->mask & IN_Q_OVERFLOW) strcat(info, "IN_Q_OVERFLOW");
    if (i->mask & IN_UNMOUNT) strcat(info, "IN_UNMOUNT");
    strcat(info, "\n");
    return info;
}

//add file to watched
int watch_directory(char *path) {
    struct stat s;
    if (stat(path, &s) != 0 || !S_ISDIR(s.st_mode))
        return -1;
    return inotify_add_watch(inotify_fd, path, IN_ALL_EVENTS);
}

//check file events
char *check_file_events() {
    char buf[BUF_LEN];
    char *res = (char *) malloc(BUF_LEN);
    strcpy(res, "");
    struct inotify_event *event;
    ssize_t nread = read(inotify_fd, buf, BUF_LEN);
    if (nread == -1 && errno != EAGAIN) error("read from inotify");
    if (nread <= 0)return res;

    //process events in buffer
    for (char *p = buf; p < buf + nread;) {
        event = (struct inotify_event *) p;
        char *info = strinotify(event);
        strcat(res, info);
        free(info);
        p += sizeof(struct inotify_event) + event->len;
    }
    return res;
}

//number of running processes
unsigned long processes_total() {
    int digits_only(char *str) {
        if (strlen(str) < 1)return 0;
        while (*str) {
            if (isdigit(*str++) == 0) return 0;
        }
        return 1;
    }
    unsigned long processes = 0;
    struct dirent *ent;
    DIR *dir = opendir("/proc");
    if (dir) {
        while ((ent = readdir(dir)))
            if (digits_only(ent->d_name))processes++;
        closedir(dir);
    }
    else {
        perror("opendir");
        features[PROCESSES] = 0;
        return 0;
    }
    return processes;
}

//number of logged in users
unsigned long users_total() {
    unsigned long users = 0;
    struct utmp usr;
    FILE *ufp = fopen(_PATH_UTMP, "r");
    if (!ufp) {
        features[USERS] = 0;
        return 0;
    }
    while (fread((char *) &usr, sizeof(usr), 1, ufp) == 1)
        if (*usr.ut_name && *usr.ut_line && *usr.ut_line != '~')
            users++;
    return users - 1; //one line is "LOGIN"
}

//total memory size
unsigned long long mem_total() {
    unsigned long long pages = (unsigned long long int) sysconf(_SC_PHYS_PAGES);
    unsigned long long page_size = (unsigned long long int) sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}

//available memory size
unsigned long long mem_available() {
    unsigned long long pages = (unsigned long long int) sysconf(_SC_AVPHYS_PAGES);
    unsigned long long page_size = (unsigned long long int) sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}

//load avg in 1s
double load_avg() {
    FILE *fp = fopen("/proc/stat", "r");
    long double a[4], b[4];
    fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &a[0], &a[1], &a[2], &a[3]);
    fclose(fp);
    sleep(1);
    fp = fopen("/proc/stat", "r");
    fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &b[0], &b[1], &b[2], &b[3]);
    fclose(fp);
    //return working ticks in 1s / total ticks in 1s
    return (double) (
            ((b[0] + b[1] + b[2]) - (a[0] + a[1] + a[2])) /
            ((b[0] + b[1] + b[2] + b[3]) - (a[0] + a[1] + a[2] + a[3])));
}

void status_msg(char *msg) {
    strcpy(msg, " ");
    char temp[512];
    if (subscribed[USERS]) {
        sprintf(temp, "Users: %lu ", users_total());
        strcat(msg, temp);
    }
    if (subscribed[MEMORY]) {
        unsigned long long t = mem_total();
        unsigned long long a = mem_available();
        sprintf(temp, "Total memory: %.3lfM Available memory: %.3lfM %.2lf%% used ", (double) t / 1000000,
                (double) a / 1000000, 100 * (double) (t - a) / t);
        strcat(msg, temp);
    }
    if (subscribed[PROCESSES]) {
        sprintf(temp, "Processes: %lu ", processes_total());
        strcat(msg, temp);
    }
    if (subscribed[LOAD]) {
        sprintf(temp, "Load %lf ", load_avg());
        strcat(msg, temp);
    }
    if (subscribed[INOTIFY]) {
        char *t = check_file_events();
        strcat(msg, t);
        free(t);
    }
}

void init() {
    for (int i = 0; i < FEATURES; i++) {
        features[i] = 1; //presume all available
        subscribed[i] = 0;
    }
    inotify_fd = inotify_init(); //inotify instance
    if (inotify_fd == -1) {
        features[INOTIFY] = 0;
        printf("No inotify available\n");
    }
    else fcntl(inotify_fd, F_SETFL, O_NONBLOCK); //set to non-blocking
}

void *get_addr(struct sockaddr *sa) {
    return &(((struct sockaddr_in *) sa)->sin_addr);
}

void conn(char *host, char *port) {
    //internet socket
    int rv;
    char s[1000];
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
    for (p = servinfo; p != NULL; p = p->ai_next) { //loop through suitable results and make a socket
        if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }
        if (connect(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sock_fd);
            perror("connect");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "connection failed\n");
        exit(1);
    }
    inet_ntop(p->ai_family, get_addr(p->ai_addr), s, sizeof s);
    printf("info: connected to %s\n", s);
    freeaddrinfo(servinfo);
}

void handle_command(struct fmsg fmsg) {
    if (fmsg.feature == INOTIFY) if (watch_directory(fmsg.path) == -1) {
        perror("inotify_add_watch");
        return;
    }
    subscribed[(int) fmsg.feature] = 1;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("usage: %s name hostname port\n", argv[0]);
        return 0;
    }
    struct timeval mtv, tv;
    mtv.tv_sec = 2;
    mtv.tv_usec = 0;
    fd_set master, readfds;
    ssize_t nbytes;
    //parse args
    strcpy(name, argv[1]);
    printf("We who think we are about to die will laugh at anything.\n");
    init();
    conn(argv[2], argv[3]);
    FD_ZERO(&master);
    FD_SET(sock_fd, &master);
    for (ever) {
        struct msg buf;
        strcpy(buf.name, name);
        status_msg(buf.text);
        for (int i = 0; i < FEATURES; i++) {
            buf.features[i] = features[i];
        }
        nbytes = send(sock_fd, &buf, sizeof(struct msg), 0);
        if (nbytes <= 0) error("disconnected");
        readfds = master;
        tv = mtv;
        select(sock_fd + 1, &readfds, NULL, NULL, &tv);
        if (FD_ISSET(sock_fd, &readfds)) {
            struct fmsg rbuf;
            if (recv(sock_fd, &rbuf, sizeof(struct fmsg), MSG_WAITALL) <= 0) {
                printf("Disconnected\n");
                return 0;
            }
            handle_command(rbuf);
        }
    }

    return 0;
}


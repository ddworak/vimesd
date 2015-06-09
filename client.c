#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <utmp.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <errno.h>

#define USERS 0
#define MEMORY 1
#define PROCESSES 2
#define LOAD 3
#define INOTIFY 4
#define FEATURES 5
#define error(msg) {perror(msg); exit(1);}
#define BUF_LEN (10 * (sizeof(struct inotify_event) + 256))

int features[FEATURES];
int subscribed[FEATURES];

int inotify_fd;


//format info from inotify_event structure
char *strinotify(struct inotify_event *i) {
    char *info = (char *) malloc(512);
    strcpy(info, "File ");
    if (i->len > 0)strcat(info, i->name);
    strcat(info, " changed:");
    if (i->mask & IN_ACCESS) strcpy(info, " IN_ACCESS");
    if (i->mask & IN_ATTRIB) strcpy(info, " IN_ATTRIB");
    if (i->mask & IN_CLOSE_NOWRITE) strcpy(info, " IN_CLOSE_NOWRITE");
    if (i->mask & IN_CLOSE_WRITE) strcpy(info, " IN_CLOSE_WRITE");
    if (i->mask & IN_CREATE) strcpy(info, " IN_CREATE");
    if (i->mask & IN_DELETE) strcpy(info, " IN_DELETE");
    if (i->mask & IN_DELETE_SELF) strcpy(info, " IN_DELETE_SELF");
    if (i->mask & IN_IGNORED) strcpy(info, " IN_IGNORED");
    if (i->mask & IN_ISDIR) strcpy(info, " IN_ISDIR");
    if (i->mask & IN_MODIFY) strcpy(info, " IN_MODIFY");
    if (i->mask & IN_MOVE_SELF) strcpy(info, " IN_MOVE_SELF");
    if (i->mask & IN_MOVED_FROM) strcpy(info, " IN_MOVED_FROM");
    if (i->mask & IN_MOVED_TO) strcpy(info, " IN_MOVED_TO");
    if (i->mask & IN_OPEN) strcpy(info, " IN_OPEN");
    if (i->mask & IN_Q_OVERFLOW) strcpy(info, " IN_Q_OVERFLOW");
    if (i->mask & IN_UNMOUNT) strcpy(info, " IN_UNMOUNT");
    strcat(info, "\n");
    return info;
}

//add file to watched
int watch_file(char *path) {
    return inotify_add_watch(inotify_fd, path, IN_ALL_EVENTS);
}

//check events
void read_file_events() {
    char buf[BUF_LEN];
    struct inotify_event *event;
    ssize_t nread = read(inotify_fd, buf, BUF_LEN);
    if (nread == -1 && errno != EAGAIN) error("read from inotify");

    printf("Read %ld bytes from inotify fd\n", (long) nread);

    /* Process all of the events in buffer returned by read() */

    for (char *p = buf; p < buf + nread;) {
        event = (struct inotify_event *) p;
        char *info = strinotify(event);
        printf("%s", info);
        free(info);
        p += sizeof(struct inotify_event) + event->len;
    }
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
        return 0;
    }
    return processes;
}

//number of logged in users
unsigned long users_total() {
    unsigned long users = 0;
    struct utmp usr;
    FILE *ufp = fopen(_PATH_UTMP, "r");
    if (!ufp) error("fopen");
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

void init() {
    for (int i = 0; i < FEATURES; i++) {
        features[i] = 1; //presume all available
        subscribed[i] = 0;
    }
    inotify_fd = inotify_init(); //inotify instance
    if (inotify_fd == -1)
        features[INOTIFY] = 0;
    else {
        //set to non-blocking
        int flags = fcntl(inotify_fd, F_GETFL);
        fcntl(inotify_fd, F_SETFL, flags & ~O_NONBLOCK);
    }
}

int main() {
    printf("We who think we are about to die will laugh at anything.\n");
    printf("P: %lu U: %lu Load: %lf Total mem: %llu Available: %llu %lf", processes_total(), users_total(), load_avg(),
           mem_total(), mem_available(), (double) mem_available() / mem_total());
    return 0;
}
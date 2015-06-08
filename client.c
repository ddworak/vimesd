#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <utmp.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>

#define error(msg) {perror(msg); exit(1);}

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

unsigned long users_total() {
    unsigned long users = 0;
    struct utmp usr;
    FILE *ufp = fopen(_PATH_UTMP, "r");
    if (!ufp) error("fopen");
    while (fread((char *) &usr, sizeof(usr), 1, ufp) == 1)
        if (*usr.ut_name && *usr.ut_line && *usr.ut_line != '~')
            users++;
    return users;
}

size_t mem_total() {
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return (size_t) (pages * page_size);
}

size_t mem_available() {
    long pages = sysconf(_SC_AVPHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return (size_t) (pages * page_size);
}

long double load_avg() {
    FILE *fp = fopen("/proc/stat", "r");
    long double a[4], b[4];
    fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &a[0], &a[1], &a[2], &a[3]);
    fclose(fp);
    sleep(1);
    fp = fopen("/proc/stat", "r");
    fscanf(fp, "%*s %Lf %Lf %Lf %Lf", &b[0], &b[1], &b[2], &b[3]);
    fclose(fp);
    //return working ticks in 1s / total ticks in 1s
    return ((b[0] + b[1] + b[2]) - (a[0] + a[1] + a[2])) / ((b[0] + b[1] + b[2] + b[3]) - (a[0] + a[1] + a[2] + a[3]));
}

int main() {
    printf("We who think we are about to die will laugh at anything.\n");
    return 0;
}
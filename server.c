#define _XOPEN_SOURCE 500

#include <sys/inotify.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>

#define USERS 0
#define MEMORY 1
#define PROCESSES 2
#define LOAD 3
#define INOTIFY 4
#define FEATURES 5
#define NAMESZ 256
#define BUF_LEN (64 * (sizeof(struct inotify_event) + 256))
#define MSG_LEN (BUF_LEN+NAMESZ+256)
#define error(msg) {perror(msg); exit(1);}

struct reg_msg {
    char name[NAMESZ];
    char features[FEATURES];
};

struct info_msg {
    char name[NAMESZ];
    char msg[MSG_LEN];
};

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

int main() {
    printf("Ninety per cent of most magic merely consists of knowing one extra fact.\n");
    return 0;
}
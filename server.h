#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h> //for threading , link with lpthread
#include <semaphore.h>
#include <libwebsockets.h>

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
#define OUT_BUFFER_SIZE 1024
#define IN_BUFFER_SIZE 1024
#define CLIENT_NUM 24

typedef struct clients_s {
  struct lws* client;
  char inbuf[IN_BUFFER_SIZE];
  char outbuf[OUT_BUFFER_SIZE];
  int filedes[2];
  struct  clients_t* next;
} clients_t;

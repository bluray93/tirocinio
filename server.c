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

char *builtin_str[]={"cd","help","exit"};
int func=0; //used to send welcome message
sem_t client_data_sem;
sem_t empty_sem;
clients_t* clients = NULL;

int lsh_cd(char **args, clients_t* aux);
int lsh_help(char **args, clients_t* aux);
int lsh_exit(char **args, clients_t* aux);

int (*builtin_func[])(char **,clients_t*)={&lsh_cd,&lsh_help,&lsh_exit};

int lsh_num_builtins(){
  return sizeof(builtin_str) / sizeof(char *);
}

int lsh_cd(char **args, clients_t* aux){
  printf("cd function\n");
  if (args[1] == NULL) {
    strcpy(aux->outbuf,"lsh: expected argument to \"cd\"\n");
  }
	else{
    if (chdir(args[1]) != 0) {
      strcpy(aux->outbuf,"No such file or directory");
    }
  }
  sem_post(&empty_sem);
  return 1;
}

int lsh_help(char **args, clients_t* aux){
  int i;
  strcpy(aux->outbuf,"Type program names and arguments, and hit enter. \n");
  strcat(aux->outbuf,"The following are built in: \n");
  for (i = 0; i < lsh_num_builtins(); i++){
    strcat(aux->outbuf, builtin_str[i]);
		strcat(aux->outbuf, " ");
  }
  strcat(aux->outbuf,"Use the man command for information on other programs.\n");
  sem_post(&empty_sem);
  return 1;
}

int lsh_exit(char **args, clients_t* aux){ //incompleta (invece di chiudere il programma ristrutturare la lista)
  sem_post(&empty_sem);
  return 0;
}

int lsh_launch(char **args, clients_t* aux){
  printf("launch function\n");
  func=1;
  pid_t pid;
  int status;
  if (pipe(aux->filedes) == -1) {
    perror("pipe");
    exit(1);
  }
  pid = fork();
  if (pid == 0) {  //child
    while ((dup2(aux->filedes[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {}
    close(aux->filedes[0]);
    close(aux->filedes[1]);
    if (execvp(args[0], args) == -1) {
      perror("lsh");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) { //error
    perror("lsh");
  } else { //parent
    do {
      close(aux->filedes[1]);
      //waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }
  printf("post\n");
  sem_post(&empty_sem);
  return 1;
}

int lsh_execute(char **args,clients_t* aux){
  printf("execute\n");
  int i;
  if (args[0] == NULL) {
    return 1;
  }
  for (i = 0; i < lsh_num_builtins(); i++) {
    if (strcmp(args[0], builtin_str[i]) == 0) {
      return (*builtin_func[i])(args, aux);
    }
  }
  printf("altro\n");
  return lsh_launch(args,aux);
}

char **lsh_split_line(char *line){
  int bufsize = IN_BUFFER_SIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token, **tokens_backup;
  if (!tokens) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }
  token = strtok(line, LSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;
    if (position >= bufsize) {
      bufsize += LSH_TOK_BUFSIZE;
      tokens_backup = tokens;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
		free(tokens_backup);
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
    token = strtok(NULL, LSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

void pipe_to_buff(clients_t* aux){
  //printf("pipe function\n");
  //printf("wait\n");
  //sem_wait(&empty_sem);
  while (1) {
    ssize_t count = read(aux->filedes[0], aux->outbuf, OUT_BUFFER_SIZE);
    if (count == -1) {
      if (errno == EINTR) {
        continue;
      } else {
        perror("read");
        exit(1);
      }
    } else if (count == 0) {
      break;
    }
  }
  close(aux->filedes[0]);
  //func =0;
  //printf("post\n");
  //sem_post(&empty_sem);
}

struct client_t* clients_func(struct lws* wsi){
  clients_t* aux = clients;
  if( clients == NULL){
    clients = (clients_t*)calloc(1,sizeof(clients_t));
    clients -> client = wsi;
    clients -> next =NULL;
    strcpy((clients->outbuf),"Welcome to websocket terminal\0");
    return clients;
  }
  else{
    while((aux-> next)!=NULL){
      if((aux->client)==wsi){
        return aux;
      }
      aux=aux->next;
    }
    aux->next=(clients_t*)malloc(sizeof(clients_t));
    aux=aux->next;
    aux -> client = wsi;
    aux -> next =NULL;
    strcpy((clients->outbuf),"Welcome to websocket terminal\0");
    return aux;
  }
}

void thread_func(void* args) {
  //session_thread_args_t* arg = (session_thread_args_t*)args;
  printf("thread\n");
  clients_t* aux = (clients_t*)args;
  char ** tmp = lsh_split_line(aux->inbuf);
  lsh_execute(tmp,aux);
  free(tmp);
  pthread_exit(NULL);
}

static int callback_http( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len ){
  switch( reason ){
		case LWS_CALLBACK_HTTP:
			lws_serve_http_file( wsi, "index.html", "text/html", NULL, 0 );
			break;
		default:
			break;
	}
	return 0;
}

static int callback_example( struct lws* wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len ){
  //sem_wait(&client_data_sem);
  clients_t* aux = clients_func(wsi);
  //sem_post(&client_data_sem);
  int ret;
  switch( reason ){
		case LWS_CALLBACK_ESTABLISHED:{
	    printf("connection established\n");
			lws_callback_on_writable( wsi );
	    break;}
		case LWS_CALLBACK_RECEIVE:
      sem_wait(&empty_sem);
			printf("received data: %s size %d \n", (unsigned char *)(in), (int) len);
			strcpy((aux->inbuf),(char *)(in));
			strcat((aux->inbuf),"\0");
      pthread_t thread;
      printf("before create\n");
      ret = pthread_create(&thread, NULL, thread_func, (void*)aux);
      ret = pthread_detach(thread);
			lws_callback_on_writable( wsi );
			break;
		case LWS_CALLBACK_SERVER_WRITEABLE:{
      printf("wait\n");
      sem_wait(&empty_sem);
      if(func==1) pipe_to_buff(aux);
      printf("%s\n", aux->outbuf);
			printf("send outbuf: %s\n", aux->outbuf);
			lws_write( aux->client, aux->outbuf, OUT_BUFFER_SIZE, LWS_WRITE_TEXT ); //add LWS_PRE to buffer
			memset(aux->outbuf, 0, OUT_BUFFER_SIZE);
      printf("post\n");
      sem_post(&empty_sem);
			break;}
		case LWS_CALLBACK_CLOSED: {
	    printf("connection closed \n");
	  break;}
		default:
			break;
	}

	return 0;
}

static struct lws_protocols protocols[] = {
    {
      "http-only",
      callback_http,
      0,
    },
		{
			"example-protocol",
			callback_example,
			0
		},
    {NULL, NULL, 0}
};

enum protocols{
	PROTOCOL_EXAMPLE = 0,
	PROTOCOL_COUNT
};

int main(void) {
  // server url will be http://localhost:8000
  int port = 8000;
  const char *interface = NULL;
  int opts = 0;
  struct lws_context_creation_info info;
  struct lws_context* context;
  sem_init(&client_data_sem,0,1);
  sem_init(&empty_sem,0,1);
  memset(&info, 0, sizeof info);
  info.port = port;
  info.iface = interface;
  info.protocols = protocols;
  info.ssl_cert_filepath = NULL;
  info.ssl_private_key_filepath = NULL;
  info.gid = -1;
  info.uid = -1;
  info.options = opts;
  context = lws_create_context(&info);
  if (context == NULL) {
   fprintf(stderr, "lws init failed\n");
   return -1;
  }
  printf("starting server...\n");
  while (1) {
    lws_service(context, 50);
  }
  lws_context_destroy(context);
  return 0;
}

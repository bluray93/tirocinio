#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h> //for threading , link with lpthread
#include <libwebsockets.h>

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
#define OUT_BUFFER_SIZE 1024
#define LSH_RL_BUFSIZE 1024
#define THREAD_NUM 24
char *buffer;
char *outbuf;
char *builtin_str[]={"cd","help","exit"};
int filedes[2];
int func=0;
struct lws_context* context[THREAD_NUM];

typedef struct session_thread_args_s {
    int i;
    void * info;
} session_thread_args_t;

int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);

int (*builtin_func[])(char **)={&lsh_cd,&lsh_help,&lsh_exit};

int lsh_num_builtins(){
  return sizeof(builtin_str) / sizeof(char *);
}

int lsh_cd(char **args){
  printf("cd function\n");
  if (args[1] == NULL) {
    strcpy(outbuf,"lsh: expected argument to \"cd\"\n");
  }
	else{
    if (chdir(args[1]) != 0) {
      strcpy(outbuf,"No such file or directory");
    }
  }
  return 1;
}

int lsh_help(char **args){
  int i;
  strcpy(outbuf,"Type program names and arguments, and hit enter. \n");
  strcat(outbuf,"The following are built in: \n");
  for (i = 0; i < lsh_num_builtins(); i++){
    strcat(outbuf, builtin_str[i]);
		strcat(outbuf, " ");
  }
  strcat(outbuf,"Use the man command for information on other programs.\n");
  return 1;
}

int lsh_exit(char **args){
  return 0;
}

int lsh_launch(char **args){
  printf("launch function\n");
  func=1;
  pid_t pid;
  int status;
  if (pipe(filedes) == -1) {
    perror("pipe");
    exit(1);
  }
  pid = fork();
  if (pid == 0) {  //child
    while ((dup2(filedes[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {}
    close(filedes[0]);
    close(filedes[1]);
    if (execvp(args[0], args) == -1) {
      perror("lsh");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) { //error
    perror("lsh");
  } else { //parent
    do {
      close(filedes[1]);
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }
  return 1;
}

int lsh_execute(char **args){
  int i;
  if (args[0] == NULL) {
    return 1;
  }
  for (i = 0; i < lsh_num_builtins(); i++) {
    if (strcmp(args[0], builtin_str[i]) == 0) {
      return (*builtin_func[i])(args);
    }
  }
  return lsh_launch(args);
}

char **lsh_split_line(char *line){
  int bufsize = LSH_TOK_BUFSIZE, position = 0;
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

void pipe_to_buff(){
  printf("pipe function\n");
  while (1) {
    ssize_t count = read(filedes[0], outbuf, OUT_BUFFER_SIZE);
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
  close(filedes[0]);
  func =0;
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

static int callback_example( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len ){
	switch( reason ){
		case LWS_CALLBACK_ESTABLISHED:{
	    printf("connection established\n");
			lws_callback_on_writable( wsi );
	    break;}
		case LWS_CALLBACK_RECEIVE:
			printf("received data: %s size %d \n", (unsigned char *)(in), (int) len);
			strcpy(buffer,(char *)(in));
			strcat(buffer,"\n");
	    char ** args = lsh_split_line(buffer);
	    lsh_execute(args);
	    free(args);
			lws_callback_on_writable( wsi );
			break;
		case LWS_CALLBACK_SERVER_WRITEABLE:{
      if(func==1) pipe_to_buff();
			printf("send outbuf: %s size %d \n", outbuf, sizeof(outbuf));
			lws_write( wsi, outbuf, OUT_BUFFER_SIZE, LWS_WRITE_TEXT );
			memset(outbuf, 0, OUT_BUFFER_SIZE);
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

static int context_create(void *arg){
  printf("context_create\n");
  session_thread_args_t* args = (session_thread_args_t*)arg;
  int i = args->i;
  void* info = args->info;
  context[i] = lws_create_context(info);
  if (context == NULL) {
   fprintf(stderr, "lws init failed\n");
   return -1;
  }
  printf("starting server...\n");
  strcpy(outbuf,"Welcome to websocket terminal\n");
  while (1) {
    lws_service(context[i], 50);
  }
  lws_context_destroy(context[i]);
}

int main(void) {
  // server url will be http://localhost:8000
  int port = 8000;
  const char *interface = NULL;
  int opts = 0;
  struct lws_context_creation_info info;
  memset(&info, 0, sizeof info);
  info.port = port;
  info.iface = interface;
  info.protocols = protocols;
  info.ssl_cert_filepath = NULL;
  info.ssl_private_key_filepath = NULL;
  info.gid = -1;
  info.uid = -1;
  info.options = opts;
  int i=0;
  buffer = malloc(sizeof(char) * LSH_RL_BUFSIZE);
  outbuf = malloc(sizeof(char) * OUT_BUFFER_SIZE);
  //printf("qualcosa\n");
  pthread_t client_thread;
  //pthread_t client2_thread;

  session_thread_args_t* args1 = (session_thread_args_t*)malloc(sizeof(session_thread_args_t));
  args1->i = i;
  args1->info = &info;


  if(pthread_create( &client_thread , NULL ,  context_create , args1)){
    perror("could not create thread");
    return 1;
  }

  /*i=1;
  session_thread_args_t* args2 = (session_thread_args_t*)malloc(sizeof(session_thread_args_t));
  args2->i = i;
  args2->info = &info;
  if(pthread_create( &client2_thread , NULL ,  context_create , args2)){
    perror("could not create thread");
    return 1;
  }*/

  if(pthread_join(&client_thread, NULL)) {
    fprintf(stderr, "Error joining thread\n");
  }
  /*if(pthread_join(&client2_thread, NULL)) {
    fprintf(stderr, "Error joining thread\n");
  }*/

  return 0;
}

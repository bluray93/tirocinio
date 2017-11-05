#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <libwebsockets.h>

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
#define EXAMPLE_RX_BUFFER_BYTES 1024
#define LSH_RL_BUFSIZE 1024
char *buffer;
char *outbuf;
char *builtin_str[]={"cd","help","exit"};

int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);

int (*builtin_func[])(char **)={&lsh_cd,&lsh_help,&lsh_exit};

int lsh_num_builtins(){
  return sizeof(builtin_str) / sizeof(char *);
}

int lsh_cd(char **args){
  if (args[1] == NULL) {
    strcpy(outbuf,"lsh: expected argument to \"cd\"\n");
  }
	else{
    if (chdir(args[1]) != 0) {
      perror("lsh");
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
  pid_t pid;
  int status;
  pid = fork();
  if (pid == 0) {  //child
    if (execvp(args[0], args) == -1) {
      perror("lsh");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) { //error
    perror("lsh");
  } else { //parent
    do {
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
			printf("send p: %s size %d \n", outbuf, (int)sizeof(outbuf));
			lws_write( wsi, outbuf, EXAMPLE_RX_BUFFER_BYTES, LWS_WRITE_TEXT );
			memset(outbuf, 0, EXAMPLE_RX_BUFFER_BYTES);
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
    struct lws_context *context;
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
    context = lws_create_context(&info);
    if (context == NULL) {
     fprintf(stderr, "lws init failed\n");
     return -1;
    }
    printf("starting server...\n");
		buffer = malloc(sizeof(char) * LSH_RL_BUFSIZE);
		outbuf = malloc(sizeof(char) * EXAMPLE_RX_BUFFER_BYTES);
		strcpy(outbuf,"Welcome to websocket terminal\n");
		while (1) {
			lws_service(context, 50);
    }
    lws_context_destroy(context);
    return 0;
}

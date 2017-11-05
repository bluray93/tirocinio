#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libwebsockets.h>

#define EXAMPLE_RX_BUFFER_BYTES (32)
int i=0;
//static struct lws *web_socket = NULL;

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
		case LWS_CALLBACK_ESTABLISHED:{ // just log message that someone is connecting
	    printf("connection established\n");
			lws_callback_on_writable( wsi );
			//memcpy(web_socket,wsi);
			//lws_callback_on_writable_all_protocol( lws_get_context( wsi ), lws_get_protocol( wsi ) );
	    break;}
		case LWS_CALLBACK_RECEIVE:
			printf("received data: %s size %d \n", (unsigned char *)(in), (int) len);
			lws_callback_on_writable( wsi );
			break;
		case LWS_CALLBACK_SERVER_WRITEABLE:{
			unsigned char buf[EXAMPLE_RX_BUFFER_BYTES];
			memset(buf,0,EXAMPLE_RX_BUFFER_BYTES);
			strcpy(buf,"Hello! ");
			char* num=malloc(sizeof(int));
			sprintf(num,"%d",i);
			printf("%s\n",num);
			strcat(buf,num);
			i++;
			printf("send p: %s size %d \n", buf, (int)sizeof(buf));
			lws_write( wsi, buf, EXAMPLE_RX_BUFFER_BYTES, LWS_WRITE_TEXT );
			break;}
		case LWS_CALLBACK_CLOSED: { // the funny part
	    printf("connection closed \n");
	  break;}
		default:
			break;
	}
	return 0;
}

static struct lws_protocols protocols[] = {
    {
      "http-only",    /* name */
      callback_http,  /* callback */
      0,              /* per_session_data_size */
    },
		{
			"example-protocol",
			callback_example,
			0
		},
    {NULL, NULL, 0  /* End of list */}
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
    // no special options
    int opts = 0;
    // create libwebsocket context representing this server
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
    while (1) {
			/*if(web_socket==NULL)	{
				struct lws_client_connect_info ccinfo = {0};
				ccinfo.context = context;
				ccinfo.address = "localhost";
				ccinfo.port = 8000;
				ccinfo.path = "/";
				ccinfo.host = lws_canonical_hostname( context );
				ccinfo.origin = "origin";
				ccinfo.protocol = protocols[PROTOCOL_EXAMPLE].name;
				web_socket = lws_client_connect_via_info(&ccinfo);
			}
			lws_callback_on_writable( web_socket );*/
			lws_service(context, 50);
    }
    lws_context_destroy(context);
    return 0;
}

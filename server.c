#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libwebsockets.h>


static int callback_http( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len ){
	switch( reason ){
		case LWS_CALLBACK_CLIENT_WRITEABLE:
	    printf("connection established\n");
	    break;
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
		case LWS_CALLBACK_ESTABLISHED: // just log message that someone is connecting
	    printf("connection established\n");
	    //client_connections.insert(wsi);
	    break;
		case LWS_CALLBACK_RECEIVE:
			printf("received some data\n");
			printf("received data: %s size %d \n", (unsigned char *)(in), (int) len);
			break;
		case LWS_CALLBACK_CLOSED: { // the funny part
	    printf("connection closed \n");
	    //client_connections.erase(wsi);
	  break;}
		default:
			break;
	}
	return 0;
}

/* list of supported protocols and callbacks */

static struct lws_protocols protocols[] = {
    /* first protocol must always be HTTP handler */
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
    //info.extensions = lws_get_internal_extensions();
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

    // infinite loop, the only option to end this serer is
    // by sending SIGTERM. (CTRL+C)
    while (1) {
        lws_service(context, 50);
        // libwebsocket_service will process all waiting events with their
        // callback functions and then wait 50 ms.
        // (this is single threaded webserver and this will keep
        // our server from generating load while there are not
        // requests to process)
    }

    lws_context_destroy(context);

    return 0;
}

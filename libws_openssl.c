
#include "libws_log.h"
#include "libws.h"
#include "libws_private.h"
#include "libws_openssl.h"
#include "openssl/ssl.h"


int _ws_global_openssl_init(ws_base_t ws_base)
{
	SSL_library_init();
	SSL_load_error_strings();

	if (!RAND_poll())
	{
		LIBWS_LOG(LIBWS_CRIT, "Got no random source, cannot init OpenSSL");
		return -1;
	}

	// Setup the SSL context.
	{
		const SSL_METHOD *ssl_method = SSLv23_client_method();

		if (!(ws_base->ssl_ctx = SSL_CTX_new(ssl_method)))
		{
			LIBWS_LOG(LIBWS_ERR, "Failed to create OpenSSL context");
			return -1;
		}
	}
}

void _ws_global_openssl_destroy(ws_base_t ws_base)
{
	if (ws_base->ssl_ctx)
	{		
		SSL_CTX_free(ws_base->ssl_ctx);
		ws_base->ssl_ctx = NULL;
	}
}

int _ws_openssl_init(ws_t ws, ws_base_t ws_base)
{

}

void _ws_openssl_destroy(ws_t ws)
{
	if (ws->ws_base->ssl_ctx)
	{
		SSL_CTX_free(ws->ws_base->ssl_ctx);
		ws->ws_base->ssl_ctx = NULL;
	}
}

int _ws_openssl_close(ws_t ws)
{
	//
	// SSL_RECEIVED_SHUTDOWN tells SSL_shutdown to act as if we had already
	// received a close notify from the other end.  SSL_shutdown will then
	// send the final close notify in reply.  The other end will receive the
	// close notify and send theirs.  By this time, we will have already
	// closed the socket and the other end's real close notify will never be
	// received.  In effect, both sides will think that they have completed a
	// clean shutdown and keep their sessions valid.  This strategy will fail
	// if the socket is not ready for writing, in which case this hack will
	// lead to an unclean shutdown and lost session on the other end.
	//
	if (ws->ssl)
	{
		SSL_set_shutdown(ws->ssl, SSL_RECEIVED_SHUTDOWN);
		SSL_shutdown(ws->ssl);
		ws->ssl = NULL;
	}

	return 0;
}

int _ws_create_bufferevent_openssl_socket(ws_t ws)
{
	assert(ws->ws_base->ssl_ctx);
	assert(ws->ssl);

	if (!(ws->bev = bufferevent_openssl_socket_new(ws->base, -1, 
			ws->ssl, BUFFEREVENT_SSL_CONNECTING, BEV_OPT_CLOSE_ON_FREE)))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to create SSL socket");
		return -1;
	}

	return 0;
}


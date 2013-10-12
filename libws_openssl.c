
#include "libws_config.h"
#include <assert.h>
#include "libws_log.h"
#include "libws.h"
#include "libws_private.h"
#include "libws_openssl.h"
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent_ssl.h>

// TODO: Allow setting client cert: SSL_CTX_use_certificate_file(ssl_ctx, "my_apple_cert_key.pem", SSL_FILETYPE_PEM);
// http://www.provos.org/index.php?/archives/79-OpenSSL-Client-Certificates-and-Libevent-2.0.3-alpha.html

int _ws_global_openssl_init(ws_base_t ws_base)
{
	SSL_library_init();
	ERR_load_crypto_strings();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();

	if (!RAND_poll())
	{
		LIBWS_LOG(LIBWS_CRIT, "Got no random source, cannot init OpenSSL");
		return -1;
	}

	return 0;
}

void _ws_global_openssl_destroy(ws_base_t ws_base)
{
	CRYPTO_cleanup_all_ex_data();
	ERR_free_strings();
	ERR_remove_state(0);
	EVP_cleanup();
}

int _ws_openssl_init(ws_t ws, ws_base_t ws_base)
{
	assert(ws);
	assert(ws_base);

	LIBWS_LOG(LIBWS_DEBUG, "OpenSSL init");

	// Setup the SSL context.
	{
		const SSL_METHOD *ssl_method = SSLv23_client_method();

		if (!(ws->ssl_ctx = SSL_CTX_new(ssl_method)))
		{
			LIBWS_LOG(LIBWS_ERR, "Failed to create OpenSSL context");
			return -1;
		}

		#if OPENSSL_VERSION_NUMBER >= 0x10000000L
		SSL_CTX_set_options(ws->ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
		#endif
	}

	if (!(ws->ssl = SSL_new(ws->ssl_ctx)))
		return -1;

	return 0;
}

void _ws_openssl_destroy(ws_t ws)
{
	_ws_openssl_close(ws);

	if (ws->ssl_ctx)
	{
		SSL_CTX_free(ws->ssl_ctx);
		ws->ssl_ctx = NULL;
	}
}

int _ws_openssl_close(ws_t ws)
{
	LIBWS_LOG(LIBWS_TRACE, "OpenSSL close");

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
		SSL_free(ws->ssl);
		ws->ssl = NULL;
	}

	LIBWS_LOG(LIBWS_TRACE, "End");

	return 0;
}

struct bufferevent * _ws_create_bufferevent_openssl_socket(ws_t ws)
{
	struct bufferevent *bev = NULL;
	assert(ws->ssl_ctx);
	assert(ws->ssl);

	if (!(bev = bufferevent_openssl_socket_new(ws->ws_base->ev_base, -1, 
			ws->ssl, BUFFEREVENT_SSL_CONNECTING, 
			BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS)))
	{
		LIBWS_LOG(LIBWS_ERR, "Failed to create SSL socket");
		return NULL;
	}

	return bev;
}


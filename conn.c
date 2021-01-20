#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <tls.h>
#include <unistd.h>

#include "conn.h"
#include "util.h"

struct tls *client = NULL;

_Bool
conn_init(void)
{
	struct tls_config *tlscfg = tls_config_new();
	ENSURE(tlscfg);

	if (tls_config_set_ciphers(tlscfg, "compat") != 0)
		return false; /* tls_config_set_ciphers error */

	/* FIXME: right way to allow self-signed certs? */
	tls_config_insecure_noverifycert(tlscfg);

	client = tls_client();
	ENSURE(client);

	if (tls_configure(client, tlscfg) != 0)
		return false; /* tls_configure error */

	tls_config_free(tlscfg);
	return true;
}

_Bool
conn_conn(char *host, char *port)
{
	ENSURE(client);

	struct addrinfo hints = {
		.ai_protocol = IPPROTO_TCP,
		.ai_socktype = SOCK_STREAM,
		.ai_family = AF_UNSPEC,
	};
	struct addrinfo *res, *r;
	int fd = -1;

	/* failed to resolve */
	if(getaddrinfo(host, port, &hints, &res) != 0) return false;

	for(r = res; r != NULL; r = r->ai_next) {
		if((fd = socket(r->ai_family, r->ai_socktype, r->ai_protocol)) == -1)
			continue;
		if(connect(fd, r->ai_addr, r->ai_addrlen) == 0)
			break;
		close(fd);
	}

	freeaddrinfo(res);

	/* can't connect */
	if (r == NULL) return false;

	if (tls_connect_socket(client, fd, host) != 0)
		return false; /* tls: connect failed */
	if (tls_handshake(client) != 0)
		return false; /* tls: handshake failed */

	return true;
}

_Bool
conn_active(void)
{
	// TODO: check if connection is still open
	return client != NULL;
}

_Bool
conn_send(char *data)
{
	ENSURE(conn_active());
	size_t len = strlen(data);

	while (len) {
		ssize_t r = -1;

		r = tls_write(client, data, len);

		if (r == TLS_WANT_POLLIN || r == TLS_WANT_POLLOUT)
			continue;
		else if (r < 0) return false;

		data += r; len -= r;
	}

	return true;
}

ssize_t
conn_recv(char *bufsrv, size_t sz)
{
	ssize_t r = tls_read(client, bufsrv, sz);

	if (r == TLS_WANT_POLLIN || r == TLS_WANT_POLLOUT) {
		return 0;
	} else if (r < 0) {
		if (errno != EINTR)
			return -2;
		return 0;
	} else if (r == 0) {
		return -1;
	}

	return r;
}

_Bool
conn_close(void)
{
	ENSURE(conn_active());
	tls_close(client);
	return true;
}

_Bool
conn_shutdown(void)
{
	ENSURE(client);
	tls_free(client);
	client = NULL;
	return true;
}

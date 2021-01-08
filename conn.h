#ifndef CONN_H
#define CONN_H

#include <stdint.h>
#include <tls.h>

extern struct tls *client;

  _Bool conn_init(void);
  _Bool conn_conn(char *host, char *port);
  _Bool conn_active(void);
  _Bool conn_send(char *data);
ssize_t conn_recv(char *bufsrv, size_t sz);
  _Bool conn_close(void);
  _Bool conn_shutdown(void);

#endif

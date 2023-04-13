

/*******************************************************************************/
/* libeport begin */
/*******************************************************************************/

#pragma once

#ifdef __cplusplus
#define eport_api extern "C"
#else
#define eport_api
#endif

#define eport_none      0
#define eport_true      1
#define eport_false     0
#define eport_error_ok  0

/*******************************************************************************/

enum struct eport_family {
  stream, dgram
};

enum struct eport_wait_t {
  read, write
};

typedef size_t  eport_handle;
typedef size_t  eport_errno;
typedef size_t  eport_count;
typedef const void* eport_context;

struct eport_endpoint {
  size_t size;
  char data[32];
};

typedef void (*eport_cb_dispatch)(eport_context);
typedef void (*eport_cb_hostname)(const char*);
typedef void (*eport_cb_accept)  (eport_errno, eport_handle, eport_handle, eport_context);
typedef void (*eport_cb_select)  (eport_errno, eport_handle, size_t, eport_context);
typedef void (*eport_cb_connect) (eport_errno, eport_handle, eport_context);

/*******************************************************************************/

eport_api eport_handle eport_create();
eport_api eport_errno  eport_release        (eport_handle handle);
eport_api eport_errno  eport_post           (eport_handle handle, eport_cb_dispatch callback, eport_context argv = 0);
eport_api eport_errno  eport_dispatch       (eport_handle handle, eport_cb_dispatch callback, eport_context argv = 0);
eport_api eport_errno  eport_restart        (eport_handle handle);
eport_api eport_errno  eport_stop           (eport_handle handle);
eport_api eport_errno  eport_stopped        (eport_handle handle);
eport_api eport_count  eport_run            (eport_handle handle, size_t expires);
eport_api eport_count  eport_run_one        (eport_handle handle, size_t expires);

eport_api eport_handle eport_socket         (eport_handle handle, eport_family family = eport_family::stream);
eport_api eport_errno  eport_close          (eport_handle fd, int linger = 0);
eport_api eport_errno  eport_resolve        (const char* host, unsigned short port, eport_endpoint& endpoint);
eport_api eport_errno  eport_port           (const eport_endpoint& endpoint, unsigned short& port);
eport_api eport_errno  eport_address        (const eport_endpoint& endpoint, char* data, size_t size);

eport_api eport_errno  eport_ssl_enable     (eport_handle fd);
eport_api eport_errno  eport_on_hostname    (eport_handle fd, eport_cb_hostname callback);
eport_api eport_errno  eport_use_verify_file(eport_handle fd, const char* filename);
eport_api eport_errno  eport_use_certificate(eport_handle fd, const char* cert, size_t size);
eport_api eport_errno  eport_use_private_key(eport_handle fd, const char* key,  size_t size, const char* pwd = 0);

eport_api eport_errno  eport_listen         (eport_handle fd, unsigned short port, const char* host = 0, int backlog = 16);
eport_api eport_errno  eport_accept         (eport_handle fd, eport_handle new_fd);
eport_api eport_errno  eport_async_accept   (eport_handle fd, eport_handle new_fd, eport_cb_accept callback, eport_context argv = 0);
eport_api eport_errno  eport_connect        (eport_handle fd, const char* host, unsigned short port);
eport_api eport_errno  eport_async_connect  (eport_handle fd, const char* host, unsigned short port, eport_cb_connect callback, eport_context argv = 0);
eport_api eport_errno  eport_select         (eport_handle fd, eport_wait_t what, eport_cb_select callback, eport_context argv = 0);
eport_api eport_errno  eport_send           (eport_handle fd, const char* data, size_t length, eport_count* count);
eport_api eport_errno  eport_async_send     (eport_handle fd, const char* data, size_t length);
eport_api eport_errno  eport_receive        (eport_handle fd, char* buffer, size_t size, eport_count* count);
eport_api eport_errno  eport_send_to        (eport_handle fd, const char* data, size_t length, const eport_endpoint& remote, eport_count* count);
eport_api eport_errno  eport_async_send_to  (eport_handle fd, const char* data, size_t length, const eport_endpoint& remote);
eport_api eport_errno  eport_receive_from   (eport_handle fd, char* buffer, size_t size, eport_endpoint& remote, eport_count* count);
eport_api eport_errno  eport_local_endpoint (eport_handle fd, eport_endpoint& local);
eport_api eport_errno  eport_remote_endpoint(eport_handle fd, eport_endpoint& remote);

/*******************************************************************************/
/* libeport end */
/*******************************************************************************/

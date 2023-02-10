
/********************************************************************************
**
** Copyright 2021-2022 stanzhao
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
**
********************************************************************************/

#ifndef __LIB_SOCKET_IO_H
#define __LIB_SOCKET_IO_H

#ifdef __cplusplus
# define lib_api extern "C"
#else
# define lib_api
#endif

#include <cstddef>

typedef int socket_fd;
typedef void (*connect_callback)(int ec, socket_fd fd);
typedef void (*receive_callback)(int ec, const char* data, unsigned int size, socket_fd fd);

/*******************************************************************************/

lib_api socket_fd ltcp_create();
lib_api void ltcp_close  (socket_fd fd);
lib_api int  ltcp_connect(socket_fd fd, const char* host, unsigned short port, connect_callback callback);
lib_api int  ltcp_wait   (socket_fd fd, receive_callback callback);
lib_api int  ltcp_send   (socket_fd fd, const char* data, unsigned int size);
lib_api int  ltcp_update ();

/*******************************************************************************/

#endif //__LIB_SOCKET_IO_H

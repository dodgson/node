/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <assert.h>

#include "uv.h"
#include "../uv-common.h"
#include "internal.h"


void uv_req_init(uv_req_t* req) {
  uv_counters()->req_init++;
  req->type = UV_UNKNOWN_REQ;
  SET_REQ_SUCCESS(req);
}


uv_req_t* uv_overlapped_to_req(OVERLAPPED* overlapped) {
  return CONTAINING_RECORD(overlapped, uv_req_t, overlapped);
}


void uv_insert_pending_req(uv_req_t* req) {
  req->next_req = NULL;
  if (LOOP->pending_reqs_tail) {
    req->next_req = LOOP->pending_reqs_tail->next_req;
    LOOP->pending_reqs_tail->next_req = req;
    LOOP->pending_reqs_tail = req;
  } else {
    req->next_req = req;
    LOOP->pending_reqs_tail = req;
  }
}


static uv_req_t* uv_remove_pending_req() {
  uv_req_t* req;

  if (LOOP->pending_reqs_tail) {
    req = LOOP->pending_reqs_tail->next_req;

    if (req == LOOP->pending_reqs_tail) {
      LOOP->pending_reqs_tail = NULL;
    } else {
      LOOP->pending_reqs_tail->next_req = req->next_req;
    }

    return req;

  } else {
    /* queue empty */
    return NULL;
  }
}


#define DELEGATE_STREAM_REQ(req, method, handle_at)                           \
  do {                                                                        \
    switch (((uv_handle_t*) (req)->handle_at)->type) {                        \
      case UV_TCP:                                                            \
        uv_process_tcp_##method##_req((uv_tcp_t*) ((req)->handle_at), req);   \
        break;                                                                \
                                                                              \
      case UV_NAMED_PIPE:                                                     \
        uv_process_pipe_##method##_req((uv_pipe_t*) ((req)->handle_at), req); \
        break;                                                                \
                                                                              \
      default:                                                                \
        assert(0);                                                            \
    }                                                                         \
  } while (0)


void uv_process_reqs() {
  uv_req_t* req;

  while (req = uv_remove_pending_req()) {
    switch (req->type) {
      case UV_READ:
        DELEGATE_STREAM_REQ(req, read, data);
        break;

      case UV_WRITE:
        DELEGATE_STREAM_REQ((uv_write_t*) req, write, handle);
        break;

      case UV_ACCEPT:
        DELEGATE_STREAM_REQ(req, accept, data);
        break;

      case UV_CONNECT:
        DELEGATE_STREAM_REQ((uv_connect_t*) req, connect, handle);
        break;

      case UV_SHUTDOWN:
        /* Tcp shutdown requests don't come here. */
        assert(((uv_shutdown_t*) req)->handle->type == UV_NAMED_PIPE);
        uv_process_pipe_shutdown_req(
            (uv_pipe_t*) ((uv_shutdown_t*) req)->handle, (uv_shutdown_t*) req);
        break;

      case UV_UDP_RECV:
        uv_process_udp_recv_req((uv_udp_t*) req->data, req);
        break;

      case UV_UDP_SEND:
        uv_process_udp_send_req(((uv_udp_send_t*) req)->handle,
                                (uv_udp_send_t*) req);
        break;

      case UV_WAKEUP:
        uv_process_async_wakeup_req((uv_async_t*) req->data, req);
        break;

      case UV_ARES_EVENT_REQ:
        uv_process_ares_event_req((uv_ares_action_t*) req->data, req);
        break;

      case UV_ARES_CLEANUP_REQ:
        uv_process_ares_cleanup_req((uv_ares_task_t*) req->data, req);
        break;

      case UV_GETADDRINFO_REQ:
        uv_process_getaddrinfo_req((uv_getaddrinfo_t*) req->data, req);
        break;

      case UV_PROCESS_EXIT:
        uv_process_proc_exit((uv_process_t*) req->data);
        break;

      case UV_PROCESS_CLOSE:
        uv_process_proc_close((uv_process_t*) req->data);
        break;

      default:
        assert(0);
    }
  }
}

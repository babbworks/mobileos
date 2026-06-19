/*
 * zako_bus.c — ZAKO System Bus Implementation
 *
 * Unix domain socket IPC with length-prefixed framing,
 * poll-based event loop, category-based routing.
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#include "zako_bus.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/* ========================================================================
 * INTERNAL HELPERS
 * ======================================================================== */

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) { return -1; }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void client_reset(zbu_client_t *c)
{
    if (c->fd >= 0) {
        close(c->fd);
    }
    c->fd = -1;
    c->state = ZBU_STATE_EMPTY;
    c->sub_count = 0;
    c->client_id = 0;
    c->rx_len = 0;
    c->rx_expected = 0;
}

static int send_all(int fd, const uint8_t *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, data + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR) { continue; }
            if (errno == EAGAIN || errno == EWOULDBLOCK) { continue; }
            return ZBU_ERR_IO;
        }
        if (n == 0) { return ZBU_ERR_CLOSED; }
        sent += (size_t)n;
    }
    return ZBU_OK;
}

/* Try to read available bytes into client rx_buf. Returns bytes read or error. */
static int client_read(zbu_client_t *c)
{
    size_t space = sizeof(c->rx_buf) - c->rx_len;
    if (space == 0) { return ZBU_ERR_SIZE; }

    ssize_t n = read(c->fd, c->rx_buf + c->rx_len, space);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) { return ZBU_ERR_AGAIN; }
        if (errno == EINTR) { return ZBU_ERR_AGAIN; }
        return ZBU_ERR_IO;
    }
    if (n == 0) { return ZBU_ERR_CLOSED; }

    c->rx_len += (size_t)n;
    return (int)n;
}

/* Check if a complete frame is available in rx_buf */
static int client_has_frame(zbu_client_t *c)
{
    if (c->rx_len < ZBU_FRAME_HEADER_SIZE) { return 0; }

    uint16_t payload_len = (uint16_t)((c->rx_buf[0] << 8u) | c->rx_buf[1]);
    size_t total = ZBU_FRAME_HEADER_SIZE + payload_len;

    if (payload_len == 0 || payload_len > ZBU_MAX_FRAME) {
        return -1; /* protocol error */
    }

    return (c->rx_len >= total) ? 1 : 0;
}

/* Extract one frame from rx_buf, shift remaining data forward */
static int client_extract_frame(zbu_client_t *c, zbu_frame_t *frame)
{
    uint16_t payload_len = (uint16_t)((c->rx_buf[0] << 8u) | c->rx_buf[1]);
    size_t total = ZBU_FRAME_HEADER_SIZE + payload_len;

    if (c->rx_len < total) { return ZBU_ERR_AGAIN; }

    frame->len = payload_len;
    memcpy(frame->data, c->rx_buf + ZBU_FRAME_HEADER_SIZE, payload_len);
    frame->from_client = c->client_id;

    /* Shift remaining bytes forward */
    size_t remaining = c->rx_len - total;
    if (remaining > 0) {
        memmove(c->rx_buf, c->rx_buf + total, remaining);
    }
    c->rx_len = remaining;

    return ZBU_OK;
}

/* ========================================================================
 * SERVER
 * ======================================================================== */

int zbu_server_init(zbu_server_t *server)
{
    size_t i;

    if (server == NULL) { return ZBU_ERR_NULL; }

    server->listen_fd = -1;
    server->running = 0;
    memset(server->socket_path, 0, sizeof(server->socket_path));

    for (i = 0; i < ZBU_MAX_CLIENTS; i++) {
        server->clients[i].fd = -1;
        server->clients[i].state = ZBU_STATE_EMPTY;
        server->clients[i].sub_count = 0;
        server->clients[i].client_id = 0;
        server->clients[i].rx_len = 0;
        server->clients[i].rx_expected = 0;
    }

    return ZBU_OK;
}

int zbu_server_start(zbu_server_t *server, const char *path)
{
    struct sockaddr_un addr;
    int fd;
    size_t path_len;

    if (server == NULL || path == NULL) { return ZBU_ERR_NULL; }

    path_len = strlen(path);
    if (path_len == 0 || path_len >= sizeof(addr.sun_path)) {
        return ZBU_ERR_INVALID;
    }

    /* Remove existing socket file (ignore error if doesn't exist) */
    unlink(path);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { return ZBU_ERR_SOCKET; }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return ZBU_ERR_SOCKET;
    }

    if (listen(fd, ZBU_MAX_CLIENTS) < 0) {
        close(fd);
        return ZBU_ERR_SOCKET;
    }

    if (set_nonblocking(fd) < 0) {
        close(fd);
        return ZBU_ERR_SOCKET;
    }

    server->listen_fd = fd;
    server->running = 1;
    strncpy(server->socket_path, path, sizeof(server->socket_path) - 1);

    return ZBU_OK;
}

int zbu_server_poll(zbu_server_t *server, int timeout_ms)
{
    struct pollfd fds[1 + ZBU_MAX_CLIENTS];
    size_t nfds = 0;
    size_t i;
    int events = 0;

    if (server == NULL || !server->running) { return ZBU_ERR_INVALID; }

    /* Add listener */
    fds[0].fd = server->listen_fd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    nfds = 1;

    /* Add active clients */
    for (i = 0; i < ZBU_MAX_CLIENTS; i++) {
        if (server->clients[i].state != ZBU_STATE_EMPTY) {
            fds[nfds].fd = server->clients[i].fd;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }
    }

    int ret = poll(fds, (unsigned int)nfds, timeout_ms);
    if (ret < 0) {
        if (errno == EINTR) { return 0; }
        return ZBU_ERR_IO;
    }
    if (ret == 0) { return 0; }

    /* Accept new connections */
    if (fds[0].revents & POLLIN) {
        int client_fd = accept(server->listen_fd, NULL, NULL);
        if (client_fd >= 0) {
            set_nonblocking(client_fd);
            /* Find empty slot */
            int placed = 0;
            for (i = 0; i < ZBU_MAX_CLIENTS; i++) {
                if (server->clients[i].state == ZBU_STATE_EMPTY) {
                    server->clients[i].fd = client_fd;
                    server->clients[i].state = ZBU_STATE_ACTIVE;
                    server->clients[i].client_id = (uint32_t)(i + 1u);
                    server->clients[i].rx_len = 0;
                    server->clients[i].rx_expected = 0;
                    server->clients[i].sub_count = 0;
                    placed = 1;
                    events++;
                    break;
                }
            }
            if (!placed) {
                close(client_fd); /* Pool full */
            }
        }
    }

    /* Read from clients */
    size_t poll_idx = 1;
    for (i = 0; i < ZBU_MAX_CLIENTS; i++) {
        if (server->clients[i].state == ZBU_STATE_EMPTY) { continue; }
        if (poll_idx >= nfds) { break; }

        if (fds[poll_idx].revents & (POLLIN | POLLHUP | POLLERR)) {
            int rc = client_read(&server->clients[i]);
            if (rc == ZBU_ERR_CLOSED || rc == ZBU_ERR_IO) {
                client_reset(&server->clients[i]);
            } else if (rc > 0) {
                events++;
            }
        }
        poll_idx++;
    }

    return events;
}

int zbu_server_recv(zbu_server_t *server, zbu_frame_t *frame)
{
    size_t i;

    if (server == NULL || frame == NULL) { return ZBU_ERR_NULL; }

    for (i = 0; i < ZBU_MAX_CLIENTS; i++) {
        if (server->clients[i].state == ZBU_STATE_EMPTY) { continue; }

        int has = client_has_frame(&server->clients[i]);
        if (has < 0) {
            /* Protocol error — disconnect client */
            client_reset(&server->clients[i]);
            continue;
        }
        if (has == 1) {
            return client_extract_frame(&server->clients[i], frame);
        }
    }

    return ZBU_ERR_AGAIN;
}

int zbu_server_send(zbu_server_t *server, size_t client_idx,
                    const uint8_t *data, size_t len)
{
    uint8_t header[ZBU_FRAME_HEADER_SIZE];
    int rc;

    if (server == NULL || data == NULL) { return ZBU_ERR_NULL; }
    if (client_idx >= ZBU_MAX_CLIENTS) { return ZBU_ERR_INVALID; }
    if (len == 0 || len > ZBU_MAX_FRAME) { return ZBU_ERR_SIZE; }
    if (server->clients[client_idx].state == ZBU_STATE_EMPTY) { return ZBU_ERR_INVALID; }

    header[0] = (uint8_t)((len >> 8u) & 0xFFu);
    header[1] = (uint8_t)(len & 0xFFu);

    rc = send_all(server->clients[client_idx].fd, header, ZBU_FRAME_HEADER_SIZE);
    if (rc != ZBU_OK) { return rc; }

    return send_all(server->clients[client_idx].fd, data, len);
}

int zbu_server_broadcast(zbu_server_t *server,
                         const uint8_t *data, size_t len)
{
    size_t i;
    int count = 0;

    if (server == NULL || data == NULL) { return ZBU_ERR_NULL; }
    if (len == 0 || len > ZBU_MAX_FRAME) { return ZBU_ERR_SIZE; }

    for (i = 0; i < ZBU_MAX_CLIENTS; i++) {
        if (server->clients[i].state == ZBU_STATE_ACTIVE) {
            if (zbu_server_send(server, i, data, len) == ZBU_OK) {
                count++;
            }
        }
    }

    return count;
}

int zbu_server_route_category(zbu_server_t *server, uint8_t category,
                              const uint8_t *data, size_t len)
{
    size_t i, s;
    int count = 0;

    if (server == NULL || data == NULL) { return ZBU_ERR_NULL; }
    if (len == 0 || len > ZBU_MAX_FRAME) { return ZBU_ERR_SIZE; }

    for (i = 0; i < ZBU_MAX_CLIENTS; i++) {
        if (server->clients[i].state != ZBU_STATE_ACTIVE) { continue; }

        for (s = 0; s < server->clients[i].sub_count; s++) {
            if (server->clients[i].subscriptions[s] == category) {
                if (zbu_server_send(server, i, data, len) == ZBU_OK) {
                    count++;
                }
                break;
            }
        }
    }

    return count;
}

void zbu_server_stop(zbu_server_t *server)
{
    size_t i;

    if (server == NULL) { return; }

    for (i = 0; i < ZBU_MAX_CLIENTS; i++) {
        if (server->clients[i].state != ZBU_STATE_EMPTY) {
            client_reset(&server->clients[i]);
        }
    }

    if (server->listen_fd >= 0) {
        close(server->listen_fd);
        server->listen_fd = -1;
    }

    if (server->socket_path[0] != '\0') {
        unlink(server->socket_path);
        server->socket_path[0] = '\0';
    }

    server->running = 0;
}

/* ========================================================================
 * CLIENT
 * ======================================================================== */

int zbu_client_init(zbu_conn_t *conn)
{
    if (conn == NULL) { return ZBU_ERR_NULL; }

    conn->fd = -1;
    conn->rx_len = 0;
    conn->rx_expected = 0;

    return ZBU_OK;
}

int zbu_client_connect(zbu_conn_t *conn, const char *path)
{
    struct sockaddr_un addr;
    int fd;

    if (conn == NULL || path == NULL) { return ZBU_ERR_NULL; }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { return ZBU_ERR_SOCKET; }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return ZBU_ERR_CONNECT;
    }

    set_nonblocking(fd);
    conn->fd = fd;
    conn->rx_len = 0;
    conn->rx_expected = 0;

    return ZBU_OK;
}

int zbu_client_send(zbu_conn_t *conn, const uint8_t *data, size_t len)
{
    uint8_t header[ZBU_FRAME_HEADER_SIZE];
    int rc;

    if (conn == NULL || data == NULL) { return ZBU_ERR_NULL; }
    if (conn->fd < 0) { return ZBU_ERR_INVALID; }
    if (len == 0 || len > ZBU_MAX_FRAME) { return ZBU_ERR_SIZE; }

    header[0] = (uint8_t)((len >> 8u) & 0xFFu);
    header[1] = (uint8_t)(len & 0xFFu);

    rc = send_all(conn->fd, header, ZBU_FRAME_HEADER_SIZE);
    if (rc != ZBU_OK) { return rc; }

    return send_all(conn->fd, data, len);
}

int zbu_client_recv(zbu_conn_t *conn, zbu_frame_t *frame)
{
    ssize_t n;
    size_t space;
    uint16_t payload_len;
    size_t total;

    if (conn == NULL || frame == NULL) { return ZBU_ERR_NULL; }
    if (conn->fd < 0) { return ZBU_ERR_INVALID; }

    /* Try to read more data */
    space = sizeof(conn->rx_buf) - conn->rx_len;
    if (space > 0) {
        n = read(conn->fd, conn->rx_buf + conn->rx_len, space);
        if (n > 0) {
            conn->rx_len += (size_t)n;
        } else if (n == 0) {
            return ZBU_ERR_CLOSED;
        }
        /* n < 0 with EAGAIN is fine — check what we have */
    }

    /* Check for complete frame */
    if (conn->rx_len < ZBU_FRAME_HEADER_SIZE) { return ZBU_ERR_AGAIN; }

    payload_len = (uint16_t)((conn->rx_buf[0] << 8u) | conn->rx_buf[1]);
    if (payload_len == 0 || payload_len > ZBU_MAX_FRAME) {
        return ZBU_ERR_SIZE; /* Protocol error */
    }

    total = ZBU_FRAME_HEADER_SIZE + payload_len;
    if (conn->rx_len < total) { return ZBU_ERR_AGAIN; }

    /* Extract frame */
    frame->len = payload_len;
    memcpy(frame->data, conn->rx_buf + ZBU_FRAME_HEADER_SIZE, payload_len);
    frame->from_client = 0; /* N/A on client side */

    /* Shift remaining */
    size_t remaining = conn->rx_len - total;
    if (remaining > 0) {
        memmove(conn->rx_buf, conn->rx_buf + total, remaining);
    }
    conn->rx_len = remaining;

    return ZBU_OK;
}

int zbu_client_subscribe(zbu_conn_t *conn, uint8_t category)
{
    /*
     * Subscription is a Wave/Role B Pure Signal with the target category.
     * The bus recognizes this as a subscription request.
     * Frame: [Meta byte (Wave/RoleB/cat=target)] — 1 byte.
     *
     * Convention: a 1-byte frame whose Meta byte is Wave/RoleB with
     * bit2=1 (ACK request) signals "subscribe me to this category."
     */
    uint8_t meta;

    if (conn == NULL) { return ZBU_ERR_NULL; }

    /* Wave mode, Role B, ACK=1 (subscribe signal), category=target */
    meta = (uint8_t)(0x40u | 0x10u | (category & 0x0Fu));
    /* bit1=0(wave) bit2=1(ack) bit3=0(complete) bit4=1(roleB) bits5-8=category */

    return zbu_client_send(conn, &meta, 1);
}

void zbu_client_disconnect(zbu_conn_t *conn)
{
    if (conn == NULL) { return; }

    if (conn->fd >= 0) {
        close(conn->fd);
        conn->fd = -1;
    }
    conn->rx_len = 0;
    conn->rx_expected = 0;
}

/* ========================================================================
 * FRAME HELPERS
 * ======================================================================== */

int zbu_frame_encode(const uint8_t *payload, size_t pay_len,
                     uint8_t *out, size_t *out_len)
{
    if (payload == NULL || out == NULL || out_len == NULL) {
        return ZBU_ERR_NULL;
    }
    if (pay_len == 0 || pay_len > ZBU_MAX_FRAME) {
        return ZBU_ERR_SIZE;
    }

    out[0] = (uint8_t)((pay_len >> 8u) & 0xFFu);
    out[1] = (uint8_t)(pay_len & 0xFFu);
    memcpy(out + ZBU_FRAME_HEADER_SIZE, payload, pay_len);
    *out_len = ZBU_FRAME_HEADER_SIZE + pay_len;

    return ZBU_OK;
}

int zbu_frame_decode(const uint8_t *wire, size_t wire_len,
                     zbu_frame_t *frame)
{
    uint16_t payload_len;
    size_t total;

    if (wire == NULL || frame == NULL) { return ZBU_ERR_NULL; }
    if (wire_len < ZBU_FRAME_HEADER_SIZE) { return ZBU_ERR_AGAIN; }

    payload_len = (uint16_t)((wire[0] << 8u) | wire[1]);
    if (payload_len == 0 || payload_len > ZBU_MAX_FRAME) {
        return ZBU_ERR_SIZE;
    }

    total = ZBU_FRAME_HEADER_SIZE + payload_len;
    if (wire_len < total) { return ZBU_ERR_AGAIN; }

    frame->len = payload_len;
    memcpy(frame->data, wire + ZBU_FRAME_HEADER_SIZE, payload_len);
    frame->from_client = 0;

    return ZBU_OK;
}

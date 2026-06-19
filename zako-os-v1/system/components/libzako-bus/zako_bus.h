/*
 * zako_bus.h — ZAKO System Bus (Unix Domain Socket IPC)
 *
 * The inter-daemon communication substrate for ZAKO OS. All daemons
 * (telux-ledgerd, telux-identd, outstack-powerd) communicate through
 * this bus using BitPads frames and C0 enhanced signals.
 *
 * Architecture:
 *   - Single event loop, non-blocking I/O (poll-based)
 *   - Unix domain sockets (SOCK_STREAM, abstract namespace on Linux)
 *   - Length-prefixed framing: [2-byte big-endian length][payload]
 *   - C0 signal routing: enhanced C0 bytes route by slot position
 *   - Category subscription: daemons subscribe to Wave Role B categories
 *   - No threads — all operations in a single-threaded event loop
 *
 * Message framing on the wire:
 *   [uint16_t length (big-endian)][BitPads frame payload (1–65535 bytes)]
 *
 * Roles:
 *   Server: one instance runs the bus (typically started by init.rc)
 *   Client: each daemon connects as a client
 *
 * MISRA-C:2012 compliance:
 * - No dynamic allocation after init (fixed connection pool)
 * - No recursion
 * - All functions return explicit error codes
 * - Fixed-size buffers (configurable at compile time)
 *
 * Copyright (c) 2026 Babb Works. Licensed under MIT.
 */

#ifndef ZAKO_BUS_H
#define ZAKO_BUS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CONFIGURATION CONSTANTS (compile-time tunables)
 * ======================================================================== */

#ifndef ZBU_MAX_CLIENTS
#define ZBU_MAX_CLIENTS    16u   /* Maximum simultaneous connections */
#endif

#ifndef ZBU_MAX_FRAME
#define ZBU_MAX_FRAME      1024u /* Maximum frame payload size (bytes) */
#endif

#ifndef ZBU_MAX_SUBSCRIPTIONS
#define ZBU_MAX_SUBSCRIPTIONS 16u /* Max category subscriptions per client */
#endif

#define ZBU_FRAME_HEADER_SIZE  2u  /* 2-byte big-endian length prefix */
#define ZBU_SOCKET_PATH_MAX    108u

/* ========================================================================
 * ERROR CODES
 * ======================================================================== */

#define ZBU_OK              0
#define ZBU_ERR_NULL       (-1)
#define ZBU_ERR_SIZE       (-2)   /* Frame too large or buffer overflow */
#define ZBU_ERR_SOCKET     (-3)   /* Socket creation/bind/listen failure */
#define ZBU_ERR_CONNECT    (-4)   /* Connection failure */
#define ZBU_ERR_FULL       (-5)   /* Client pool exhausted */
#define ZBU_ERR_IO         (-6)   /* Read/write failure */
#define ZBU_ERR_CLOSED     (-7)   /* Connection closed by peer */
#define ZBU_ERR_INVALID    (-8)   /* Invalid argument or state */
#define ZBU_ERR_TIMEOUT    (-9)   /* Poll timeout (not an error, informational) */
#define ZBU_ERR_AGAIN      (-10)  /* Would block (non-blocking mode) */

/* ========================================================================
 * CLIENT STATE
 * ======================================================================== */

#define ZBU_STATE_EMPTY      0u  /* Slot not in use */
#define ZBU_STATE_CONNECTED  1u  /* Connected, not yet identified */
#define ZBU_STATE_ACTIVE     2u  /* Connected and identified */
#define ZBU_STATE_CLOSING    3u  /* Graceful shutdown in progress */

/* ========================================================================
 * STRUCTURES
 * ======================================================================== */

/* Per-client connection state (managed by bus server) */
typedef struct {
    int      fd;                                /* Socket file descriptor (-1 if empty) */
    uint8_t  state;                             /* ZBU_STATE_* */
    uint8_t  subscriptions[ZBU_MAX_SUBSCRIPTIONS]; /* Category codes subscribed to */
    uint8_t  sub_count;                         /* Number of active subscriptions */
    uint32_t client_id;                         /* Unique client identifier (from Layer 1) */
    /* Receive buffer for partial frame assembly */
    uint8_t  rx_buf[ZBU_FRAME_HEADER_SIZE + ZBU_MAX_FRAME];
    size_t   rx_len;                            /* Bytes accumulated in rx_buf */
    size_t   rx_expected;                       /* Expected total (header + payload), 0 if unknown */
} zbu_client_t;

/* Bus server instance */
typedef struct {
    int           listen_fd;                    /* Listening socket (-1 if not started) */
    zbu_client_t  clients[ZBU_MAX_CLIENTS];    /* Fixed connection pool */
    char          socket_path[ZBU_SOCKET_PATH_MAX]; /* Bound path */
    uint8_t       running;                      /* 1 if event loop active */
} zbu_server_t;

/* Bus client handle (daemon side) */
typedef struct {
    int      fd;                                /* Connected socket (-1 if disconnected) */
    uint8_t  rx_buf[ZBU_FRAME_HEADER_SIZE + ZBU_MAX_FRAME];
    size_t   rx_len;
    size_t   rx_expected;
} zbu_conn_t;

/* Received frame (returned by receive functions) */
typedef struct {
    uint8_t  data[ZBU_MAX_FRAME];  /* Frame payload (BitPads frame) */
    size_t   len;                   /* Payload length */
    uint32_t from_client;           /* Sender client_id (server only) */
} zbu_frame_t;

/* ========================================================================
 * PUBLIC API — SERVER (bus daemon)
 * ======================================================================== */

/*
 * zbu_server_init — Initialize a bus server instance.
 *
 * Does NOT create the socket — call zbu_server_start() for that.
 *
 * @param server  Server instance to initialize
 * @return ZBU_OK
 */
int zbu_server_init(zbu_server_t *server);

/*
 * zbu_server_start — Create socket, bind, and listen.
 *
 * @param server  Initialized server
 * @param path    Socket path (abstract namespace if starts with '\0' on Linux)
 * @return ZBU_OK on success, ZBU_ERR_SOCKET on failure
 */
int zbu_server_start(zbu_server_t *server, const char *path);

/*
 * zbu_server_poll — Run one iteration of the event loop.
 *
 * Accepts new connections, receives frames, handles disconnections.
 * Non-blocking: returns immediately if no events (with timeout_ms=0),
 * or blocks up to timeout_ms milliseconds.
 *
 * @param server      Running server
 * @param timeout_ms  Poll timeout (0=immediate, -1=infinite)
 * @return Number of events processed, or negative error
 */
int zbu_server_poll(zbu_server_t *server, int timeout_ms);

/*
 * zbu_server_recv — Get the next complete frame from any client.
 *
 * Call after zbu_server_poll() returns > 0. Returns one frame at a time.
 *
 * @param server  Running server
 * @param frame   Output: received frame
 * @return ZBU_OK if frame available, ZBU_ERR_AGAIN if none ready
 */
int zbu_server_recv(zbu_server_t *server, zbu_frame_t *frame);

/*
 * zbu_server_send — Send a frame to a specific client.
 *
 * @param server     Running server
 * @param client_idx Index in clients[] array
 * @param data       Frame payload
 * @param len        Payload length
 * @return ZBU_OK on success
 */
int zbu_server_send(zbu_server_t *server, size_t client_idx,
                    const uint8_t *data, size_t len);

/*
 * zbu_server_broadcast — Send a frame to all active clients.
 *
 * @param server  Running server
 * @param data    Frame payload
 * @param len     Payload length
 * @return Number of clients sent to
 */
int zbu_server_broadcast(zbu_server_t *server,
                         const uint8_t *data, size_t len);

/*
 * zbu_server_route_category — Send a frame to clients subscribed to a category.
 *
 * @param server    Running server
 * @param category  Wave Role B category code (0-15)
 * @param data      Frame payload
 * @param len       Payload length
 * @return Number of clients sent to
 */
int zbu_server_route_category(zbu_server_t *server, uint8_t category,
                              const uint8_t *data, size_t len);

/*
 * zbu_server_stop — Close all connections and the listening socket.
 */
void zbu_server_stop(zbu_server_t *server);

/* ========================================================================
 * PUBLIC API — CLIENT (daemon side)
 * ======================================================================== */

/*
 * zbu_client_init — Initialize a client connection handle.
 */
int zbu_client_init(zbu_conn_t *conn);

/*
 * zbu_client_connect — Connect to the bus server.
 *
 * @param conn  Initialized client handle
 * @param path  Socket path (must match server's path)
 * @return ZBU_OK on success, ZBU_ERR_CONNECT on failure
 */
int zbu_client_connect(zbu_conn_t *conn, const char *path);

/*
 * zbu_client_send — Send a BitPads frame to the bus.
 *
 * @param conn  Connected client
 * @param data  Frame payload
 * @param len   Payload length (1–ZBU_MAX_FRAME)
 * @return ZBU_OK on success
 */
int zbu_client_send(zbu_conn_t *conn, const uint8_t *data, size_t len);

/*
 * zbu_client_recv — Receive a frame from the bus (non-blocking).
 *
 * @param conn   Connected client
 * @param frame  Output: received frame
 * @return ZBU_OK if frame available, ZBU_ERR_AGAIN if incomplete/none
 */
int zbu_client_recv(zbu_conn_t *conn, zbu_frame_t *frame);

/*
 * zbu_client_subscribe — Subscribe to a Wave Role B category.
 *
 * Sends a subscription request frame to the server.
 *
 * @param conn      Connected client
 * @param category  Category code (0-15) to subscribe to
 * @return ZBU_OK on success
 */
int zbu_client_subscribe(zbu_conn_t *conn, uint8_t category);

/*
 * zbu_client_disconnect — Gracefully close the connection.
 */
void zbu_client_disconnect(zbu_conn_t *conn);

/* ========================================================================
 * PUBLIC API — FRAME HELPERS
 * ======================================================================== */

/*
 * zbu_frame_encode — Wrap payload in length-prefixed wire format.
 *
 * @param payload   BitPads frame data
 * @param pay_len   Payload length
 * @param out       Output buffer (must be pay_len + 2 bytes)
 * @param out_len   Output: total wire bytes written
 * @return ZBU_OK, or ZBU_ERR_SIZE if payload > ZBU_MAX_FRAME
 */
int zbu_frame_encode(const uint8_t *payload, size_t pay_len,
                     uint8_t *out, size_t *out_len);

/*
 * zbu_frame_decode — Extract payload from length-prefixed wire data.
 *
 * @param wire      Wire data (length prefix + payload)
 * @param wire_len  Available bytes in wire buffer
 * @param frame     Output: extracted frame
 * @return ZBU_OK if complete frame extracted,
 *         ZBU_ERR_AGAIN if not enough bytes yet,
 *         ZBU_ERR_SIZE if declared length exceeds max
 */
int zbu_frame_decode(const uint8_t *wire, size_t wire_len,
                     zbu_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* ZAKO_BUS_H */

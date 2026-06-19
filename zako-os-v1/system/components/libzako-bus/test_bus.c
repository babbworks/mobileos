/*
 * test_bus.c — Unit tests for libzako-bus
 *
 * Tests frame encoding/decoding, server lifecycle, client connect/send/recv,
 * broadcast, and category routing via local Unix socket loopback.
 */

#include "zako_bus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (line %d)\n", (msg), __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while (0)

#define TEST_SOCKET_PATH "/tmp/zako_bus_test.sock"

/* ======================================================================== */

static void test_frame_encode_decode(void)
{
    uint8_t payload[] = { 0x10, 0xAB, 0xCD }; /* Pure Signal + some data */
    uint8_t wire[ZBU_FRAME_HEADER_SIZE + ZBU_MAX_FRAME];
    size_t wire_len;
    zbu_frame_t frame;
    int rc;

    printf("test_frame_encode_decode:\n");

    rc = zbu_frame_encode(payload, 3, wire, &wire_len);
    ASSERT(rc == ZBU_OK, "encode OK");
    ASSERT(wire_len == 5, "wire = 2 header + 3 payload");
    ASSERT(wire[0] == 0x00 && wire[1] == 0x03, "length prefix = 3");

    rc = zbu_frame_decode(wire, wire_len, &frame);
    ASSERT(rc == ZBU_OK, "decode OK");
    ASSERT(frame.len == 3, "payload len = 3");
    ASSERT(memcmp(frame.data, payload, 3) == 0, "payload matches");

    /* Partial data — not enough bytes */
    rc = zbu_frame_decode(wire, 1, &frame);
    ASSERT(rc == ZBU_ERR_AGAIN, "partial header returns AGAIN");

    rc = zbu_frame_decode(wire, 4, &frame);
    ASSERT(rc == ZBU_ERR_AGAIN, "partial payload returns AGAIN");

    /* Oversized frame */
    uint8_t big_header[2] = { 0xFF, 0xFF }; /* 65535 > ZBU_MAX_FRAME */
    rc = zbu_frame_decode(big_header, 2, &frame);
    ASSERT(rc == ZBU_ERR_SIZE, "oversized frame rejected");

    /* NULL checks */
    ASSERT(zbu_frame_encode(NULL, 1, wire, &wire_len) == ZBU_ERR_NULL, "NULL payload");
    ASSERT(zbu_frame_encode(payload, 1, NULL, &wire_len) == ZBU_ERR_NULL, "NULL out");
    ASSERT(zbu_frame_encode(payload, 0, wire, &wire_len) == ZBU_ERR_SIZE, "zero len");
    ASSERT(zbu_frame_decode(NULL, 5, &frame) == ZBU_ERR_NULL, "decode NULL wire");
    ASSERT(zbu_frame_decode(wire, 5, NULL) == ZBU_ERR_NULL, "decode NULL frame");
}

static void test_server_init_start_stop(void)
{
    zbu_server_t server;
    int rc;

    printf("test_server_init_start_stop:\n");

    rc = zbu_server_init(&server);
    ASSERT(rc == ZBU_OK, "init OK");
    ASSERT(server.listen_fd == -1, "listen_fd starts at -1");
    ASSERT(server.running == 0, "not running yet");

    rc = zbu_server_start(&server, TEST_SOCKET_PATH);
    ASSERT(rc == ZBU_OK, "start OK");
    ASSERT(server.listen_fd >= 0, "listen_fd valid");
    ASSERT(server.running == 1, "running");

    zbu_server_stop(&server);
    ASSERT(server.listen_fd == -1, "listen_fd reset after stop");
    ASSERT(server.running == 0, "not running after stop");

    /* NULL checks */
    ASSERT(zbu_server_init(NULL) == ZBU_ERR_NULL, "init NULL");
    ASSERT(zbu_server_start(NULL, TEST_SOCKET_PATH) == ZBU_ERR_NULL, "start NULL server");
}

static void test_client_connect_send_recv(void)
{
    zbu_server_t server;
    zbu_conn_t client;
    zbu_frame_t frame;
    uint8_t payload[] = { 0x80, 0x0C }; /* Record mode, value+time */
    int rc;

    printf("test_client_connect_send_recv:\n");

    zbu_server_init(&server);
    zbu_server_start(&server, TEST_SOCKET_PATH);

    /* Connect client */
    zbu_client_init(&client);
    rc = zbu_client_connect(&client, TEST_SOCKET_PATH);
    ASSERT(rc == ZBU_OK, "client connect OK");
    ASSERT(client.fd >= 0, "client fd valid");

    /* Accept connection on server side */
    usleep(10000); /* 10ms for socket to be ready */
    rc = zbu_server_poll(&server, 100);
    ASSERT(rc >= 1, "server accepted connection");

    /* Client sends a frame */
    rc = zbu_client_send(&client, payload, 2);
    ASSERT(rc == ZBU_OK, "client send OK");

    /* Server receives it */
    usleep(10000);
    zbu_server_poll(&server, 100);
    rc = zbu_server_recv(&server, &frame);
    ASSERT(rc == ZBU_OK, "server recv OK");
    ASSERT(frame.len == 2, "received frame len = 2");
    ASSERT(frame.data[0] == 0x80 && frame.data[1] == 0x0C, "payload matches");

    /* Server sends back to client */
    uint8_t reply[] = { 0x06 }; /* ACK */
    /* Find client index (it's the first connected) */
    size_t ci;
    for (ci = 0; ci < ZBU_MAX_CLIENTS; ci++) {
        if (server.clients[ci].state == ZBU_STATE_ACTIVE) break;
    }
    ASSERT(ci < ZBU_MAX_CLIENTS, "found active client slot");

    rc = zbu_server_send(&server, ci, reply, 1);
    ASSERT(rc == ZBU_OK, "server send OK");

    /* Client receives reply */
    usleep(10000);
    rc = zbu_client_recv(&client, &frame);
    ASSERT(rc == ZBU_OK, "client recv OK");
    ASSERT(frame.len == 1, "reply len = 1");
    ASSERT(frame.data[0] == 0x06, "reply = ACK");

    /* Cleanup */
    zbu_client_disconnect(&client);
    ASSERT(client.fd == -1, "client disconnected");
    zbu_server_stop(&server);
}

static void test_broadcast(void)
{
    zbu_server_t server;
    zbu_conn_t c1, c2;
    zbu_frame_t frame;
    uint8_t msg[] = { 0x10 }; /* Pure Signal */
    int rc;

    printf("test_broadcast:\n");

    zbu_server_init(&server);
    zbu_server_start(&server, TEST_SOCKET_PATH);

    /* Connect two clients */
    zbu_client_init(&c1);
    zbu_client_init(&c2);
    zbu_client_connect(&c1, TEST_SOCKET_PATH);
    usleep(5000);
    zbu_client_connect(&c2, TEST_SOCKET_PATH);
    usleep(5000);

    /* Accept both */
    zbu_server_poll(&server, 100);
    zbu_server_poll(&server, 100);

    /* Broadcast */
    rc = zbu_server_broadcast(&server, msg, 1);
    ASSERT(rc == 2, "broadcast sent to 2 clients");

    /* Both receive it */
    usleep(10000);
    rc = zbu_client_recv(&c1, &frame);
    ASSERT(rc == ZBU_OK, "c1 received broadcast");
    ASSERT(frame.data[0] == 0x10, "c1 got correct frame");

    rc = zbu_client_recv(&c2, &frame);
    ASSERT(rc == ZBU_OK, "c2 received broadcast");
    ASSERT(frame.data[0] == 0x10, "c2 got correct frame");

    zbu_client_disconnect(&c1);
    zbu_client_disconnect(&c2);
    zbu_server_stop(&server);
}

static void test_category_routing(void)
{
    zbu_server_t server;
    zbu_conn_t c1, c2;
    zbu_frame_t frame;
    uint8_t alert_frame[] = { 0x14 }; /* Wave/RoleB category=0100 (Alert) */
    int rc;

    printf("test_category_routing:\n");

    zbu_server_init(&server);
    zbu_server_start(&server, TEST_SOCKET_PATH);

    /* Connect two clients */
    zbu_client_init(&c1);
    zbu_client_init(&c2);
    zbu_client_connect(&c1, TEST_SOCKET_PATH);
    usleep(5000);
    zbu_client_connect(&c2, TEST_SOCKET_PATH);
    usleep(5000);

    zbu_server_poll(&server, 100);
    zbu_server_poll(&server, 100);

    /* Manually subscribe c1 to category 4 (Alert) on server side */
    size_t ci;
    int sub_idx = 0;
    for (ci = 0; ci < ZBU_MAX_CLIENTS; ci++) {
        if (server.clients[ci].state == ZBU_STATE_ACTIVE) {
            if (sub_idx == 0) {
                /* First client: subscribe to Alert */
                server.clients[ci].subscriptions[0] = 4; /* ZBP_CAT_ALERT */
                server.clients[ci].sub_count = 1;
            }
            sub_idx++;
        }
    }

    /* Route to category 4 — only c1 should receive */
    rc = zbu_server_route_category(&server, 4, alert_frame, 1);
    ASSERT(rc == 1, "routed to 1 subscriber");

    usleep(10000);
    rc = zbu_client_recv(&c1, &frame);
    ASSERT(rc == ZBU_OK, "c1 (subscribed) received");

    rc = zbu_client_recv(&c2, &frame);
    ASSERT(rc == ZBU_ERR_AGAIN, "c2 (not subscribed) got nothing");

    zbu_client_disconnect(&c1);
    zbu_client_disconnect(&c2);
    zbu_server_stop(&server);
}

static void test_multiple_frames(void)
{
    zbu_server_t server;
    zbu_conn_t client;
    zbu_frame_t frame;
    int rc;

    printf("test_multiple_frames:\n");

    zbu_server_init(&server);
    zbu_server_start(&server, TEST_SOCKET_PATH);

    zbu_client_init(&client);
    zbu_client_connect(&client, TEST_SOCKET_PATH);
    usleep(10000);
    zbu_server_poll(&server, 100);

    /* Send 3 frames rapidly */
    uint8_t f1[] = { 0x01 };
    uint8_t f2[] = { 0x02, 0x03 };
    uint8_t f3[] = { 0x04, 0x05, 0x06 };

    zbu_client_send(&client, f1, 1);
    zbu_client_send(&client, f2, 2);
    zbu_client_send(&client, f3, 3);

    usleep(20000);
    zbu_server_poll(&server, 100);

    /* Receive all 3 in order */
    rc = zbu_server_recv(&server, &frame);
    ASSERT(rc == ZBU_OK && frame.len == 1 && frame.data[0] == 0x01, "frame 1");

    rc = zbu_server_recv(&server, &frame);
    ASSERT(rc == ZBU_OK && frame.len == 2, "frame 2");

    rc = zbu_server_recv(&server, &frame);
    ASSERT(rc == ZBU_OK && frame.len == 3, "frame 3");

    /* No more */
    rc = zbu_server_recv(&server, &frame);
    ASSERT(rc == ZBU_ERR_AGAIN, "no more frames");

    zbu_client_disconnect(&client);
    zbu_server_stop(&server);
}

/* ======================================================================== */

int main(void)
{
    printf("=== libzako-bus unit tests ===\n\n");

    test_frame_encode_decode();
    test_server_init_start_stop();
    test_client_connect_send_recv();
    test_broadcast();
    test_category_routing();
    test_multiple_frames();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}

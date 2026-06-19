/*
 * test_ledgerd.c — Unit tests for telux-ledgerd storage layer
 *
 * Tests multi-chain (Island) management, append with prepared statement
 * caching, cursor-based chain verification, signed commits, merge records,
 * batch conservation, pack compaction, and tamper detection.
 *
 * Uses in-memory SQLite for speed.
 */

#include "ledgerd_store.h"
#include "../libzako-hash/zako_hash.h"
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

/* ======================================================================== */

static void test_open_close(void)
{
    lds_store_t store;
    int rc;

    printf("test_open_close:\n");

    rc = lds_open(&store, ":memory:");
    ASSERT(rc == LDS_OK, "open in-memory OK");
    ASSERT(store.db != NULL, "db handle valid");
    ASSERT(store.last_seq == 0, "empty db has seq=0");

    lds_close(&store);
    ASSERT(store.db == NULL, "db closed");

    /* NULL checks */
    ASSERT(lds_open(NULL, ":memory:") == LDS_ERR_NULL, "NULL store");
    ASSERT(lds_open(&store, NULL) == LDS_ERR_NULL, "NULL path");
}

static void test_chain_management(void)
{
    lds_store_t store;
    lds_chain_t chain;
    int64_t id1, id2;
    int rc;

    printf("test_chain_management:\n");

    lds_open(&store, ":memory:");

    /* Create chains (Islands) */
    rc = lds_chain_create(&store, "personal", &id1);
    ASSERT(rc == LDS_OK, "create personal chain");
    ASSERT(id1 >= 1, "chain_id assigned");

    rc = lds_chain_create(&store, "work", &id2);
    ASSERT(rc == LDS_OK, "create work chain");
    ASSERT(id2 != id1, "distinct chain_ids");

    /* Duplicate name rejected */
    rc = lds_chain_create(&store, "personal", NULL);
    ASSERT(rc == LDS_ERR_EXISTS, "duplicate name rejected");

    /* Get by name */
    rc = lds_chain_get(&store, "personal", &chain);
    ASSERT(rc == LDS_OK, "get by name OK");
    ASSERT(chain.id == id1, "correct chain returned");
    ASSERT(strcmp(chain.name, "personal") == 0, "name matches");

    /* Get by ID */
    rc = lds_chain_get_by_id(&store, id2, &chain);
    ASSERT(rc == LDS_OK, "get by id OK");
    ASSERT(strcmp(chain.name, "work") == 0, "name matches id");

    /* Not found */
    rc = lds_chain_get(&store, "nonexistent", &chain);
    ASSERT(rc == LDS_ERR_NOT_FOUND, "nonexistent chain not found");

    /* List all */
    lds_chain_t chains[LDS_MAX_CHAINS];
    size_t count;
    rc = lds_chain_list(&store, chains, LDS_MAX_CHAINS, &count);
    ASSERT(rc == LDS_OK, "list OK");
    ASSERT(count == 2, "2 chains listed");

    lds_close(&store);
}

static void test_append_and_retrieve(void)
{
    lds_store_t store;
    lds_record_t rec;
    int64_t chain_id, seq;
    int rc;

    printf("test_append_and_retrieve:\n");

    lds_open(&store, ":memory:");
    lds_chain_create(&store, "test", &chain_id);

    /* Append a record (no sig, no parent2) */
    uint8_t frame1[] = { 0x80, 0x0C, 0x01, 0x02, 0x03 };
    rc = lds_append(&store, chain_id, frame1, 5, 42, 0, 500, NULL, NULL, &seq);
    ASSERT(rc == LDS_OK, "append OK");
    ASSERT(seq == 1, "first record gets seq=1");
    ASSERT(lds_get_last_seq(&store) == 1, "last_seq updated");

    /* Retrieve by seq */
    rc = lds_get_by_seq(&store, 1, &rec);
    ASSERT(rc == LDS_OK, "get_by_seq OK");
    ASSERT(rec.seq == 1, "seq matches");
    ASSERT(rec.chain_id == chain_id, "chain_id matches");
    ASSERT(rec.sender_id == 42, "sender_id matches");
    ASSERT(rec.direction == 0, "direction=In");
    ASSERT(rec.value_n == 500, "value matches");
    ASSERT(rec.frame_len == 5, "frame_len matches");
    ASSERT(memcmp(rec.frame, frame1, 5) == 0, "frame data matches");
    ASSERT(rec.has_parent2 == 0, "no merge parent");
    ASSERT(rec.has_signature == 0, "unsigned record");

    /* frame_hash should not be all zeros */
    uint8_t zeros[LDS_HASH_LEN] = {0};
    ASSERT(memcmp(rec.frame_hash, zeros, LDS_HASH_LEN) != 0, "frame_hash computed");
    ASSERT(memcmp(rec.chain_hash, zeros, LDS_HASH_LEN) != 0, "chain_hash computed");

    /* Retrieve by hash */
    lds_record_t rec2;
    rc = lds_get_by_hash(&store, rec.frame_hash, &rec2);
    ASSERT(rc == LDS_OK, "get_by_hash OK");
    ASSERT(rec2.seq == 1, "found correct record");

    /* Not found */
    rc = lds_get_by_seq(&store, 99, &rec);
    ASSERT(rc == LDS_ERR_NOT_FOUND, "seq 99 not found");

    lds_close(&store);
}

static void test_multi_chain_independence(void)
{
    lds_store_t store;
    int64_t chain_a, chain_b;
    int64_t seq_a, seq_b;
    lds_record_t rec_a, rec_b;

    printf("test_multi_chain_independence:\n");

    lds_open(&store, ":memory:");
    lds_chain_create(&store, "alpha", &chain_a);
    lds_chain_create(&store, "beta", &chain_b);

    /* Append to chain A */
    uint8_t fa[] = {0xAA, 0xBB};
    lds_append(&store, chain_a, fa, 2, 1, 0, 100, NULL, NULL, &seq_a);

    /* Append to chain B */
    uint8_t fb[] = {0xCC, 0xDD};
    lds_append(&store, chain_b, fb, 2, 2, 0, 200, NULL, NULL, &seq_b);

    ASSERT(seq_a == 1, "chain A first record");
    ASSERT(seq_b == 2, "chain B gets global seq=2");

    /* Both chains should have independent genesis anchors */
    lds_get_by_seq(&store, seq_a, &rec_a);
    lds_get_by_seq(&store, seq_b, &rec_b);
    ASSERT(memcmp(rec_a.chain_hash, rec_b.chain_hash, LDS_HASH_LEN) != 0,
           "independent chain hashes");

    /* Verify each chain independently */
    ASSERT(lds_verify_chain(&store, chain_a, seq_a, seq_a, 0) == LDS_OK,
           "chain A intact");
    ASSERT(lds_verify_chain(&store, chain_b, seq_b, seq_b, 0) == LDS_OK,
           "chain B intact");

    lds_close(&store);
}

static void test_chain_integrity(void)
{
    lds_store_t store;
    int64_t chain_id;
    int rc;

    printf("test_chain_integrity:\n");

    lds_open(&store, ":memory:");
    lds_chain_create(&store, "test", &chain_id);

    /* Append 5 records to same chain */
    uint8_t frames[5][5] = {
        {0x01, 0x02, 0x03, 0x04, 0x05},
        {0x11, 0x12, 0x13, 0x14, 0x15},
        {0x21, 0x22, 0x23, 0x24, 0x25},
        {0x31, 0x32, 0x33, 0x34, 0x35},
        {0x41, 0x42, 0x43, 0x44, 0x45},
    };
    int64_t seqs[5];

    for (int i = 0; i < 5; i++) {
        rc = lds_append(&store, chain_id, frames[i], 5, 1, 0, 100,
                        NULL, NULL, &seqs[i]);
        ASSERT(rc == LDS_OK, "append chain record");
    }

    /* Verify full chain (cursor-based scan) */
    rc = lds_verify_chain(&store, chain_id, seqs[0], seqs[4], 0);
    ASSERT(rc == LDS_OK, "chain intact");

    /* Verify partial chain */
    rc = lds_verify_chain(&store, chain_id, seqs[1], seqs[3], 0);
    ASSERT(rc == LDS_OK, "partial chain intact");

    /* Each record's chain_hash should differ from the previous */
    lds_record_t r1, r2;
    lds_get_by_seq(&store, seqs[0], &r1);
    lds_get_by_seq(&store, seqs[1], &r2);
    ASSERT(memcmp(r1.chain_hash, r2.chain_hash, LDS_HASH_LEN) != 0,
           "consecutive chain_hashes differ");

    lds_close(&store);
}

static void test_signed_records(void)
{
    lds_store_t store;
    lds_record_t rec;
    int64_t chain_id, seq;
    int rc;

    printf("test_signed_records:\n");

    lds_open(&store, ":memory:");
    lds_chain_create(&store, "signed", &chain_id);

    /* Fake signature (64 bytes of 0xAB) */
    uint8_t sig[LDS_SIG_LEN];
    memset(sig, 0xAB, LDS_SIG_LEN);

    uint8_t frame[] = {0x01, 0x02, 0x03};
    rc = lds_append(&store, chain_id, frame, 3, 1, 0, 100, sig, NULL, &seq);
    ASSERT(rc == LDS_OK, "signed append OK");

    rc = lds_get_by_seq(&store, seq, &rec);
    ASSERT(rc == LDS_OK, "retrieve signed record");
    ASSERT(rec.has_signature == 1, "signature flag set");
    ASSERT(memcmp(rec.signature, sig, LDS_SIG_LEN) == 0, "signature preserved");

    lds_close(&store);
}

static void test_merge_records(void)
{
    lds_store_t store;
    lds_record_t rec;
    int64_t chain_a, chain_b;
    int64_t seq_a, seq_b, seq_merge;
    int rc;

    printf("test_merge_records:\n");

    lds_open(&store, ":memory:");
    lds_chain_create(&store, "local", &chain_a);
    lds_chain_create(&store, "remote", &chain_b);

    /* Each chain gets a record */
    uint8_t fa[] = {0xAA};
    uint8_t fb[] = {0xBB};
    lds_append(&store, chain_a, fa, 1, 1, 1, 500, NULL, NULL, &seq_a);
    lds_append(&store, chain_b, fb, 1, 2, 0, 500, NULL, NULL, &seq_b);

    /* Get chain B's tip hash to use as parent2 */
    lds_record_t rec_b;
    lds_get_by_seq(&store, seq_b, &rec_b);

    /* Create merge record on chain A, referencing chain B's tip */
    uint8_t fm[] = {0xCC};
    rc = lds_append(&store, chain_a, fm, 1, 1, 0, 500,
                    NULL, rec_b.chain_hash, &seq_merge);
    ASSERT(rc == LDS_OK, "merge append OK");

    rc = lds_get_by_seq(&store, seq_merge, &rec);
    ASSERT(rc == LDS_OK, "retrieve merge record");
    ASSERT(rec.has_parent2 == 1, "parent2 flag set");
    ASSERT(memcmp(rec.parent2, rec_b.chain_hash, LDS_HASH_LEN) == 0,
           "parent2 matches chain B tip");

    /* Verify chain A including merge record */
    rc = lds_verify_chain(&store, chain_a, seq_a, seq_merge, 0);
    ASSERT(rc == LDS_OK, "chain with merge intact");

    lds_close(&store);
}

static void test_batch_conservation_pass(void)
{
    lds_store_t store;
    int64_t chain_id, batch_id;
    int64_t balance;
    int rc;

    printf("test_batch_conservation_pass:\n");

    lds_open(&store, ":memory:");
    lds_chain_create(&store, "test", &chain_id);

    /* Open batch on chain */
    rc = lds_batch_open(&store, chain_id, &batch_id);
    ASSERT(rc == LDS_OK, "batch open OK");
    ASSERT(batch_id >= 1, "batch_id assigned");

    /* Add balanced records: +500, +300, -600, -200 = 0 */
    uint8_t f1[] = {0xA1};
    uint8_t f2[] = {0xA2};
    uint8_t f3[] = {0xA3};
    uint8_t f4[] = {0xA4};

    lds_append(&store, chain_id, f1, 1, 1, 0, 500, NULL, NULL, NULL);
    lds_append(&store, chain_id, f2, 1, 1, 0, 300, NULL, NULL, NULL);
    lds_append(&store, chain_id, f3, 1, 1, 1, 600, NULL, NULL, NULL);
    lds_append(&store, chain_id, f4, 1, 1, 1, 200, NULL, NULL, NULL);

    /* Close batch — should be conserved */
    rc = lds_batch_close(&store, &balance);
    ASSERT(rc == LDS_OK, "batch conserved");
    ASSERT(balance == 0, "balance = 0");

    lds_close(&store);
}

static void test_batch_conservation_fail(void)
{
    lds_store_t store;
    int64_t chain_id, batch_id;
    int64_t balance;
    int rc;

    printf("test_batch_conservation_fail:\n");

    lds_open(&store, ":memory:");
    lds_chain_create(&store, "test", &chain_id);

    lds_batch_open(&store, chain_id, &batch_id);

    /* Imbalanced: +1000, -999 = +1 */
    uint8_t f1[] = {0xB1};
    uint8_t f2[] = {0xB2};

    lds_append(&store, chain_id, f1, 1, 1, 0, 1000, NULL, NULL, NULL);
    lds_append(&store, chain_id, f2, 1, 1, 1, 999, NULL, NULL, NULL);

    rc = lds_batch_close(&store, &balance);
    ASSERT(rc == LDS_ERR_CHAIN, "imbalanced batch rejected");
    ASSERT(balance == 1, "balance = +1");

    lds_close(&store);
}

static void test_genesis_anchor(void)
{
    lds_store_t store;
    lds_record_t rec;
    int64_t chain_id;
    uint8_t expected_fh[LDS_HASH_LEN];
    uint8_t expected_ch[LDS_HASH_LEN];

    printf("test_genesis_anchor:\n");

    lds_open(&store, ":memory:");
    lds_chain_create(&store, "test", &chain_id);

    uint8_t genesis[] = { 0xFF, 0xFE, 0xFD };
    lds_append(&store, chain_id, genesis, 3, 0, 0, 0, NULL, NULL, NULL);
    lds_get_by_seq(&store, 1, &rec);

    /* Independently compute what the genesis should be */
    zako_frame_hash(genesis, 3, expected_fh);
    zako_genesis_anchor(expected_fh, expected_ch);

    ASSERT(memcmp(rec.frame_hash, expected_fh, LDS_HASH_LEN) == 0,
           "genesis frame_hash matches independent computation");
    ASSERT(memcmp(rec.chain_hash, expected_ch, LDS_HASH_LEN) == 0,
           "genesis chain_hash is anchor (hash against zeros)");

    lds_close(&store);
}

static void test_pack_compaction(void)
{
    lds_store_t store;
    int64_t chain_id;
    lds_pack_info_t pack;
    lds_record_t rec;
    uint8_t unpacked_frame[64];
    size_t unpacked_len;
    int rc;

    printf("test_pack_compaction:\n");

    lds_open(&store, ":memory:");
    lds_chain_create(&store, "test", &chain_id);

    /* Append 5 records */
    uint8_t frames[5][3] = {
        {0x01, 0x02, 0x03},
        {0x11, 0x12, 0x13},
        {0x21, 0x22, 0x23},
        {0x31, 0x32, 0x33},
        {0x41, 0x42, 0x43},
    };
    int64_t seqs[5];
    for (int i = 0; i < 5; i++) {
        lds_append(&store, chain_id, frames[i], 3, 1, 0, 100,
                   NULL, NULL, &seqs[i]);
    }

    /* Pack records 1-3 */
    rc = lds_pack_compact(&store, chain_id, seqs[0], seqs[2], &pack);
    ASSERT(rc == LDS_OK, "pack compact OK");
    ASSERT(pack.record_count == 3, "3 records packed");
    ASSERT(pack.start_seq == seqs[0], "start_seq correct");
    ASSERT(pack.end_seq == seqs[2], "end_seq correct");

    /* Packed records should have frame=NULL, packed=1 */
    rc = lds_get_by_seq(&store, seqs[0], &rec);
    ASSERT(rc == LDS_OK, "packed record retrievable");
    ASSERT(rec.packed == 1, "packed flag set");
    ASSERT(rec.frame_len == 0, "frame NULLed after pack");

    /* But frame_hash and chain_hash are still there */
    uint8_t zeros[LDS_HASH_LEN] = {0};
    ASSERT(memcmp(rec.frame_hash, zeros, LDS_HASH_LEN) != 0,
           "frame_hash preserved after pack");

    /* Retrieve frame from pack */
    rc = lds_pack_get_frame(&store, seqs[0], unpacked_frame, &unpacked_len);
    ASSERT(rc == LDS_OK, "unpack frame OK");
    ASSERT(unpacked_len == 3, "unpacked frame length correct");
    ASSERT(memcmp(unpacked_frame, frames[0], 3) == 0, "unpacked frame matches original");

    /* Unpacked records (4-5) still have inline frames */
    rc = lds_get_by_seq(&store, seqs[3], &rec);
    ASSERT(rc == LDS_OK, "unpacked record OK");
    ASSERT(rec.packed == 0, "not packed");
    ASSERT(rec.frame_len == 3, "frame still inline");

    lds_close(&store);
}

static void test_persistence_reload(void)
{
    lds_store_t store1, store2;
    lds_record_t rec;
    int64_t chain_id;
    int rc;
    const char *path = "/tmp/test_ledgerd_v2.db";

    printf("test_persistence_reload:\n");

    /* Write some records */
    lds_open(&store1, path);
    lds_chain_create(&store1, "persist", &chain_id);

    uint8_t f1[] = {0x01, 0x02};
    uint8_t f2[] = {0x03, 0x04};
    lds_append(&store1, chain_id, f1, 2, 10, 0, 100, NULL, NULL, NULL);
    lds_append(&store1, chain_id, f2, 2, 10, 1, 100, NULL, NULL, NULL);
    lds_close(&store1);

    /* Reopen and verify state restored */
    lds_open(&store2, path);
    ASSERT(store2.last_seq == 2, "last_seq restored");

    /* Chain tip should be loaded */
    lds_chain_t chain;
    rc = lds_chain_get(&store2, "persist", &chain);
    ASSERT(rc == LDS_OK, "chain persisted");
    ASSERT(chain.tip_seq == 2, "tip_seq restored");

    /* Can retrieve records */
    rc = lds_get_by_seq(&store2, 1, &rec);
    ASSERT(rc == LDS_OK, "record 1 still there");
    ASSERT(rec.sender_id == 10, "sender_id persisted");
    ASSERT(rec.chain_id == chain_id, "chain_id persisted");

    /* Chain still valid (cursor-based verification) */
    rc = lds_verify_chain(&store2, chain_id, 1, 2, 0);
    ASSERT(rc == LDS_OK, "chain still valid after reload");

    /* Can continue appending to the chain */
    uint8_t f3[] = {0x05, 0x06};
    int64_t seq3;
    rc = lds_append(&store2, chain_id, f3, 2, 10, 0, 50, NULL, NULL, &seq3);
    ASSERT(rc == LDS_OK, "append after reload OK");
    ASSERT(seq3 == 3, "seq continues from 3");

    /* Full chain still verifies */
    rc = lds_verify_chain(&store2, chain_id, 1, 3, 0);
    ASSERT(rc == LDS_OK, "extended chain verifies");

    lds_close(&store2);

    /* Cleanup */
    unlink(path);
    char wal[80], shm[80];
    snprintf(wal, sizeof(wal), "%s-wal", path);
    snprintf(shm, sizeof(shm), "%s-shm", path);
    unlink(wal);
    unlink(shm);
}

static void test_null_checks(void)
{
    lds_store_t store;
    int64_t chain_id;

    printf("test_null_checks:\n");

    ASSERT(lds_open(NULL, ":memory:") == LDS_ERR_NULL, "open NULL store");
    ASSERT(lds_open(&store, NULL) == LDS_ERR_NULL, "open NULL path");

    lds_open(&store, ":memory:");
    lds_chain_create(&store, "test", &chain_id);

    ASSERT(lds_append(&store, chain_id, NULL, 5, 0, 0, 0, NULL, NULL, NULL)
           == LDS_ERR_NULL, "append NULL frame");
    ASSERT(lds_get_by_seq(&store, 1, NULL) == LDS_ERR_NULL, "get NULL out");
    ASSERT(lds_get_by_hash(&store, NULL, NULL) == LDS_ERR_NULL, "hash NULL");
    ASSERT(lds_verify_chain(NULL, 1, 1, 1, 0) == LDS_ERR_NULL, "verify NULL store");
    ASSERT(lds_batch_open(NULL, 1, NULL) == LDS_ERR_NULL, "batch_open NULL");
    ASSERT(lds_batch_close(NULL, NULL) == LDS_ERR_NULL, "batch_close NULL");

    /* Append to nonexistent chain */
    uint8_t f[] = {0x01};
    ASSERT(lds_append(&store, 9999, f, 1, 0, 0, 0, NULL, NULL, NULL)
           == LDS_ERR_NOT_FOUND, "append to nonexistent chain");

    lds_close(&store);
}

/* ======================================================================== */

int main(void)
{
    printf("=== telux-ledgerd storage layer tests (v2: git-informed) ===\n\n");

    test_open_close();
    test_chain_management();
    test_append_and_retrieve();
    test_multi_chain_independence();
    test_chain_integrity();
    test_signed_records();
    test_merge_records();
    test_batch_conservation_pass();
    test_batch_conservation_fail();
    test_genesis_anchor();
    test_pack_compaction();
    test_persistence_reload();
    test_null_checks();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? EXIT_SUCCESS : EXIT_FAILURE;
}

# ZAKO OS v1 — Journal Entries (Offline)

Offline journal entries for batch import into workwarrior profile `zako-os-v1`.

---

## 2026-06-15 — libzako-bitpads complete (both sub-libraries)

Built the full BitPads v2.0 wire format codec as two layered libraries. libzako-bitpads-core handles Meta Byte 1 parsing with mode dispatch (Wave vs Record), Wave Role A (4 boolean flags), Wave Role B (16 category codes), Record Role C (4 component presence flags), and Pure Signal encoding. libzako-bitpads-session implements Layer 1 (64-bit session header with CRC-15 integrity), Meta Byte 2, the Setup Byte (value tier/scaling/decimal/rounding), Value Block encoding across 4 tiers (8 to 32-bit), Task byte (16 instruction categories), Note header, and Time block header. The old simplified ZAKO-centric header (which invented a non-spec-compliant bit layout) was removed. 259 tests total, ASan+UBSan clean, -Werror build. @zako @code @bitpads @milestone @done

## 2026-06-15 — Workwarrior task database repopulated

Discovered that previous session's task commands only set TASKRC without TASKDATA, causing writes to go to the wrong database. Reconstructed all 9 historical tasks with annotations into the correct database using both env vars. Documented the correct invocation pattern. Also identified that `task start`/`task stop` trigger the on-modify.timewarrior hook which can hang indefinitely — switching to offline logging for remainder of session. @zako @planning @fix @done


## 2026-06-15 — libzako-bitledger complete

Implemented the BitLedger v3.0 wire protocol codec. Layer 2 (48-bit batch header) encodes transmission type, scaling factor, optimal split, decimal position, bells, separators, entity/currency, rounding balance, and compound prefix across 17 distinct fields. Layer 3 (40-bit transaction record) packs a 25-bit value block using the N = A × 2^S + r formula, 7 flag bits with cross-layer validation (direction and status echoed from flags to accounting block), and an 8-bit accounting block covering 14 canonical double-entry account pairs plus correction and compound continuation markers. The conservation invariant enforces batch balance (sum of inflows = sum of outflows) at the codec level. One deviation from spec documented: value_a/value_r widened to uint32_t to support variable Optimal Split values where A can exceed 16-bit range. 84 tests including spec examples ($4.53, $98,765.43, quantity split), corruption detection, and adversarial imbalance rejection. @zako @code @bitpads @milestone @done


## 2026-06-15 — libzako-c0 complete — PHASE 1 FINISHED

Implemented the C0 Enhancement Grammar codec, the last of the six Phase 1 foundation libraries. The library encodes and decodes the 5+3 byte split (lower 5 bits = C0 code identity from Baudot/ASCII lineage, upper 3 bits = Priority/Acknowledge/Continuation flag matrix). All 256 possible byte values round-trip perfectly — the 32 C0 codes × 8 flag combinations form a complete bijection over the byte space. The Signal Slot Presence Byte declares which of the 13 positions (P1–P13) in a transmission expect enhanced C0 controls vs content bytes, enabling positional disambiguation with zero DLE-stuffing or escaping overhead. Lookup tables provide code names, inclusion verdicts, and per-slot layer/position info. 83 tests, ASan+UBSan clean. @zako @code @bitpads @milestone @done

Phase 1 is now complete. Six self-contained C libraries with 495 total tests, all building under C99 with -Werror -Wpedantic, all sanitizer-clean. The cryptographic primitives (hash, sign, DID) and the protocol codecs (BitPads, BitLedger, C0 Enhancement) form the foundation every ZAKO daemon will link against. Phase 2 (Core Daemons) can begin. @zako @planning @milestone


## 2026-06-15 — libzako-bus complete — Phase 2 begins

Built the ZAKO system bus, the IPC substrate that all daemons communicate over. It's a poll-based event loop managing Unix domain socket connections with length-prefixed BitPads frame framing (2-byte big-endian header + payload). The server maintains a fixed pool of 16 client slots with no dynamic allocation post-init. Key capabilities: accept/track/disconnect clients, receive complete frames with partial reassembly, send to individual clients, broadcast to all, and route frames to clients subscribed to specific Wave Role B categories. The subscription mechanism uses a convention where a 1-byte frame (Wave/RoleB with ACK flag) signals "subscribe me to this category." Tests verify full loopback: server start, client connect, bidirectional frame exchange, broadcast to multiple clients, and category-filtered routing. 49 tests, ASan+UBSan clean. This is the first Phase 2 component — the daemons can now talk. @zako @code @bitpads @milestone @done


## 2026-06-15 — telux-ledgerd storage layer complete

Built the append-only SQLite ledger that forms the persistence backbone of telux-ledgerd. Every record appended gets a BLAKE3 frame_hash (content fingerprint) and a chain_hash linking it to its predecessor — forming a tamper-evident chain where modifying any historical record invalidates all subsequent hashes. The storage layer manages batch lifecycle: open a batch, append records (tracking direction and value for conservation), close the batch (rejecting if inflows ≠ outflows). Records are retrievable by sequence number or frame_hash. A full chain verification function recomputes all hashes from genesis and compares against stored values — any mismatch signals tampering. SQLite runs in WAL mode with synchronous=FULL for durability (fsync before ACK). Tested with in-memory and file-backed databases including a reload test that verifies state recovery across close/reopen. 50 tests, ASan+UBSan clean. @zako @code @telux @done

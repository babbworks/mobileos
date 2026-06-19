# Git as Substrate: An Overlay Narrative for ZAKO OS

## How Git's Internals Express the ZAKO System in Its Own Terms

---

### Prologue: The Object and the Record

ZAKO begins with the **record**. A BitPads frame — Meta Byte, Layer 1 session header, Layer 2 batch header, Layer 3 transaction body — is the atomic unit of meaning in the system. Every event that matters is encoded as a record: a payment, a capability grant, a power mode transition, a task signoff, an identity assertion. The record is sovereign because the Sovereign signed it.

Git begins with the **object**. A blob of bytes, identified not by where it lives or who put it there, but by the hash of its contents. Git has four object types: blobs (raw content), trees (directory listings), commits (pointers to trees with ancestry), and tags (named references to commits, optionally signed). Every object is immutable once written. Every object is retrievable by its hash alone.

The ZAKO record and the git object occupy the same architectural position. They are the atoms. What follows is the story of how every structure ZAKO builds from its atoms has a corresponding structure git builds from its own.

---

### 1. The Frame Hash Is the Blob

When `telux-ledgerd` receives a BitPads frame, the first thing it does is compute the **frame hash**: `BLAKE3(frame_bytes)`. This produces a 32-byte fingerprint that uniquely identifies the record by its content. Two identical frames produce identical hashes. The frame hash is the record's name — not assigned, but derived.

When git receives content, it computes `SHA-256(header + content)`. This produces the **object ID**. Two identical contents produce identical IDs. The object ID is the content's name — not assigned, but derived.

The `zako_frame_hash()` function and git's `hash-object` command are the same operation performed on the same kind of input for the same reason: to give content an identity that is intrinsic rather than imposed.

In ZAKO's terms: the frame hash establishes that *this record exists and says what it says*. In git's terms: the blob hash establishes that *this object exists and contains what it contains*. The verb is the same. The guarantee is the same.

---

### 2. The Chain Hash Is the Commit

ZAKO's tamper-evidence comes from **chain hashing**: `chain_hash[n] = BLAKE3(frame_hash[n] || chain_hash[n-1])`. Each record is welded to its predecessor. Modify any earlier record and its frame hash changes, which invalidates its chain hash, which invalidates every subsequent chain hash forward to the present. The chain is a one-way ratchet. It only moves forward. It cannot be edited without breaking.

Git's tamper-evidence comes from **commit hashing**: `commit_hash = SHA-256(tree_hash + parent_commit_hash + author + timestamp + message)`. Each commit incorporates the hash of its parent. Modify any earlier commit and its hash changes, which invalidates every descendant commit. The commit chain is a one-way ratchet. It only moves forward. It cannot be edited without breaking.

The structural identity is exact. ZAKO concatenates `frame_hash || prev_chain_hash` and hashes. Git concatenates `tree_hash + parent_hash + metadata` and hashes. Both produce a single value that encodes the entire history leading to this point. Both produce a value that can be verified by anyone who recomputes the chain from genesis.

In the ZAKO ledger, `lds_verify_chain(store, seq_start, seq_end)` walks the records table, recomputes each chain hash from its frame hash and predecessor, and compares. In git, `git fsck` walks the object graph, recomputes each commit hash from its components, and compares. They are the same audit.

The `ledgerd_store` schema — `records(seq, frame, frame_hash, chain_hash, sender_id, batch_id, ...)` — is a relational projection of what git stores natively as its commit graph. The `seq` column is the topological order. The `frame` column is the blob. The `frame_hash` is the blob hash. The `chain_hash` is the commit hash. The `sender_id` is the author. The `batch_id` is the branch context. SQLite is the container; git's object store could be the container instead, and the chain integrity would come free rather than being manually enforced.

---

### 3. The Genesis Anchor Is the Root Commit

Every ZAKO chain starts from a **genesis anchor**: `BLAKE3(frame_hash[0] || zeros_32)`. The first record has no predecessor, so its chain hash is computed against 32 zero bytes. This is deterministic — anyone with the genesis record can independently verify the anchor. No external reference is needed. No authority blesses the beginning.

Every git repository starts from a **root commit**: a commit with no parent field. Git represents this not with zeros but with the absence of a parent pointer. The hash of the root commit is determined entirely by its tree, author, and timestamp. Anyone with the root commit can verify it. No external reference is needed.

`zako_genesis_anchor()` and git's initial commit serve the same purpose: they are the fixed point from which the entire chain's integrity is measured. In ZAKO, every new Island begins with a genesis anchor. In git, every orphan branch begins with a parentless commit. The operations are synonymous.

---

### 4. The Island Is the Branch

ZAKO partitions the world into **Islands**: `/data/zako/islands/personal/`, `/data/zako/islands/work/`. Each Island is an independent record domain with its own genesis anchor, its own chain, its own sovereignty context. The personal Island tracks personal transactions. The work Island tracks field records, task signoffs, PADS operations. They do not share chain state. They cannot contaminate each other's integrity guarantees.

Git partitions the world into **branches**: `refs/heads/main`, `refs/heads/feature-x`. Each branch is an independent commit history. Branches can share common ancestry or be entirely independent (orphan branches). They do not share HEAD state. They cannot contaminate each other's history.

The correspondence is direct:

| ZAKO | Git |
|---|---|
| `personal` Island | `refs/islands/personal` |
| `work` Island | `refs/islands/work` |
| Island genesis anchor | Orphan branch root commit |
| New Island creation | `git checkout --orphan <island-name>` + genesis commit |
| Island record append | `git commit` on the Island's branch |
| Island chain verification | `git log --verify-signatures refs/islands/<name>` |

The `/data/zako/islands/` directory tree, with its per-Island subdirectories, maps onto git's refs namespace. The directory is the namespace. The branch tip is the latest chain hash. The branch history is the complete ledger for that Island.

Outstack's power ledger — currently conceived as `/data/zako/ledger/power.db` — fits the same model: `refs/ledgers/power`. Every MODE_CHANGE record, every GATE event, every thermal sample that triggered a transition becomes a commit on the power branch. The power history becomes auditable with the same tool that audits the financial ledger.

---

### 5. The Batch Is the Merge Base

BitLedger's **batch** is a bounded group of records that must conserve — the sum of all Plus/In and Minus/Out values within the batch must equal zero. `lds_batch_open()` begins the batch. Records accumulate. `lds_batch_close()` runs the conservation check. If the batch doesn't balance, it is rejected (NAK). If it balances, it is sealed.

In git, a **merge** is a point where two histories converge and their combined state must satisfy a condition (no conflicts, or in ZAKO's case, conservation). The batch-close is a merge commit: it takes the sequence of records added since batch-open and seals them with a conservation-verified commit. The merge commit's message (or a structured trailer) records the batch balance: zero. The merge commit's signature attests that the daemon verified conservation at close time.

The `zbl_conservation_check()` function — which sums all records and returns `ZBL_OK` if the sum is zero — is the merge strategy. Git allows custom merge strategies. A ZAKO merge strategy would be: accept the merge if and only if the conservation invariant holds. Reject otherwise. The machinery exists.

---

### 6. The Bilateral Exchange Is the Two-Parent Merge

The exchange engine manages the most delicate operation in ZAKO: the **bilateral exchange** between two Sovereigns. One party creates a SEND leg. The counterparty creates a RECEIVE leg. Both legs must conserve (values sum to zero). Both legs must be signed by their respective authors. Both must be posted atomically — both or neither.

In git, a **merge commit with two parents** is exactly this structure:

- **Parent A**: the local Sovereign's SEND commit (on our Island branch)
- **Parent B**: the counterparty's RECEIVE commit (on a remote-tracking branch representing their Island)
- **The merge commit itself**: the atomic settlement event, authored by the local device, containing the conservation proof

`exe_acknowledge()` — which creates our ACK leg, runs conservation, and calls `post_atomic()` to post both legs together — is the merge operation. The callbacks `post_atomic` (ledger write) and `verify_sig` (signature check) are the pre-merge hooks. If conservation fails, the merge is rejected. If signatures fail, the merge is rejected. If both pass, the merge commit is created and both parent histories are incorporated.

The exchange states map to merge states:

| Exchange State | Git Equivalent |
|---|---|
| `PENDING_OUTBOUND` | Local branch has a commit; awaiting remote to push their branch |
| `PENDING_INBOUND` | Remote branch fetched; merge not yet executed |
| `COMPLETED` | Merge commit created with two parents |
| `FAILED` | Merge rejected (conservation failed) |
| `CANCELLED` | Branch deleted before merge |

---

### 7. The DID Is the Signing Key, and Git Knows Signing Keys

ZAKO's identity model is: the key *is* the identity. `did:key:z6Mk...` encodes an ed25519 public key directly. No resolution server. No certificate authority. Anyone with the DID string can extract the public key and verify signatures. `telux-identd` manages the keys; `zako_sign` performs signing; `zako_did` formats the identity.

Git 2.34+ supports **ed25519 signing natively** via SSH keys. `git commit -S` signs a commit. `git verify-commit` checks the signature. The signing key is configured per-repository or globally. Git doesn't care where the key lives — it calls out to an agent (or reads a file) for the private key, and embeds the public key reference in the commit.

The ZAKO identity architecture maps onto git's signing infrastructure:

- The **sovereign keypair** (`ids_generate_sovereign`) is the repository's signing key
- **Per-Island derived keys** (`ids_generate_key` with label `"island:work"`) are per-branch signing identities
- **Signing a record** (`ids_sign`) is `git commit -S`
- **Verifying a signature** (`ids_verify`) is `git verify-commit`
- **Identity lock** (`ids_lock`) disables the signing agent — no commits can be signed until unlock
- **DID resolution** (`zako_did_to_pubkey`) is how git resolves allowed-signers — given a key ID, extract the public key and verify

The `identd_daemon` — which listens on the bus for SIGN, VERIFY, KEYGEN requests — is functionally a **git signing agent**. It holds the private keys, services signing requests from other daemons, and refuses to sign when locked. Git's `ssh-agent` protocol serves the same role. The daemon's request/response protocol (`[opcode][payload]` -> `[opcode|0x80][status][payload]`) parallels the SSH agent protocol's structure.

---

### 8. The Capability Grant Is the Signed Tag with Ancestry

ZAKO's capability system is a chain of trust: the Sovereign grants a capability to entity A at depth 0. Entity A can delegate to entity B at depth 1. Entity B can delegate to entity C at depth 2. Maximum depth is 3. Revocation cascades: revoking the depth-0 grant invalidates all downstream delegations.

Git's **signed tags** and **signed commits** form the same chain:

- A **signed tag** by the Sovereign on a specific commit is a depth-0 grant: "I, the Sovereign, assert that this commit (and the state it represents) is authorized."
- A **signed commit** by a delegatee, whose parent is the Sovereign's tagged commit, is a depth-1 delegation: "I, entity A (authorized by the Sovereign), extend this authorization to the following state."
- Ancestry depth = delegation depth. `git log --ancestry-path sovereign-tag..current-commit` gives the chain. Count the commits = count the delegation depth.

`ids_revoke()` with cascade is equivalent to **tagging the revocation point**: a signed commit on a `refs/capabilities/` branch that records the revocation. All commits descended from the revoked grant are invalidated — not by deleting them (append-only), but by the presence of the revocation record later in the chain. Any verification walk that encounters the revocation record before reaching the capability in question returns `IDS_ERR_REVOKED`.

`ids_check_capability()` — which walks the capability chain from the queried DID back to the Sovereign, checking depth and revocation at each level — is `git merge-base --is-ancestor` combined with signature verification at each step.

---

### 9. The System Bus Carries Frames; Git Bundles Carry History

`libzako-bus` is the runtime IPC layer: Unix domain sockets, length-prefixed framing, category-based subscription, poll-based event loop. It carries BitPads frames between daemons in real time. It is not storage. It is not history. It is the nervous system.

Git has no bus. Git does not do real-time IPC. The bus stays.

But what the bus *carries* — the frames that flow through it — those frames, once received and validated, become git objects in the ledger. The bus is the transport membrane between the runtime world (daemons, state machines, HAL readings) and the persistent world (the git object store). Frames enter through the bus; commits emerge in the repository.

Where git *does* intersect with transport is in **bundles**: self-contained files that encode a complete segment of git history (objects + refs) for offline transfer. A git bundle is:

- A set of objects (blobs, commits)
- A set of ref pointers (branch tips)
- Entirely self-contained — no server needed
- Verifiable on receipt — hash chain intact

This is what `telux-sharedb` does. The sharedb queue takes a frame, selects a carrier (SMS, IP, BLE, QR), formats it, sends it. On receipt, the counterparty validates and posts to their ledger. A git bundle over SMS would replace the `sdb_enqueue -> sdb_process_queue -> carrier.send()` pipeline with:

1. `git bundle create outbound.bundle refs/exchanges/pending-42` — package the exchange leg
2. Encode the bundle as base64url (like `zpu_encode` does for the pads-v1 URL)
3. Send via carrier
4. Counterparty receives, runs `git bundle verify inbound.bundle` — integrity check
5. `git fetch inbound.bundle` — import the objects and refs
6. Merge (bilateral exchange settlement)

The `#1pa/` URL prefix and the pads-v1 encoding scheme could wrap a git bundle fragment instead of a raw BitLedger frame. The payload size math still works: a minimal git bundle (one blob + one commit) is roughly the same size as a signed BitLedger frame (frame + signature + metadata ~ 130-200 bytes). Base64url of that fits in 300 characters.

---

### 10. The C0 Enhancement Grammar Is the Commit Trailer

The C0 Enhanced byte — 5-bit code + 3-bit P-A-C flags — signals protocol events: session open (SOH), batch boundary (LF), record separator (RS), acknowledgement (ACK), cancellation (CAN). These are metadata about the *structure* of the data stream, not the data itself. They tell the receiver how to parse what follows.

Git commits have **trailers**: structured key-value lines at the end of the commit message. `Signed-off-by:`, `Reviewed-by:`, `Co-authored-by:` — these are metadata about the *nature* of the commit, not its content. They tell the reader how to interpret what the commit represents.

The 13 signal slot positions (P1-P13) map onto structured trailers in the git commit:

| Signal Slot | Position | Git Trailer |
|---|---|---|
| P1 (Session open) | Before Layer 1 | `Session-Open: <session-id>` |
| P3 (Before batch) | Before Layer 2 | `Batch-Open: <batch-id>` |
| P5 (Before record) | Before record body | `Record-Type: <type>` |
| P6 (After record) | After record body | `Record-Hash: <frame_hash>` |
| P12 (Batch close) | Batch boundary | `Batch-Close: <batch-id>` `Conservation: 0` |
| P13 (Session close) | Session boundary | `Session-Close: <session-id>` |

The P-A-C flags — Priority, Acknowledge, Continuation — become trailer qualifiers:

- `Priority: elevated` (P flag set)
- `ACK-Required: yes` (A flag set)
- `Continuation: 1-of-3` (C flag set)

This isn't a stretch. Git's trailer system was designed for exactly this kind of structured metadata, and the C0 grammar was designed to express exactly this kind of protocol-level annotation. The encoding differs (byte vs. text), but the information model is the same.

---

### 11. The Power State Machine Writes Its Own Branch

`outstack-powerd` operates a five-mode state machine: FULL -> STANDARD -> CONSERVE -> CRITICAL -> EMERGENCY. Each transition is triggered by battery/thermal readings. Each transition emits a MODE_CHANGE record through the bus. Each transition gates process classes via cgroup freezer.

The MODE_CHANGE record — `[type][prev_mode][new_mode][reason][gate_mask]` — is a commit on `refs/power/outstack`:

```
commit a1b2c3d...
Author: outstack-powerd <did:key:z6Mk...>
Date:   Mon Jun 16 14:22:03 2026

    MODE_CHANGE STANDARD->CONSERVE

    Prev-Mode: STANDARD
    New-Mode: CONSERVE
    Reason: battery_below_30
    Gate-Mask: 0x0C
    Battery-Pct: 28
    Thermal-C: 34
    Signed-off-by: outstack-powerd
```

The GATE record — `[type][gate_mask]` — and the RESTORE record — `[type][restore_mask]` — become subsequent commits. The complete power governance history of the device is a single git branch, readable with `git log`, auditable with `git verify-commit`, and tamper-evident by construction.

When the device reaches Phase 7 hardening, the power branch becomes audit evidence. "Prove that this device was in CONSERVE mode between 14:00 and 16:30 on June 16" = `git log --after="14:00" --before="16:30" refs/power/outstack`. The proof is the signed commit chain.

---

### 12. The Sovereignty Dashboard Reads `git log`

Phase 6 envisions a Sovereignty Dashboard: Island management, capability grants, identity, ledger browser. Currently this requires querying SQLite through Unix socket IPC to `telux-ledgerd` and `telux-identd`.

With git as substrate, the dashboard reads the repository directly:

- **Ledger browser**: `git log refs/islands/personal` — each commit is a record, displayed with its value, direction, account pair, timestamp, and signature status
- **Island management**: `git branch --list 'refs/islands/*'` — enumerate all Islands, show latest commit (chain tip) for each
- **Capability grants**: `git log refs/capabilities/` — each commit is a GRANT, REVOKE, or DELEGATE event
- **Identity**: `git config user.signingkey` — the sovereign DID; `git log --format='%GK' refs/islands/personal | sort -u` — all signing keys that have authored records

The NLQ engine — the natural language query interface — parses questions like "how much did I spend on fuel this week" into `git log` queries with filters on trailers (`Account-Pair: Operating Expense / Asset`), date ranges (`--after`), and value sums. The git log is already structured, already indexed, already searchable.

---

### 13. The Distribution and OTA System Is `git pull`

Phase 7 specifies an OTA system: full + incremental updates, signing, deployment. The current plan is to use AOSP's standard OTA generation tools.

But the ZAKO OS system partition itself could be a git repository. An OS update is a signed commit on `refs/updates/stable`. The device fetches:

```
git fetch https://updates.babb.tel/zako-os refs/updates/stable
git verify-commit FETCH_HEAD   # Babb Works' signing key
# apply update
```

Incremental updates are git's native delta compression — only changed files are transmitted. Rollback is `git checkout v1.0.3`. The signing chain ensures that only Babb Works can author updates. The hash chain ensures that the update is the exact bytes intended.

The Cat S22 Flip's 16GB eMMC and intermittent connectivity make git's efficiency model ideal: pack files are compact, transfers are resumable, and the complete history enables rollback without re-downloading.

---

### Epilogue: One Model, One Store, One Proof

The deepest observation is not that git *can* do these things. It is that ZAKO already *does* these things — chain hashing, content addressing, append-only storage, signed authorship, branched namespaces, bilateral merge, integrity verification — but implements each one independently in custom C code backed by SQLite.

Git unifies them. One object store replaces `ledgerd_store`, `identd_store`, the power ledger, and the capability database. One hash chain replaces `zako_chain_hash` and `lds_verify_chain`. One branch model replaces the Islands directory tree and the per-daemon storage paths. One signing model replaces the `identd_daemon` request/response protocol. One transport model replaces the sharedb queue and carrier abstraction.

What remains is what git cannot express: the wire codec (BitPads, BitLedger, C0), the real-time bus (libzako-bus), the power state machine (outstack-powerd's HAL-driven evaluation loop), and the hardware interface. These are runtime concerns. Git is the persistence and verification layer beneath them.

The system that results is not "ZAKO on git." It is ZAKO *expressed as* git — the same sovereignty guarantees, the same conservation invariants, the same identity model, the same tamper-evidence, rendered in a substrate that already solves the distributed, offline-capable, cryptographically-verified record-keeping problem because it was built for exactly that purpose by someone who needed exactly that thing.

---

### Structural Correspondence Summary

| ZAKO Element | Git Equivalent | Current Implementation |
|---|---|---|
| `chain_hash[n] = BLAKE3(frame_hash[n] \|\| chain_hash[n-1])` | Commit chain (parent pointer + tree hash) | Custom in `libzako-hash` + `ledgerd_store` |
| `frame_hash` (content fingerprint) | Blob SHA (content-addressable object) | Custom BLAKE3 wrapper |
| `genesis_anchor` (zeros_32 seed) | Initial commit (no parent) | Custom in `zako_genesis_anchor()` |
| `lds_verify_chain()` (tamper detection) | `git fsck` (object graph integrity) | Custom SQLite walk |
| Append-only `records` table | Append-only object store | SQLite |
| `ids_cap_t` capability chain (grantor->grantee, depth 0-3) | Commit ancestry (reachability) | SQLite + depth field |
| Islands (personal, work) | Branches (independent history lines) | `/data/zako/islands/` directory structure |
| Bilateral exchange (atomic dual-leg posting) | Merge commit with two parents | `exchange_engine.c` |
| `telux-sharedb` outbound queue | `git bundle` / `git push` | Custom carrier abstraction |
| C0 signal slot metadata | Commit trailers | In-band enhanced bytes |
| Sovereign DID + ed25519 signing | Git commit signing (ed25519) | `libzako-sign` + `libzako-did` |
| Power mode history | Dedicated branch (`refs/power/outstack`) | `power.db` SQLite |
| OTA updates | Signed commit on update branch | AOSP OTA tools |

*This document is an architectural overlay analysis. It does not prescribe adoption of git/libgit2 as a runtime dependency. See `05-git-efficiency-analysis.md` for the execution-level cost comparison and the recommended hybrid approach.*

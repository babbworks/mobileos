# libzako-hash

BLAKE3-based chain hashing library for ZAKO OS.

## Purpose

Provides the three hash operations that underpin ZAKO's tamper-evident record chain:
- `zako_frame_hash()` — fingerprint a single record
- `zako_chain_hash()` — link a record to its predecessor
- `zako_genesis_anchor()` — create the starting point of a new chain

## Dependencies

- BLAKE3 reference C implementation (public domain, CC0)
- Standard C99 libc only

## Building

```bash
make            # builds libzako-hash.a (static library)
make test       # builds and runs unit tests
make fuzz       # runs AFL++ fuzzing against hash inputs
```

## API

See `zako_hash.h` for the complete interface.

# oware engine

Pure C99 rules engine for Oware (Abapa variant) — ZAKO OS Phase 6.

- I/O-free, no dynamic allocation, ~16-byte game state.
- Configurable variants: capture rule (2/3 or 3/4), four grand-slam rules,
  end mode (first-to-N or all-captured), cycle limit, agreed ending.

## Build

```bash
make            # builds liboware-engine.a
make test       # builds and runs unit tests (ASan + UBSan)
```

## API

See `oware_engine.h`. Core flow: `oware_init` → loop { `oware_is_over`?,
`oware_legal_moves`, `oware_apply_move` } → read `oware_result_t`.

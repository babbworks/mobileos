# oware engine + ai

Pure C99 rules engine and AI for Oware (Abapa variant) — ZAKO OS Phase 6.

- I/O-free, no dynamic allocation, ~16-byte game state.
- Configurable variants: capture rule (2/3 or 3/4), four grand-slam rules,
  end mode (first-to-N or all-captured), cycle limit, agreed ending.
- AI: alpha-beta search with Easy / Medium / Hard presets.

## Build

```bash
make            # builds liboware-engine.a and liboware-ai.a
make test       # builds and runs engine + AI unit tests (ASan + UBSan)
```

## API

**Engine:** see `oware_engine.h`. Core flow: `oware_init` → loop {
`oware_is_over`?, `oware_legal_moves`, `oware_apply_move` } → read
`oware_result_t`.

**AI:** see `oware_ai.h`. `oware_ai_config_default` → `oware_ai_choose_move`
when `s->turn` matches the AI player. Returns a ring index (0–11) for
`oware_apply_move`.

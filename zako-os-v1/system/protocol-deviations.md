# ZAKO OS v1 — Protocol Deviations & Implementation Adjustments

Log of every change, deviation, interpretation decision, or type widening applied to normative protocol specifications during ZAKO implementation. Each entry records what the spec says, what we did differently, and why.

---

## BitLedger v3.0

### BL-DEV-001: Value split fields widened from 17-bit/8-bit to uint32_t

**Spec says:** At default Optimal Split S=8, the value block is described as "17-bit multiplicand A" and "8-bit remainder r" (§ Value Encoding). The narrative implies fixed 17/8 sizing.

**What we did:** Declared `value_a` and `value_r` as `uint32_t` in `zbl_record_t`, and the `zbl_value_split()` / `zbl_value_join()` functions take `uint32_t *` parameters.

**Why:** The Optimal Split S is variable (bits 10-13 of Layer 2, range 0-15). At S=8 (default), A uses 17 bits and r uses 8 bits. But at S=4, A uses 21 bits and r uses 4 bits. At S=12, A uses 13 bits and r uses 12 bits. The actual width of A and r depends on the batch header's S value. Using `uint16_t` for A would overflow at S≤8 where A can reach 131,071 (17 bits > 16 bits). Using `uint32_t` accommodates all valid S values without overflow.

**Spec conformance:** No wire format change. The 25-bit N is still packed identically into bits 1-25 of Layer 3. Only the C representation of decoded fields is widened. A conformant implementation must support variable S; fixed uint16_t for A is only valid if S≥9 is guaranteed.

**Date:** 2026-06-15

---

## BitPads v2.0

### BP-DEV-001: Simplified ZAKO-centric header replaced with full spec implementation

**Previous state:** An earlier `zako_bitpads.h` defined a non-spec-compliant Meta byte layout using bits 7-6 as a 2-bit "frame type" (Pure Signal, Anon Wave, Full Record, Full BitLedger). This layout does not exist in the BitPads v2.0 specification.

**What we did:** Deleted the old header entirely. Implemented the actual Meta Byte 1 from the spec: bit 1 = Mode (Wave/Record), bit 2 = ACK/SysCtx, bit 3 = Continuation, bit 4 = Treatment Switch, bits 5-8 = Role A/B/C content field.

**Why:** The old header was a design sketch that diverged from the published protocol. Coding to the full spec ensures wire compatibility with any other BitPads v2.0 implementation and avoids maintaining a fork.

**Spec conformance:** Full conformance with BitPads Protocol v2.0 §2 (Meta Byte 1), §3 (Role A), §4 (Role B), §5 (Role C), §6 (Pure Signal), §14 (Decoder Tree).

**Date:** 2026-06-15

---

## Template for new entries

### XX-DEV-NNN: Brief title

**Spec says:** What the normative document states.

**What we did:** The implementation choice.

**Why:** Technical justification.

**Spec conformance:** Whether the wire format is affected, and whether another conformant implementation would interoperate.

**Date:** YYYY-MM-DD

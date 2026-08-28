# Audit Report — oes32-hls

**Repository:** sparkainlp-x/oes32-hls  
**Audit date:** 2026-08-28  
**Auditor:** Copilot automated audit (TASK 7)  
**Repository status at time of audit:** Experimental / Engineering Prototype

---

## 1. Test Execution Results

Tests were executed on the audit date using:

```
Compiler: g++ (GCC 11, Ubuntu 22.04)
Command:  g++ -std=c++14 -Wall -Wextra -Wno-unknown-pragmas \
              -o test_oes32_hls test_oes32_hls.cpp oes32_hls_top.cpp
          ./test_oes32_hls
```

### Pre-audit baseline (6 test cases / 8 assertions)

```
Results: 8 / 8 tests passed
```

All 8 assertions passed.

### Post-audit (11 test cases / 17 assertions)

```
Results: 17 / 17 tests passed
```

All 17 assertions passed. See `test_oes32_hls.cpp` for deterministic test vectors.

---

## 2. Issues Found and Changes Made

### 2.1 Missing LICENSE file (FIXED)
- **Issue:** README and badge declared MIT license; no `LICENSE` file existed in the repository.
- **Fix:** Added `LICENSE` with standard MIT text.

### 2.2 README test count ambiguity (FIXED)
- **Issue:** README stated "6 test cases" in the file table but "8 / 8 tests passed" in the expected output. These figures refer to different things (test cases vs. assertions) and were confusing.
- **Fix:** Updated README to state "11 test cases / 17 assertions" consistently.

### 2.3 Latency target presented as achieved measurement (FIXED)
- **Issue:** README table listed "Latency target: < 20 ns (pipelined, II = 1)" without making clear this is a design intent, not a synthesis result.
- **Fix:** Added explicit label "design intent, not a measured result" and added a "Synthesis Reports Pending" section.

### 2.4 No synthesis reports (DOCUMENTED)
- **Issue:** No synthesis, place-and-route, or hardware validation results are present.
- **Fix:** Added a "Synthesis Reports (Pending)" table documenting all metrics as pending.

### 2.5 Missing contribution and security files (FIXED)
- **Issue:** No `CONTRIBUTING.md`, `SECURITY.md`, or `CHANGELOG.md`.
- **Fix:** All three files added.

### 2.6 No project status badge (FIXED)
- **Issue:** README contained no status indicator; project could be mistaken for production-ready.
- **Fix:** Added `Status: Experimental` badge and a "Project Status" section.

### 2.7 Related repository link broken (FIXED)
- **Issue:** README referenced `sparkainlp-x/oes32-membrane-shield` which does not exist in the public profile.
- **Fix:** Replaced with actual existing repositories (`oes32-residual`, `oes32_engine`).

### 2.8 Insufficient test coverage (FIXED)
- **Issue:** Original 6 test cases did not cover negative values, ring-3 FOLD8 imbalance, non-zero reference, or uniform offsets.
- **Fix:** Added TC7–TC11 covering these cases.

---

## 3. Unresolved Issues

| Issue | Reason unresolved |
|-------|-------------------|
| No synthesis/timing reports | Requires AMD Vitis HLS; not available in CI environment |
| No hardware-in-the-loop validation | Requires ZCU111 board; not available |
| Floating-point host vs. FPGA parity | Cannot verify without synthesis |
| No version/release tag | Intentional — pending first stable synthesis run |

---

## 4. Claims Removed or Weakened

| Original claim | Change |
|----------------|--------|
| "Latency target: < 20 ns (pipelined, II = 1)" | Added "design intent, not a measured result" |
| Related repo link to non-existent `oes32-membrane-shield` | Replaced with existing repos |

---

## 5. No Secrets, Binaries, or Legal Documents Committed

No secrets, API keys, proprietary binaries, bitstreams, or unverified legal documents were added in this audit.

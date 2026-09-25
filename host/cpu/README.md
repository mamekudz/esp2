# 6502 host harness

Selected core: **fake6502** (see `docs/apple2/core-evaluation.md`,
`third_party/fake6502/`).

Wrapper: `esp_bracket::Cpu6502`  
Harness: `esp_bracket::CpuHarness` (64 KiB synthetic memory)

```
npm run test:apple2
```

Requires LLVM-MinGW under `tools/host-toolchain/` (gitignored).

Never commit Apple ROMs — use synthetic ROM only.

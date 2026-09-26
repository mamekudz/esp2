# Synthetic Disk II fixtures

Project-owned media for host Disk II tests. **Not** Apple DOS / System disks.

| File | Description |
| --- | --- |
| `Esp2DiskTest.dsk` | 143360-byte DOS-order DSK; T0S0 boot payload writes `ESP][ DISK BOOT OK` |

Regenerate: `node host/tools/gen_esp2_disk_test.mjs`

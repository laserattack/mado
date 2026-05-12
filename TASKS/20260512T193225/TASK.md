- NAME: Remove GNU extensions for POSIX compliance
- PRIORITY: 10
- TAGS: portability, posix, refactor
- STATUS: opened

### Description

Currently `tamd` depends on several GNU extensions which makes it
Linux-only.  Replace these with POSIX-compliant alternatives to
support macOS, BSD, and other Unix-like systems.

### GNU extensions to replace

1. `get_current_dir_name()`
2. `fmemopen()` (use `yy_scan_string()`)
3. `nftw()` (posix ????)

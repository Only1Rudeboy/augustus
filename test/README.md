# Fork regression tests

Lightweight pure-logic tests that do **not** link Augustus or SDL.

```bat
cl /nologo /W3 /Fe:test_fork_logic.exe test\test_fork_logic.c
test_fork_logic.exe
```

Or with MinGW:

```bash
gcc -Wall -Wextra -o test_fork_logic test/test_fork_logic.c
./test_fork_logic
```

Covers:

- Blueprint footprint rotate / mirror math  
- Storage staffing threshold (100% vs 50% config)  
- Depot order threshold cycle (never 0)  
- Tax mood anti-cheese max(live, settled)  

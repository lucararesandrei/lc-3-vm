# Critical Coding Rules

1. C++ RULES:
   - Always include `#include <bits/stdc++.h>`.
   - Always include fast I/O at the start of main(): `ios_base::sync_with_stdio(false); cin.tie(NULL);`.
   - `main()` must always end with `return 0;`.
   - Declare `int i;` and all loop counters *before* `for` loops (e.g., `for (i = 0; i < n; i++)`).
   - Declare all `main` variables at the top of `main()`.
   - Declare all arrays/vectors/matrices globally outside `main()` with explicit constants (e.g., `const int NMAX = 1000;`).
   - Use only basic STL (`vector`, `set`, `map`, `stack`, `queue`).

2. C RULES (Old-School C):
   - Strictly NO `break` or `continue` statements.
   - Do NOT include any extra libraries other than `#include <stdio.h>`.
   - Declare `int i;` and loop variables before loops; keep general variables at the top of `main`.
   - File pointers must be declared as `FILE *fin, *fout;` at the start or globally, assigned via `fopen()` *only* at first use, and closed via `fclose()` immediately when done.

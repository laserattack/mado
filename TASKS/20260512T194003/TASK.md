- NAME: there are too many allocations???
- PRIORITY: 100
- TAGS: critical, allocation, memory, refactor, optomization, malloc
- STATUS: opened

```
~/projects/tamd
[serr@lap]-> valgall tamd -p 'tag = malloc'
==5324== Memcheck, a memory error detector
==5324== Copyright (C) 2002-2024, and GNU GPL'd, by Julian Seward et al.
==5324== Using Valgrind-3.25.1 and LibVEX; rerun with -h for copyright info
==5324== Command: tamd -p tag\ =\ malloc
==5324==
/home/serr/projects/tamd/TASKS/20260512T194003/TASK.md:1:1
==5324==
==5324== HEAP SUMMARY:
==5324==     in use at exit: 0 bytes in 0 blocks
==5324==   total heap usage: 18,034 allocs, 18,034 frees, 1,938,042 bytes allocated
==5324==
==5324== All heap blocks were freed -- no leaks are possible
==5324==
==5324== For lists of detected and suppressed errors, rerun with: -s
==5324== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

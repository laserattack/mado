- NAME: there are too many allocations???
- PRIORITY: 100
- TAGS: critical, refactor, optomization, malloc, regex
- STATUS: closed

# Problem

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

# What has been done to solve the problem

- [x] precompile regexs
- [x] limit task parsing to first 10 lines and rename globals

```
~/projects/tamd
[serr@lap]-> valgall tamd -p 'tag = malloc'
==13011== Memcheck, a memory error detector
==13011== Copyright (C) 2002-2024, and GNU GPL'd, by Julian Seward et al.
==13011== Using Valgrind-3.25.1 and LibVEX; rerun with -h for copyright info
==13011== Command: tamd -p tag\ =\ malloc
==13011==
/home/serr/projects/tamd/TASKS/20260512T194003/TASK.md:1:1
==13011==
==13011== HEAP SUMMARY:
==13011==     in use at exit: 0 bytes in 0 blocks
==13011==   total heap usage: 1,731 allocs, 1,731 frees, 298,386 bytes allocated
==13011==
==13011== All heap blocks were freed -- no leaks are possible
==13011==
==13011== For lists of detected and suppressed errors, rerun with: -s
==13011== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

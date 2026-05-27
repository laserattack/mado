- NAME: try to avoid using die somewhere other than the main function.
- PRIORITY: 30
- TAGS: refactor
- STATUS: closed

```
-*- mode: compilation; default-directory: "~/projects/tamd/" -*-
Compilation started at Tue May 26 19:19:33

grep -rni "die("
utils/util.h:4:void die(const char *errstr, ...);
utils/util.h:15:void die(const char *errstr, ...) {
main.c:69:        die("Failed to compile task_dir regex");
main.c:71:        die("Failed to compile name regex");
main.c:74:        die("Failed to compile priority regex");
main.c:76:        die("Failed to compile tags regex");
main.c:79:        die("Failed to compile status regex");
main.c:171:            die("Failed to create tasks directory: %s", tasks_dir);
main.c:565:        die("Failed to parse query");
main.c:572:        die("Tasks directory not found");
main.c:591:    die("usage: %s [-h] [-i] [n] [-D dir] [-f format] [-p query] [-r query]\n"
main.c:626:            die("-D requires a directory name argument");
main.c:638:            die("-f requires a format argument");
main.c:647:            die("Unknown format '%s'", fmt_str);
main.c:654:            die("-p requires a query argument");
main.c:662:            die("-r requires a query argument");
main.c:668:        die("Unknown flag '%c'", ARGC());
main.c:680:            die("Tasks directory not found");

Compilation finished at Tue May 26 19:19:33, duration 0.03 s
```

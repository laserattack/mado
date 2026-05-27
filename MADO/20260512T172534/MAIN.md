- NAME: Implement recursive downward search for TASKS directories
- PRIORITY:
- TAGS: feature, feat, filesystem, fs
- STATUS: opened

Current behavior: `find_dir_up()` searches only upward from current
directory for the nearest TASKS directory.

Goal: Add optional recursive search (-a or -R flag) that finds ALL TASKS
directories in the current directory tree (downward) and aggregates
tasks from all of them.

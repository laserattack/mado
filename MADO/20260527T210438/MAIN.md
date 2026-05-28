- NAME: Limit the length of a line when searching for a title
- PRIORITY: 100
- TAGS: fix, critical, optimization, allocation
- STATUS: closed

# Problem

There is currently a limit on the number of lines, but not on the
length of the line. The string can be huge and this can lead to a
large memory allocation.

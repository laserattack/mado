# tamd — tasks and markdown

A command-line task manager that stores tasks as markdown files and
supports powerful filtering with a query language

![](./static/repo_image.jpg)

## Features

- **Per-project isolation**: Each project has its own `TASKS/`
  directory, similar to `.git` — no global directories are used. This
  allows you to version‑control `TASKS/` alongside your code
- **Task storage**: Tasks stored as `TASK.md` files in timestamped
  directories (`YYYYMMDDTHHMMSS/TASK.md`) in `TASKS/` directory.  The
  task directory can also contain any additional files related to the
  task — attachments, screenshots, logs, scripts, etc. Everything
  stays organized in one place

## Usage Example

Start from scratch in a new project:

``` bash
# Create a project directory and enter it
mkdir myproject
cd myproject

# Initialize TASKS directory (like git init)
tamd -i
# Output: Created tasks directory: /home/user/myproject/TASKS
```

Once the `TASKS/` directory is initialized, you can work with tasks
from any subdirectory within the project — just like Git, tamd
automatically finds the nearest `TASKS/` directory by walking up the
file tree

``` bash
# Create your first task
tamd -n
```

The task will be created with the following content:

```
- NAME:
- PRIORITY:
- TAGS:
- STATUS:
```

You can fill it out as needed, for example:

```
- NAME: Fix login bug
- PRIORITY: 10
- TAGS: bug, critical, auth
- STATUS: opened

The login page returns 500 error when using special characters.
...
```

When you have many tasks, you'll want to filter them:

``` bash
# List all tasks
tamd -p 'all'

# Find critical bugs
tamd -p 'tag = bug and priority > 5'

# Delete low priority tasks
tamd -r 'priority < 5'

# Filter by task name (exact match)
tamd -p 'name = login'

# Filter by task name (substring)
tamd -p 'name ~ login'
tamd -p 'name ~ "fix login"' # multiple words — quotes required

# Filter by creation time
tamd -p 'time ~ 20260516' # Tasks created on 2026-05-16
tamd -p 'time > 20260516T12' # Tasks created after 2026-05-16 12:00:00
tamd -p 'time >= 2023 and time <= 2025' # Tasks created between 2023 and 2025 (inclusive)

# Find tasks with complex conditions
tamd -p '(tag = bug or tag = critical) and status = opened'
tamd -p 'not (priority < 3 or status = closed)'
tamd -p '(priority > 5 and tag = urgent) or status = reopened'

# Syntactic sugar for matching multiple values
tamd -p 'status = anyof(opened, reopened)' # Status equals "opened" OR "reopened"
tamd -p 'priority = anyof(10, 20, 30)' # Priority equals 10 OR 20 OR 30
tamd -p 'tag = allof(bug, critical)' # Task has BOTH "bug" AND "critical" tags
```

## Output Formats

The `-f` flag controls how tasks are displayed:

``` bash
# Default unix format: path:1:1: fields
tamd -p 'all'
# /home/user/project/TASKS/20260516T161611/TASK.md:1:1: TIME:[20260516T123214] NAME:[cmp operators for strings] PRIORITY:[10] STATUS:[closed] TAGS:[feat,search]
# Compatible with Emacs compile buffer and other tools that parse file:line:col

# Paths only — useful for piping to other tools
tamd -p 'all' -f path
# /home/user/project/TASKS/20260516T161611/TASK.md
# Search across task bodies with grep
grep 'match' $(tamd -p 'all' -f path)
# /home/user/project/TASKS/20260512T224126/TASK.md:Operator '=' remains for exact matches.

# Newline-delimited JSON for scripts
tamd -p 'all' -f jsonl
# {"time":"20260516T161611","name":"Fix login bug","priority":10,"status":"opened","tags":["bug","critical"],"path":"/home/user/project/TASKS/20260516T161611/TASK.md"}
# Pipe JSON output to jq for advanced processing
tamd -p 'priority > 5' -f jsonl | jq '.name'
tamd -p 'all' -f jsonl | jq -s 'group_by(.status)'
tamd -p 'all' -f jsonl | jq -s 'sort_by(.priority)'
```

## Query Syntax

The query language supports filtering tasks using operators and
keywords

### Operators

| Operator | Description |
|----------|-------------|
| `>` | Greater than |
| `<` | Less than |
| `>=` | Greater than or equal |
| `<=` | Less than or equal |
| `=` | Equal / exact match |
| `!=` | Not equal |
| `~` | Contains |
| `!~` | Not contains |

### Logical Operators

| Operator | Description |
|----------|-------------|
| `and` | Logical AND |
| `or` | Logical OR |
| `not` | Logical NOT |

### Types

| Type | Description | Format | Examples |
|------|-------------|--------|----------|
| **number** | Integer value | 0-999 | `0`, `10`, `999` |
| **string** | Text value | Unquoted: `[a-zA-Z_][a-zA-Z0-9_-]*` or quoted: `"..."` or `'...'` | `bug`, `"fix login"`, `'проблема'` |
| **timestamp** | Creation time of task | 4, 6 or 8 digits + optional (`T` + 0, 2, 4 or 6 digits) | `2026`, `20260516`, `20260516T`, `20260516T1230` |

### Keywords

| Keyword | Type    | Operators                      | Example                           |
|---------|---------|--------------------------------|-----------------------------------|
| `priority` | number | `>`, `<`, `>=`, `<=`, `=`, `!=`, `~`, `!~` | `priority > 5`, `priority != 10` |
| `tag`      | string  | `>`, `<`, `>=`, `<=`, `=`, `!=`, `~`, `!~`           | `tag = bug`, `tag ~ crit` |
| `status`   | string  | `>`, `<`, `>=`, `<=`, `=`, `!=`, `~`, `!~`           | `status = opened`, `status ~ open` |
| `name`     | string  | `>`, `<`, `>=`, `<=`, `=`, `!=`, `~`, `!~`           | `name = "Fix bug"`, `name ~ login` |
| `time`     | timestamp  | `>`, `<`, `>=`, `<=`, `=`, `!=`, `~`, `!~`           | `time > 20260505T1230 and time < 20260510T` |
| `all`      | special | -                              | `all`                  |

> **Note:** all operators also work with `anyof(...)` and `allof(...)`.
> These are syntactic sugar that expand to multiple conditions.
> `anyof(...)` expands with `or`, `allof(...)` expands with `and`.
> Examples:
> - `status = anyof(opened, reopened)` is equivalent to `status = opened or status = reopened`
> - `tag ~ allof(bug, crit, fix)` is equivalent to `tag ~ bug and tag ~ crit and tag ~ fix`

## Installation

Clone the repository and build:

```
make
```

This will produce an executable file `./tamd`

## Requirements

- **Unix system** (Linux, possibly macOS/BSD)
- **Build dependencies**:
  - C compiler (gcc/clang)
  - `make`
  - `flex`
  - `bison`

## Acknowledgments

Inspired by [Compiler for a Query
Language](https://youtu.be/8NdRGmp70Go?si=hZ7cQ-PBZQXqD9mG) by
Tsoding. In the video, he outlined the specification for such a system
but didn't release the code — so this is my own implementation

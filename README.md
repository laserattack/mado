# mado — markdown organizer

A command-line tool that stores entries (tasks, notes) as markdown
files and supports powerful filtering with a query language

![](./static/repo_image.jpg)

## Features

- **Per-project isolation**: Each project has its own `MADO/`
  directory, similar to `.git` — no global directories are used. This
  allows you to version‑control `MADO/` alongside your code
- **Entry storage**: Entries stored as `MAIN.md` files in timestamped
  directories (`YYYYMMDDTHHMMSS/MAIN.md`) in `MADO/` directory. The
  entry directory can also contain any additional files related to the
  entry — attachments, screenshots, logs, scripts, etc. Everything
  stays organized in one place

## Usage Example

Start from scratch in a new project:

``` bash
# Create a project directory and enter it
mkdir myproject
cd myproject

# Initialize MADO directory (like git init)
mado -i
```

Once the `MADO/` directory is initialized, you can work with entries
from any subdirectory within the project — just like Git, mado
automatically finds the nearest `MADO/` directory by walking up the
file tree

``` bash
# Create your first entry
mado -n
```

The entry will be created with the following content:

```
- NAME:
- PRIORITY:
- TAGS:
- STATUS:
- DEADLINE:
```

You can fill it out as needed, for example:

```
- NAME: Fix login bug
- PRIORITY: 10
- TAGS: bug, critical, auth
- STATUS: opened
- DEADLINE: 20260615

The login page returns 500 error when using special characters.
...
```

> No fields are required — you can omit any field entirely or leave its value empty. For example, when writing a note, you probably won't need the priority, status and deadline fields.
> When a field is omitted or left empty:
> - NAME, STATUS default to empty string ""
> - PRIORITY defaults to 0
> - DEADLINE defaults to 99990000T000000
> - TAGS defaults to a list with one empty element [""]
> - TIME is a system field, always present and set to the entry's directory name (creation timestamp in YYYYMMDDTHHMMSS format). It cannot be changed or removed — it reflects when the entry was created

When you have many entries, you'll want to filter them:

``` bash
# List all entries
mado -p 'all'

# Find critical bugs
mado -p 'tag = bug and priority > 5'

# Delete low priority entries
mado -r 'priority < 5'

# Filter by entry name (exact match)
mado -p 'name = login'

# Filter by entry name (substring)
mado -p 'name ~ login'
mado -p 'name ~ "fix login"' # multiple words — quotes required

# Filter by creation time
mado -p 'time ~ 20260516' # Entries created on 2026-05-16
mado -p 'time > 20260516T12' # Entries created after 2026-05-16 12:00:00
mado -p 'time >= 2023 and time <= 2025' # Entries created between 2023 and 2025 (inclusive)

# Find entries with complex conditions
mado -p '(tag = bug or tag = critical) and status = opened and deadline < 20260602'
mado -p 'not (priority < 3 or status = closed)'
mado -p '(priority > 5 and tag = urgent) or status = reopened'

# Syntactic sugar for matching multiple values
mado -p 'status = anyof(opened, reopened)' # Status equals "opened" OR "reopened"
mado -p 'priority = anyof(10, 20, 30)' # Priority equals 10 OR 20 OR 30
mado -p 'tag = allof(bug, critical)' # Entry has BOTH "bug" AND "critical" tags
```

## Customizing Templates

Templates are stored in `MADO/.templates/` as markdown files:

``` bash
# Create a bug report template
cat > MADO/.templates/bug.md << 'EOF'
- NAME:
- PRIORITY: 10
- TAGS: bug
- STATUS: opened

## Steps to Reproduce
1.

## Expected Behavior

## Actual Behavior
EOF
```

You can create entries with custom templates:

``` bash
mado -n -t bug
# The template must exist at: MADO/.templates/bug.md
```

## Output Formats

The `-f` flag controls how entries are displayed:

``` bash
# Default unix format: path:1:1: fields
mado -p 'all'
# /home/user/project/mado/MADO/20260521T204844/MAIN.md:1:1: TIME:[20260521T204844] NAME:[The ability to specify the name of the main directory (TASKS by default)] PRIORITY:[10] DEADLINE:[] STATUS:[closed] TAGS:[feat,flag]
# Compatible with Emacs compile buffer and other tools that parse file:line:col

# Paths only — useful for piping to other tools
mado -p 'all' -f path
# /home/user/project/MADO/20260516T161611/MAIN.md
# Search across entry bodies with grep
grep 'match' $(mado -p 'all' -f path)
# /home/user/project/MADO/20260512T224126/MAIN.md:Operator '=' remains for exact matches.

# Newline-delimited JSON for scripts
mado -p 'all' -f jsonl
# {"time":"20260521T204844","name":"The ability to specify the name of the main directory (TASKS by default)","priority":10,"deadline":"","status":"closed","tags":["feat","flag"],"path":"/home/user/project/MADO/20260521T204844/MAIN.md"}
# Pipe JSON output to jq for advanced processing
mado -p 'priority > 5' -f jsonl | jq '.name'
mado -p 'all' -f jsonl | jq -s 'group_by(.status)'
mado -p 'all' -f jsonl | jq -s 'sort_by(.priority)'
```

## Query Syntax

The query language supports filtering entries using operators and
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
| **timestamp** | Creation time of entry | 4 digits, 6 digits or (8 digits + optional (`T` + 0, 2, 4 or 6 digits)) | `2026`, `20260516`, `20260516T`, `20260516T1230` |

### Keywords

| Keyword | Type    | Operators                      | Example                           |
|---------|---------|--------------------------------|-----------------------------------|
| `priority` | number | `>`, `<`, `>=`, `<=`, `=`, `!=`, `~`, `!~` | `priority > 5`, `priority != 10` |
| `tag`      | string  | `>`, `<`, `>=`, `<=`, `=`, `!=`, `~`, `!~`           | `tag = bug`, `tag ~ crit` |
| `status`   | string  | `>`, `<`, `>=`, `<=`, `=`, `!=`, `~`, `!~`           | `status = opened`, `status ~ open` |
| `name`     | string  | `>`, `<`, `>=`, `<=`, `=`, `!=`, `~`, `!~`           | `name = "Fix bug"`, `name ~ login` |
| `time`     | timestamp  | `>`, `<`, `>=`, `<=`, `=`, `!=`, `~`, `!~`           | `time > 20260505T1230 and time < 20260510T` |
| `deadline`     | timestamp  | `>`, `<`, `>=`, `<=`, `=`, `!=`, `~`, `!~`           | `deadline > 20260505T1230 and deadline < 20260510T` |
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

This will produce an executable file `./mado`

## Requirements

- **Unix system** (Linux, possibly macOS/BSD)
- **Build dependencies**:
  - C compiler (gcc/clang)
  - `make`
  - `flex`
  - `bison`

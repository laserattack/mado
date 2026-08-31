# mado — markdown organizer

A command-line tool that stores entries (tasks, notes) as markdown
files and supports powerful filtering with a query language

![](./static/demo_mado.gif)

## Table of Contents

- [Features](#features)
- [Usage Example](#usage-example)
- [Customizing Templates](#customizing-templates)
- [Sorting Entries](#sorting-entries)
- [Output Formats](#output-formats)
- [Query Syntax](#query-syntax)
  - [Operators](#operators)
  - [Logical Operators](#logical-operators)
  - [Types](#types)
  - [Keywords](#keywords)
  - [Macros](#macros)
- [Installation](#installation)
  - [Download static binary](#download-static-binary)
  - [Build from source](#build-from-source)
- [Performance](#performance)
- [Interfaces](#interfaces)
  - [Emacs interface](#emacs-interface)
- [Requirements](#requirements)
- [Inspiration](#inspiration)

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
mado init
```

Once the `MADO/` directory is initialized, you can work with entries
from any subdirectory within the project — just like Git, mado
automatically finds the nearest `MADO/` directory by walking up the
file tree

``` bash
# Create your first entry
mado new
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
> - DEADLINE defaults to 99990101T000000
> - TAGS defaults to a list with one empty string [""]
> - PATH is a system field, always present and set to the full path of the entry's MAIN.md file. It cannot be changed or removed manually
> - TIME is a system field, always present and set to the entry's directory name (creation timestamp in YYYYMMDDTHHMMSS format). It cannot be changed or removed — it reflects when the entry was created
> - MTIME is a system field, automatically set to the modification time of the entry's file (YYYYMMDDTHHMMSS format). It updates whenever the entry file changes and cannot be manually edited

When you have many entries, you'll want to filter them:

``` bash
# List all entries
mado list 'all'

# Find critical bugs
mado list 'tag = bug and priority > 5'

# Delete low priority entries
mado remove 'priority < 5'

# Filter by entry name (exact match)
mado list 'name = login'

# Filter by entry name (substring)
mado list 'name ~ login'
mado list 'name ~ "fix login"' # multiple words — quotes required
mado list 'name ^~ "Fix"' # Starts with "Fix"
mado list 'name !~$ "bug"' # NOT ends with "bug"

# Glob patterns (wildcards)
mado list 'name %~ "add*"' # Names starting with "add"
mado list 'name !%~ "temp*"' # Names NOT starting with "temp"
mado list 'name %~ "add\\*"' # Names literally "add*"

# Filter by entry name (fuzzy match)
mado list 'name ~~ "fx lgn b"' # Finds "Fix login bug"
mado list 'name !~~ lgn' # Entries that do NOT fuzzy match

# Filter by creation time
mado list 'time ~ 20260516' # Entries created on 2026-05-16
mado list 'time > 20260516T12' # Entries created after 2026-05-16 12:00:00
mado list 'time > 2023 and time < @year+1' # Entries created between 2023 and current year (inclusive)

# Search across all fields
mado list 'any ~ login' # Finds in string, timestamp fields
mado list 'any ~ 10' # Finds in numeric, string, timestamp fields

# Find entries with complex conditions
mado list '(tag = bug or tag = critical) and status = opened and deadline < @now'
mado list 'not (priority < 3 or status = closed)'
mado list '(priority > 5 and tag = urgent) or status = reopened'

# Syntactic sugar for matching multiple values
mado list 'status in (opened, reopened)' # Status equals "opened" OR "reopened"
mado list 'priority in (10, 20, 30)' # Priority equals 10 OR 20 OR 30
mado list 'tag has (bug, critical)' # Entry has BOTH "bug" AND "critical" tags
mado list 'tag ~~ anyof(lgn, fix, err)' # Tag fuzzy matches ANY of: "lgn", "fix", "err"
mado list 'tag ^~ allof("namespace1:","namespace2:")' # Entry has tags starting with ALL of the given prefixes

# Syntactic sugar for ranges
mado list 'priority in [50..100]' # Priority between 50 and 100 inclusive
mado list 'priority in [..50]' # Priority less than or equal to 50
mado list 'priority in [50..]' # Priority greater than or equal to 50
mado list 'deadline in [@today..@today+7]' # Deadline within next 7 days
mado list 'time in [20260601..20260630]' # Created in June 2026
mado list 'name in [h..z]' # Name starts with a letter from 'h' to 'z'

# Sort results
mado list -s +priority 'tag = bug' # bugs sorted by priority
mado list -s -priority,+time 'all' # highest priority first, oldest first
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
mado new -t bug
# The template must exist at: MADO/.templates/bug.md
```

## Sorting Entries

The `-s` / `--sort` flag allows you to sort entries by one or more fields:

``` bash
# Sort by priority (ascending by default)
mado list -s priority 'all'

# Sort by priority descending
mado list -s -priority 'all'

# Sort by multiple fields (priority ascending, then time descending)
mado list -s +priority,-time 'all'

# Sort by status, then name
mado list -s status,name 'all'
```

Sort order:

- `+field` or `field` — ascending order (default)
- `-field` — descending order

> **Note:** sort fields are matched fuzzily. `prio` is interpreted as
> `priority`, `stts` as `status`, etc. Exact match is always preferred

## Output Formats

The `-f` flag controls how entries are displayed:

``` bash
# Default format: path:1: fields
mado list 'all'
# Compatible with Emacs compile buffer and other tools that parse file:line:

# Paths only — useful for piping to other tools
mado list -f path 'all'
# Search across entry bodies with grep
grep 'match' $(mado list -f path 'all')

# Newline-delimited JSON for scripts
mado list -f jsonl 'all'

# Pipe JSON output to jq for advanced processing
mado list -f jsonl 'priority > 5' | jq '.name'
mado list -f jsonl 'all' | jq -s 'group_by(.status)'
mado list -f jsonl 'all' | jq -s 'sort_by(.priority)'
```

> **Note:** output formats are matched fuzzily. `js` is interpreted as
> `jsonl`, `pth` as `path`, etc. Exact match is always preferred

## Query Syntax

The query language supports filtering entries using operators and
keywords

### Operators

| Operator | Description | Note |
|----------|-------------|------|
| `>` | Greater than | For string/timestamp: lexicographic |
| `<` | Less than | For string/timestamp: lexicographic |
| `>=` | Greater than or equal | For string/timestamp: lexicographic |
| `<=` | Less than or equal | For string/timestamp: lexicographic |
| `=` | Equal / exact match | |
| `!=` | Not equal | |
| `~` | Contains | For numbers: alias for `=` |
| `!~` | Not contains | For numbers: alias for `!=` |
| `^~` or `~^` | Starts with | For numbers: alias for `=` |
| `^!~` or `!~^` | Not starts with | For numbers: alias for `!=` |
| `$~` or `~$` | Ends with | For numbers: alias for `=` |
| `$!~` or `!~$` | Not ends with | For numbers: alias for `!=` |
| `%~` | Glob match (wildcard) | For numbers: alias for `=` |
| `!%~` | Not glob match | For numbers: alias for `!=` |
| `~~` | Fuzzy match | For numbers: alias for `=` |
| `!~~` | Not fuzzy match | For numbers: alias for `!=` |

### Logical Operators

| Operator | Description |
|----------|-------------|
| `and` | Logical AND |
| `or` | Logical OR |
| `xor` | Logical XOR (exactly one) |
| `not` | Logical NOT |

Precedence (from highest to lowest): `not` > `and` > `xor` > `or`

### Types

| Type | Description | Format | Examples |
|------|-------------|--------|----------|
| **number** | Integer value | 0-999 | `0`, `10`, `999` |
| **string** | Text value | Unquoted: `[a-zA-Z_][a-zA-Z0-9_-]*` or quoted: `"..."` or `'...'` | `bug`, `"fix login"`, `'проблема'` |
| **timestamp** | Timestamp value | Subset of ISO 8601: `YYYYMMDDTHHMMSS` with optional shorter forms: `YYYY`, `YYYYMM`, `YYYYMMDD`, `YYYYMMDDT`, `YYYYMMDDTHH`, `YYYYMMDDTHHMM`, `YYYYMMDDTHHMMSS` | `2026`, `202605`, `20260516`, `20260516T`, `20260516T12`, `20260516T1230`, `20260516T123012` |

### Keywords

| Keyword | Type    | Operators                      | Example                           |
|---------|---------|--------------------------------|-----------------------------------|
| `priority` | number | all | `priority > 5`, `priority != 10` |
| `tag`      | string  | all | `tag = bug`, `tag ~ crit` |
| `status`   | string  | all | `status = opened`, `status ~ ope` |
| `name`     | string  | all | `name = "Fix bug"`, `name ~~ "fx lgn"` |
| `path`     | string  | all | `path ~ "/home/user/projects"` |
| `time`     | timestamp  | all | `time > 20260505T1230 and time < 20260510T` |
| `mtime`    | timestamp  | all | `mtime > 20260505T1230` |
| `deadline` | timestamp  | all | `deadline > 20260505T1230 and deadline < 20260510T` |
| `any`      | string/timestamp/number  | all | `any ~ login`, `any = 10` |
| `all`      | special | -                              | `all`                  |
| `untagged` | special | -                              | `untagged`                  |
| `unstatused` | special | -                            | `unstatused`                  |
| `unnamed` | special | -                                | `unnamed`                  |
| `unprioritized` | special | -                          | `unprioritized`                  |
| `undeadlined` | special | -                            | `undeadlined`                  |

> **Note:** keywords are matched fuzzily. `prio` is interpreted as
> `priority`, `upred` as `unprioritized`, etc. Exact match is always preferred

> **Note 2:** `any` searches across all fields

> **Note 3:** all operators also work with `anyof(...)` and `allof(...)`.
> These are syntactic sugar that expand to multiple conditions.
> `anyof(...)` expands with `or`, `allof(...)` expands with `and`.
> Examples:
> - `status = anyof(opened, reopened)` is equivalent to `status = opened or status = reopened`
> - `tag ~ allof(bug, crit, fix)` is equivalent to `tag ~ bug and tag ~ crit and tag ~ fix`
>
> For `=` operator there are shorter alternatives:
> - `status in (opened, reopened)` is equivalent to `status = anyof(opened, reopened)`
> - `tag has (bug, crit)` is equivalent to `tag = allof(bug, crit)`

> **Note 4:** `in [...]` is syntactic sugar for range conditions.
> It expands to comparisons joined with `and`.
> Examples:
> - `priority in [50..100]` is equivalent to `priority >= 50 and priority <= 100`
> - `priority in [..50]` is equivalent to `priority <= 50`
> - `priority in [50..]` is equivalent to `priority >= 50`
> - `deadline in [@today..@today+7]` is equivalent to `deadline >= @today and deadline <= @today+7`
> - `name in [a..z]` is equivalent to `name >= a and name <= z`

### Macros

Macros start with `@` and are resolved at tokenize time — the value is
substituted before the AST is evaluated. Macros are simple: they
expand to a single token, not to expressions

> **Note:** macros are matched fuzzily. `@tday` is interpreted as
> `@today`, `@mnth` as `@month`, etc. Exact match is always preferred

#### Time Macros

Resolve to timestamps. Accept optional offsets with `+`/`-`

| Macro | Resolution | Offset unit |
|-------|-----------|-------------|
| `@now` | Current timestamp (`YYYYMMDDTHHMMSS`) | days |
| `@today` | Today (`YYYYMMDD`) | days |
| `@yesterday` | Yesterday (`YYYYMMDD`) | days |
| `@tomorrow` | Tomorrow (`YYYYMMDD`) | days |
| `@week` | Current week monday (`YYYYMMDD`) | weeks |
| `@month` | Current month (`YYYYMM`) | months |
| `@year` | Current year (`YYYY`) | years |
| `@never` | 99990101T000000 | days |

Examples:

``` bash
mado list 'deadline > @week'
mado list 'deadline > @year'
mado list 'time > @today-4'
mado list 'time >= @week-1'
mado list 'deadline < @today+30'
mado list 'time ~ @month-2'
mado list 'mtime > @now-1'
mado list 'deadline != @never' # entries with non-default deadline
```

#### Number Macros

Resolve to numeric values

| Macro | Value |
|-------|-------|
| `@max` | 999 |
| `@min` | 0 |

Examples:

``` bash
mado list 'priority = @max' # highest priority entries
mado list 'priority = @min' # lowest/default priority entries
```

## Installation

### Download static binary

Download the latest static binary from [GitHub
Releases](https://github.com/laserattack/mado/releases/latest). The
binary works on any x86_64 Linux without dependencies

### Build from source

Clone the repository and build:

``` bash
make
```

If your standard library requires TBB for parallel algorithms, build
with:

``` bash
make USE_TBB=1
```

This will produce an executable file `./mado`

To build a statically linked binary:

```
make release
```

This will produce `./release/mado`

## Performance

Time to list `100,000` entries with filter query: `~1.2s`

> **Note:** benchmarked on AMD Ryzen 5 5600H (6 cores, 12 threads,
> 3.30 GHz base) with NVMe SSD

> **Note 2:** Performance can be improved by hiding unnecessary
> fields. A field is considered unnecessary if it is hidden, not used
> in sorting, and not referenced in the query. Such fields are skipped
> during parsing. For example, hiding `mtime` (`--hide-mtime`)
> eliminates one `stat()` system call per entry file, reducing I/O
> overhead

Time to list `100,000` entries with filter query and `--hide-mtime`:
`~0.9s`

## Interfaces

`mado` is designed as a backend that can power different
frontends. The core is a fast C++ engine with a query language, while
interfaces handle display and interaction

### Emacs interface

[emado](https://github.com/laserattack/emado) provides a pretty Emacs
interface with transient menus, interactive statistics, outline-mode
sections, direct deletion from the buffer, etc

![](./static/demo_emado.gif)

## Requirements

- **Linux system** (x86_64 for static binary; any arch for build from
  source)
- **Build dependencies**:
  - C compiler
  - C++ compiler (TBB may be required depending on the standard
    library implementation — see [Installation](#installation))
  - `make`
  - `flex`
  - `bison`

## Acknowledgments

- [Tsoding's video on building a query language
  compiler](https://youtu.be/8NdRGmp70Go) — the original idea and
  inspiration
- [philj56/fuzzy-match](https://github.com/philj56/fuzzy-match) —
  fuzzy matching algorithm used in the query language
- Based on glob implementation from
  [Ciremun/password-manager](https://github.com/Ciremun/password-manager)

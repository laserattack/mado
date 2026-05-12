# tamd (tasks and markdown) - Task Management with Query Language

A command-line task manager that stores tasks as markdown files and
supports powerful filtering with a query language.

## Features

- **Per-project isolation**: Each project has its own `TASKS/`
  directory, similar to `.git` - no global directories are used. This
  allows you to version‑control `TASKS/` alongside your code.
- **Task storage**: Tasks stored as `TASK.md` files in timestamped
  directories (`YYYYMMDDTHHMMSS/TASK.md`) in `TASKS/` directory.  The
  task directory can also contain any additional files related to the
  task — attachments, screenshots, logs, scripts, etc. Everything
  stays organized in one place.

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
automatically finds the nearest TASKS/ directory by walking up the
file tree.

``` bash
# Create your first task
tamd -n
# Output: /home/user/myproject/TASKS/20260512T172534/TASK.md:1:1
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

# Find tasks with complex conditions
tamd -p '(tag = bug or tag = critical) and status = opened'
tamd -p 'not (priority < 3 or status = closed)'
tamd -p '(priority > 5 and tag = urgent) or status = reopened'
```

## Query Syntax

The query language supports filtering tasks using keywords

### Keywords

| Keyword | Type    | Operators          | Example                |
|---------|---------|--------------------|------------------------|
| `priority` | integer | `>`, `<`, `>=`, `<=`, `=` | `priority > 5` |
| `tag`      | string  | `=`                | `tag = bug`            |
| `status`   | string  | `=`                | `status = opened`      |
| `all`      | special | -                  | `all`                  |

### Operators

| Operator | Description |
|----------|-------------|
| `and` | Logical AND (both conditions must be true) |
| `or` | Logical OR (at least one condition must be true) |
| `not` | Logical NOT (negates a condition) |

## Installation

Clone the repository and build:

```
make
```

This will produce an executable file `./tamd`.

## Requirements

- **Linux only** (uses GNU extensions: `get_current_dir_name`, `fmemopen`)
- **Build dependencies**:
  - C compiler (gcc/clang)
  - `make`
  - `flex`
  - `bison`

## Acknowledgments

Inspired by [Compiler for a Query
Language](https://youtu.be/8NdRGmp70Go?si=hZ7cQ-PBZQXqD9mG) by
Tsoding. In the video, he outlined the specification for such a system
but didn't release the code - so this is my own implementation.

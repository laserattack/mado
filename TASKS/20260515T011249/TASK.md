- NAME: Improve error messages with position information
- PRIORITY: 20
- TAGS: errors, ux, parser, lexer
- STATUS: closed

## Current Problem

Error messages are currently unhelpful for debugging query syntax:

```bash
$ tamd -p 'priority > and status = opened'
Syntax error: syntax error

$ tamd -p 'priority = '
Syntax error: syntax error

$ tamd -p 'tag = "unclosed'
Lexical error: invalid character '"'
```

## What i need

```bash
Syntax error: priority > and status = opened
                          ^
```

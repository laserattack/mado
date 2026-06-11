- NAME: implement modifier for single-tag multi-condition matching
- PRIORITY: 0
- TAGS: feat, parser, lexer, ast
- STATUS: not need
- DEADLINE:

# Problem

Currently `tag ~ bug and tag ~ critical` matches entries where
*different* tags satisfy each condition. There's no way to require
that a *single tag* satisfies multiple conditions simultaneously.

Example: find entries with a tag that is `!= bug` AND `~ crit`. Use
case: "tag not exactly 'bug' but contains 'crit'".

# Solution

Add functionality like this

```
mado -p 'tag: != bug, ~ crit'
```

adding this functionality will allow tags to be used as namespaces.

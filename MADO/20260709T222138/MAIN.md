- NAME: refactoring
- PRIORITY: 101
- TAGS: refactoring, error, chore
- STATUS: closed

# [x] 1

вот такого не должно быть, все ошибки только через `mado_print_error`,
а в cerr напрямую только хинты выводить можно

```
    if (err == Mado_Error::NOT_FOUND) {
        std::cerr << "Error: main directory '" << g_mado_config.main_dir_name << "' not found\n";
        return -1;
```

# [ ] 2

сделай чтобы пропуск пробелов был внутри lookup keyword

хз не надо это наверно

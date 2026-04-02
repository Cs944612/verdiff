# Contributing to Verdiff

Thanks for helping improve `verdiff`.

## Development Flow

1. Fork the repository.
2. Create a feature branch from `main`.
3. Run:

```sh
make
make test
make bench
```

4. Keep changes focused and well-explained.
5. Open a pull request with:
   - motivation
   - behavior changes
   - benchmark or correctness notes

## Coding Guidelines

- Keep the fast path simple and measurable.
- Prefer metadata-first and streaming algorithms.
- Avoid full-file loads unless there is no better option.
- Preserve deterministic output.
- Add tests for behavioral changes.

## Performance Guidelines

- Any new file I/O in hot paths should be justified.
- Favor bounded concurrency over aggressive parallelism.
- When possible, include before/after benchmark notes in your PR.

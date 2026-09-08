# test-results/

This folder holds the exact terminal output (command + result) from the
project's build and test pipeline, so a reviewer can see precisely what
was run and what it produced without having to run anything themselves.

**Full combined output is also published as a GitHub Gist for quick
viewing without cloning the repo:**
https://gist.github.com/BensuEKER/adbdbd26cdc26da2f985d9304a7b8808

## How these files are generated

Run, from the repository root:

```
./scripts/run_all_tests.sh
```

This regenerates all three log files below by actually running the
corresponding commands and capturing their full output (via `tee`, so
you also see it live in your terminal).

## Files

- `unit-tests.log` — output of `ceedling test:all` (Görev 7 unit tests,
  mock/fake I2C, no hardware or Renode dependency)
- `firmware-build.log` — output of `cmake --preset debug` and
  `cmake --build --preset debug` (Görev 3/9 firmware build)
- `renode-integration-tests.log` — output of `pytest -v test_integration.py`
  (Görev 8 integration tests, driving a real Renode simulation)

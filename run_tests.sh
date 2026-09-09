#!/usr/bin/env bash
#
# run_tests.sh
#
# Thin wrapper at the repo root, matching the assignment's suggested
# `./run_tests.sh` entry point. The actual implementation lives in
# scripts/run_all_tests.sh.

set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$REPO_ROOT/scripts/run_all_tests.sh"

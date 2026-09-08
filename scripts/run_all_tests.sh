#!/usr/bin/env bash
#
# scripts/run_all_tests.sh
#
# Runs the project's full local build/test pipeline and saves each
# command together with its exact output under test-results/, so anyone
# reviewing the repository can see precisely what was run and what it
# produced without having to run anything themselves.
#
# Usage (from the repo root):
#   ./scripts/run_all_tests.sh

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

mkdir -p test-results

echo "==> Running Ceedling unit tests (Görev 7)..."
{
  echo "\$ ceedling test:all"
  ceedling test:all
} 2>&1 | tee test-results/unit-tests.log

echo "==> Configuring and building the firmware with CMake (Görev 3/9)..."
{
  echo "\$ cmake --preset debug"
  cmake --preset debug
  echo
  echo "\$ cmake --build --preset debug"
  cmake --build --preset debug
} 2>&1 | tee test-results/firmware-build.log

echo "==> Running the Renode/Python integration tests (Görev 8)..."
{
  echo "\$ cd python && pytest -v test_integration.py"
  ELF_PATH="$REPO_ROOT/build/STM32F407VG_ADS1x1x.elf"
  (cd python && STM32_ELF="$ELF_PATH" pytest -v test_integration.py)
} 2>&1 | tee test-results/renode-integration-tests.log

echo
echo "All logs saved under test-results/:"
ls -la test-results/

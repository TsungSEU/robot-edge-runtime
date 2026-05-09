#!/bin/bash
# cleanup.sh
# Clean up FastDDS shared memory resources before starting DCP
# This prevents accumulated FastDDS resources from causing memory issues

set -e

echo "[Cleanup] Removing FastDDS shared memory resources..."

# Remove all FastDDS shared memory segments
rm -rf /dev/shm/fastrtps_* 2>/dev/null || true

echo "[Cleanup] FastDDS shared memory cleanup completed"

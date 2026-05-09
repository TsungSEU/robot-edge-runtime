#!/bin/bash
# Install commit-msg hook for Aurora Edge Runtime

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
GIT_DIR="$PROJECT_ROOT/.git"
HOOKS_DIR="$GIT_DIR/hooks"

echo "Installing commit-msg hook..."

# Check if we're in a git repository
if [[ ! -d "$GIT_DIR" ]]; then
    echo "❌ Error: Not in a git repository"
    exit 1
fi

# Copy hook script
cp "$SCRIPT_DIR/commit-msg-hook" "$HOOKS_DIR/commit-msg"
chmod +x "$HOOKS_DIR/commit-msg"

echo "✅ commit-msg hook installed successfully"
echo ""
echo "Commit messages will now be validated against Conventional Commits format."
echo ""
echo "Supported types:"
echo "  feat, fix, docs, style, refactor, perf, test, build, ci, chore, revert"
echo ""
echo "Format: type(scope): subject"
echo "Example: feat(logger): fix use-after-free error"

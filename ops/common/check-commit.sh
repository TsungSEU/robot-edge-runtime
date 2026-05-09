#!/bin/bash
# Check if the last commit message follows Conventional Commits format

set -e

LAST_COMMIT=$(git log -1 --pretty=%B)

echo "Checking last commit message:"
echo "$LAST_COMMIT"
echo ""

# Simple regex check
PATTERN='^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)(\(.+\))?: .{1,72}'

if echo "$LAST_COMMIT" | head -n1 | grep -qE "$PATTERN"; then
    echo "✅ Commit message follows Conventional Commits format"

    # Parse and display the commit components
    if echo "$LAST_COMMIT" | head -n1 | grep -qE "^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)\(.+\):"; then
        TYPE=$(echo "$LAST_COMMIT" | head -n1 | sed -E 's/^([a-z]+)\(.+\):.*/\1/')
        SCOPE=$(echo "$LAST_COMMIT" | head -n1 | sed -E 's/^[a-z]+\(([^)]+)\):.*/\1/')
        SUBJECT=$(echo "$LAST_COMMIT" | head -n1 | sed -E 's/^[a-z]+\([^)]+\): (.*)/\1/')
        echo "  Type:    $TYPE"
        echo "  Scope:   $SCOPE"
        echo "  Subject: $SUBJECT"
    else
        TYPE=$(echo "$LAST_COMMIT" | head -n1 | sed -E 's/^([a-z]+):.*/\1/')
        SUBJECT=$(echo "$LAST_COMMIT" | head -n1 | sed -E 's/^[a-z]+: (.*)/\1/')
        echo "  Type:    $TYPE"
        echo "  Subject: $SUBJECT"
    fi

    exit 0
else
    echo "❌ Commit message does not follow Conventional Commits format"
    echo ""
    echo "Expected format: type(scope): subject"
    echo ""
    echo "Supported types:"
    echo "  feat, fix, docs, style, refactor, perf, test, build, ci, chore, revert"
    exit 1
fi

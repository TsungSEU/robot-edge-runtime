#!/bin/bash
#
# release.sh - GitLab Release Automation Script
# Usage: ./ops/release.sh [VERSION] [--dry-run]
#

set -e

# Color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project paths
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CHANGELOG_FILE="${PROJECT_ROOT}/CHANGELOG.md"

# GitLab configuration
GITLAB_HOST="${GITLAB_HOST:-gitlab.t3caic.com}"
GITLAB_TOKEN="jBfE9eBfYAdfjZ4z4EZY"
GITLAB_PROJECT="${GITLAB_PROJECT:-icr11/dataengine/data-infra/Aurora/aurora-edge-runtime}"
PROJECT_ID=$(echo "$GITLAB_PROJECT" | sed 's/\//%2F/g')

# Dry run mode
DRY_RUN=false

# Generate only mode (only generate CHANGELOG, don't create release)
GENERATE_ONLY=false

# Logging functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# Print usage
print_usage() {
    echo -e "${BLUE}GitLab Release Automation Script${NC}"
    echo ""
    echo "Usage: $0 [VERSION] [OPTIONS]"
    echo ""
    echo "Arguments:"
    echo "  VERSION       Version tag (e.g., v1.1.5). If not specified, reads from CHANGELOG.md"
    echo ""
    echo "Options:"
    echo "  --dry-run     Run without creating actual tags or releases"
    echo "  --generate-only Only generate CHANGELOG entry, don't create release"
    echo "  --help, -h    Show this help message"
    echo ""
    echo "Environment Variables:"
    echo "  GITLAB_TOKEN  Personal Access Token for GitLab API (required if glab not available)"
    echo "  GITLAB_HOST   GitLab host (default: gitlab.t3caic.com)"
    echo ""
    echo "Examples:"
    echo "  $0 v1.1.5                    # Create release for v1.1.5"
    echo "  $0 --dry-run v1.1.5          # Preview release without creating it"
    echo "  $0 --generate-only v1.1.5    # Only generate CHANGELOG entry"
    echo "  $0                           # Use latest version from CHANGELOG.md"
    exit 0
}

# Validate version format
validate_version() {
    local version=$1
    # Remove 'v' prefix for validation
    local clean_version=${version#v}

    if [[ ! "$clean_version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9.]+)?$ ]]; then
        log_error "Invalid version format: $version"
        log_info "Expected format: vMAJOR.MINOR.PATCH or vMAJOR.MINOR.PATCH-PRERELEASE"
        return 1
    fi

    return 0
}

# Check if workspace is clean
check_workspace_clean() {
    if [[ -n $(git status -s) ]]; then
        log_error "Workspace is not clean. Please commit or stash changes first."
        git status
        return 1
    fi
    return 0
}

# Check if version exists in CHANGELOG.md
check_changelog_version() {
    local version=$1
    local clean_version=${version#v}

    if [[ ! -f "$CHANGELOG_FILE" ]]; then
        log_error "CHANGELOG.md not found at: $CHANGELOG_FILE"
        return 1
    fi

    if ! grep -q "## \[$clean_version\]" "$CHANGELOG_FILE"; then
        log_error "Version [$clean_version] not found in CHANGELOG.md"
        log_info "Please add the version entry to CHANGELOG.md first."
        return 1
    fi

    return 0
}

# Extract release notes from CHANGELOG.md
extract_release_notes() {
    local version=$1
    local clean_version=${version#v}

    # Extract content between version headers
    awk "
        /## \[$clean_version\]/ { in_section=1; next }
        in_section && /^## / { exit }
        in_section { print }
    " "$CHANGELOG_FILE"
}

# Get latest version from CHANGELOG.md
get_latest_version() {
    grep -m1 "^## \[" "$CHANGELOG_FILE" | sed 's/## \[\([^]]*\)\].*/\1/'
}

# Get current commit SHA
get_commit_sha() {
    git rev-parse HEAD
}

# Get current commit message
get_commit_message() {
    git log -1 --pretty=%B
}

# Create git tag
create_git_tag() {
    local tag=$1
    local message=$2

    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY RUN] Would create git tag: $tag"
        return 0
    fi

    log_step "Creating annotated tag: $tag"
    git tag -a "$tag" -m "$message"

    if [[ $? -eq 0 ]]; then
        log_info "Tag created successfully"
        return 0
    else
        log_error "Failed to create tag"
        return 1
    fi
}

# Push git tag to remote
push_git_tag() {
    local tag=$1

    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY RUN] Would push tag to remote: $tag"
        return 0
    fi

    log_step "Pushing tag to remote: $tag"
    git push origin "$tag"

    if [[ $? -eq 0 ]]; then
        log_info "Tag pushed successfully"
        return 0
    else
        log_error "Failed to push tag"
        return 1
    fi
}

# Create GitLab release using glab CLI
create_glab_release() {
    local tag=$1
    local notes=$2

    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY RUN] Would create GitLab release using glab for tag: $tag"
        return 0
    fi

    log_step "Creating GitLab release using glab CLI"

    # Create release with notes from file
    local notes_file
    notes_file=$(mktemp)
    echo "$notes" > "$notes_file"

    glab release create "$tag" --notes "$notes_file" --name "$tag"

    local result=$?
    rm -f "$notes_file"

    if [[ $result -eq 0 ]]; then
        log_info "GitLab release created successfully using glab"
        return 0
    else
        log_warn "Failed to create release using glab, will try API"
        return 1
    fi
}

# Create GitLab release using API
create_gitlab_api_release() {
    local tag=$1
    local notes=$2

    if [[ -z "$GITLAB_TOKEN" ]]; then
        log_error "GITLAB_TOKEN environment variable not set"
        log_info "Create a Personal Access Token at: https://${GITLAB_HOST}/-/profile/personal_access_tokens"
        log_info "Required scopes: api, write_repository"
        log_info "Then run: export GITLAB_TOKEN=<your-token>"
        return 1
    fi

    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY RUN] Would create GitLab release using API for tag: $tag"
        return 0
    fi

    log_step "Creating GitLab release using API"

    # Write notes to temp file for proper handling
    local notes_file
    notes_file=$(mktemp)
    echo "$notes" > "$notes_file"

    # JSON escape the notes
    local escaped_notes
    escaped_notes=$(jq -Rs . "$notes_file")

    rm -f "$notes_file"

    # Create release via API using JSON payload
    local response
    response=$(curl -s -X POST \
        --header "PRIVATE-TOKEN: $GITLAB_TOKEN" \
        --header "Content-Type: application/json" \
        --data "{
            \"name\": \"$tag\",
            \"tag_name\": \"$tag\",
            \"description\": $escaped_notes
        }" \
        "https://${GITLAB_HOST}/api/v4/projects/${PROJECT_ID}/releases")

    # Check for errors in response
    if echo "$response" | jq -e '.message' >/dev/null 2>&1; then
        log_error "Failed to create release via GitLab API:"
        echo "$response" | jq -r '.message' 2>/dev/null || echo "$response"
        return 1
    fi

    log_info "GitLab release created successfully using API"
    return 0
}

# Create GitLab release (try glab first, then API)
create_gitlab_release() {
    local tag=$1
    local notes=$2

    # Try glab CLI first
    if command -v glab &> /dev/null; then
        log_info "Found glab CLI, attempting to create release..."
        create_glab_release "$tag" "$notes"
        if [[ $? -eq 0 ]]; then
            return 0
        fi
        log_warn "glab CLI failed, falling back to API"
    fi

    # Fallback to API
    create_gitlab_api_release "$tag" "$notes"
}

# Generate CHANGELOG entry from commits
generate_changelog_entry() {
    local version=$1
    local prev_tag=$2

    # Strip 'v' prefix for CHANGELOG entry
    local clean_version=${version#v}

    # Get commits since previous tag
    local commits
    if [[ -n "$prev_tag" ]]; then
        commits=$(git log "${prev_tag}..HEAD" --pretty=format:"%H|%s|%an" --reverse)
    else
        commits=$(git log --pretty=format:"%H|%s|%an" --reverse)
    fi

    if [[ -z "$commits" ]]; then
        echo "ERROR: No commits found since previous tag" >&2
        return 1
    fi

    # Categorize commits by type and scope
    declare -A categories
    declare -A by_scope

    while IFS='|' read -r sha msg author; do
        # Skip version bump commits and CHANGELOG updates
        if [[ "$msg" =~ ^(chore|docs).*\(changelog|version\) ]] || [[ "$msg" =~ ^chore:.*bump ]]; then
            continue
        fi

        # Parse commit type and scope
        local pattern='^([a-z]+)[(]([^)]+)[)]: (.+)'
        if [[ "$msg" =~ $pattern ]]; then
            local type="${BASH_REMATCH[1]}"
            local scope="${BASH_REMATCH[2]}"
            local subject="${BASH_REMATCH[3]}"

            # Map type to category
            local category
            case "$type" in
                feat) category="Features" ;;
                fix) category="Bug Fixes" ;;
                docs) category="Documentation" ;;
                style) category="Style" ;;
                refactor) category="Code Refactoring" ;;
                perf) category="Performance" ;;
                test) category="Tests" ;;
                build|ci) category="Build System" ;;
                chore) category="Maintenance" ;;
                revert) category="Reverts" ;;
                *) category="Other" ;;
            esac

            # Add to categories
            categories["$category"]+="$msg"$'\n'

            # Add to scope-specific category
            local scope_key="$category:$scope"
            by_scope["$scope_key"]+="$msg"$'\n'
        fi
    done <<< "$commits"

    # Check if we have any commits
    if [[ ${#categories[@]} -eq 0 ]]; then
        echo "ERROR: No valid commits found for CHANGELOG generation" >&2
        return 1
    fi

    # Generate markdown
    local changelog="## [$clean_version] - $(date +%Y-%m-%d)"$'\n\n'

    # Output by category and scope (following the project's CHANGELOG style)
    for category in "Features" "Bug Fixes" "Performance" "Documentation" "Code Refactoring" "Tests" "Build System" "Maintenance"; do
        if [[ -n "${categories[$category]}" ]]; then
            # Convert category to match project style
            case "$category" in
                "Code Refactoring") changelog+="### Refactor"$'\n\n' ;;
                "Bug Fixes") changelog+="### Bug Fixes"$'\n\n' ;;
                "Build System") changelog+="### Build"$'\n\n' ;;
                *) changelog+="### $category"$'\n\n' ;;
            esac

            # Group by scope within category
            declare -A seen_scopes
            local scope_keys=()
            for scope_key in "${!by_scope[@]}"; do
                if [[ "$scope_key" == "$category:"* ]]; then
                    scope_keys+=("$scope_key")
                fi
            done

            # Sort scope keys
            IFS=$'\n' scope_keys=($(sort <<<"${scope_keys[*]}"))
            unset IFS

            # Output commits for this category
            for scope_key in "${scope_keys[@]}"; do
                local scope="${scope_key#$category:}"
                local msgs="${by_scope[$scope_key]}"

                # Collect subjects for this scope
                local subjects=()
                local pattern2='^[a-z]+[(][^)]+)[)]: (.+)'
                while IFS= read -r msg; do
                    [[ -z "$msg" ]] && continue
                    if [[ "$msg" =~ $pattern2 ]]; then
                        subjects+=("${BASH_REMATCH[1]}")
                    fi
                done <<< "$msgs"

                # Output as bullet point with sub-bullets if multiple items
                if [[ ${#subjects[@]} -gt 0 ]]; then
                    changelog+="* **$scope:**"
                    if [[ ${#subjects[@]} -eq 1 ]]; then
                        changelog+=" ${subjects[0]}"$'\n'
                    else
                        changelog+=$'\n'
                        for subject in "${subjects[@]}"; do
                            changelog+="  - $subject"$'\n'
                        done
                    fi
                fi
            done

            changelog+=$'\n'
        fi
    done

    changelog+="---"$'\n'

    echo "$changelog"
}

# Update CHANGELOG.md with new entry
update_changelog() {
    local version=$1
    local new_entry=$2

    log_step "Updating CHANGELOG.md"

    # Check if CHANGELOG exists
    if [[ ! -f "$CHANGELOG_FILE" ]]; then
        log_error "CHANGELOG.md not found at: $CHANGELOG_FILE"
        return 1
    fi

    # Check if version already exists
    local clean_version=${version#v}
    if grep -q "## \[$clean_version\]" "$CHANGELOG_FILE"; then
        log_warn "Version $clean_version already exists in CHANGELOG.md"
        log_info "Skipping CHANGELOG update"
        return 0
    fi

    # Create backup
    cp "$CHANGELOG_FILE" "${CHANGELOG_FILE}.backup"

    # Find insertion point (after header, before first version)
    # Insert new entry at the top of versions section
    awk -v new_entry="$new_entry" '
        /^## \[/ && !inserted {
            print new_entry
            inserted=1
        }
        { print }
    ' "$CHANGELOG_FILE.backup" > "$CHANGELOG_FILE"

    rm "${CHANGELOG_FILE}.backup"

    log_info "CHANGELOG.md updated successfully"
    return 0
}

# Confirm release action
confirm_release() {
    local version=$1
    local commit_sha=$2

    echo ""
    echo -e "${YELLOW}=== Release Summary ===${NC}"
    echo -e "Version:  ${GREEN}$version${NC}"
    echo -e "Commit:   $commit_sha"
    echo -e "Branch:   $(git branch --show-current)"
    echo -e "Remote:   $(git remote get-url origin)"
    echo -e "Dry Run:  $DRY_RUN"
    echo ""

    if [[ "$DRY_RUN" == true ]]; then
        echo -e "${YELLOW}[DRY RUN MODE] - No actual changes will be made${NC}"
        return 0
    fi

    if [[ "$GENERATE_ONLY" == true ]]; then
        echo -e "${YELLOW}[GENERATE ONLY MODE] - Only generating CHANGELOG${NC}"
        return 0
    fi

    read -p "Do you want to proceed with the release? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_info "Release cancelled"
        exit 0
    fi
}

# Main release function
perform_release() {
    local version=$1

    log_step "Starting release process for $version"

    # Validate version format
    validate_version "$version" || exit 1

    # Check workspace is clean
    check_workspace_clean || exit 1

    # Get previous tag for CHANGELOG generation
    local prev_tag
    prev_tag=$(git describe --tags --abbrev=0 2>/dev/null || echo "")

    # Generate CHANGELOG entry from commits
    log_info "Previous tag: ${prev_tag:-none (first release)}"
    log_step "Generating CHANGELOG entry for $version"

    local changelog_entry
    changelog_entry=$(generate_changelog_entry "$version" "$prev_tag")

    if [[ $? -ne 0 || -z "$changelog_entry" ]]; then
        log_error "Failed to generate CHANGELOG entry"
        log_info "Falling back to manual CHANGELOG check"
        check_changelog_version "$version" || exit 1
    else
        # Update CHANGELOG.md
        update_changelog "$version" "$changelog_entry" || exit 1

        # Commit CHANGELOG updates if not in dry-run mode
        if [[ -n "$(git status -s CHANGELOG.md 2>/dev/null)" ]]; then
            if [[ "$DRY_RUN" == false ]]; then
                log_step "Committing CHANGELOG.md updates"
                git add CHANGELOG.md
                git commit -m "docs: update CHANGELOG for $version"
                log_info "CHANGELOG.md committed"
            fi
        fi

        # Verify version now exists in CHANGELOG
        check_changelog_version "$version" || exit 1
    fi

    # If generate-only mode, stop here
    if [[ "$GENERATE_ONLY" == true ]]; then
        log_info "CHANGELOG generation complete. Exiting (generate-only mode)."
        return 0
    fi

    # Get release information
    local commit_sha
    commit_sha=$(get_commit_sha)
    local commit_msg
    commit_msg=$(get_commit_message)
    local release_notes
    release_notes=$(extract_release_notes "$version")

    # Validate we have release notes
    if [[ -z "$release_notes" ]]; then
        log_error "No release notes found for version $version"
        exit 1
    fi

    # Show release summary and confirm
    confirm_release "$version" "$commit_sha"

    # Create tag message
    local tag_message
    tag_message="Release $version"$'\n\n'"$commit_msg"

    # Create git tag
    create_git_tag "$version" "$tag_message" || exit 1

    # Push git tag
    push_git_tag "$version" || exit 1

    # Create GitLab release
    create_gitlab_release "$version" "$release_notes" || exit 1

    # Success
    echo ""
    log_info "Release $version completed successfully!"
    echo ""
    echo -e "${BLUE}Release URL:${NC} https://${GITLAB_HOST}/${GITLAB_PROJECT}/-/releases/$version"
    echo ""
    echo -e "${BLUE}Next steps:${NC}"
    echo "  1. Verify the release at the URL above"
    echo "  2. Test the release artifacts"
    echo "  3. Announce the release to stakeholders"
}

# Parse arguments
VERSION=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --generate-only)
            GENERATE_ONLY=true
            shift
            ;;
        --help|-h)
            print_usage
            ;;
        v*)
            VERSION="$1"
            shift
            ;;
        *)
            if [[ -z "$VERSION" ]]; then
                VERSION="$1"
            else
                log_error "Unknown argument: $1"
                print_usage
            fi
            shift
            ;;
    esac
done

# If no version specified, get from CHANGELOG
if [[ -z "$VERSION" ]]; then
    VERSION=$(get_latest_version)
    if [[ -z "$VERSION" ]]; then
        log_error "Could not determine version from CHANGELOG.md"
        exit 1
    fi
    VERSION="v$VERSION"
    log_info "No version specified, using latest from CHANGELOG.md: $VERSION"
fi

# Change to project root
cd "$PROJECT_ROOT"

# Perform release
perform_release "$VERSION"

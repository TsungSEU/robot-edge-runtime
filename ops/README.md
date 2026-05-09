# Ops Directory Structure

This directory contains operational scripts organized by environment.

## 📁 Directory Structure

```
ops/
├── dev/                    # Development Environment
│   ├── aer                 # AER command-line tool
│   ├── install-aer.sh      # Install aer command to PATH
│   └── cleanup.sh          # Clean up FastDDS shared memory
│
├── prod/                   # Production Environment
│   ├── aer.service         # systemd service file
│   ├── aer.conf            # AER configuration file
│   ├── aer-start.sh        # AER startup script
│   └── install-aer-service.sh  # Install systemd service
│
└── common/                 # Common Tools
    ├── check-commit.sh     # Check commit message format
    ├── commit-msg-hook     # Git commit message hook
    ├── install-commit-hooks.sh  # Install commit hooks
    └── release.sh          # Release automation script
```

## 🚀 Development Environment

### AER Command

The `aer` command is a convenient tool for managing AER service during development.

**Installation:**
```bash
cd ops/dev
./install-aer.sh
```

**Usage:**
```bash
aer start              # Start AER service
aer status             # Show service status
aer logs aer           # View logs
aer stop               # Stop service
aer restart            # Restart service
```

**Features:**
- ✅ No root privileges required
- ✅ Quick start/stop cycles
- ✅ Real-time log viewing
- ✅ Flexible command-line options

### Cleanup Script

```bash
cd ops/dev
./cleanup.sh
```

Cleans up FastDDS shared memory and temporary files.

## 🏭 Production Environment

### Systemd Service

Production deployment uses systemd for robust service management.

**Installation:**
```bash
cd ops/prod
sudo ./install-aer-service.sh install
```

**Usage:**
```bash
sudo systemctl start aer              # Start service
sudo systemctl stop aer               # Stop service
sudo systemctl status aer             # Check status
sudo systemctl enable aer             # Auto-start on boot
sudo journalctl -u aer -f              # View logs
```

**Features:**
- ✅ Auto-start on boot
- ✅ Auto-restart on failure
- ✅ System logging (journald)
- ✅ Resource limits and security

### Configuration

Edit `ops/prod/aer.conf` to configure AER service:

```bash
vim ops/prod/aer.conf

# Reload configuration
sudo systemctl reload aer
```

## 🛠️ Common Tools

### Commit Hooks

Install Git commit message hooks for automated validation:

```bash
cd ops/common
./install-commit-hooks.sh
```

### Version Bump Script

Automatically update version numbers across all project files:

```bash
cd ops/common
./bump-version.sh 1.2.0
```

**Features:**
- ✅ Updates CMakeLists.txt MODULE_VERSION
- ✅ Updates package.xml version
- ✅ Updates README.md version badge and history
- ✅ Updates src/main.cpp version banner
- ✅ Updates CLAUDE.md version references
- ✅ Generates CHANGELOG.md entry from git commits
- ✅ Validates semantic version format (X.Y.Z)
- ✅ Follows Conventional Commits format

**Supported Version Formats:**
- `1.2.0` - Standard release
- `1.2.0-rc.1` - Release candidate
- `1.2.0-beta.1` - Beta release

**Example Workflow:**
```bash
# Bump version to 1.2.0
./ops/common/bump-version.sh 1.2.0

# Review changes
git diff

# Commit changes
git add .
git commit -m "chore(release): bump version to 1.2.0"

# Create tag
git tag v1.2.0

# Push to remote
git push origin feature/robot
git push origin v1.2.0
```

### Release Script

Automated release management:

```bash
cd ops/common
./release.sh v1.2.0
```

## 📖 Documentation

For detailed information, see:
- `docs/AER_QUICKSTART.md` - Quick start guide
- `docs/AER_COMMAND.md` - Command reference
- `docs/SERVICE_ARCHITECTURE.md` - Architecture overview
- `docs/AER_STARTUP_FLOW.md` - Startup process details

## 🔧 Quick Reference

### Development
```bash
# Install aer command
cd ops/dev && ./install-aer.sh

# Use aer command
aer start
aer status
aer logs aer
aer stop
```

### Production
```bash
# Install systemd service
cd ops/prod && sudo ./install-aer-service.sh install

# Manage service
sudo systemctl start aer
sudo systemctl status aer
sudo journalctl -u aer -f
```

## 📝 Notes

- **Development scripts** are in `ops/dev/`
- **Production scripts** are in `ops/prod/`
- **Common tools** are in `ops/common/`
- All paths in scripts are relative to project root
- robot_sim is **not managed** by any service, start it manually if needed

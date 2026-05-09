# AER Operations Quick Reference

## 🚀 Development

### Install & Use

```bash
# 1. Install aer command
cd ops/dev
./install-aer.sh

# 2. Add to PATH (if needed)
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# 3. Use aer command
aer start              # Start AER service
aer status             # Show status
aer logs aer           # View logs
aer stop               # Stop service
aer restart            # Restart service
```

### Files

- `ops/dev/aer` - AER command-line tool
- `ops/dev/install-aer.sh` - Installation script
- `ops/dev/cleanup.sh` - Cleanup script

## 🏭 Production

### Install & Use

```bash
# 1. Install systemd service
cd ops/prod
sudo ./install-aer-service.sh install

# 2. Manage service
sudo systemctl start aer              # Start
sudo systemctl stop aer               # Stop
sudo systemctl status aer             # Status
sudo systemctl enable aer             # Auto-start on boot
sudo systemctl disable aer            # Disable auto-start
sudo journalctl -u aer -f              # View logs

# 3. Configure
vim ops/prod/aer.conf                 # Edit configuration
sudo systemctl reload aer             # Reload configuration
```

### Files

- `ops/prod/aer.service` - systemd service file
- `ops/prod/aer.conf` - Configuration file
- `ops/prod/aer-start.sh` - Startup script
- `ops/prod/install-aer-service.sh` - Installation script

## 📊 Comparison

| Feature | Development | Production |
|---------|-------------|------------|
| Command | `aer start` | `systemctl start aer` |
| Location | `ops/dev/` | `ops/prod/` |
| Privileges | User | Root |
| Auto-start | ❌ | ✅ |
| Auto-restart | ❌ | ✅ |
| Logs | `/tmp/aer.log` | journald |
| Config | CLI args | `aer.conf` |

## 🛠️ Common Tools

```bash
# Commit hooks
cd ops/common
./install-commit-hooks.sh

# Release
./release.sh v1.2.0
```

## 📝 Configuration

### Development (CLI args)

```bash
aer start --mode auto          # Auto mode
aer start --mode humanoid      # Humanoid mode (default)
AER_MODE=auto aer start        # Environment variable
```

### Production (aer.conf)

```bash
# Edit configuration
vim ops/prod/aer.conf

# Main settings
AER_MODE=humanoid              # Running mode
AER_MODEL_PATH=/path/to/model  # ONNX model
AER_CONFIG_PATH=/path/to/config # Config file
AER_CPU_AFFINITY=0             # CPU core
```

## 🔍 Troubleshooting

### Development

```bash
# Check status
aer status

# View logs
aer logs aer
cat /tmp/aer.log

# Clean up
rm -f .pids/*.pid
cd ops/dev && ./cleanup.sh
```

### Production

```bash
# Check status
sudo systemctl status aer

# View logs
sudo journalctl -u aer -n 50
sudo journalctl -u aer -f

# Restart
sudo systemctl restart aer
```

## 📖 Full Documentation

- `docs/AER_QUICKSTART.md` - Quick start guide
- `docs/AER_COMMAND.md` - Command reference
- `docs/SERVICE_ARCHITECTURE.md` - Architecture
- `docs/AER_STARTUP_FLOW.md` - Startup details

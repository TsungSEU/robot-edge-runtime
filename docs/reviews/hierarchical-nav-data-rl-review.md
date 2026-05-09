# Remote Review: hierarchical-nav-data-rl

Repository: `TsungSEU/robot-edge-runtime`
Branch: `feat/hierarchical-nav-data-rl`
Review date: 2026-05-09

## Scope

This review was performed remotely through GitHub APIs without cloning the repository locally. It covers repository hygiene, build configuration, ROS package metadata, and security-sensitive configuration exposure on `main` at commit `5adb90646f7bcf427f6114e140632364f97fe41e`.

## Findings

### P0: Public repository contains live secrets and private key material

`config/app_config.json` contains sensitive runtime fields including MQTT credentials, device identifiers, cloud endpoint configuration, and object-storage access credentials. `resource/pki/client_ck.pem` is an RSA private key. Since these files are already public, deleting them in a later commit is not sufficient.

Recommended response:

- Immediately revoke and rotate all exposed MQTT, object-storage, and certificate/private-key credentials.
- Remove `config/app_config.json` and private key material from the repository.
- Rewrite repository history or use GitHub secret-removal support so the exposed values are no longer available from prior commits.
- Keep only `.template` files in git and load sensitive values through environment variables, deployment secrets, or a secret manager.
- Add secret scanning and a pre-commit check for private keys and cloud credentials.

### P1: `ENABLE_ROS2=OFF` build path likely fails

`src/CMakeLists.txt` defines `runtime_symlinks` only inside the `ENABLE_ROS2` block, but later unconditionally adds dependencies from `robot_sim` and `aer` to `runtime_symlinks`. A non-ROS build will reference a target that does not exist.

Recommended fix:

- Guard `add_dependencies(${ROBOT_SIM_APP} runtime_symlinks)` and `add_dependencies(${EDGE_APP} runtime_symlinks)` with `if(ENABLE_ROS2)`.
- Add a CI build matrix entry for `-DENABLE_ROS2=OFF`.

### P1: ONNX Runtime download URL appears inconsistent

The root `CMakeLists.txt` points to a GitHub release path using `v1.10.0` while downloading archive names for `1.16.3`. A clean build with `DOWNLOAD_ONNXRUNTIME=ON` may fail if the release path does not contain those assets.

Recommended fix:

- Align the release path with the archive version.
- Prefer a checksum-pinned dependency fetch or documented preinstalled dependency path.
- Add a clean-container build check that exercises the download path.

### P1: Sensitive config is ignored but already tracked

`.gitignore` marks `config/app_config.json` as sensitive and points users toward template files, but the sensitive concrete config is already tracked in the repository.

Recommended fix:

- Remove the tracked file from git with `git rm --cached config/app_config.json` after replacing it with a safe template.
- Ensure CI rejects future commits that add concrete secret-bearing config files.

### P2: Generated and temporary artifacts are committed

The repository includes runtime and generated artifacts such as `.pids/aer.pid`, `src/tmp.zip`, `src/tmp/*`, coverage HTML/results under `tools/coverage/results/*`, and performance logs under `tools/performance/memory_results/*`. These files increase repository size, distort GitHub language statistics, and make code review noisier.

Recommended fix:

- Remove generated artifacts from git.
- Add ignore rules for `.pids/`, coverage output, performance logs, temporary archives, and `src/tmp/`.
- Publish large reports as CI artifacts instead of committing them.

### P2: Version metadata is inconsistent

README and CMake metadata indicate version `1.10.3`, while `package.xml` still declares `1.2.0` and uses a placeholder maintainer email.

Recommended fix:

- Update `package.xml` to the release version and real maintainer metadata.
- Consider a single source of truth for module/package versioning.

## Review Notes

The current remote branch adds this review document only. It intentionally does not include secrets or secret values.

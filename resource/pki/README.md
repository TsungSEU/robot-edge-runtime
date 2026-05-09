# PKI Material

Do not commit environment-specific certificates, private keys, or gateway trust material to this repository.

Expected runtime files are supplied by deployment automation or a secret manager and are referenced by `config/app_config.json.template` through `AER_PKI_PATH`:

- `client_cc.pem`
- `client_ck.pem`
- `server_ca.pem`
- `public_key.pem`

If any PKI material was committed to a public branch, treat it as compromised, rotate it, and remove it from repository history.

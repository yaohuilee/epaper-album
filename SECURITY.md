# Security

English | [简体中文](SECURITY.zh-CN.md)

## Operating boundary

The device HTTP server is intended for a trusted local network. Its request token provides request validation and cross-site request protection; it is not user authentication. HTTP has no TLS. The provisioning hotspot is intentionally open and password-free. Do not forward the HTTP port to the public internet.

Wi-Fi credentials are stored on device SD/NVS or in ignored local build configuration. They are not public repository content. Do not include real credentials, access tokens or private device data in source, issues, screenshots or logs.

## Reporting

Use **Security → Report a vulnerability** when available. Otherwise describe reproduction steps and impact in an issue without sensitive data; do not publish credentials or complete private logs.

If credentials were exposed, revoke or rotate them first, then address the repository and its history. Deleting the current file does not remove historical content.

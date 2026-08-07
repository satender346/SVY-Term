# SVY-Term parity map against MobaXterm documentation

This document maps features from the MobaXterm documentation to SVY-Term implementation status.

## 1. Core application model

- All-in-one remote toolbox: In progress
- Single desktop app entrypoint: Implemented
- Saved sessions workflow: Implemented (MVP persistence)

## 2. Session types

- SSH session: Implemented (libssh backend service)
- Local terminal session: Implemented
- SFTP session: Implemented (libssh+sftp backend service)
- Telnet/Rlogin/Serial/RDP/VNC/XDMCP/Mosh: Planned modules

## 3. Workspace UX

- Multi-tab environment: Implemented
- Split terminals (2/4 layouts): Planned (API hooks in place)
- Detach/reattach tabs: Planned
- Multi-execution mode: Planned

## 4. SSH and file transfer

- SSH connection profile settings: Implemented (data model)
- Private key path support: Implemented (data model)
- SSH command-at-connect: Implemented (data model)
- SFTP side browser: Planned
- SCP/SFTP transfer engines: Implemented (SFTP upload/download/list via libssh)

## 5. SSH tunneling

- Tunnel profile model: Implemented
- Tunnel manager engine: Scaffolded
- Graphical tunnel builder: Planned

## 6. Settings and persistence

- Global settings file: Implemented
- Terminal defaults (font/charset/logging): Implemented (data model)
- Keyboard shortcuts model: Planned
- Alternate config file location via CLI: Implemented

## 7. Startup automation and CLI

- Execute command at startup: Implemented (--exec)
- Open specific tab/session at startup: Implemented (--newtab)
- Alternate configuration path: Implemented (--config)
- Hidden startup mode: Implemented (--hideterm)

## 8. Security

- Password storage strategy: Planned (Keychain integration)
- Host key validation storage: Planned
- SSH keepalive/compression toggles: Implemented (session model)

## 9. Plugin and extensibility

- Internal protocol abstraction layer: Implemented
- Runtime plugin loader: Planned

## 10. macOS-specific direction

- Native app bundle: Implemented
- Home directory persistence under user profile: Implemented
- macOS keychain integration for secrets: Planned

## Notes

- This project does not copy proprietary code from MobaXterm.
- It re-implements comparable capability categories in original C/C++ code for macOS.

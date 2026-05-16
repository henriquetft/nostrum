# Nostrum Relay

<p align="center">
  <img src="docs/nostrum.png" alt="nostrum logo" width="200"/>
</p>

Nostrum Relay is a Nostr relay implementation written in C, focused on performance, simplicity, and low-level control.

## 🚧 Project Status

> ⚠️ This project is in early development.

The codebase is still evolving and many features are incomplete or unstable. Breaking changes may occur frequently, and it is not recommended for production use at this stage.

## Implemented NIPs
* [x] NIP-01 — Basic protocol flow description
* [x] NIP-02 — Follow List
* [x] NIP-09 — Event Deletion Request
* [x] NIP-11 — Relay Information Document
* [x] NIP-12 — Generic Tag Queries *(NIP-01 related)*
* [x] NIP-16 — Event Treatment *(NIP-01 related)*
* [x] NIP-20 — Command Results *(NIP-01 related)*
* [x] NIP-33 — Parameterized Replaceable Events *(NIP-01 related)*


## ⚙️ Tech Stack

- C (C11)
- GLib
- libsoup
- SQLite (or configurable backend)

## 🚀 Build and run

Clone repository and build the relay:
```bash
apt-get install -y git cmake libsqlite3-dev libglib2.0-dev libjson-glib-dev libsecp256k1-dev libsoup-3.0-dev
git clone https://github.com/henriquetft/nostrum && cd nostrum
cmake -B build/ -DCMAKE_BUILD_TYPE=Release
cmake --build build/ --config Release
```

The nostrum relay executable is generated in build/nostrum. 

To run the relay:
```bash
cd build
./nostrum
```

This will use the configuration file that was copied into the current directory (build/). Note that the configuration file specifies the directory where the SQLite .db database file will be stored.
Nostrum searches for the configuration file in the following order:
1. Passed as an argument via `--config`
2. `NOSTRUM_CONFIG` environment variable
3. `/etc/nostrum.conf`
4. `./nostrum.conf`


## 🤝 Contributing

Contributions are welcome. Keep in mind that the project is still in an early stage and the structure may change frequently.

## 📄 License

This project is licensed under the BSD 3-Clause License.

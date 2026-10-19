# Embedded default resources

The authoritative `.enigmaxml` and gzip resources under `resources/defaults/`, how the C++ byte
arrays under `src/defaults/` are generated, and their expected SHA-256 values.

The raw `.enigmaxml` files are the authoritative, inspectable source artifacts: Finale 27's
"New Document Without Libraries" for macOS and for Windows. They are Finale-generated files with
intentional CRLF endings; their exact bytes are preserved even though `git diff --check` reports
carriage returns as trailing whitespace. The committed deterministic `gzip -n -9` files are the
inputs to the resource generator.

The C++ byte arrays are generated and committed with `scripts/generate_embedded_defaults.py`;
they are not generated during a normal CMake build and not edited by hand. `--check` detects
stale output, and CI runs it. The XML is never wrapped in ZIP or MUSX containers. Tests cover
inflation, expected byte counts or hashes, platform selection, musxdom parsing, and the presence
of required option instances.

Expected SHA-256 values:

| Resource | SHA-256 |
| --- | --- |
| macOS EnigmaXML | `cebcc5af8d625979e1baa11c7350a1fc1cbb8475c776bdb5c34aea059e9a9120` |
| macOS gzip | `c58e69ab810451f7b295b3fe1e5545f9e1dd9d064b10e84c9253fe7a90a1ff66` |
| Windows EnigmaXML | `b151b38bd48580db7dd64a73b1364323936391abb19d74e424f27d35070fd2cb` |
| Windows gzip | `745444c37c44c13b17c72e1c6aad9f05e3e04ac2ab04bce027a2f55850201a5f` |

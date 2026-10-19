# VS Code reference configurations

These tracked samples provide starter VS Code configurations for macOS, Linux,
and Windows. They follow the neighboring musxdom project's local configuration
while making this project's sibling musxdom dependency explicit.

Each sample configures CMake with:

```text
FINALE_MUS_READER_MUSXDOM_SOURCE_DIR=${workspaceFolder}/../musxdom
```

This assumes `finale-mus-reader` and `musxdom` are sibling directories. Adjust
the path in `settings.json` if your checkout layout differs.

## Setup

1. Choose `macos/`, `linux/`, or `windows/`.
2. Copy that directory's JSON files into the ignored workspace `.vscode/`
   directory.
3. Run **CMake: Delete Cache and Reconfigure** so an existing build that fetched
   musxdom is replaced with the local checkout.

For example, on macOS or Linux:

```bash
mkdir -p .vscode
cp .vscode_template/macos/*.json .vscode/
```

The build task uses the existing `build` directory. The launch configuration
builds and starts `finale_mus_reader_tests` with the platform's customary VS
Code debugger.

Suggested extensions:

- CMake Tools (`ms-vscode.cmake-tools`)
- CodeLLDB (`vadimcn.vscode-lldb`) on macOS
- C/C++ (`ms-vscode.cpptools`) on Linux and Windows

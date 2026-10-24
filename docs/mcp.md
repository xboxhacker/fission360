# Fission 360 MCP (Model Context Protocol)

Shipped in **1.13.29**. Current source: **1.13.41**; latest GitHub release: **1.13.35**.

Headless agent interface over **stdio** (JSON-RPC 2.0, Content-Length framing).  
Same idea as peer CAD tools: tools call a shared **command registry**, not dialogs.

**IP:** Behaviour inspired by public MCP docs elsewhere — no third-party source copied.

## Start

```sh
# Headless session (no window)
Fission360 --mcp

# Open a design first
Fission360 --mcp --in part.f360

# Extra folders the agent may read/write (home + temp + open design folder are always allowed)
Fission360 --mcp --allow-path D:\designs --allow-path D:\exports

# Disable path sandbox (not recommended)
Fission360 --mcp --allow-any-path
```

Stdout is protocol only; diagnostics go to stderr.

### Cursor / Claude example

```json
{
  "mcpServers": {
    "fission360": {
      "command": "C:/path/to/Fission360.exe",
      "args": ["--mcp", "--allow-path", "C:/Users/you/Designs"]
    }
  }
}
```

## Tools

| Tool | Purpose |
|------|---------|
| `list_commands` | Registry catalog (`filter` optional) |
| `execute` | `{command, params?}` — any registry id |
| `batch` | Ordered `{commands:[{command,params}]}`; stops on first error |
| `inspect_design` | Bodies / parameters / features |
| `measure` | Body volume / area / CoM / bbox |
| `export` | `{path, format?}` STEP / STL / 3MF / … |
| `new_design` / `open` / `save` | Document lifecycle |
| `undo` / `redo` | Edit history |
| `set_parameter` | `{name, expression\|value, unit?}` + rebuild |

## Registry commands (execute)

| Id | Notes |
|----|--------|
| `system.version` | Version string |
| `doc.new` / `doc.open` / `doc.save` / `doc.export` / `doc.rebuild` / `doc.inspect` | Document |
| `measure.bodies` / `measure.selection` | Measure |
| `edit.undo` / `edit.redo` | History |
| `param.set` | User parameters |
| `feature.add` | `{type, props?}` then rebuild |
| `body.list` | Indices and names |

In the GUI, the same registry is exposed to QML as `fissionCommands` (`listCommands`, `execute`).

## Limits (MVP)

- No bridged `--connect` to a live window yet  
- No `screenshot` tool (use GUI `--screenshot` for now)  
- `batch` does not auto-rollback (use `edit.undo`)  
- Feature modelling still goes through `feature.add` props — not every toolbar dialog field  

## See also

- `docs/peer-inspiration.md` — roadmap chips  
- Scripts: `fissionScripts` (JS/Python) remain available in the GUI  

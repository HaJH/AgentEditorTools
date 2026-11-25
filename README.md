# AgentEditorTools

An Unreal Engine editor plugin that lets AI agents edit assets without driving the editor UI. Agents that share one running editor through MCP otherwise fight over keyboard focus, the clipboard and the mouse cursor, and a modal dialog opened by one of them stops every other agent's calls.

## Who it is for

The primary user is an AI agent. The same functions are planned to be callable by people from Blueprint and Python, but there is no dedicated UI.

Planned features:

- Material graph import: pastes material editor text (T3D) into a material or material function in one synchronous call, without opening the asset editor, and returns a result with diagnostics.
- Modal dialog log: records which modal dialogs open and close to a JSON Lines file under the project's `Saved` directory, so an agent whose calls stopped responding can read what is blocking the editor. It records only; it never answers a dialog.

Planned entry points, all sharing one core:

- An MCP toolset (requires the engine's experimental `ToolsetRegistry` plugin; the rest of the plugin loads without it)
- A Blueprint function library (Blueprint and Python, including headless `-run=pythonscript`)

## Status

Under development. This version is a skeleton: the modules load but provide no functionality yet.

## License

MIT. See [LICENSE](LICENSE).

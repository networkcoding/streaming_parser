# Using Next AI Draw.io in VS Code

This workspace is configured to use the Next AI Draw.io MCP server from VS Code
Chat. The server creates and updates diagrams in a browser-based draw.io
preview.

## Requirements

- VS Code with GitHub Copilot Chat and MCP support.
- Node.js 18 or later, installed where the MCP server runs. For a Remote SSH or
  other remote workspace, this is the remote host.
- Network access to download `@next-ai-drawio/mcp-server` through `npx` if it
  is not already cached.

## MCP server configuration

The workspace configuration is in [`.vscode/mcp.json`](.vscode/mcp.json). The
Draw.io server entry is:

```json
"drawio": {
  "type": "stdio",
  "command": "npx",
  "args": ["@next-ai-drawio/mcp-server@latest"]
}
```

Keep this entry inside the existing `"servers"` object. Do not replace the
whole file if it contains other MCP servers.

After opening the workspace, start or restart the `drawio` server if needed:

1. Open VS Code Chat and check that the Draw.io tools are available.
2. If not, use the Command Palette to run **MCP: List Servers**, select
   `drawio`, and start or restart it. Reload the VS Code window if necessary.

## Create a diagram from Chat

Ask for the diagram in natural language, for example:

- “Read this repo and draw its architecture.”
- “Create a UML class diagram for this library system.”
- “Update the diagram to show the data flow and label the relationships.”

When the agent uses Draw.io, the preview opens in a browser and updates as the
diagram is drawn. Use the preview's URL and its current session ID; a new
session can have a different URL and port.

The preview is not embedded in the chat response. Next AI Draw.io's preview
does not allow embedding in VS Code's Simple Browser, so use **Open in Browser**
or a regular browser if Simple Browser is blank.

## Forward the preview port to a Windows VS Code client

For a remote workspace, the Draw.io server listens on the remote host. A
`localhost` URL opened on Windows points to the Windows machine, not the remote
host, unless VS Code forwards the port.

The workspace's [`.vscode/settings.json`](.vscode/settings.json) labels port
6002 and asks VS Code to open a browser when it auto-forwards the port:

```json
"remote.portsAttributes": {
  "6002": {
    "label": "Next AI Draw.io",
    "protocol": "http",
    "onAutoForward": "openBrowser"
  }
}
```

This setting describes the port; it does **not** create a port-forwarding
tunnel by itself. To forward it from the Windows VS Code client:

1. Connect to the remote workspace in VS Code.
2. Open the Command Palette with `Ctrl+Shift+P` and run
   **Ports: Focus on Ports View**. Alternatively, open the **Ports** tab in
   the bottom panel.
3. Select **Forward a Port** (`+`), enter `6002`, and press Enter.
4. Confirm that the Ports view shows port `6002` and a forwarded/local address.
   Use **Open in Browser** on that row.
5. If the local address is `localhost:6002`, open the preview URL returned by
   the Draw.io session, for example:

   ```text
   http://localhost:6002/shell/?mcp=<session-id>
   ```

   Replace `<session-id>` with the value in the actual preview URL. If VS Code
   assigns another local port, use that port instead of `6002`.

If the page reports `ERR_CONNECTION_REFUSED`, check that the Draw.io MCP
server is running, then verify the port row is still forwarded in the **Ports**
view. If the server's preview URL uses a port other than 6002, forward that
port and update the `"6002"` entry in `remote.portsAttributes` accordingly.

## Save or export a diagram

The MCP server auto-saves sessions under `~/.next-ai-drawio` on the host where
the server runs; this is separate from the repository. To save the current
live preview into the remote workspace, ask VS Code Chat to export it, for
example:

> Export the current live diagram as an editable `.drawio` file to
> `/home/penglei/streaming_parser/architecture.drawio`.

The agent should use the `drawio-export_diagram` MCP tool with the requested
path. Use an **absolute path on the remote host** so the file lands in the
workspace; relative paths may resolve against the MCP server's working
directory instead. The exported file should appear in VS Code Explorer.

- Use `.drawio` or `.drawio.svg` to keep the diagram editable in Draw.io.
- Use `.png` or `.svg` when you only need an image for viewing or sharing.
- Port forwarding is needed to open the live preview from the Windows client,
  but not to export a file: the MCP server writes directly to the remote
  workspace.

To continue editing a saved `.drawio` file later, load it into the Draw.io
session, make the changes, then export it to the same workspace path.

## References

- [Next AI Draw.io: MCP clients](https://next-ai-drawio.jiang.jp/en/docs/mcp-clients)
- [Next AI Draw.io MCP server README](https://raw.githubusercontent.com/DayuanJiang/next-ai-draw-io/main/packages/mcp-server/README.md)

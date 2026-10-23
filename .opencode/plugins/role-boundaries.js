import { realpath } from "node:fs/promises";
import path from "node:path";

const roles = ["leader", "architect", "coder", "reviewer", "guardian", "auditor"];
const readOnly = new Set(["architect", "reviewer", "auditor"]);
const protectedRules = {
  "*.env": "ask", "*.env.*": "ask", "*.env/*": "ask",
  "*.pem": "ask", "*.key": "ask",
  "*credentials*": "ask", "*creds*": "ask", "*secret*": "ask",
};
for (const name of [".git", ".agents", ".github", ".opencode", ".vscode"]) {
  protectedRules[name] = "deny";
  protectedRules[name + "/*"] = "deny";
  protectedRules["*/" + name] = "deny";
  protectedRules["*/" + name + "/*"] = "deny";
}
// Put protected files after sensitive-name asks: protected always wins.
Object.assign(protectedRules, {
  "*AGENTS.md": "deny", "*opencode.json": "deny", "*opencode.jsonc": "deny",
  ".mcp.json": "deny", "*/.mcp.json": "deny",
});

// Preserve the source's patch-move prohibition and exact whole-command gate.
// This is a cooperative tool boundary, not an OS sandbox.
export const RoleBoundaries = async ({ directory, worktree = directory }) => {
  const agents = new Map();
  const commands = new Map();
  return {
    config: async config => {
      const prefix = path.relative(worktree, directory).replaceAll("\\", "/");
      for (const role of ["leader", "guardian"]) {
        const agent = config.agent?.[role];
        if (!agent) throw new Error("Missing role definition: " + role);
        const configured = agent.permission.edit;
        if (!configured || typeof configured !== "object" || configured["*"] !== "deny") {
          throw new Error("Documentation roles require bounded edit permissions.");
        }
        const edit = { ...configured };
        Object.assign(edit, protectedRules);
        agent.permission.edit = Object.fromEntries(Object.entries(edit).map(([pattern, action]) =>
          [prefix && !pattern.startsWith("*") ? prefix + "/" + pattern : pattern, action]));
      }
      for (const role of readOnly) {
        const agent = config.agent?.[role];
        if (!agent) throw new Error("Missing role definition: " + role);
        const rules = agent.permission.bash;
        if (rules["*"] !== "deny") throw new Error("Read-only roles require default-deny shell permissions.");
        for (const [command, action] of Object.entries(rules)) {
          if (action === "allow" && (/[\r\n*?;&|<>`]/.test(command) || /^git\s+(?:commit|push)\b/.test(command))) {
            throw new Error("Read-only shell grants must be exact simple non-writing commands.");
          }
        }
        commands.set(role, new Set(Object.entries(rules)
          .filter(([command, action]) => action === "allow" && !/[*?]/.test(command))
          .map(([command]) => command)));
      }
    },
    "chat.params": async ({ sessionID, agent }) => { agents.set(sessionID, agent); },
    event: async ({ event }) => {
      if (event.type === "session.deleted") agents.delete(event.properties.info.id);
    },
    "tool.execute.before": async ({ tool, sessionID }, { args }) => {
      if (tool === "apply_patch" && /^\s*\*\*\* Move to:/m.test(args.patchText ?? "")) {
        throw new Error("Patch moves are disabled: use separately permission-checked edits.");
      }
      if (tool !== "bash") return;
      const agent = agents.get(sessionID);
      if (!roles.includes(agent)) throw new Error("Shell blocked until the current agent is identified.");
      if (!readOnly.has(agent)) return;
      if (!commands.get(agent)?.has(args.command)) {
        throw new Error("Read-only shell requires an exact allowed whole command.");
      }
      const root = await realpath(directory);
      const cwd = await realpath(path.resolve(directory, args.workdir ?? "."));
      const location = path.relative(root, cwd);
      if (location === ".." || location.startsWith(".." + path.sep) || path.isAbsolute(location)) {
        throw new Error("Read-only validation requires a work directory inside the repository.");
      }
    },
  };
};

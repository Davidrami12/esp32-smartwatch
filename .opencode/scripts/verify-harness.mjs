import assert from "node:assert/strict";
import { readFile, readdir, lstat, mkdtemp, mkdir, writeFile, cp, rm } from "node:fs/promises";
import { createHash } from "node:crypto";
import { tmpdir } from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { RoleBoundaries } from "../plugins/role-boundaries.js";
const gitChecks = ["git status --short", "git --no-pager diff --no-ext-diff --no-textconv",
  "git --no-pager diff --no-ext-diff --no-textconv --cached", "git --no-pager log -5 --oneline", "git diff --check"];

const root = fileURLToPath(new URL("../", import.meta.url));
const names = ["leader", "architect", "coder", "reviewer", "guardian", "auditor"];
const read = file => readFile(path.join(root, file), "utf8");
const digest = bytes => createHash("sha256").update(bytes).digest("hex");
let checks = 0;
function check(value, message) { assert.ok(value, message); checks++; }

async function files(dir, prefix = "") {
  const result = [];
  for (const name of (await readdir(dir)).sort()) {
    if (name === "node_modules" || name === ".git") continue;
    const full = path.join(dir, name);
    const stat = await lstat(full);
    check(!stat.isSymbolicLink(), "No symlinks: " + prefix + name);
    if (stat.isDirectory()) result.push(...await files(full, prefix + name + "/"));
    else result.push(prefix + name);
  }
  return result;
}

// A source-directory digest is an optional extraction-time check, not a dependency
// of the portable harness. Output hashes only, never source contents.
if (process.argv[2] === "--source-digest") {
  const source = path.resolve(process.argv[3]);
  const manifest = {};
  for (const file of await files(source)) manifest[file] = digest(await readFile(path.join(source, file)));
  console.log(JSON.stringify({ digest: digest(JSON.stringify(manifest)), files: manifest }, null, 2));
  process.exit(0);
}

// This parser intentionally accepts only the template's simple frontmatter subset.
// The installed OpenCode loader is the authoritative YAML/configuration validator.
function metadata(text) {
  const match = /^---\r?\n([\s\S]*?)\r?\n---\r?\n/.exec(text);
  check(match, "Frontmatter exists");
  const result = {};
  const stack = [{ indent: -1, value: result }];
  for (const line of match[1].split(/\r?\n/)) {
    const field = /^( *)("[^"]+"|[^:]+):(?: (.*))?$/.exec(line);
    check(field, "Supported frontmatter line");
    const indent = field[1].length;
    while (stack.at(-1).indent >= indent) stack.pop();
    const key = field[2].startsWith('"') ? JSON.parse(field[2]) : field[2];
    const parent = stack.at(-1).value;
    check(!Object.hasOwn(parent, key), "Unique frontmatter key");
    const raw = field[3];
    parent[key] = raw === undefined ? {} : raw.startsWith('"') ? JSON.parse(raw) : raw;
    if (raw === undefined) stack.push({ indent, value: parent[key] });
  }
  return result;
}
function match(input, pattern) {
  return new RegExp("^" + pattern.replace(/[.+^${}()|[\]\\]/g, "\\$&")
    .replaceAll("*", ".*").replaceAll("?", ".") + "$", process.platform === "win32" ? "i" : "")
    .test(input.replaceAll("\\", "/"));
}
function action(rules, input) {
  if (typeof rules === "string") return rules;
  let result = "ask";
  for (const [pattern, value] of Object.entries(rules ?? {})) if (match(input, pattern)) result = value;
  return result;
}
const config = JSON.parse(await read("opencode.json"));
const agents = {};
assert.deepEqual((await readdir(path.join(root, "agents"))).sort(), names.map(n => n + ".md").sort());
for (const name of names) {
  agents[name] = metadata(await read("agents/" + name + ".md"));
  check(agents[name].mode === (name === "leader" ? "primary" : "subagent"), "Role mode");
  for (const target of names) check(action(agents[name].permission.task, target) ===
    (name === "leader" && ["architect", "coder", "reviewer", "guardian"].includes(target) ? "allow" : "deny"), "Delegation matrix");
  for (const tool of Object.values(agents[name].permission)) {
    for (const value of typeof tool === "string" ? [tool] : Object.values(tool))
      check(["allow", "ask", "deny"].includes(value), "Permission action");
  }
  check(action(agents[name].permission.bash, "git commit -m example") === "deny", "Commit deny");
  check(action(agents[name].permission.bash, "git push origin main") === "deny", "Push deny");
}
check(config.default_agent === "leader", "Default Leader");
check(config.permission.task.auditor === "deny", "Direct-only Auditor");
const command = metadata(await read("commands/sdd-feature.md"));
check(command.agent === "leader", "Command routes to Leader");
assert.deepEqual(Object.keys(config.mcp ?? {}), ["context7"], "Only Context7 is configured");
check(config.mcp.context7.headers.CONTEXT7_API_KEY === "{env:CONTEXT7_API_KEY}", "Context7 uses environment credentials");
for (const file of [".env", "module/.env.local", "keys/private.key", "credentials.json", "docs/secrets/note.md", "docs/.env/page.md"]) {
  check(action(config.permission.read, file) === "ask", "Sensitive read requires approval");
}
check(action(config.permission.read, "module/.env.example") === "allow", "Environment example exception");
check(action(config.permission.read, "docs/secrets/.env.example") === "ask", "Sensitive parent overrides example");
check(action(config.permission.read, "docs/nested/.git/.env.example") === "deny", "Protected read overrides example");
for (const file of await files(root)) {
  const text = await read(file);
  if (file.endsWith(".md")) {
    check([...text.matchAll(/^```/gm)].length % 2 === 0, "Balanced documentation fences");
    for (const link of text.matchAll(/\]\(([^)]+)\)/g)) {
      if (/^(?:https?:|#)/.test(link[1])) continue;
      const target = path.resolve(root, path.dirname(file), link[1].split("#")[0]);
      check((await lstat(target)).isFile(), "Local documentation link resolves");
    }
  }
  check(!/(?:[A-Za-z]:\\(?:Users|home)\\[^\\\s]+\\|https?:\/\/(?:localhost|127(?:\.\d+){3})(?::\d+)?\/)/i.test(text), "No machine-specific paths or local service URLs: " + file);
  check(!/-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|\bgh[pousr]_[A-Za-z0-9]{30,}|\bsk-[A-Za-z0-9]{30,}/.test(text), "No recognized credential patterns: " + file);
  if (file.startsWith("agents/") || file.startsWith("commands/") || file === "opencode.json")
    check(!/\bskill(?:s)?\b/i.test(text), "No removed instruction dependencies: " + file);
}

const temporary = await mkdtemp(path.join(tmpdir(), "portable-orchestration-"));
let native = "not requested";
try {
  const project = path.join(temporary, "unrelated-project");
  await mkdir(project);
  const copy = path.join(project, ".opencode");
  await cp(root, copy, { recursive: true, filter: source => !source.split(path.sep).includes("node_modules") });
  for (const instruction of config.instructions) {
    check((await readFile(path.join(project, instruction), "utf8")).length > 0, "Renamed instruction resolves");
  }
  const hooks = await RoleBoundaries({ directory: project, worktree: project });
  const resolved = { agent: structuredClone(agents) };
  await hooks.config(resolved);
  for (const role of ["leader", "guardian"]) {
    for (const file of ["docs/AGENTS.md", "docs/.git/config", "docs/nested/.github/workflow.yml", "src/file.txt", ".opencode/opencode.json", "docs/opencode.json"]) {
      check(action(resolved.agent[role].permission.edit, file) === "deny", "Protected documentation boundary: " + file);
    }
    check(action(resolved.agent[role].permission.edit, "docs/nested/page.md") === "allow", "Documentation write");
    check(action(resolved.agent[role].permission.edit, "docs/secrets/note.md") === "ask", "Sensitive write gate");
  }
  const nonGit = await RoleBoundaries({ directory: project, worktree: path.parse(project).root });
  const prefixed = { agent: structuredClone(agents) };
  await nonGit.config(prefixed);
  const relativeDocs = path.relative(path.parse(project).root, path.join(project, "docs/page.md"));
  check(action(prefixed.agent.leader.permission.edit, relativeDocs) === "allow", "Non-Git prefix");
  const invoke = (command, workdir = project, sessionID = "review") =>
    hooks["tool.execute.before"]({ tool: "bash", sessionID }, { args: { command, workdir } });
  await assert.rejects(invoke(gitChecks[0]), /agent is identified/); checks++;
  for (const role of ["architect", "reviewer", "auditor"]) {
    await hooks["chat.params"]({ sessionID: "review", agent: role });
    check(resolved.agent[role].permission.edit === "deny", "Read-only edits");
    for (const command of gitChecks) await invoke(command);
    for (const command of ["git status --short; echo change", "git diff --check > output.txt", "npm run test", "git reset --hard", "git push origin main", "node -e 'process.exit()'"]) {
      await assert.rejects(invoke(command), /exact allowed whole command/); checks++;
    }
  }
  await assert.rejects(invoke(gitChecks[0], temporary), /inside the repository/); checks++;
  const child = path.join(project, "component");
  await mkdir(child);
  await invoke(gitChecks[0], child); checks++;
  await assert.rejects(hooks["tool.execute.before"]({ tool: "apply_patch" }, { args: { patchText: "*** Update File: docs/a.md\n*** Move to: src/a.md" } }), /Patch moves/); checks++;
  await hooks["chat.params"]({ sessionID: "code", agent: "coder" });
  await invoke("destination-test-command", project, "code"); // Hook allows; native permission still asks.
  await hooks.event({ event: { type: "session.deleted", properties: { info: { id: "review" } } } });
  await assert.rejects(invoke(gitChecks[0]), /agent is identified/); checks++;

  // Native agent permissions are the only command configuration source.
  const customAgents = structuredClone(agents);
  customAgents.reviewer.permission.bash["check-tool verify"] = "allow";
  const customHooks = await RoleBoundaries({ directory: project, worktree: project });
  await customHooks.config({ agent: customAgents });
  await customHooks["chat.params"]({ sessionID: "review", agent: "reviewer" });
  const customInvoke = () => customHooks["tool.execute.before"]({ tool: "bash", sessionID: "review" }, { args: { command: "check-tool verify" } });
  await customInvoke(); checks++;
  const invalidAgents = structuredClone(agents);
  invalidAgents.reviewer.permission.bash["git commit -m example"] = "allow";
  const invalidHooks = await RoleBoundaries({ directory: project, worktree: project });
  await assert.rejects(invalidHooks.config({ agent: invalidAgents }), /exact simple non-writing commands/); checks++;

  if (process.argv.includes("--native")) {
    const offlineConfig = structuredClone(config);
    offlineConfig.mcp.context7.enabled = false;
    await writeFile(path.join(copy, "opencode.json"), JSON.stringify(offlineConfig));
    // Isolate global config, credentials, plugins, caches, and unrelated parent configs.
    // No model invocation is made. The CLI may initialize its own temporary files.
    const isolated = path.join(temporary, "home");
    await mkdir(isolated);
    const env = { ...process.env, HOME: isolated, USERPROFILE: isolated,
      XDG_CONFIG_HOME: path.join(isolated, "config"), XDG_DATA_HOME: path.join(isolated, "data"),
      XDG_CACHE_HOME: path.join(isolated, "cache"), XDG_STATE_HOME: path.join(isolated, "state"),
      OPENCODE_CONFIG_DIR: copy, OPENCODE_DISABLE_DEFAULT_PLUGINS: "1",
      OPENCODE_DISABLE_EXTERNAL_SKILLS: "1", OPENCODE_DISABLE_CLAUDE_CODE_SKILLS: "1",
      OPENCODE_DISABLE_AUTOUPDATE: "1" };
    delete env.OPENCODE_CONFIG;
    delete env.OPENCODE_CONFIG_CONTENT;
    delete env.OPENCODE_DISABLE_PROJECT_CONFIG;
    for (const args of [["debug", "config"], ["agent", "list"]]) {
      const result = spawnSync("opencode", args, { cwd: project, env, encoding: "utf8", shell: process.platform === "win32", timeout: 120000, maxBuffer: 8 * 1024 * 1024 });
      assert.equal(result.status, 0, "Native " + args.join(" ") + ": " + (result.stderr || result.error || result.stdout));
      if (args[0] === "debug") {
        const loaded = JSON.parse(result.stdout);
        check(loaded.default_agent === "leader", "Native default agent");
        for (const name of names) check(loaded.agent?.[name], "Native discovered " + name);
        check(loaded.command?.["sdd-feature"], "Native discovered command");
        check(loaded.instructions.some(p => p.endsWith("AGENTS.md")), "Native shared instructions");
        check(loaded.mcp.context7.enabled === false, "Native validation does not connect to MCP");
        check(loaded.plugin?.some(p => String(p).includes("role-boundaries.js")), "Native discovered boundary plugin");
      } else for (const name of names) check(new RegExp("\\b" + name + "\\b").test(result.stdout), "Native registered " + name);
      checks++;
    }
    native = "installed CLI debug config and agent list passed in isolated copied project";
  }
} finally {
  check(path.dirname(temporary) === path.resolve(tmpdir()) && path.basename(temporary).startsWith("portable-orchestration-"), "Temporary cleanup boundary");
  await rm(temporary, { recursive: true, force: true });
}
const manifest = {};
for (const file of await files(root)) manifest[file] = digest(await read(file));
console.log(JSON.stringify({ result: "PASS", checks, native, artifactDigest: digest(JSON.stringify(manifest)), files: Object.keys(manifest).length,
  scope: "static configuration, hook simulations, and renamed-copy portability; no model execution" }, null, 2));

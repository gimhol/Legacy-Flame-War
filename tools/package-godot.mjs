#!/usr/bin/env node
// 把 Godot 宿主打成一个免安装目录（运行时零参数）。
//
//   npm run godot:pack       → native/release/Legacy Flame War_godot_<ver>/
//   npm run godot:pack:zip   → 同上 + 同名 .zip
//
// **所有输入路径都在这里推导，不接受路径参数**：
//   运行时   GODOT_BIN → PATH（godot4 / godot）→ 桌面上的 Godot_v*-stable_win64*.exe
//   工程     native/host/godot/project
//   扩展     project/bin/lfw_godot.dll（缺了就先 `npm run native godot`）
//   资源根   dist/builtin_data + dist/version.json（仓库根 `npm run build` 的产物）
// 数据包   dist/{prel,data}.zip（直接随包发成 resources/*.zip）；`--unpack` 时才展开成目录
//   图标     src/assets/btn_*.png（顶栏那几张，**不在数据包里**）
//   窗口图标 host/godot/project/icon.(ico|png) → 否则 extension/icon256.png（自动补 icon.ico）
//
// 产物布局（宿主会按 `<exe 所在目录>/resources` 自动找资源，不需要任何参数）：
//   <out>/start.exe
//   <out>/project.godot  main.tscn  lfw_godot.gdextension[.uid]
//   <out>/bin/lfw_godot.dll
//   <out>/.godot/extension_list.cfg   ← 让 GDExtension 在启动时注册
//   <out>/resources/root/   resources/prel.zip   resources/data.zip   resources/icons/
//
// 用的是本机 Godot 运行时（编辑器版也能直接跑工程），所以**不需要安装 export templates**。
import { spawnSync } from "node:child_process";
import {
  copyFileSync, cpSync, existsSync, mkdirSync, readdirSync, readFileSync, rmSync, statSync,
  writeFileSync,
} from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { inflateRawSync } from "node:zlib";

const HERE = dirname(fileURLToPath(import.meta.url));
const NATIVE = resolve(HERE, "..");
const REPO = resolve(NATIVE, "..");
const PROJECT = join(NATIVE, "host", "godot", "project");
const DIST = join(REPO, "dist");
const HOST_DATA = join(NATIVE, "build", "host-data");
const ASSETS = join(REPO, "src", "assets");
const RELEASE = join(NATIVE, "release");
const APP_NAME = "Legacy Flame War";

const FLAGS = new Set(process.argv.slice(2));
if (FLAGS.has("--help") || FLAGS.has("-h")) {
  console.log("usage: node tools/package-godot.mjs [--zip] [--unpack]");
  console.log("  --zip     同时压一个同名 zip（`npm run godot:pack:zip` 就是这条）");
  console.log("  --unpack  数据包展开成目录（缺省是把 dist/*.zip 原样放进 resources/）");
  console.log("  所有输入路径都用脚本里的推导，没有路径参数。");
  process.exit(0);
}
const ZIP = FLAGS.has("--zip");
const UNPACK = FLAGS.has("--unpack");

if (process.platform !== "win32") {
  console.error("[godot-pack] 这个打包脚本只做 Windows 版（start.exe + lfw_godot.dll）");
  process.exit(1);
}

function step(msg) {
  console.log(`[godot-pack] ${msg}`);
}
function fail(msg) {
  console.error(`[godot-pack] ${msg}`);
  process.exit(1);
}
function is_dir(p) {
  try {
    return statSync(p).isDirectory();
  } catch {
    return false;
  }
}
function read_json(path) {
  try {
    return JSON.parse(readFileSync(path, "utf8"));
  } catch {
    return {};
  }
}
// 把 PNG 包成单尺寸 ICO（Vista+ 允许条目直接放 PNG 数据），Windows 会自己缩放
function png_to_ico(png) {
  if (png.length < 24 || png.readUInt32BE(0) !== 0x89504e47) return null;
  const w = png.readUInt32BE(16);
  const h = png.readUInt32BE(20);
  if (w === 0 || h === 0 || w > 256 || h > 256) return null;
  const header = Buffer.alloc(22);
  header.writeUInt16LE(1, 2); // 1 = ICO
  header.writeUInt16LE(1, 4); // 1 张图
  header.writeUInt8(w === 256 ? 0 : w, 6);
  header.writeUInt8(h === 256 ? 0 : h, 7);
  header.writeUInt16LE(1, 10); // planes
  header.writeUInt16LE(32, 12); // bpp
  header.writeUInt32LE(png.length, 14);
  header.writeUInt32LE(22, 18);
  return Buffer.concat([header, png]);
}

// ---------------------------------------------------------------------------
// Godot 运行时：GODOT_BIN → PATH → 桌面（同版本优先非 console 版，双击不出黑框）
// ---------------------------------------------------------------------------
function find_godot() {
  const env = process.env.GODOT_BIN;
  if (env && existsSync(env)) return env;
  const which = process.platform === "win32" ? "where" : "which";
  for (const name of ["godot4", "godot"]) {
    const r = spawnSync(which, [name], { encoding: "utf8" });
    if (r.status === 0) {
      const first = (r.stdout ?? "")
        .split(/\r?\n/)
        .map((s) => s.trim())
        .find(Boolean);
      if (first) return first;
    }
  }
  const home = process.env.USERPROFILE ?? process.env.HOME ?? "";
  const found = [];
  for (const desk of [join(home, "Desktop"), join(home, "OneDrive", "Desktop")]) {
    if (!is_dir(desk)) continue;
    for (const f of readdirSync(desk)) {
      if (/^Godot_v[\d.]+-stable_win64(_console)?\.exe$/i.test(f)) found.push(join(desk, f));
    }
  }
  if (found.length === 0) return null;
  const ver = (p) => {
    const m = /Godot_v(\d+)\.(\d+)(?:\.(\d+))?/.exec(p);
    return m ? Number(m[1]) * 1e6 + Number(m[2]) * 1e3 + Number(m[3] ?? 0) : 0;
  };
  found.sort((a, b) => {
    if (ver(a) !== ver(b)) return ver(b) - ver(a);
    // 同版本优先**不带** _console 的（打包出去双击不该弹控制台）
    return (/_console\.exe$/i.test(a) ? 1 : 0) - (/_console\.exe$/i.test(b) ? 1 : 0);
  });
  return found[0];
}

// ---------------------------------------------------------------------------
// 最小 zip 解包（中央目录 + inflateRaw），免得为了 `dist/*.zip` 加依赖
// ---------------------------------------------------------------------------
function unzip_dir(zip_path, out_dir) {
  const buf = readFileSync(zip_path);
  let eocd = -1;
  const stop = Math.max(0, buf.length - 22 - 0xffff);
  for (let i = buf.length - 22; i >= stop; --i) {
    if (buf.readUInt32LE(i) === 0x06054b50) {
      eocd = i;
      break;
    }
  }
  if (eocd < 0) throw new Error(`不是 zip：${zip_path}`);
  const count = buf.readUInt16LE(eocd + 10);
  let off = buf.readUInt32LE(eocd + 16);
  let files = 0;
  for (let i = 0; i < count; ++i) {
    if (buf.readUInt32LE(off) !== 0x02014b50) throw new Error(`中央目录损坏：${zip_path}`);
    const method = buf.readUInt16LE(off + 10);
    const csize = buf.readUInt32LE(off + 20);
    const nlen = buf.readUInt16LE(off + 28);
    const elen = buf.readUInt16LE(off + 30);
    const clen = buf.readUInt16LE(off + 32);
    const lho = buf.readUInt32LE(off + 42);
    const name = buf.toString("utf8", off + 46, off + 46 + nlen).replace(/\\/g, "/");
    off += 46 + nlen + elen + clen;
    if (!name || name.endsWith("/")) {
      if (name) mkdirSync(join(out_dir, name), { recursive: true });
      continue;
    }
    const data_start = lho + 30 + buf.readUInt16LE(lho + 26) + buf.readUInt16LE(lho + 28);
    const raw = buf.subarray(data_start, data_start + csize);
    const data = method === 0 ? raw : method === 8 ? inflateRawSync(raw) : null;
    if (data === null) throw new Error(`不支持的压缩方式 ${method}：${name}`);
    const target = join(out_dir, name);
    mkdirSync(dirname(target), { recursive: true });
    writeFileSync(target, data);
    ++files;
  }
  return files;
}

// ---------------------------------------------------------------------------
// 压缩：优先 bsdtar（Win10+），回退 PowerShell Compress-Archive
// ---------------------------------------------------------------------------
function make_zip(dir, zip_path) {
  rmSync(zip_path, { force: true });
  const tar = spawnSync("tar", ["-a", "-c", "-f", zip_path, "-C", dir, "."], { stdio: "inherit" });
  if (tar.status === 0) return true;
  if (process.platform !== "win32") return false;
  const ps = spawnSync(
    "powershell",
    ["-NoProfile", "-Command",
      `Compress-Archive -Path '${dir}\\*' -DestinationPath '${zip_path}' -Force`],
    { stdio: "inherit" },
  );
  return ps.status === 0;
}

// ---------------------------------------------------------------------------
// 主流程
// ---------------------------------------------------------------------------
const dll = join(PROJECT, "bin", "lfw_godot.dll");
step("检查 GDExtension");
if (!existsSync(dll)) {
  fail(`缺 ${dll}\n           先跑 \`npm run native godot\`（需要 GODOT_CPP_DIR 与 build/godot-api/extension_api.json）`);
}

step("检查资源根（dist）");
if (!is_dir(join(DIST, "builtin_data"))) {
  fail(`缺 ${join(DIST, "builtin_data")}\n           先在仓库根跑 \`npm run build\``);
}

const godot = find_godot();
if (!godot) {
  fail("找不到 Godot 可执行文件：设 GODOT_BIN，或把 Godot 放到 PATH / 桌面上");
}

const version =
  read_json(join(DIST, "version.json")).version ??
  read_json(join(NATIVE, "package.json")).version ??
  "0.0.0";
const name = `${APP_NAME}_godot_${version}`;
const out = join(RELEASE, name);

step(`产物目录 ${out}`);
mkdirSync(RELEASE, { recursive: true });
try {
  rmSync(out, { recursive: true, force: true, maxRetries: 3, retryDelay: 300 });
} catch {
  fail(`${out} 被占用（可能还开着里面的 start.exe，或用资源管理器打开了它）`);
}
mkdirSync(out, { recursive: true });

// 1) 运行时
step(`运行时 ${godot}`);
copyFileSync(godot, join(out, "start.exe"));

// 2) 工程（根目录就是 res:// ⇒ 双击 start.exe 不用 --path）
step("复制工程");
for (const f of ["project.godot", "main.tscn", "lfw_godot.gdextension", "lfw_godot.gdextension.uid"]) {
  const src = join(PROJECT, f);
  if (existsSync(src)) copyFileSync(src, join(out, f));
}
cpSync(join(PROJECT, "bin"), join(out, "bin"), { recursive: true });

// GDExtension 的注册表：没有它启动时 `LfwGodot` 类找不到
mkdirSync(join(out, ".godot"), { recursive: true });
const ext_list = join(PROJECT, ".godot", "extension_list.cfg");
if (existsSync(ext_list)) {
  copyFileSync(ext_list, join(out, ".godot", "extension_list.cfg"));
} else {
  writeFileSync(join(out, ".godot", "extension_list.cfg"), "res://lfw_godot.gdextension\n");
}

// 3) 资源：root（builtin_data + version.json）
step("复制资源根");
const res_root = join(out, "resources", "root");
mkdirSync(res_root, { recursive: true });
cpSync(join(DIST, "builtin_data"), join(res_root, "builtin_data"), { recursive: true });
const version_json = join(DIST, "version.json");
if (existsSync(version_json)) copyFileSync(version_json, join(res_root, "version.json"));

// 4) 数据包：缺省原样放 zip（宿主直接读 zip），`--unpack` 才展开成目录
const res_dir = join(out, "resources");
function stage_package(pkg) {
  const zip = join(DIST, `${pkg}.zip`);
  const unpacked = join(HOST_DATA, pkg);
  if (!UNPACK && existsSync(zip)) {
    step(`数据包 ${pkg}.zip <- ${zip}`);
    copyFileSync(zip, join(res_dir, `${pkg}.zip`));
    return;
  }
  if (is_dir(unpacked) && readdirSync(unpacked).length > 0) {
    step(`数据包 ${pkg}/ <- ${unpacked}`);
    cpSync(unpacked, join(res_dir, pkg), { recursive: true });
    return;
  }
  if (!existsSync(zip)) fail(`缺 ${pkg}：既没有 ${unpacked}，也没有 ${zip}`);
  step(`数据包 ${pkg}/ <- ${zip}（解包）`);
  const target = join(res_dir, pkg);
  mkdirSync(target, { recursive: true });
  step(`  解开 ${unzip_dir(zip, target)} 个条目`);
}
stage_package("prel");
stage_package("data");

// 5) 顶栏图标（不在数据包里）
step("复制顶栏图标");
const icons_out = join(res_dir, "icons");
mkdirSync(icons_out, { recursive: true });
let icons = 0;
if (is_dir(ASSETS)) {
  for (const f of readdirSync(ASSETS)) {
    if (!/^btn_.*\.png$/i.test(f)) continue;
    copyFileSync(join(ASSETS, f), join(icons_out, f));
    ++icons;
  }
}
// 窗口 / 任务栏图标：优先用工程里自带的 `icon.(ico|png)`，否则回落到游戏图标
// （宿主 `apply_window_icon()` 先试 resources/icons/icon.ico，再试 icon.png）
const icon_custom = [
  join(NATIVE, "host", "godot", "project", "icon.ico"),
  join(NATIVE, "host", "godot", "project", "icon.png"),
  join(NATIVE, "icon.ico"),
  join(NATIVE, "icon.png"),
].find((p) => existsSync(p));
const icon_fallback = [
  join(REPO, "extension", "icon256.png"),
  join(REPO, "dist-extension", "icon256.png"),
].find((p) => existsSync(p));
if (icon_custom) {
  copyFileSync(icon_custom, join(icons_out, basename(icon_custom)));
  step(`  窗口图标 ${icon_custom}`);
} else if (icon_fallback) {
  copyFileSync(icon_fallback, join(icons_out, "icon.png"));
  const ico = png_to_ico(readFileSync(icon_fallback));
  if (ico) {
    writeFileSync(join(icons_out, "icon.ico"), ico);
    step(`  窗口图标 ${icon_fallback} + icon.ico`);
  } else {
    step(`  窗口图标 ${icon_fallback}（>256px，跳过 .ico）`);
  }
} else {
  step("  窗口图标（无：沿用 Godot 默认图标）");
}
step(`  ${icons} 张`);
if (icons === 0) console.warn("[godot-pack] 警告：没找到 btn_*.png，自绘标题栏会只有命中盒");

// 6) 模组目录（空的也能直接往里丢 *.zip；`mods.json5` 控制顺序/禁用）
step("建立 mods/ 目录");
mkdirSync(join(out, "mods"), { recursive: true });
writeFileSync(
  join(out, "mods", "readme.txt"),
  [
    "Legacy Flame War 模组目录（Godot 版）",
    "",
    "把模组包（*.zip）放到本目录，下一次启动游戏就会自动加载，不需要任何命令行参数。",
    "",
    "两种 zip 都支持",
    "- 普通数据包 zip：会和本体数据合并（同名资源以模组为准）",
    "- 自定义游戏包 zip（内含 index.json / index.json5，里面用 paths 指向包内的 prel.zip / data.zip 等）：",
    "  整个游戏被它替换（本目录有多个时只认第一个，按文件名排序）",
    "",
    "控制顺序 / 禁用（可选）：在本目录新建 mods.json5",
    "{",
    "  // 加载顺序：先列出的先加载，后面的覆盖前面的；没列出的按文件名排在最后",
    "  order: [\"基础模组.zip\", \"覆盖补丁.zip\"],",
    "  // 不加载",
    "  disabled: [\"先别用的模组.zip\"],",
    "}",
    "（也可以命名为 mods.json，写法相同；改完重启游戏生效）",
    "",
    "也可以直接把 zip 拖进游戏窗口：普通数据包会立即热加载（同名去重），",
    "自定义游戏包会自动重启并换成它。",
    "入口页的「附加模组」按钮会弹系统文件对话框，选中的 zip 同样立即加载。",
    "入口页的「自定义游戏」按钮选的是 full.zip（内含 index.json），整包替换游戏数据。",
    "",
    "另一个模组目录",
    "- 用户数据目录（Godot 的 user://mods，通常是 %APPDATA%\\\\Godot\\\\app_userdata\\\\Legacy Flame War\\\\mods）",
    "  里也可以放模组，优先级比本目录高",
    "- 想临时换个目录测试：start.exe -- --mods D:\\my_mods",
    "- 想完全不加载模组：start.exe -- --no-mods",
    "",
  ].join("\r\n"),
);

// 7) 说明
writeFileSync(
  join(out, "readme.txt"),
  [
    "Legacy Flame War (Godot host)",
    "",
    "双击 start.exe 直接开始（不需要任何命令行参数）。",
    "资源在 resources\\ 里：root = builtin_data + version.json，prel.zip/data.zip = 数据包，icons = 顶栏图标。",
    "模组：把 *.zip 放进 mods\\ 即可自动加载（可选 mods.json5 控制顺序/禁用）。",
    "诊断参数仍可用：start.exe -- --frames 120，详见 native/docs/commands.md。",
    "",
  ].join("\r\n"),
);

let total = 0;
const walk = (d) => {
  for (const ent of readdirSync(d, { withFileTypes: true })) {
    const p = join(d, ent.name);
    if (ent.isDirectory()) walk(p);
    else total += statSync(p).size;
  }
};
walk(out);
step(`解包 ${(total / 1024 / 1024).toFixed(1)} MB -> ${out}`);

if (ZIP) {
  const zip_path = join(RELEASE, `${name}.zip`);
  step(`压缩 -> ${zip_path}`);
  if (!make_zip(out, zip_path)) {
    fail("压缩失败（tar / Compress-Archive 都不可用）；目录已经打好了，可手动压缩");
  }
  step(`完成 ${zip_path}（${(statSync(zip_path).size / 1024 / 1024).toFixed(1)} MB）`);
} else {
  step("完成（只出目录）；要 zip 就跑 `npm run godot:pack:zip`");
}

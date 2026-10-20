# 差分测试协议（历史文档）

**状态：TS 对照侧已退役。** 本仓库不再带 `subjects/`（TS 与 C++ 两侧的执行器）与 `mutations/`
（变异档），`tools/mutate.mjs`、`tools/check_mutation_anchors.mjs` 一并移除。
留下的是 `cases/`（用例语料）与 `run.mjs`（调度器，没有 subject 时会直接报 "no subjects found"）。

本文保留下来是因为它仍然是**用例 DSL 与 trace 格式**的唯一说明 —— 当初的协议是：
同一份用例喂给两侧，TS 侧导入 `src/LFW` 的**真实实现**、C++ 侧是移植版，逐行 diff 暴露语义偏差。
将来若要恢复对照，按 §0/§5 重新补 `subjects/` 即可（`run.mjs` 用 `subjects/*.ts` 自动发现）。

## 0. 结构

```
tests/differential/
  run.mjs                      调度：自动发现 subject，两侧跑同一 case，逐行比对
  cases/
    <subject>/<case>.txt       一个 subject 一个目录，可放多个 case 文件
```

（`subjects/` 与 `mutations/` 已移除，见上。）

---
## 1. case 文件（DSL）

### 1.1 通用规则

- `#` 到行尾是注释；空行忽略
- token 之间用任意空白分隔
- 缺省参数两边必须一致（见各 subject 的说明）

### 1.2 subject: `mersenne_twister`

```
seed  <number> [d]                      # mt.reset(seed[, true])；同时输出一行 run
int   <count>                           # 取 count 个原始 32 位整数
float <count>                           # 取 count 个 [0,1) 浮点（已量化到 1/1000）
range <min> <max> <count>               # 取 count 个 [min,max) 内的值
pick  <item> <item> ...                 # 用 items 建数组，pick 一次（不改数组）
take  <item> <item> ...                 # 用 items 建数组，take 一次（会移除元素）
state                                   # 输出当前 MT 状态哈希
mark  <token|"literal">                 # mt.mark = …
debug <0|1>                             # mt.debugging = …
case  <valueLiteral> ...                # mt.case(...)；输出参数个数（记录进 mt_cases）
cases                                   # mt_cases.submit()：整段文本 + submit 后的 cases.length
cinfo                                   # mt_cases 的 name / separator
creset                                  # mt_cases.reset
pure                                    # mt.pure() 逐字段（matrix/upper/lower/index/seed/times/
                                        #   mt.length/mt[0]/mt[1]/mt[623]/mark）
load  <field> <value> [value]           # 以 mt.pure() 为底改一个字段后 mt.load(info)；
                                        #   输出参数回显 + state 哈希 + mark
pickv <valueLiteral>                    # mt.pick<T>(a)（Value 版）：返回值 + 操作后的 a
takev <valueLiteral>                    # mt.take<T>(a)：同上（会 splice，打印删除后的数组）
```

### 1.3 subject: `math`

镜像 `src/LFW/utils/math/*`。**参数缺省值必须与 TS 完全一致**，否则会假报差异。

```
clamp            <value> <lo> <hi>
clamp_add        <value> <offset> <lo> <hi>
normalize        <n> [<p>]                  # p 缺省 = 1000
float_equal      <x> <y>
equal            <x> <y>
eqgt             <x> <y>
eqlt             <x> <y>
range            <from> <to> [<gap>]        # gap 缺省 = 1
probability      <times> <p>
normalize_plane  <a> <b> <c> <d>
calc_plane       <x1> <y1> <z1> <x2> <y2> <z2> <x3> <y3> <z3>
line_plane       <a> <b> <c> <d> <x1> <y1> <z1> <x2> <y2> <z2> [<is_direction>] [<is_segment>]
project_to_line  <x> <y> <m> <n>
alias_normalize_plane  <a1> <b1> <c1> <d1> <a2> <b2> <c2> <d2>
alias_calc_plane       <18 个数>
alias_line_plane       <20 个数>
```

**`throw` 的映射**：TS 侧 `range` / `project_to_line` 会 `throw`，
C++ 侧返回 `std::optional` 的 `nullopt`。执行器把两者都渲染成 `null`。

**`alias_*` 是干什么的**：`normalize_plane` / `calc_plane` / `line_plane_intersection`
在 TS 里都返回**模块级共享的可变单例对象**。C++ 侧必须镜像这一点（返回 `const Plane*` /
`const Vec3*` 指向文件级 static），否则"先拿引用、再调用一次、回头读第一次的引用"这种
用法会静默出错。`alias_*` 连续调用两次并**同时输出两次捕获到的引用**：

- 若两侧都是共享单例 → 两次输出都等于**第二次**的结果
- 若 C++ 改成按值返回 → 第一次输出会是**第一次**的结果 → 差异暴露

### 1.4 subject: `utils`

镜像 `src/LFW/utils/`（`math/` 之外的部分）。

```
ease_linearity            <factor> [<from>] [<to>]
ease_linearity_backward   <v> [<from>] [<to>]
ease_in_out_sine          <factor> [<from>] [<to>]
ease_in_out_sine_backward <v> [<from>] [<to>]
ease_in_out_quint         <factor> [<from>] [<to>]
ease_in_out_quint_backward <v> [<from>] [<to>]
cross_bounding            <l0> <r0> <t0> <b0> <n0> <f0> <l1> <r1> <t1> <b1> <n1> <f1>
utf8_encode               <u16hex>...
utf8_decode               <bytehex>...
times_new                 [<min>] [<max>]
times_set_range           <a> <b>
times_set_lifes           [<v>]
times_set_min             <v>
times_set_max             <v>
times_set_value           <v>
times_reset
times_reborn
times_state
times_add                 <d>
times_write_nums
times_read_nums           <v> <min> <max> <lifes> <remains>
times_snapshot
times_read_snapshot       <v> <min> <max> <lifes> <remains>
```

缺省值：`<from>` = 0，`<to>` = 1；`times_new` 的 `<min>` = 0、`<max>` = `Number.MAX_SAFE_INTEGER`；
`times_set_lifes` = `-1`。

**名字映射**：TS 把 `backward` 挂在函数对象上（`ease_linearity.backward`），
C++ 侧是独立函数 `ease_linearity_backward`。TS 里 `times.min` / `times.max` /
`times.value` 是 getter+setter，C++ 里是 `min()` / `set_min()`。

**`utf8_encode` / `utf8_decode` 用十六进制 token 传参**，因为需要能表达
孤立代理项（`d83d`）、非法前导字节（`f8` / `ff`）这类 UTF-16 / UTF-8 的边角。
输出是**十进制**的字节值 / UTF-16 code unit 值。

### 1.5 subject: `collections`

镜像 `base/Graves` + `utils/array/*` + `utils/container_help/*`。

```
graves_new
graves_add            <v>
graves_take
graves_l

filter_gt             <t> <x>...
find_gt               <t> <x>...
find_last_gt          <t> <x>...
fisrt_gt              <t> <x>...
last_gt               <t> <x>...
map_no_void_gt        <t> <x>...
intersection          <x>... | <x>...
ensure                [<x>...] | <x>...
loop_offset           <current> <offset> <x>...
make_arr              <size>
map_arr_mul           <k> <x>...
map_arr_scalar        <k> <x>
loop_arr_idx          <x>...
loop_arr_scalar       <x>

nested_set            <k1> <k2> <v>
nested_get            <k1> <k2>
nested_has            <k1> <k2>
nested_del            <k1> <k2>
nested_clear
nested_multi_add      <k1> <k2> <v>
nested_multi_first    <k1> <k2>
nested_multi_has      <k1> <k2>
nested_multi_collect  <k1> <k2>
nested_multi_del      <k1> <k2>
nested_multi_clear
```

**谓词词汇表**：为了避免在 case 里写函数，谓词统一是“大于阈值”：

| 后缀 | 实际谓词 |
|---|---|
| `filter_gt` / `find_gt` / `find_last_gt` | `v => v > t` |
| `fisrt_gt` / `last_gt` | `v => v > t ? v : undefined` |
| `map_no_void_gt` | `v => v > t ? v * 2 : undefined` |

`|` 是两个数组的分隔符。`ensure` 的左侧为空 = output 是 `undefined`；
**右侧不能为空**（TS 的 `item` 是必填项）。

`make_arr` / `map_arr_*` / `loop_arr_*` 用的是恒等 / 乘常数这样的小函数 ——
测的是**辅助函数的机制**（含 `map_arr` / `loop_arr` 的标量分支），不是那个 fn。

---
### 1.6 subject: `fields`（`fields.ts`）

TS 侧用**仓库里真正的 `src/LFW/fields.ts`**，C++ 侧是 `lfw/fields.cpp`。
因为描述符本身就是普通对象，所以直接复用值字面量；字段表是 TS 的 `Map`，
C++ 侧用 `Object` 顶替，TS 侧用 `new Map(Object.entries(o))` 还原。

```
fw "<type>" <n> <v1>..<vn>      构造描述符（type 取 string/float/int/boolean/object/map/空串）
ff <obj>                         fields(obj)         → 字段表
fm <map>                         fields_map_2_fields_obj
fr <obj> <map>                   reorder_fields（就地重排）后打印 obj
fa <v>                           to_array
fas <v>                          to_array 是否与原数组同一个引用
fv <data> <map>                  validate_fields → 1 行 ok + 每行 `e <err>` / `w <warn>`
```

`fields` subject 额外有“残留 token”自检：任何 op 解析完后若还有 token 未消费，直接报错
（挡的是“个数写少了”这类静默错误）。

输出统一走 `trace_util` 的 `render_value` / `renderValue`：
`u` / `z` / `b0|b1` / `n<number_to_string>:<16位十六进制位模式>` / `s"..."`（`esc` 转义）/
`[...]` / `{"key":...}`（键按 `Object.keys` 序，TS 的 `Map` 也渲染成 `{}`）。
**同一个 `render*` 两边必须产出完全相同的文本**，所以 `Map` 与 `Object` 的差异在这里被抹平。

### 1.7 切词器的坑：token 不能被静默截断

`split_ws` 原来按空白切，**不认引号**。后果：

- `p5 "[1, 2]"` 被切成 `p5` / `"[1,` / `2]"` —— 两侧拿到的是**同一个被截断的**
  参数，所以对拍照样"通过"，只是**根本没测到想测的东西**。
- 改成引号感知后，`json/parse` 的输出行数从 **150 → 157**，说明原来确实有 7 行
  在测残料。（`json5/parse` 也从 304 → 302。）
- 同一处还有 `#` 注释剥离：原来是 `replace(/#.*$/, "")`，引号里的 `#` 也会被砍。现在只在引号外识别。

**教训**：对拍两侧用同一个有缺陷的解析器时，"通过"是假的。
**测试基础设施本身的缺陷不会被对拍发现** —— 只能靠"输出行数/覆盖范围符合预期"来察觉。
所以每次加 subject 时顺手核对一下行数是否合理，是有价值的。

（另外：用例文件里的**真实换行**仍然不可用 —— `readCaseLines` / `std::getline`
先按行切。控制字符一律写成 `\uXXXX` 转义，由 `parse_js_string_literal` 还原。）

`json5` 的 TS 侧参考实现就是 `node_modules/json5` 本身 —— **它是要对齐的真相源，不是待测对象**。
被验的是 `lfw/core/json5.cpp` 与它的逐字节一致。

顺带一条：移植 `lib/parse.js` 的 `string` 状态时我"记得"上游允许字符串里出现裸换行，
差点把 `fail_char` 改成放行。查源码才发现**上游就是报错的**（只有 `\u2028`/`\u2029` 放行并 warn）。
**差分测试的价值之一就是阻止你"修"一个不是 bug 的地方。**

倒数第二条值得记：`Graves::add` 的 `_l[_i--]` 会让 `_i` 回绕成 `SIZE_MAX`，
下一次访问直接越界崩掉。**这是我自己写的 bug**，`/W4` 没报，差分测试第 6 行就抓到了。
顺带说明：差分的失败形式有两种 —— “漂移”（能逐行定位）和“崩溃”（只能知道哪一侧挂了），
两者都是有效的失败信号。

**另外两条的教训**：

- `nested_map.h` 的那条最初**没被抓住** —— 因为当时用例里 `clear()` 之后设的键
  正好是之前设过的键，回收的脏 map 被覆写了。补上“`clear()` 后设一个**不同**的键，
  再查旧键应当不存在”才变可区分。**回收池（`Graves`）的 bug 必须用“回收后访问旧键”来抓。**
- 两个变异一起打上去时，二者都报 “C++ failed” 而分不清是谁 —— **变异测试要一个一个来。**

P.S. TS 侧那个 `Times.lifes` 的无限递归（`return this.lifes`）就是在写 `times` subject 时
被执行器直接撞出来的 —— `RangeError: Maximum call stack size exceeded`。

**最后一条值得记一笔**：最初写的边界用例是错的（算出来 `t = 0.5`，离边界很远），
变异**没被抓住**。原因是 `t > 1+eps` 与 `t > 1` 只在 `1 < t <= 1+eps` 时才有区别，
而在 1 附近 double 的间距恰好等于 eps —— **只有 `t = 1.0000000000000002` 一个值可分**。
负侧宽松得多（0 附近是次正规数，`[-eps, 0)` 里有很多 double）。
所以边界用例是**搜出来的，不是猜出来的**。

## 2. 输出 trace

每行一个 token 序列，token 之间**单个空格**，行末**无空格**。
浮点一律输出 **IEEE-754 位模式的 16 位小写十六进制**（不做十进制格式化 ——
格式化本身就会引入差异）。

`null` 表示"无结果"（TS 的 `null` / `undefined` 或 `throw`，C++ 的 `nullopt` / `nullptr`）。

| subject | op | 输出 |
|---|---|---|
| mersenne_twister | `run` | `run <bits16>[ debug=1]` |
| | `state` | `state <hex16>` |
| | `int` | `int <decimal>` |
| | `float` | `float <bits16>` |
| | `range` | `range <bits16min> <bits16max> <bits16result>` |
| | `pick` / `take` | `<op> <bits16\|-> <remaining>` |
| | `mark` / `debug` | `mark <esc>` / `debug <0\|1>` |
| | `case` | `case <参数个数>` |
| | `cases` | `cases <esc(整段文本)> <submit 后的 cases.length>` |
| | `cinfo` | `cinfo <esc(name)> <esc(separator)>` |
| | `pure` | `pure <hex8×3> <index> <bits16seed> <times> <mt.length> <mt[0]/[1]/[623] hex8> <esc(mark)>` |
| | `load` | `load <参数回显…> <hex16state> <esc(mark)>` |
| | `pickv` / `takev` | `<op> <renderValue(返回值)> <renderValue(操作后的参数)>` |
| math | `clamp` `clamp_add` `normalize` | `<op> <bits16>` |
| | `float_equal` `equal` `eqgt` `eqlt` | `<op> <true\|false>` |
| | `range` | `range <count> <bits16>...` 或 `range null` |
| | `probability` | `probability <bits16>` |
| | `normalize_plane` | `normalize_plane <bits16> <bits16> <bits16> <bits16>` |
| | `calc_plane` | `calc_plane <bits16>×4` 或 `calc_plane null` |
| | `line_plane` | `line_plane <bits16>×3` 或 `line_plane null` |
| | `project_to_line` | `project_to_line <bits16> <bits16>` 或 `project_to_line null` |
| | `alias_*` | `<op> <两次捕获到的值依次输出>` |
| utils | `ease_linearity` / `_backward` | `<op> <bits16>` |
| | `ease_in_out_sine` / `_backward` | `<op> <bits16>`（量化） |
| | `ease_in_out_quint` / `_backward` | `<op> <bits16>`（量化） |
| | `cross_bounding` | `cross_bounding <bits16>×6` |
| | `utf8_encode` | `utf8_encode <byteCount> <byteDecimal>...` |
| | `utf8_decode` | `utf8_decode <u16Count> <codeUnitDecimal>...` |
| | `times_*` | `<op> <bits16>×5`（`times_add` 前面多一个 `<true\|false>`） |
| collections | `graves_add` | `graves_add <l.length>` |
| | `graves_take` / `graves_l` | `<op> <bits16\|->...`（`graves_l` 前面多一个长度） |
| | `filter_gt` `map_no_void_gt` `intersection` `ensure` `make_arr` `map_arr_*` `nested_multi_collect` | `<op> <count> <bits16>...` |
| | `find_gt` `find_last_gt` `fisrt_gt` `last_gt` `loop_offset` `nested_get` `nested_multi_first` | `<op> <bits16\|->` |
| | `nested_has` `nested_del` `nested_multi_has` `nested_multi_del` | `<op> <true\|false>` |
| | `loop_arr_idx` `loop_arr_scalar` | `<op> <count> <idxDecimal>...` |

### 2.1 判据：量化后再比（**不是**原始位模式）

**跨语言不要求原始浮点位级相同。** 判据是：

> 经过 `round_float(v)` 之后一致 —— 也就是 `Math.round(v * 1000) / 1000`。

两侧各自调用**自己那份真实实现**：

| | 实现 |
|---|---|
| TS | `src/LFW/utils/math/round_float.ts` |
| C++ | `lfw::round_float()`（`lfw/utils/math/round_float.h`） |

理由：`round_float` 是**游戏自己的可观测精度**。两个值量化到同一个格点，
对游戏而言就是同一个值；量化后不同，才是真的不同。
另外 `Math.round` 的半值规则（平局朝 +∞，且 `|x| < 0.5` 返回带符号 0）
本来就与 `std::round` 不同，所以这一步本身也在校验 `js_round`。

**量化只用在需要它的地方**，不是无差别套用：

| 类别 | 判据 | 为什么 |
|---|---|---|
| `mersenne_twister` 的 `float` / `range` 结果 / `pick` / `take` | 量化 | 浮点通道 |
| `mersenne_twister` 的 `int` / `state` / 剩余长度 | **逐位精确** | 整数与内部状态，是最后一道防线 |
| `math` 的 `probability` | 量化 | 内部走 `pow`（libm），跨实现不可保证 |
| `math` 其余全部 | **逐位精确** | 纯 IEEE 算术（`+ - * /`、比较、`std::floor`），精确是可达且是应有的标准 |
| `utils` 的 `ease_in_out_sine` / `_backward` | 量化 | `cos` / `acos` |
| `utils` 的 `ease_in_out_quint` / `_backward` | 量化 | `pow` |
| `utils` 其余全部（含 `ease_linearity` / `cross_bounding` / `utf8_*` / `times_*`） | **逐位精确** | 纯 IEEE 算术或整数 |
| `collections` 全部 | **逐位精确** | 下标 / 计数 / 整数，没有浮点运算 |

`-0` 与 `+0` 位模式不同（`8000000000000000` vs `0000000000000000`），
所以位模式输出顺带锁住了符号零的行为。
`math/scalar` 里刻意放了一批 `-0` 用例。

---
## 3. 状态哈希的字段顺序（**这是协议**）

FNV-1a 64（offset basis `0xcbf29ce484222325`，prime `0x00000100000001b3`）。
每个字段按**小端字节**喂入。

```
u32 matrix
u32 upper_mask
u32 lower_mask
u32 index
f64 seed          ← 注意是 f64，不是 u32（TS 存的是原始 number）
u64 times
u32 mt[0]
...
u32 mt[623]
```

改这个顺序 = **两侧同时改**，否则差分测试会报"状态不同"。
实现：C++ 见 `lfw/core/state_hash.{h,cpp}` + `mersenne_twister.cpp`
的 `state_hash()`；TS 见 `subjects/mersenne_twister.ts`。

---
## 4. 怎么跑

```
node tools/native.mjs test                 # 全部 subject、全部 case
node tools/native.mjs test mersenne_twister
node tools/native.mjs test math plane      # subject + case 名
```

产物落在 `build/gen/`：

```
trace.<subject>.<case>.cpp.txt    C++ 侧输出
trace.<subject>.<case>.ts.txt     TS 侧输出
```

diff 失败时脚本会打印**第一处不同的行号**与两侧内容 —— 那就是漂移点。

---
## 5. 加一个新的 subject

1. 写 `subjects/<name>.cpp`（`#include "trace_util.h"`）+ `subjects/<name>.ts`
   （`import ... from "./trace_util"`）
2. 建 `cases/<name>/*.txt`
3. `node tools/native.mjs build` —— CMake 的 glob 会自动生成 `lfw_trace_<name>.exe`

不用改 `run.mjs`，不用改 `CMakeLists.txt`。

---

## 6. 变异测试（已退役）

原先每一份 subject 都配一份 `mutations/*.mjs`：故意改坏一处，确认差分能抓到。TS 对照侧与
`subjects/` 已从本仓库移除（本仓库不再做 TS 差分），变异档随之退役；`cases/` 与 `run.mjs`
保留为**协议语料**（DSL 与 trace 格式的参考）。

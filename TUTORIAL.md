# ItemAndLabel 完整教程

自制跨平台「标签-值」特殊数据库工具。纯 C++17 单文件，Windows / Android / Linux / macOS / iOS 可移植。MIT 许可。

## 概念

- **项（Item）**：一行记录，含唯一 10 位字母数字 id + 一个主标签 + 若干标签。
- **标签（Label）**：`名字` + `类型` + `值`。名字仅支持数字、英文、`_`。值分 `char` / `int` / `float` / `bool` 四类。
- **通配符**：`*`（任意序列）、`?`（单个字符），用于搜索与过滤。

## 基础用法

### 1. 新建项

```
db.exe "x.ildb" --new_item --label_add "-name" -v --char "xiaoming" --main_label "-name"
```

必须指定 `--main_label`。批量创建加 `--cnt N`：

```
db.exe "x.ildb" --new_item --label_add "-name" -v --char "user*" --main_label "-name" --cnt 50
```

每项独立生成不重复的 10 位字母数字 id。

### 2. 批量值通配符

创建或批量加标签时，值可写成模板，按项序号自动展开：

| 语法 | 含义 | 示例 |
|---|---|---|
| `*` | 递增序号（从 1、步长 1） | char `"*_a"` ×10 → `1_a 2_a ... 10_a` |
| `**<step>` | 递增、步长 step（可小数） | float `"**0.5"` ×4 → `1 1.5 2 2.5` |
| `**<step>@<s>` | 递增、从 s 开始步长 step | int `"**2@10"` ×4 → `10 12 14 16` |
| `*<w>` | 序号补零到 w 位 | char `"*04"` ×5 → `0001 0002 ... 0005` |
| `**-1` | 负步长递减 | int `"**-1"` ×4 → `1 0 -1 -2` |
| `@(a,b,c)` | 枚举遍历，轮流取值 | char `"@(红,绿,蓝)"` ×6 → `红 绿 蓝 红 绿 蓝` |
| 组合 | 占位可拼接、可多个 | char `"@(A,B)-*02"` ×4 → `A-01 B-02 A-03 B-04` |

- char 值**禁止** `% & # \ /`；`*` 与 `@(...)` 为批量通配语法（单独 `@` 报错）。
- 通配符仅在批量合法：单量创建 / `--set` 单项含通配符会报错；bool 不支持通配。
- 补零对 int 类型显示层会规范化为数字（0001→1），需保留前导零请用 char。

### 3. 给已有项加标签 / 删标签

```
db.exe "x.ildb" --label_add "-age" -v --int "0" "*"            # 全部项加 age=0
db.exe "x.ildb" --label_add "-age" -v --int "*" "*"            # 全部项加 age=1,2,3...
db.exe "x.ildb" --label_add "-tag" -v --char "a" "-name" "-age"  # 给同时含 name 和 age 的项加 tag
db.exe "x.ildb" --label_del "-age" "*"                         # 全部项删除 age
```

过滤标签可多个（同时满足），`*` 表示全部。

### 4. 搜索

```
db.exe "x.ildb" --search "age>=18" --and "name=user*" --or "active=true"
```

从左到右按 `--and` / `--or` 结合。条件支持：标签名（存在性）、`标签=值`、`标签>=值`、`!=`、`>`、`<`、`<=`，值支持 `* ?` 通配。

按任意标签的值搜索：

```
db.exe "x.ildb" --search -v "xiaoming"
```

### 5. 查看与取值

```
db.exe "x.ildb" --list
db.exe "x.ildb" --count
db.exe "x.ildb" --show <id>
db.exe "x.ildb" -g "name"   # 输出所有 name 标签的值, 每行一个(纯文本)
```

### 6. 删除项 / 修改

```
db.exe "x.ildb" --delete "*"            # 删除全部(需确认)
db.exe "x.ildb" --delete "-name"        # 删除所有含 name 标签的项
db.exe "x.ildb" --delete "a" "b"       # 删除同时含 a 和 b 的项
```

> 过滤里 `*` 只作为恒真项，不会短路其余条件——例如 `--delete a *` 仍只删除含 `a` 的项，不会误删全部。

修改单项：

```
db.exe "x.ildb" --set <id> "-age" -v --int "30"
```

## 企业级存储

- **并发安全**：伴随文件 `<库>.lock` 提供进程级文件锁——写命令独占、只读命令共享（Windows `LockFileEx`，POSIX `fcntl` 记录锁，阻塞等待），多进程同时读、以及读写之间互斥，避免"读-改-写"互相踩踏。锁粒度是整库（整个命令），非记录级。
- **v2 紧凑二进制（`ILDB` 魔数）**：标签名字典去重 + 数值二进制化，实测 19.4KB → 5.5KB（-71%）。
- **原子写入 + 写前快照**：先写临时文件、`fsync` 落盘后整体原子替换（Windows `MoveFileEx`，POSIX `rename`）；每次写入前自动把当前库备份到 `<库>.bak`。进程中断 / 断电不损坏。
- **崩溃回滚**：误操作（如 `--delete *`）或上次写入异常时，`--restore` 从 `<库>.bak` 快照回滚到上一次写入前的一致状态。
- **完整性**：`--check` 严格校验文件头、字典、每条记录及尾部残余，截断/损坏文件正确报错。
- **向后兼容**：自动读 v1 文本旧库，`--vacuum` 一键升级。

```
db.exe "x.ildb" --stats              # 大小 / 项数 / 标签去重
db.exe "x.ildb" --vacuum             # 压缩重写为紧凑格式
db.exe "x.ildb" --check              # 完整性校验
db.exe "x.ildb" --backup "backup.ildb"
db.exe "x.ildb" --restore            # 从快照 <库>.bak 回滚
```

## JSON 导入导出（数据交换）

`--export` 把全部项输出为可读 JSON，便于查看 / 对接外部系统 / 脚本处理；`--import` 读回并按 id 合并（已存在 id 的项被更新，新 id 追加），`--replace` 先清空库再导入。中文与转义字符、四种类型值均完整保留。

```
db.exe "x.ildb" --export "x.json"
db.exe "x.ildb" --import "x.json"            # 合并
db.exe "x.ildb" --import "x.json" --replace  # 清空后导入
```

导出结构示例：
```json
{"db":"ItemAndLabel","version":1,"items":[
  {"id":"9aBcDeFgHi","main":"name","labels":{"name":{"type":"char","value":"xiaoming"},"age":{"type":"int","value":"30"}}}
]}
```

## 多平台

- Windows：`db.exe`（x64）、`db32.exe`（x86）、`db-arm64.exe`（ARM64）
- Android：`db-android-arm64` / `arm32` / `x86_64` / `x86` / `riscv64`（NDK 交叉编译，**静态链接 libc++、已 strip**，ELF 可直接 push 执行）
- APK：安卓图形界面版（内置 native，输入框敲命令 → 运行 → 显示输出）
- Linux / macOS：源码 `g++` / `clang++` 一条命令编译；iOS 需 macOS + Xcode，仅越狱设备可运行

详见 `BUILD.md`。

## GUI 命令组合器

`db_composer.exe`（Qt Widgets）：左侧命令块拖到右侧组合、实时校验红框报错、一键运行；运行前会检查 db.exe 路径与库路径。

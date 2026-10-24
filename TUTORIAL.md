# ItemAndLabel 自制特殊数据库 —— 完整教程

一个用 C++17 从零实现的跨平台特殊数据库工具（标签-值 存储模型）。每个"项"(item)是一个 10 位字母数字的唯一 id，挂若干"标签"(label)，每个标签有一个值，值分 char / int / float / bool 四种类型。数据存为 `.mdb` 文件。

## 快速开始（Windows）

```
cd E:\codes\my_special_db

db.exe "demo.mdb" --new_item --label_add "-name" -v --char "xiaoming" --main_label "-name"
db.exe "demo.mdb" --list
db.exe "demo.mdb" --stats
```

> 命令行里标签常写成 `-name`（仅风格标识，真实标签名不含 `-`）。库文件不存在时自动创建。

## 命令行详解

统一格式：`db.exe <数据库.mdb> <操作...>`

### 创建项 `--new_item`

```
db.exe "x.mdb" --new_item --label_add "-name" -v --char "xiaoming" --main_label "-name"
```

- 必须指定 `--main_label`（主标签），且主标签名必须同时出现在某个 `--label_add` 中。
- 一条命令可 `--label_add` 多个标签。
- 每项自动生成独立不重复的 10 位字母数字 id。

批量创建 `--cnt N`：

```
db.exe "x.mdb" --new_item --label_add "-name" -v --char "user" --main_label "-name" --cnt 50
```

### 加标签 `--label_add`

给满足过滤条件的项加标签。过滤参数：标签名（多条件 AND）/ 值条件 / `*`（全部）。

```
db.exe "x.mdb" --label_add "-age" -v --int "0" "*"
db.exe "x.mdb" --label_add "-tag" -v --char "member" "-name" "-age"
```

### 删标签 `--label_del`

```
db.exe "x.mdb" --label_del "-age" "*"
```

### 搜索 `--search`

条件：`标签名` / `标签=值` / `标签>=值` / `标签<=值` / `标签!=值` / `标签>值` / `标签<值`，char 值支持 `*` `?` 通配。

```
db.exe "x.mdb" --search "name=xiaoming"
db.exe "x.mdb" --search "name=x*" --and "age>=18" --or "active=true"
db.exe "x.mdb" --search -v "xiaoming"   # 按任意标签的值搜索
```

### 取标签值 `-g`

```
db.exe "x.mdb" -g "name"   # 输出该标签所有值，每行一个
```

### 查看 / 修改 / 删除

```
db.exe "x.mdb" --list
db.exe "x.mdb" --count
db.exe "x.mdb" --show <id>
db.exe "x.mdb" --set <id> "-name" -v --char "新值"
db.exe "x.mdb" --delete "old"      # 删除含 old 标签的项
```

> `--set` 是单项操作，值不支持批量通配符。

### 管理命令（企业级）

```
db.exe "x.mdb" --stats
db.exe "x.mdb" --vacuum            # 压缩重写为紧凑格式
db.exe "x.mdb" --check             # 完整性校验
db.exe "x.mdb" --backup "y.mdb"    # 原子复制备份
db.exe "x.mdb" --help
```

## 标签规则与值类型

- 标签名：只支持数字、英文、`_`。
- 类型：`--char`（禁止 `% & # \ /` 符号）、`--int`（long long）、`--float`（double）、`--bool`（true/false）。

## 批量值通配符

批量场景（`--cnt > 1` 创建，或 `--label_add` 匹配多项）的值模板可含通配符占位，按序号依次展开：

| 语法 | 含义 | 示例 |
|---|---|---|
| `*` | 递增序号（从 1、步长 1） | int `"*"` ×10 → `1 2 3 ... 10` |
| `*_a` | `*` 位置被序号替换 | char `"*_a"` ×10 → `1_a ... 10_a` |
| `**<step>` | 递增、步长可小数 | float `"**0.5"` ×4 → `1 1.5 2 2.5` |
| `**<step>@<s>` | 递增、从 s 开始 | int `"**2@10"` ×4 → `10 12 14 16` |
| `*<w>` | 序号补零到 w 位 | char `"*04"` ×5 → `0001 ... 0005` |
| `**-1` | 负步长递减 | int ×4 → `1 0 -1 -2` |
| `@(a,b,c)` | 枚举遍历，轮流取值 | char `"@(红,绿,蓝)"` ×6 → 轮流 |
| 组合/多占位 | 可拼接、可多个 | `"@(A,B)-*02"` ×4 → `A-01 B-02 ...` |

规则：通配符仅批量时合法（单量/`--set` 报错）；bool 不支持通配；补零对 int 会被规范化为数字，需保留前导零用 char。

```
db.exe "x.mdb" --new_item --label_add "-name" -v --char "*_a" --main_label "-name" --cnt 10
db.exe "x.mdb" --new_item --label_add "-score" -v --float "**0.5" --main_label "-score" --cnt 4
db.exe "x.mdb" --label_add "-age" -v --int "*" "*"
```

## 企业级存储

- v2 紧凑二进制（`ILDB` 魔数）：标签名字典去重 + 数值二进制化，实测 19.4KB → 5.5KB（-71%）。
- 原子写入：先写临时文件再整体替换，进程中断不损坏。
- 向后兼容：自动读 v1 文本旧库，`--vacuum` 一键升级。

## GUI 命令组合器

`db_composer_app\db_composer.exe`（Qt）：左侧命令块面板拖到右侧组合，实时校验红框报错，▶ 一键运行；需与 `db.exe` 同目录。

## 跨平台产物

| 文件 | 平台 |
|---|---|
| db.exe | Windows x64 |
| db32.exe | Windows x86 |
| db-arm64.exe | Windows on ARM64 |
| db-android-arm64 / arm32 / x86_64 / x86 / riscv64 | Android 5 ABI ELF |
| ItemAndLabel.apk | Android 图形版 |

Linux / iOS 需在目标环境编译（见 BUILD.md）。

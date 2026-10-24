# ItemAndLabel 特殊数据库

自制跨平台「标签-值」特殊数据库工具，纯 C++17 单文件，仅依赖标准库。MIT 许可。

## 核心特性

- 每项 = 唯一 10 位字母数字 id + 若干标签；标签值分 `char` / `int` / `float` / `bool` 四类
- 批量创建 / 批量加标签支持值通配符：`*` 递增、`**<step>` 步长、`**<step>@<s>` 起始值、`*04` 补零、`**-1` 递减、`@(a,b,c)` 枚举遍历，可组合多个占位
- **企业级存储**：v2 紧凑二进制压缩（实测 -71%）、fsync + 原子写入（Windows `MoveFileEx` / POSIX `rename`）、旧版 v1 兼容、严格 `--check` / `--vacuum` / `--backup`
- 多平台：Windows x64 / x86 / ARM64，Android 5 个 ABI + APK 图形版，源码可移植 Linux / macOS / iOS
- **GUI 命令组合器** `db_composer`（Qt Widgets）：拖拽命令块、实时校验报错、一键运行

## 快速开始

```
db.exe "x.mdb" --new_item --label_add "-name" -v --char "user*" --main_label "-name" --cnt 10
db.exe "x.mdb" --label_add "-age" -v --int "*" "*"
db.exe "x.mdb" --search "age>=18" --and "name=user*"
db.exe "x.mdb" -g "name"
```

完整教程见 `TUTORIAL.md`，多平台编译 / 部署见 `BUILD.md`，许可见 `LICENSE`。
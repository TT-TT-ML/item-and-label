# ItemAndLabel 数据库 — 多平台编译说明

纯 C++17 单文件程序，仅依赖标准库（Windows 上额外调用 `SetConsoleOutputCP` 修复中文乱码，非 Windows 平台自动忽略该段代码）。

- 源码：`item_and_label.cpp`（UTF-8 编码，带 BOM）
- 已编译产物：
  - `db.exe`                —— Windows x64
  - `db32.exe`              —— Windows x86 / 32 位
  - `db-arm64.exe`          —— Windows on ARM64
  - `db-android-arm64`      —— Android 64 位 ARM（AArch64，NDK，静态链接 libc++ 并 strip）
  - `db-android-arm32`      —— Android 32 位 ARM（NDK）
  - `db-android-x86_64`     —— Android x86_64（NDK）
  - `db-android-x86`        —— Android x86 / 32 位（NDK）
  - `db-android-riscv64`    —— Android RISC-V 64 位（NDK）
  - `ItemAndLabel.apk`      —— Android 图形界面版（手搓 APK）

---

## Windows

### x64（MSVC，推荐）
打开 “x64 Native Tools Command Prompt for VS”，或执行：
```
vcvars64.bat && cl /nologo /O2 /std:c++17 /EHsc /utf-8 item_and_label.cpp /Fe:db.exe
```
（MSVC 必须带 `/utf-8`，否则无 BOM 环境下的中文字符串会按 GBK 误解；本源码已带 BOM，双保险。）

### x86 / 32 位（MSVC）
```
vcvarsall.bat x86 && cl /nologo /O2 /std:c++17 /EHsc /utf-8 item_and_label.cpp /Fe:db32.exe
```

### x64（MinGW-w64）
```
g++ -O2 -std=c++17 item_and_label.cpp -o db.exe
```
> 注意：MinGW-w64 若安装在带空格路径（如 `C:\Program Files\...`），链接器 `ld` 会把路径拆开导致链接失败。遇到此问题请改用 MSVC，或把工具链整体复制到无空格路径（如 `C:\MinGW`）再编译。

### ARM64（Windows on ARM，llvm-mingw）
本机用 Qt 自带的 llvm-mingw 交叉编译（`F:\Qt\Tools\llvm-mingw2217_64\bin`）：
```
aarch64-w64-mingw32-g++.exe -O2 -std=c++17 item_and_label.cpp -o db-arm64.exe
```

---

## Android

使用 Android NDK 的 clang 交叉编译器。**静态链接 libc++ 并 strip（`-static-libstdc++ -s`），产物不依赖 libc++_shared.so**。本机 NDK 位于 `F:\SDKs\ndk\<版本>\toolchains\llvm\prebuilt\windows-x86_64\bin`：

```
aarch64-linux-android21-clang++.cmd -O2 -std=c++17 -static-libstdc++ -s item_and_label.cpp -o db-android-arm64
armv7a-linux-androideabi21-clang++.cmd -O2 -std=c++17 -static-libstdc++ -s item_and_label.cpp -o db-android-arm32
x86_64-linux-android21-clang++.cmd   -O2 -std=c++17 -static-libstdc++ -s item_and_label.cpp -o db-android-x86_64
i686-linux-android21-clang++.cmd     -O2 -std=c++17 -static-libstdc++ -s item_and_label.cpp -o db-android-x86
riscv64-linux-android35-clang++.cmd  -O2 -std=c++17 -static-libstdc++ -s item_and_label.cpp -o db-android-riscv64
```

部署到设备（需 adb，设备需开启 USB 调试）：
```
adb push db-android-arm64 /data/local/tmp/db
adb shell chmod 755 /data/local/tmp/db
adb shell /data/local/tmp/db /sdcard/test.ildb --help
```

### APK（安卓图形界面版）

手搓 APK：用 Android SDK + JDK，把 native 程序内置进一个极简安卓应用（输入框敲命令 → 按钮执行 → 下方显示输出）。源码在 `apk/`，产物 `ItemAndLabel.apk`。

构建命令（顺序执行，工程位于 `apk/`）：
```
javac  -source 8 -target 8 -bootclasspath F:\SDKs\platforms\android-36\android.jar -d build\classes src\com\itemlabel\db\MainActivity.java
d8     --lib F:\SDKs\platforms\android-36\android.jar --release --min-api 21 --output build\dex build\classes\com\itemlabel\db\*.class
aapt2  link -o build\unsigned.apk -I F:\SDKs\platforms\android-36\android.jar --manifest AndroidManifest.xml -A assets --min-sdk-version 21 --target-sdk-version 36
jar    --update --file build\unsigned.apk -C build\dex classes.dex
zipalign -f 4 build\unsigned.apk build\aligned.apk
apksigner sign --ks %USERPROFILE%\.android\debug.keystore --ks-pass pass:android --ks-key-alias androiddebugkey --key-pass pass:android --out ItemAndLabel.apk build\aligned.apk
```

说明：`apk/assets/db/{arm64-v8a,armeabi-v7a,x86_64}/db` 是三个 ABI 的 native 二进制（已静态链接，无需随 APK 携带 libc++_shared.so），运行时按设备 ABI 自动解压到应用私有目录并 `chmod 755`。数据库默认 `/sdcard/Android/data/com.itemlabel.db/files/data.ildb`（界面可改）。安装：
```
adb install ItemAndLabel.apk
```

---

## Linux（x64 / ARM64 / ARM）

> 本机（Windows）没有 Linux 工具链（无 WSL、无 Linux 交叉编译器），**无法直接产出 Linux 二进制**。请把源码拷到目标 Linux 机器上编译（装 `g++` 即可，无需额外依赖）：

```
g++ -O2 -std=c++17 -o db item_and_label.cpp
```

适用于 x86_64 服务器、Raspberry Pi、以及 S905X3 等 ARM 开发板（Armbian 等系统同样执行以上命令）。Windows 侧若装好 Linux 交叉工具链后可交叉编译。

---

## macOS

```
clang++ -O2 -std=c++17 -o db item_and_label.cpp
```

---

## iOS

> iOS 需要 Apple 官方工具链（Xcode + iPhoneOS SDK），**只能在 macOS 上编译**，Windows 无法产出 iOS 二进制。在装有 Xcode 的 Mac 上交叉编译：
```
xcrun --sdk iphoneos clang++ -O2 -std=c++17 -arch arm64 item_and_label.cpp -o db-ios-arm64
```
产出的是 iOS 原生命令行 ELF，仅可在越狱（jailbreak）设备上运行；未越狱的 iOS 不允许执行任意命令行程序，建议改用 APK/安卓版。

---

## 本机（Windows）无法产出的平台

以下平台需要专用工具链，本机未安装，无法直接产出二进制，但源码是纯 C++17 + 标准库，可移植到对应环境编译（`g++`/`clang++` 一条命令）：
- **iOS / iPadOS**：需 macOS + Xcode（见上）。
- **Linux x64 / ARM64 / ARM**：需 Linux 交叉编译器，本机无（见 Linux 一节，在目标机编译）。
- **FreeBSD / OpenBSD / Haiku 等 Unix 系**：在对应系统用 `clang++` 编译即可。
- **DOS（16 位）**：需要 16 位编译器（如 Turbo C++ / DJGPP），本机无；且 64 位 API/代码不适用。
- **裸机嵌入式（无 OS，如 Cortex-M）**：需要交叉编译器 + 无标准库运行环境的移植，不适用本 CLI 设计。

---

## 用法速览

```
db <数据库.ildb> --new_item --label_add "-name" -v --char "xiaoming" --main_label "-name" [--cnt N]
db <数据库.ildb> --label_add "-age" -v --int "0" "*"                    # 给满足过滤的项加标签,*=全部
db <数据库.ildb> --label_del "-age" "*"                                 # 删除标签
db <数据库.ildb> --search "-name" --and "level>=1" --or "active=true"   # 搜索
db <数据库.ildb> --search -v "xiaoming"                                 # 按任意标签的值搜索
db <数据库.ildb> -g "-name"                                             # 输出该标签所有值,换行隔开
db <数据库.ildb> --list | --count | --show <id> | --delete ... | --set <id> <标签> -v --类型 "值"
db <数据库.ildb> --stats                                            # 统计(大小/项数/标签去重)
db <数据库.ildb> --vacuum                                          # 压缩重写为紧凑格式(回收空间)
db <数据库.ildb> --check                                           # 完整性校验
db <数据库.ildb> --backup <目标.ildb>                                 # 原子复制备份
```

标签名仅支持数字、英文、`_`；`*`/`?` 为通配符；类型：`--char` / `--int` / `--float` / `--bool`。

### 批量值通配符
批量场景（`--cnt > 1` 创建，或 `--label_add` 匹配到多项）的值模板可含多个通配符占位，按项序号依次展开：

| 语法 | 含义 | 示例 |
|---|---|---|
| `*` | 递增序号（从 1、步长 1） | int `"*"` ×10 → `1 2 3 ... 10` |
| `**<step>` | 递增、步长 step（可小数） | float `"**0.5"` ×4 → `1 1.5 2 2.5` |
| `**<step>@<s>` | 递增、从 s 开始步长 step | int `"**2@10"` ×4 → `10 12 14 16` |
| `*<w>` | 序号补零到 w 位 | char `"*04"` ×5 → `0001 0002 ... 0005` |
| `**-1` | 负步长递减 | int `"**-1"` ×4 → `1 0 -1 -2` |
| `@(a,b,c)` | 枚举遍历，轮流取值 | char `"@(红,绿,蓝)"` ×6 → `红 绿 蓝 红 绿 蓝` |
| 组合 | 占位可拼接、可多个 | char `"@(A,B)-*02"` ×4 → `A-01 B-02 A-03 B-04` |

- char 类型值**禁止** `% & # \ /`；`*` 与 `@(...)` 为批量通配语法（`@` 必须成对为 `@(...)`，单独 `@` 报错）
- 通配符仅在批量时合法：单量创建（cnt=1）、`--set` 单项操作时含通配符会报错；bool 类型不支持通配符
- 补零对 int 类型会被显示层规范化为数字（0001→1），需保留前导零请用 char 类型
- 示例：
  ```
  db x.ildb --new_item --label_add "-name" -v --char "*_a"      --main_label "-name" --cnt 10
  db x.ildb --new_item --label_add "-score" -v --float "**0.5"  --main_label "-score" --cnt 4
  db x.ildb --new_item --label_add "-color" -v --char "@(red,green,blue)" --main_label "-color" --cnt 6
  db x.ildb --label_add "-age" -v --int "*" "*"
  ```

### 企业级存储

> 定位：**单用户工具**。无文件锁，避免多个进程同时写同一个库（读-改-写非并发安全），否则可能互相覆盖。

- **v2 紧凑二进制**：标签名全局字典去重 + 数值二进制化（int 用 varint/zigzag、float 用 8 字节、bool 1 字节），大幅压缩文件。实测 200 项重复标签库 19.4KB → 5.5KB（-71%）。
- **原子写入**：所有写操作先写临时文件、`fsync` 落盘后，再整体原子替换——Windows 用 `MoveFileEx(REPLACE_EXISTING)`，POSIX 用 `rename`（对已存在目标本就是原子的覆盖，无需先删除）。进程中断 / 断电不会损坏 `.ildb`。
- **向后兼容**：自动识别并读取旧的 v1 文本格式库，`--vacuum` 一次性升级为 v2 紧凑格式。
- **完整性**：`--check` 做严格解析校验——最小文件头、标签字典条数与内容、每条记录（类型字节、int/float 定长、char 长度）全部完整，且无尾部残余字节；截断、头部缺失、字典损坏、尾随垃圾均正确报「损坏」而非误报 OK。

---

## GUI 命令组合器 (db_composer.exe)

一个可视化命令组合器（Qt Widgets），把命令块**拖拽组合**成一条条 db 操作，实时自动校验报错，一键运行：

- **左侧面板**列出全部命令块（新建项 / 加标签 / 删标签 / 搜索 / 按值搜索 / 取标签值 / 设置值 / 删除项 / 备份 / 计数 / 列出 / 统计 / 压缩 / 校验），**拖到右侧主区**即成为一个块。
- 每个块带参数输入区；**实时校验**：标签名合法性（数字/英文/`_`）、主标签必填、int/float/bool 值类型匹配、数量≥1 等，出错该块**红框 + 红字**提示。
- 右侧主区内的块可**拖动排序**（决定执行顺序），每个块可 ✕ 删除。
- 顶部配置 **db.exe 路径**与**库文件路径**（默认与 db_composer.exe 同目录的 `db.exe` / `data.ildb`）。
- **▶ 运行全部**：按块顺序依次调用 db.exe 执行，输出显示在下方结果区；删除项含 `*` 时弹窗确认。
- 运行要求与 db_composer.exe **同目录**存在 `db.exe`（部署目录已含）。

构建（Qt 6.11.2 mingw）：
```
cd dbcomposer
qmake dbcomposer.pro && mingw32-make -j4
windeployqt --dir <输出目录> release/db_composer.exe
```
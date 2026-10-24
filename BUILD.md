# ItemAndLabel 数据库 — 多平台编译说明

纯 C++17 单文件程序，仅依赖标准库（Windows 上额外调用 `SetConsoleOutputCP` 修复中文乱码）。

- 源码：`item_and_label.cpp`（UTF-8 带 BOM）
- 产物：Windows x64 / x86 / ARM64，Android 5 ABI，APK

## Windows

### x64（MSVC，推荐）
```
vcvars64.bat && cl /nologo /O2 /std:c++17 /EHsc /utf-8 item_and_label.cpp /Fe:db.exe
```
（MSVC 必须带 `/utf-8`。）

### x86（MSVC）
```
vcvarsall.bat x86 && cl /nologo /O2 /std:c++17 /EHsc /utf-8 item_and_label.cpp /Fe:db32.exe
```

### ARM64（llvm-mingw 交叉编译）
```
aarch64-w64-mingw32-g++.exe -O2 -std=c++17 item_and_label.cpp -o db-arm64.exe
```

> MinGW-w64 装在带空格路径时 `ld` 会拆路径导致链接失败，改用 MSVC 或复制到无空格路径。

## Android（NDK）

```
aarch64-linux-android21-clang++.cmd  -O2 -std=c++17 item_and_label.cpp -o db-android-arm64
armv7a-linux-androideabi21-clang++.cmd -O2 -std=c++17 item_and_label.cpp -o db-android-arm32
x86_64-linux-android21-clang++.cmd   -O2 -std=c++17 item_and_label.cpp -o db-android-x86_64
i686-linux-android21-clang++.cmd     -O2 -std=c++17 item_and_label.cpp -o db-android-x86
riscv64-linux-android35-clang++.cmd  -O2 -std=c++17 item_and_label.cpp -o db-android-riscv64
```

部署：`adb push db-android-arm64 /data/local/tmp/db && adb shell chmod 755 /data/local/tmp/db`

### APK（安卓图形版）

手搓 APK：javac → d8 → aapt2 link → jar → zipalign → apksigner。`apk/assets/db/{arm64-v8a,armeabi-v7a,x86_64}/db` 为三个 ABI 的 native，运行时按设备 ABI 解压执行。

```
adb install ItemAndLabel.apk
```

## Linux / macOS

Linux（在目标机）：`g++ -O2 -std=c++17 -o db item_and_label.cpp`
macOS：`clang++ -O2 -std=c++17 -o db item_and_label.cpp`

## iOS

需 macOS + Xcode，仅可在越狱设备上运行命令行程序：
```
xcrun --sdk iphoneos clang++ -O2 -std=c++17 -arch arm64 item_and_label.cpp -o db-ios-arm64
```

## GUI 命令组合器（db_composer，Qt 6）

```
cd dbcomposer
qmake dbcomposer.pro && mingw32-make -j4
windeployqt --dir <输出目录> release/db_composer.exe
```

## 企业级存储

- **v2 紧凑二进制**：标签名字典去重 + 数值二进制化（int varint/zigzag、float 8 字节、bool 1 字节），实测 200 项重复标签库 19.4KB → 5.5KB（-71%）。
- **原子写入**：先写临时文件、`fsync` 落盘后再整体原子替换——Windows `MoveFileEx(REPLACE_EXISTING)`，POSIX `rename`（本就是原子的覆盖）。进程中断 / 断电不损坏。
- **向后兼容**：自动读 v1 文本旧库，`--vacuum` 一键升级 v2。
- **严格校验**：`--check` 校验最小文件头、字典条数与内容、每条记录（类型字节、int/float 定长、char 长度）及尾部残余；截断 / 头部缺失 / 字典损坏 / 尾随垃圾均正确报「损坏」。

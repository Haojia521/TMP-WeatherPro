# 编译构建

本项目为 Visual Studio 解决方案（`.sln`），依赖通过 [vcpkg](https://github.com/microsoft/vcpkg) 管理，所有依赖库以**静态库**形式链接。

## 开发环境

- **Windows** 系统
- **Visual Studio 2022**（MSVC 工具集 **v143**）/ **C++20**
- [vcpkg](https://github.com/microsoft/vcpkg) 已安装并完成初始化（`bootstrap-vcpkg.bat`），建议使用 **2026-07-29 Release** 版本

## 依赖库

| 依赖 | 特性（features） |
| --- | --- |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | `openssl`, `zlib` |
| [jwt-cpp](https://github.com/Thalhammer/jwt-cpp) | `picojson` |
| [simpleini](https://github.com/brofield/simpleini) | — |
| [yyjson](https://github.com/ibireme/yyjson) | — |

## 一、安装依赖（vcpkg）

依赖需分别针对目标平台编译为静态库。

**64 位（x64）依赖：**

```bat
vcpkg install cpp-httplib[openssl,zlib]:x64-windows-static ^
              jwt-cpp[picojson]:x64-windows-static ^
              simpleini:x64-windows-static ^
              yyjson:x64-windows-static
```

**32 位（x86）依赖（如需编译 x86 版本）：**

```bat
vcpkg install cpp-httplib[openssl,zlib]:x86-windows-static ^
              jwt-cpp[picojson]:x86-windows-static ^
              simpleini:x86-windows-static ^
              yyjson:x86-windows-static
```

## 二、设置环境变量

项目通过环境变量定位 vcpkg 静态库安装目录。**系统环境变量**或**用户环境变量**均可（推荐用户变量，无需管理员权限）。

| 环境变量 | 适用平台 | 值 |
| --- | --- | --- |
| `VCPKG_X64_STATIC` | x64 | `your_vcpkg_dir\installed\x64-windows-static` |
| `VCPKG_X86_STATIC` | x86 | `your_vcpkg_dir\installed\x86-windows-static` |

请将 `your_vcpkg_dir` 替换为你的 vcpkg 实际安装目录。

> 提示：设置环境变量后需**重启 Visual Studio**（或重新登录系统）使变量生效。

## 三、编译

1. 用 **Visual Studio 2022** 打开项目根目录下的 `.sln` 解决方案文件。
2. 在工具栏选择目标平台（`x64` 或 `x86`）与`Release`配置。
3. 确认项目属性中 C++ 语言标准为 **C++20**，平台工具集为 **v143**。
4. 生成解决方案（`生成` → `生成解决方案`，或按 `Ctrl+Shift+B`）。

> 说明：本项目主要在 **x64** 环境下开发调试；**x86 版本仅保证可编译，未经实际测试**，可能存在未知问题。

## 常见问题

- **找不到依赖 / 链接错误**：确认对应平台的环境变量已正确设置，且已安装对应 triplet 的静态库。
- **环境变量修改后不生效**：重启 Visual Studio 或重新登录系统。
- **x86 编译失败**：确认已安装 x86 静态库并设置了 `VCPKG_X86_STATIC`。
- **语言标准/工具集报错**：确认使用 VS2022、工具集 v143，并已启用 C++20。

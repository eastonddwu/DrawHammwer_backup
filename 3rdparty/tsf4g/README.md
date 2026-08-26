# TSF4G (tapp / tlog / tdr) 三方库说明

本目录内容摘自 `TSF4G_BASE-2.8.1.a2fc3fed9_X86_64_Release`（release/x86_64），
用于给 app_server 项目提供 **tapp**（应用框架）、**tlog**（日志库）、**tdr**（协议序列化）能力。
以下内容均已在本机 gcc 8.5.0 环境下**实测编译、链接、运行通过**（分别验证了 tlog HelloWorld 与 tapp helloworld 两个官方示例）。

## 目录结构

```
3rdparty/tsf4g/
├── include/          # 头文件（tapp/tlog/tdr 互相依赖，一并带上，不能只拷 tapp/tlog/tdr 三个目录）
│   ├── tapp/         # 应用框架
│   ├── tlog/         # 日志核心
│   ├── tloghelp/     # tlog_init_from_file 等辅助函数
│   ├── tdr/          # 协议描述/序列化核心
│   ├── tdr_cpp_comm/ # TDR生成的C++代码依赖的辅助类(TdrTypeUtil/TdrError等)
│   ├── pal/          # 平台抽象层（tapp.h 依赖）
│   ├── tbus/         # 进程间通信（tapp.h 依赖）
│   ├── tmng/         # 内存/共享内存管理（tloghelp 依赖）
│   ├── comm/         # 通用工具
│   └── baseversion.h
├── lib/
│   ├── libtsf4g.a       # 合并静态库（含 tapp+tlog+tdr+tbus+comm 全部符号），推荐直接用它
│   ├── libtapp.a / libtapp++.a
│   ├── libtlog.a / libtloghelp.a
│   ├── libtdr.a / libtdr_xml.a / libtdr_comm.a
│   ├── libtrapidxml.a  # XML解析，配置/协议解析依赖，必须静态链接
│   └── liblinenoise.a  # tapp 控制台交互(tapp_controller.c)依赖，用tapp必须带
├── tools/
│   └── tdr              # TDR 代码生成工具（把 .xml 协议描述转成 .h/.c/.cpp）
└── tsf4g.mk              # 编译公共变量（本项目手写，非原始 configure 生成）
```

## 用法

### 1. Makefile 里引入

```makefile
include 3rdparty/tsf4g/tsf4g.mk

APP_OBJ = your_app.o your_other.o

your_app: $(APP_OBJ)
	$(CC) -o $@ $(APP_OBJ) $(TSF4G_LIBS)

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@
```

`tsf4g.mk` 提供的变量：
- `TSF4G_INC` / `TSF4G_LIB` / `TSF4G_TOOLS`：三方库路径
- `CFLAGS` / `CXXFLAGS`：已包含 `-I$(TSF4G_INC)`
- `TSF4G_LIBS`：完整链接参数（已验证可用，含 `-lstdc++ -lanl -llinenoise` 等易漏项）

### 2. 用 tdr 工具生成协议代码

```bash
3rdparty/tsf4g/tools/tdr -P -O <输出目录> your_protocol.xml   # 生成C++
3rdparty/tsf4g/tools/tdr -H -O <输出目录> your_protocol.xml   # 生成C头文件
```

### 3. 代码里引用

```c
#include "tapp/tapp.h"
#include "tlog/tlog.h"
#include "tloghelp/tlogload.h"
#include "tdr/tdr.h"
```

## 已知链接依赖（容易漏，均已验证）

| 库/参数 | 缺失时报错 | 原因 |
|---|---|---|
| `-lstdc++` | `undefined reference to std::string::...` | libtsf4g.a 内含 C++ 代码(tdr宏表达式计算等) |
| `-lanl` | `undefined reference to gai_error / getaddrinfo_a` | tlog 网络日志异步DNS解析 |
| `-llinenoise` | `undefined reference to linenoise / linenoiseHistoryAdd` | tapp 控制台交互功能 |
| `-Wl,-Bstatic -ltrapidxml -Wl,-Bdynamic` | `cannot find -ltrapidxml` 或符号冲突 | 必须静态链接，且要用 `-Bstatic/-Bdynamic` 分组包裹 |
| `-lnsl` | （现代glibc已移除该库） | 旧版本遗留依赖，当前系统直接去掉即可，不影响功能 |

## 说明

- 没有走原始的 `./configure && make && ./install.sh` 流程，而是直接从发布包按需摘取文件，所以 `tsf4g.mk` 是手写的具体版本，不是 `tsf4g.mk.in` 模板替换出来的。
- 如果之后需要 TBUS 进程间通信、ZooKeeper 服务发现等更多能力，需要从原始发布包 `TSF4G_BASE-2.8.1.a2fc3fed9_X86_64_Release/release/x86_64/` 补充对应的 lib（如 `libtbus_zk.a`、`libzookeeper_*.a`）。
- 完整的接入说明和示例代码见 `/data/workspace/generate.md`。

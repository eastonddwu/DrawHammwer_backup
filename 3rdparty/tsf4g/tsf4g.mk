# tsf4g.mk —— app_server 项目引入 TSF4G(tapp/tlog/tdr) 的编译公共变量
# 本文件是手动编写的具体版本（而非 tsf4g.mk.in 模板，因为没有走 ./configure 流程）
# 路径均相对本文件所在目录 3rdparty/tsf4g/

TSF4G_HOME := $(dir $(lastword $(MAKEFILE_LIST)))
TSF4G_INC  := $(TSF4G_HOME)include
TSF4G_LIB  := $(TSF4G_HOME)lib
TSF4G_TOOLS:= $(TSF4G_HOME)tools

CC  = gcc
CXX = g++
RM  = rm -f

CINC  = -I$(TSF4G_INC)/

CFLAGS  = -Wall -Wextra -pipe -D_GNU_SOURCE -D_REENTRANT -fPIC -g -O2
CFLAGS += $(CINC)
CXXFLAGS = $(CFLAGS)

LDPATH  = -L$(TSF4G_LIB)/

# 常用链接方式：合并库 libtsf4g.a（已含 tapp/tlog/tdr/tbus/comm 等全部符号，内含C++代码）
# + 静态链接 trapidxml（XML解析，配置/协议解析依赖）
# + -lstdc++ -lanl 是必需的（libtsf4g.a里有C++符号和getaddrinfo_a依赖），已实测验证
# + -llinenoise 是 tapp 控制台交互(tapp_controller.c)依赖的，用tapp必须带
# 注：-lnsl 在当前系统 glibc 已废弃移除，如遇 "cannot find -lnsl" 直接去掉即可
# 以下组合已在本机实测跑通 tlog HelloWorld 和 tapp helloworld 两个示例，编译链接运行全部成功。
TSF4G_LIBS = $(LDPATH) $(TSF4G_LIB)/libtsf4g.a -lrt -lstdc++ -lanl -llinenoise \
             -Wl,-Bstatic -ltrapidxml -Wl,-Bdynamic -lpthread -ldl -lm

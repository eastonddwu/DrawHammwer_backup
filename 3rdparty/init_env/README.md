## 1. 方便UA快速初始化新机器新环境
```
将整个项目clone到部署目录下,如：/data/home/user00
执行./init_env.sh [all|tcm|tbuspp]
```


## 2. dev_tool 工具使用说明

如果要在自己的开发环境使用
```
1:确保工具路径为 ~/dev_tool
2:运行 sh ~/dev_tool/init.sh
3:修改 ~/dev_tool/tool_env.lua 中的user变量
```

===========================================
.vim 目录是log文件 语法染色的脚本

===========================================
log.lua 看日志工具 
快捷别名 see

--用法
see 服务器名*n [日志类型*n] 时间 [grep参数]

--例子
see zone team ERROR WARN 30 
查看最近30分钟 zonesvr teamsvr 所有 error和warn日志

see zone 251000-500 ZoneLoginReq 
查看 25日10:00之后500分钟内 zonesvr 所有带ZoneLoginReq日志


============================================
debug.lua 查看corefile工具 快捷别名 debug

--用法
debug 进程名|core_file [跳过bin文件数量]

--例子
debug zonesvr 调试zonesvr

debug core_zonesvr_1587868561
会查找 早于corefile时间且  最接近  的zonesvr 来查看这个corefile

debug core_zonesvr_1587868561 1
会查找 早于corefile时间  第二接近  的zonesvr 来查看这个corefile


===========================================
do.lua 对tcm的再次包装 快捷别名 ddo

--用法
ddo 指令 [服务器名字*n] = cmd runshell [服务器id] 指令
--例子
ddo clean team match room 
清理 teamsvr matchsvr roomsvr共享内存


ddo rs team match room 
重启 teamsvr matchsvr roomsvr






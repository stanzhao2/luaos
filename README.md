# luaos

***用lua编写服务器程序变得更简单***

1. 支持一个进程同时执行多个 lua 虚拟机并独立运行；
2. 多个 lua 虚拟机之间可以通过订阅机制实现通讯；
3. 内置 timeingwheel 组件，可以方便的实现定时任务；
4. 内置网络模块，支持标准的 TCP/UDP 通讯，并且支持 SSL 加密，实现 http/https/ws/wss 协议，方便为第三方提供服务接口；
5. 内置全局存储 (storage)，可以让不同的 lua 虚拟机实现数据共享；
6. 内置常用编码/HASH算法，包括 md5, sha, base64 等，还包括rsa加密签名等功能；
7. 内置 curl 模块，方便程序调用外部 http/https 接口；
8. 使用简单，只需要 local luaos = require("luaos") 即可包含所有功能

LuaOS 启动后默认加载lua/main.lua 或者 lua/main/init.lua 文件作为启动文件，如果该 lua 文件有 main 函数，则启动后自动调用该函数
使用者可以在该模块内通过调用 luaos.execute 启动其他需要的 lua 模块(被启动的模块在独立 lua 虚拟机和线程下运行)

luaos可以对所有的 lua 文件进行编译打包，生成 rom 文件，该 rom 文件可以被 luaos 直接执行 （原始文件可以在发布时删除，避免代码泄露）。编译命令为：luaos -c xxx.rom，编译后会在进程目录下生成 xxx.rom 文件，运行该 rom 时可以执行 luaos -r xxx.rom 即可

**这里要注意，rom文件里的代码是编译且加密的，无法还原成源代码**

lua文件存在加载顺序，同名文件优先从 lua 目录加载，如果不存在则从 usr 目录加载，如果依然不存在则会从指定的 rom 中加载（如果指定了 rom 文件）
顺序如下：
1. lua目录  用于存放用户编写的 lua 脚本文件
2. usr目录  用于存放系统提供的 lua 脚本文件
3. rom文件  编译并打包后的 rom 文件

为了扩展 lua 功能，用户可以自己编写 lua 扩展库，将编译好的 dll/so 文件存放在 ext 目录下，其中 sys 目录用于存放系统使用的扩展库（不建议存放其他文件），ext目录存放用户使用的扩展库。

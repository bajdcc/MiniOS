MiniOS
===============================
Simple x86-based OS.


### 开发阶段

1. **[0.2.0]** 从零开始，保留核心文件，慢慢添加功能，实现Hello world


### 构建工具
**平台：Ubuntu 16.04.2 x86**
* make
* nasm
* gcc
* binutils
* cgdb
* qemu

```bash
sudo apt install make nasm gcc binutils cgdb qemu
sudo ln -s /usr/bin/qemu-system-i386 /usr/bin/qemu
```

### 编译并运行
```shell
make init   # only for first time
make fs     # build root file system and user routines, root privilege required
make        # build kernel
make run    # run with qemu
```


### References
* [OS67](https://github.com/SilverRainZ/OS67)

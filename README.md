MiniOS
===============================
Simple x86-based OS.


###Build requirements
Platform: Linux
* make
* nasm
* gcc
* binutils
* cgdb
* qemu

```shell
sudo apt install make nasm gcc binutils cgdb qemu
sudo ln -s /usr/bin/qemu-system-i386 /usr/bin/qemu
```

###How to compile  
```shell
make init   # only for first time
make fs     # build root file system and user routines, root privilege required
make        # build kernel
make run    # run with qemu
```


###References
* [OS67](https://github.com/SilverRainZ/OS67)

# lc-3-vm
lc-3-vm is a virtual machine that emulates the [LC-3](https://en.wikipedia.org/wiki/Little_Computer_3).  
The purpose of this project is to learn to program my own virtual machine in C, and even add my own stuff meanwhile. I'll be following [Justin Meiners and Ryan Pendleton's guide](https://www.jmeiners.com/lc3-vm/) on writing a virtual machine for the LC-3.

## How to run the VM
To build the VM yourself with Clang (or another C compiler):

```sh
clang -std=c11 -Wall -Wextra -Wpedantic -o lc3 lc3.c
```

Run it with one of the included game images:

```sh
./lc3 2048.obj
```

### Platform note:
The source currently supports Unix-like systems such as macOS and Linux. It uses POSIX terminal and input APIs and will not build natively on Windows without portability changes. The included executable is also platform-specific. Support for Windows coming soon.

## Games

The game images are already included in the repository:

- [2048](#2048obj)
- [Rogue](#rogueobj)

You can also open or download them directly:

### 2048.obj

[Download `2048.obj`](./2048.obj)

### rogue.obj

[Download `rogue.obj`](./rogue.obj)

# phoenix-rtos-kernel
This repository contains the source for the Phoenix-RTOS microkernel.
IA32, ARM, ARMv7, RISCV64 architectures are supported.

## Building toolchain
To build cross-compile toolchain use `toolchain/build-toolchain.sh` script.
Targets supported currently by the script:
- i386-pc-phoenix
- arm-phoenix
- riscv64-phoenix-elf

Sample invocation:
```
cd toolchain
./build-toolchain.sh arm-phoenix ~/arm-phoenix-toolchain
```

For other targets You need to build toolchain manually

After successful compilation add the toolchain to the PATH variable, for example:
```
export PATH=~/arm-phoenix-toolchain/arm-phoenix/bin/:$PATH
```

## Building microkernel
To compile microkernel for target architecture edit Makefile and TARGET variable and after this type:

	$ make clean
	$ make

## License
This work is licensed under a BSD license. See the LICENSE file for details.

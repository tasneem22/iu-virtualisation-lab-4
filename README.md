# iu-virtualization-lab-4

This is a linux container written in C language.

## How to run?
Pre-requirements: gcc compiler.

1. Prepare the rootfs from linux alpine image:
```shell
mkdir root && cd root
curl -Ol http://nl.alpinelinux.org/alpine/v3.7/releases/x86_64/alpine-minirootfs-3.7.0-x86_64.tar.gz
tar -xvf alpine-minirootfs-3.7.0_rc1-x86_64.tar.gz
```
2. Compile and run the container as a root:
```shell
gcc ./container.c -o container && sudo ./container
```
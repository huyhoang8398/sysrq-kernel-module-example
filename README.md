git clone https://github.com/huyhoang8398/sysrq-kernel-module-example
cd sysrq-kernel-module-example

## Kernel Module
make
make test

## Userspace

gcc client.c -o client
./client

## Test sysrq
echo "A" > /proc/sysrc-trigger

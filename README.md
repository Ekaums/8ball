# Magic 8ball
Linux kernel module to create a char device magic 8ball.

## Prerequisites
You must have your kernel development environment configured. This means you
will need your system's kernel headers (from installing the appropriate
distro-specific package).

## Compilation
After cloning and entering the repo, run
```bash
make -C /lib/modules/$(uname -r)/build M=$PWD modules
```

# Usage
After compiling, add the module using
```bash
sudo insmod 8ball.ko
```

After the module has been added, a device will be created under `/dev/8ball`. You can give the 8ball your question and read
it's response
``` bash
echo "Was this project worth it?" > /dev/8ball
...
cat /dev/8ball
```

The 8ball will then respond with the message it sees fit.

Lastly, removing the module is done by simply:
```bash
sudo rmmod 8ball
```


# Cross-compilation Process

```
export PATH=$PATH:/media/pc/HIKSEMI_EXTERNAL/EmbeddedLinux/OpenWrt/prplos/staging_dir/toolchain-aarch64_cortex-a72_gcc-13.3.0_musl/bin

export STAGING_DIR=/media/pc/HIKSEMI_EXTERNAL/EmbeddedLinux/OpenWrt/prplos/staging_dir

export CFLAGS="-I/media/pc/HIKSEMI_EXTERNAL/EmbeddedLinux/OpenWrt/prplos/staging_dir/target-aarch64_cortex-a72_musl/usr/include"

export LDFLAGS="-L/media/pc/HIKSEMI_EXTERNAL/EmbeddedLinux/OpenWrt/prplos/staging_dir/target-aarch64_cortex-a72_musl/usr/lib"
```
# To open container
```
docker exec -ti -u $USER -e USER=$USER \
    -w /home/$USER/usp/ambiorix-uspdev-env ambiorix-uspdev-env /bin/bash
```
# To test use :
```
aarch64-openwrt-linux-gcc --version
```
# Simple compilation command for fast testing
```
aarch64-openwrt-linux-gcc $CFLAGS $LDFLAGS -o hello hello.c -lamxc
```

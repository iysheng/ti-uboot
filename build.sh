#!/bin/sh
# A script to build rico uboot
if [ $# -eq 0 ]
then
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- distclean
elif [ $1 = c ]
then
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- distclean
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- am43xx_evm_config
elif [ $1 = b ]
then
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
elif [ $1 = o ]
then
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- distclean
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- am43xx_evm_config
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
else
echo "c for config , o for build out"
fi

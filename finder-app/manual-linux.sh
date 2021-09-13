#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu- 
#writer, finder-test utlities directory path
<<<<<<< HEAD
UTILITY_DIR=$(pwd)
#UTILITY_DIR=/home/mehul/Desktop/f21_aesd/assignment-1-MehulCUB/finder-app 
=======
#UTILITY_DIR=$(pwd)
UTILITY_DIR=/home/mehul/Desktop/f21_aesd/assignment-1-MehulCUB/finder-app 
>>>>>>> 84dee7baf3190ee1ceee77f02eba2baf7f0adc1e

 
if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

#fail and exit if directoy not present and then can not be created 
if [ $? -eq 0 ]; then
	echo "directory create : successful"	 
else
	echo error
	echo "directory create : failed"	 
	exit 1
fi 

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
	# building cross compiled kernel image, module and database using aarch64-none-linux-gnu-gcc compiler 
	make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} mrproper
	make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} defconfig
	
	make -j4 ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} all	
	
	make -j4 ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} modules
	make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi 

#TODO: Add Image to outdir
echo "Adding the Image in outdir"

cp "${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image" "${OUTDIR}"


echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

cd "$OUTDIR"

# TODO: Create necessary base directories
echo "Creating base directories"

mkdir rootfs
cd rootfs
mkdir bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir usr/bin usr/lib usr/sbin
mkdir -p var/log
tree -d #print file system tree 

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
	echo "Configuring busybox"
	make distclean
	make defconfig
else
    cd busybox  #if already present 
fi

# TODO: Make and insatll busybox
echo "insatlling busybox"

make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install
cd ${OUTDIR}/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"


echo "busybox installation complete"

# TODO: Add library dependencies to rootfs
echo "Adding Library dependencies to rootfs/lib"

cd ${OUTDIR}/rootfs

export SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
echo $SYSROOT

sudo cp -L $SYSROOT/lib/ld-linux-aarch64.* ${OUTDIR}/rootfs/lib/
sudo cp -L $SYSROOT/lib64/libm.so.* ${OUTDIR}/rootfs/lib64/
sudo cp -L $SYSROOT/lib64/libresolv.so.* ${OUTDIR}/rootfs/lib64/
sudo cp -L $SYSROOT/lib64/libc.so.* ${OUTDIR}/rootfs/lib64/

echo " Library dependencies added rootfs successful"

# TODO: Make device nodes
echo "Make device nodes"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1

#list created device node 
ls -l dev 

cd ${UTILITY_DIR}  ##move to utility directory
# TODO: Clean and build the writer utility
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

#copy utlitiles from finder-app directories to rootfs/home 
# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp writer ${OUTDIR}/rootfs/home/
cp finder.sh ${OUTDIR}/rootfs/home/
cp finder-test.sh ${OUTDIR}/rootfs/home/
cp autorun-qemu.sh ${OUTDIR}/rootfs/home/
cp conf/ -r ${OUTDIR}/rootfs/home/

# TODO: Chown the root directory
cd ${OUTDIR}/rootfs
sudo chown -R root:root *


# TODO: Create initramfs.cpio.gz
echo "Creating initramfs.cpio.gz"
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ..
gzip -f initramfs.cpio


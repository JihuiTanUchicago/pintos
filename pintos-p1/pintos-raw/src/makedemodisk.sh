#!/bin/bash

#
# Build a bootable disk for Pintos
# This script was originally developed at Virginia Tech
#
# Note: this script is intended to work with the version
# of Pintos provided through the UChicago PhoenixForge site
#
# It's intended to run from the command line in the src/ 
# directory
#

# CHANGE this line to install a P3/P4 kernel
BUILDDIR=./userprog/build
EXAMPLEDIR=./examples
DISKIMAGE=usbdisk.img

# check that BUILDDIR exists
test -d ${BUILDDIR} || cat << EOF
${BUILDDIR} does not exist --- 

did you build your Pintos P2 kernel and are you currently in the src/ directory ? 
EOF
test -d ${BUILDDIR} || exit

/bin/rm -f ${DISKIMAGE}
test -e ${BUILDDIR}/kernel.bin || echo 'No kernel found in ' ${BUILDDIR}

# Create a new disk image and start up the kernel to populate it with
# several useful programs.  
#
# CHANGE this to place other apps on the disk
#
# For a P3 kernel, you should also create a swap disk via
# --swap-size=4
#
pintos -v -k --qemu \
        --make-disk ${DISKIMAGE} \
        --filesys-size=4 \
        --loader ${BUILDDIR}/loader.bin \
        --kernel=${BUILDDIR}/kernel.bin \
        --align=full \
        -p ${EXAMPLEDIR}/shell -a shell \
        -p ${EXAMPLEDIR}/ls -a ls \
        -p ${EXAMPLEDIR}/halt -a halt \
        -p ${EXAMPLEDIR}/echo -a echo \
        -p ${EXAMPLEDIR}/cat -a cat \
        -p ${EXAMPLEDIR}/insult -a insult \
        -p ${EXAMPLEDIR}/shell.c -a shell.c \
        -- -q -f

# Set the kernel command line to run the 'shell' as the first program
pintos-set-cmdline \
        ${DISKIMAGE} \
        -- -q run shell

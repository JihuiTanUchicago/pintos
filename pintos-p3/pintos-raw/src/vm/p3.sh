#!/bin/bash

# This grading script is intended to show students what would happen when TA
# grade p3 project.
#
# Do the following steps to run the script:
# - copy this script to src/vm/
#
# - change file attribute to allow script execution
#
#   chmod 700 p3.sh
#
# - run this script to start grading
#
#   ./p3.sh
#
# This script will download pintos-raw and extract it to ~/cs230-grading/pintos,
# copy specific files that you are allowed to modify there, and do the grading
# on ~/cs230-grading/pintos.

cd ../

rm -rf ~/cs230-grading &> /dev/null

# Download the pintos-raw code and extract it.
wget http://people.cs.uchicago.edu/~wangm12/cs230/pintos-raw.tgz -P ~/cs230-grading
tar -zxvf ~/cs230-grading/pintos-raw.tgz -C ~/cs230-grading/
rm ~/cs230-grading/pintos-raw.tgz
mv ~/cs230-grading/pintos-raw ~/cs230-grading/pintos

SRC_DIR=$HOME/cs230-grading/pintos/src
VM_DIR=$SRC_DIR/vm

# replace the make files
rm $SRC_DIR/Make.config $SRC_DIR/Makefile.build $SRC_DIR/Makefile.kernel
wget http://people.cs.uchicago.edu/~yuyangh/cs230/Make.config -O $SRC_DIR/Make.config
wget http://people.cs.uchicago.edu/~yuyangh/cs230/Makefile.build -O $SRC_DIR/Makefile.build
wget http://people.cs.uchicago.edu/~yuyangh/cs230/Makefile.kernel -O $SRC_DIR/Makefile.kernel

# Determine the acceptable changed files and copy it from the student directory
# to the temporary grading directory. Notice that in here we have several files since p3 requires both finished p2
# and modified files from p1/p2.
files=(
       threads/init.c
       threads/interrupt.c
       threads/thread.c
       threads/thread.h
       userprog/exception.c
       userprog/process.c
       userprog/syscall.c
       userprog/syscall.h
       userprog/pagedir.c
       devices/timer.c
       vm/frame.c
       vm/frame.h
       vm/page.c
       vm/page.h
       vm/swap.c
       vm/swap.h)

for file in ${files[@]}; do
    echo "Copying [$file]"
    cp $file $SRC_DIR/$file
done

cd $VM_DIR

make

cd build/

echo "Running make check for all test cases..."
make check | tee check_output

##########################################################################################
# Grading functions
##########################################################################################

calc() {
    echo "scale=4; $1" | bc ;exit
}

function cal(){
    if [ "$1" -eq "63" ] ; then
  sumvrf=$((sumvrf+3));
    fi
    if [ "$1" -eq "71" ] ; then
  sumvrf=$((sumvrf+3));
    fi
    if [ "$1" -eq "66" ] ; then
  sumvrf=$((sumvrf+3));
    fi
    if [ "$1" -eq "64" ] ; then
  sumvrf=$((sumvrf+3));
    fi
    if [ "$1" -eq "72" ] ; then
  sumvrf=$((sumvrf+3));
    fi
    if [ "$1" -eq "73" ] ; then
  sumvrf=$((sumvrf+3));
    fi
    if [ "$1" -eq "78" ] ; then
  sumvrf=$((sumvrf+3));
    fi
    if [ "$1" -eq "74" ] ; then
  sumvrf=$((sumvrf+4));
    fi
    if [ "$1" -eq "75" ] ; then
  sumvrf=$((sumvrf+4));
    fi
    if [ "$1" -eq "76" ] ; then
  sumvrf=$((sumvrf+4));
    fi
    if [ "$1" -eq "67" ] ; then
  sumvrb=$((sumvrb+2));
    fi
    if [ "$1" -eq "68" ] ; then
  sumvrb=$((sumvrb+3));
    fi
    if [ "$1" -eq "69" ] ; then
  sumvrb=$((sumvrb+2));
    fi
    if [ "$1" -eq "70" ] ; then
  sumvrb=$((sumvrb+3));
    fi
    if [ "$1" -eq "65" ] ; then
  sumvrb=$((sumvrb+4));
    fi
}

sumrf=0;
sunrb=0;
sumvrf=0;
sumvrb=0;
sumfile=0;
sumtotal=0;
sum=0;
while read line; do
    pass=$(echo $line | grep "pass");
    i=$(($i+1));
    if [ ! -z "$pass" ]; then
  		cal $i
    fi
done < results

echo ""
echo "sum of /vm/Rubric.functionality is :$sumvrf (33)"
echo "sum of /vm/Rubric.robustness is :$sumvrb (14)"
sumtotal=$(calc "$sumvrf + $sumvrb")

if [ "$sumtotal" = "" ]; then
   sumtotal=0
fi

echo "Total grade out of 47: $sumtotal"
echo ""

##########################################################################################
##########################################################################################

# Delete the temporary grading directory.
rm -rf ~/cs230-grading &> /dev/null

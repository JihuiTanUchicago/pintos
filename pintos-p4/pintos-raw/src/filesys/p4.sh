#!/bin/bash

# CS230 Project 4 Grading script
#
# How to use this script:
# (1). Save this script as $PINTOS/src/filesys/p4.sh, $PINTOS is your pintos
#      code top directory.
# (2). Open a terminal:
#      $ cd $PINTOS/src/filesys
#      $ chmod u+x p4.sh
#      $ ./p4.sh                    # run the script to do the grading
#
# This script will download pintos-raw code from cs230 website and extract it to
# $GDIR as defined below, then it copies specific files that you are allowed to
# modify there. Last, it compiles, tests your code and does the grading

GDIR=~/cs230-grading
SRCDIR=$GDIR/pintos/src
P4DIR=$SRCDIR/filesys

# where we download raw source files
# PINTOS_RAW=https://www.classes.cs.uchicago.edu/archive/2019/fall/23000-1/proj/files/pintos-raw.tgz
FIXEDPOINT=https://www.classes.cs.uchicago.edu/archive/2019/fall/23000-1/proj/p1/fixed-point.h

tar_name="pintos-raw$1.tgz"
pintos_raw_link="https://people.cs.uchicago.edu/~rayandrew/cs230/$tar_name"

cd ../

rm -rf $GDIR &>/dev/null

echo ""
echo "===> (1/3) Preparing pintos environment in $SRCDIR"
echo ""

# download the pintos-raw code and extract it
echo "Downloading $tar_name"
wget $pintos_raw_link -P $GDIR

echo "Extracting $tar_name"
tar -zxf $GDIR/$tar_name -C $GDIR
rm $GDIR/$tar_name
mv $GDIR/pintos-raw $GDIR/pintos

# grading is based on the following files
P4FILES=(
	filesys/directory.c
	filesys/directory.h
	filesys/file.c
	filesys/file.h
	filesys/filesys.c
	filesys/filesys.h
	filesys/free-map.c
	filesys/free-map.h
	filesys/fsutil.c
	filesys/inode.c
	filesys/inode.h
	threads/thread.c
	threads/thread.h
	userprog/exception.c
	userprog/process.c
	userprog/syscall.c
	userprog/syscall.h)

for file in ${P4FILES[@]}; do
	cp $file $SRCDIR/$file
done

# download fixed-point.h
wget $FIXEDPOINT -P $SRCDIR/threads/ >/dev/null 2>&1

cd $P4DIR

# make >make.log 2>&1 || (echo "===> Error: Code Compilation failed, check detailed
# make >make.log 2>&1 || (echo "===> Error: Code Compilation failed, check detailed log in $P4DIR/make.log"; exit)
make

echo ""
echo "===> (2/3) Performing P4 testing under $P4DIR"
echo ""
cd build/
make check | tee check_output
cd tests/userprog/

# calculating score based on passed tests
calc() {
	echo "scale=4; $1" | bc
	exit
}

sum=0
sumef=0
sumer=0
sumep=0
sumFunc=0
sumRob=0
sumBas=0
emphFunc=5
emphBas=5
emphRob=5
emFunc=40
emRob=20
emPer=25

if [ -f args-none.result ] && [ "$(echo $(head -n 1 args-none.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func
	#
fi
if [ -f args-single.result ] && [ "$(echo $(head -n 1 args-single.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func
	#
fi
if [ -f args-multiple.result ] && [ "$(echo $(head -n 1 args-multiple.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func

fi
if [ -f args-many.result ] && [ "$(echo $(head -n 1 args-many.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func

fi
if [ -f args-dbl-space.result ] && [ "$(echo $(head -n 1 args-dbl-space.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func

fi
if [ -f sc-bad-sp.result ] && [ "$(echo $(head -n 1 sc-bad-sp.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 3))
	sum=$(($sum + ($emphRob * 3)))
	# Rob

fi
if [ -f sc-bad-arg.result ] && [ "$(echo $(head -n 1 sc-bad-arg.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 3))
	sum=$(($sum + ($emphRob * 3)))
	# Rob

fi
if [ -f sc-boundary.result ] && [ "$(echo $(head -n 1 sc-boundary.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 5))
	sum=$(($sum + ($emphRob * 5)))
	# Rob

fi
if [ -f sc-boundary-2.result ] && [ "$(echo $(head -n 1 sc-boundary-2.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 5))
	sum=$(($sum + ($emphRob * 5)))
	# Rob

fi
if [ -f halt.result ] && [ "$(echo $(head -n 1 halt.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func

fi
if [ -f exit.result ] && [ "$(echo $(head -n 1 exit.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 5))
	sum=$(($sum + ($emphFunc * 5)))
	# Func

fi

if [ -f create-normal.result ] && [ "$(echo $(head -n 1 create-normal.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func

fi
if [ -f create-empty.result ] && [ "$(echo $(head -n 1 create-empty.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func

fi

if [ -f create-null.result ] && [ "$(echo $(head -n 1 create-null.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 2))
	sum=$(($sum + ($emphRob * 2)))
	# Rob

fi
if [ -f create-bad-ptr.result ] && [ "$(echo $(head -n 1 create-bad-ptr.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 3))
	sum=$(($sum + ($emphRob * 3)))
	# Rob

fi

if [ -f create-long.result ] && [ "$(echo $(head -n 1 create-long.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func

fi
if [ -f create-exists.result ] && [ "$(echo $(head -n 1 create-exists.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func

fi

if [ -f create-bound.result ] && [ "$(echo $(head -n 1 create-bound.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 3))
	sum=$(($sum + ($emphRob * 3)))
	# Func

fi

if [ -f open-normal.result ] && [ "$(echo $(head -n 1 open-normal.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func

fi
if [ -f open-missing.result ] && [ "$(echo $(head -n 1 open-missing.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func

fi

if [ -f open-boundary.result ] && [ "$(echo $(head -n 1 open-boundary.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 3))
	sum=$(($sum + ($emphRob * 3)))
	# Rob

fi

if [ -f open-empty.result ] && [ "$(echo $(head -n 1 open-empty.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 2))
	sum=$(($sum + ($emphRob * 2)))
	# Rob

fi

if [ -f open-null.result ] && [ "$(echo $(head -n 1 open-null.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 2))
	sum=$(($sum + ($emphRob * 2)))
	# Rob

fi

if [ -f open-bad-ptr.result ] && [ "$(echo $(head -n 1 open-bad-ptr.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 3))
	sum=$(($sum + ($emphRob * 3)))
	# Rob

fi

if [ -f open-twice.result ] && [ "$(echo $(head -n 1 open-twice.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func

fi

if [ -f close-normal.result ] && [ "$(echo $(head -n 1 close-normal.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func

fi

if [ -f close-twice.result ] && [ "$(echo $(head -n 1 close-twice.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 2))
	sum=$(($sum + ($emphRob * 2)))
	# Func

fi

if [ -f close-stdin.result ] && [ "$(echo $(head -n 1 close-stdin.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 2))
	sum=$(($sum + ($emphRob * 2)))
	# Rob

fi

if [ -f close-stdout.result ] && [ "$(echo $(head -n 1 close-stdout.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 2))
	sum=$(($sum + ($emphRob * 2)))
	# Rob

fi

if [ -f close-bad-fd.result ] && [ "$(echo $(head -n 1 close-bad-fd.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 2))
	sum=$(($sum + ($emphRob * 2)))
	# Rob

fi

if [ -f read-normal.result ] && [ "$(echo $(head -n 1 read-normal.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func

fi

if [ -f read-bad-ptr.result ] && [ "$(echo $(head -n 1 read-bad-ptr.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 3))
	sum=$(($sum + ($emphRob * 3)))
	# Rob

fi

if [ -f read-boundary.result ] && [ "$(echo $(head -n 1 read-boundary.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 3))
	sum=$(($sum + ($emphRob * 3)))
	# Rob

fi

if [ -f read-zero.result ] && [ "$(echo $(head -n 1 read-zero.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Rob

fi
if [ -f read-stdout.result ] && [ "$(echo $(head -n 1 read-stdout.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 2))
	sum=$(($sum + ($emphRob * 2)))
	# Rob

fi

if [ -f read-bad-fd.result ] && [ "$(echo $(head -n 1 read-bad-fd.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 2))
	sum=$(($sum + ($emphRob * 2)))
	# Rob

fi
if [ -f write-normal.result ] && [ "$(echo $(head -n 1 write-normal.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func

fi

if [ -f write-bad-ptr.result ] && [ "$(echo $(head -n 1 write-bad-ptr.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 3))
	sum=$(($sum + ($emphRob * 3)))
	# Rob

fi

if [ -f write-boundary.result ] && [ "$(echo $(head -n 1 write-boundary.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 3))
	sum=$(($sum + ($emphRob * 3)))
	# Rob

fi

if [ -f write-zero.result ] && [ "$(echo $(head -n 1 write-zero.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
	# Func

fi

if [ -f write-stdin.result ] && [ "$(echo $(head -n 1 write-stdin.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 2))
	sum=$(($sum + ($emphRob * 2)))
	# Rob

fi

if [ -f write-bad-fd.result ] && [ "$(echo $(head -n 1 write-bad-fd.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 2))
	sum=$(($sum + ($emphRob * 2)))
	# Rob

fi

if [ -f exec-once.result ] && [ "$(echo $(head -n 1 exec-once.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 5))
	sum=$(($sum + ($emphFunc * 5)))
	# Func

fi

if [ -f exec-arg.result ] && [ "$(echo $(head -n 1 exec-arg.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 5))
	sum=$(($sum + ($emphFunc * 5)))
	# Func

fi

if [ -f exec-multiple.result ] && [ "$(echo $(head -n 1 exec-multiple.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 5))
	sum=$(($sum + ($emphFunc * 5)))
	# Func

fi

if [ -f exec-missing.result ] && [ "$(echo $(head -n 1 exec-missing.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 5))
	sum=$(($sum + ($emphRob * 5)))
	# Rob

fi

if [ -f exec-bad-ptr.result ] && [ "$(echo $(head -n 1 exec-bad-ptr.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 3))
	sum=$(($sum + ($emphRob * 3)))
	# Rob

fi

if [ -f wait-simple.result ] && [ "$(echo $(head -n 1 wait-simple.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 5))
	sum=$(($sum + ($emphFunc * 5)))
fi

if [ -f wait-twice.result ] && [ "$(echo $(head -n 1 wait-twice.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 5))
	sum=$(($sum + ($emphFunc * 5)))
fi

if [ -f wait-killed.result ] && [ "$(echo $(head -n 1 wait-killed.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 5))
	sum=$(($sum + ($emphRob * 5)))
fi

if [ -f wait-bad-pid.result ] && [ "$(echo $(head -n 1 wait-bad-pid.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 5))
	sum=$(($sum + ($emphRob * 5)))
fi

if [ -f multi-recurse.result ] && [ "$(echo $(head -n 1 multi-recurse.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 15))
	sum=$(($sum + ($emphFunc * 15)))
fi

if [ -f multi-child-fd.result ] && [ "$(echo $(head -n 1 multi-child-fd.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 2))
	sum=$(($sum + ($emphRob * 2)))
fi

if [ -f rox-simple.result ] && [ "$(echo $(head -n 1 rox-simple.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
fi

if [ -f rox-child.result ] && [ "$(echo $(head -n 1 rox-child.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
fi

if [ -f rox-multichild.result ] && [ "$(echo $(head -n 1 rox-multichild.result) | grep "PASS")" ]; then
	sumFunc=$(($sumFunc + 3))
	sum=$(($sum + ($emphFunc * 3)))
fi

if [ -f bad-read.result ] && [ "$(echo $(head -n 1 bad-read.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 1))
	sum=$(($sum + ($emphRob * 1)))
fi

if [ -f bad-write.result ] && [ "$(echo $(head -n 1 bad-write.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 1))
	sum=$(($sum + ($emphRob * 1)))
fi

if [ -f bad-read2.result ] && [ "$(echo $(head -n 1 bad-read2.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 1))
	sum=$(($sum + ($emphRob * 1)))
fi

if [ -f bad-write2.result ] && [ "$(echo $(head -n 1 bad-write2.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 1))
	sum=$(($sum + ($emphRob * 1)))
fi

if [ -f bad-jump.result ] && [ "$(echo $(head -n 1 bad-jump.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 1))
	sum=$(($sum + ($emphRob * 1)))
fi

if [ -f bad-jump2.result ] && [ "$(echo $(head -n 1 bad-jump2.result) | grep "PASS")" ]; then
	sumRob=$(($sumRob + 1))
	sum=$(($sum + ($emphRob * 1)))
fi

if [ -f ../filesys/base/lg-create.result ] && [ "$(echo $(head -n 1 ../filesys/base/lg-create.result) | grep "PASS")" ]; then
	sumBas=$(($sumBas + 1))
	sum=$(($sum + ($emphBas * 1)))
fi

if [ -f ../filesys/base/lg-full.result ] && [ "$(echo $(head -n 1 ../filesys/base/lg-full.result) | grep "PASS")" ]; then
	sumBas=$(($sumBas + 2))
	sum=$(($sum + ($emphBas * 2)))
fi

if [ -f ../filesys/base/lg-random.result ] && [ "$(echo $(head -n 1 ../filesys/base/lg-random.result) | grep "PASS")" ]; then
	sumBas=$(($sumBas + 2))
	sum=$(($sum + ($emphBas * 2)))
fi

if [ -f ../filesys/base/lg-seq-block.result ] && [ "$(echo $(head -n 1 ../filesys/base/lg-seq-block.result) | grep "PASS")" ]; then
	sumBas=$(($sumBas + 2))
	sum=$(($sum + ($emphBas * 2)))
fi

if [ -f ../filesys/base/lg-seq-random.result ] && [ "$(echo $(head -n 1 ../filesys/base/lg-seq-random.result) | grep "PASS")" ]; then
	sumBas=$(($sumBas + 3))
	sum=$(($sum + ($emphBas * 3)))
fi

if [ -f ../filesys/base/sm-create.result ] && [ "$(echo $(head -n 1 ../filesys/base/sm-create.result) | grep "PASS")" ]; then
	sumBas=$(($sumBas + 1))
	sum=$(($sum + ($emphBas * 1)))
fi

if [ -f ../filesys/base/sm-full.result ] && [ "$(echo $(head -n 1 ../filesys/base/sm-full.result) | grep "PASS")" ]; then
	sumBas=$(($sumBas + 2))
	sum=$(($sum + ($emphBas * 2)))
fi

if [ -f ../filesys/base/sm-random.result ] && [ "$(echo $(head -n 1 ../filesys/base/sm-random.result) | grep "PASS")" ]; then
	sumBas=$(($sumBas + 2))
	sum=$(($sum + ($emphBas * 2)))
fi

if [ -f ../filesys/base/sm-seq-block.result ] && [ "$(echo $(head -n 1 ../filesys/base/sm-seq-block.result) | grep "PASS")" ]; then
	sumBas=$(($sumBas + 2))
	sum=$(($sum + ($emphBas * 2)))
fi

if [ -f ../filesys/base/sm-seq-random.result ] && [ "$(echo $(head -n 1 ../filesys/base/sm-seq-random.result) | grep "PASS")" ]; then
	sumBas=$(($sumBas + 3))
	sum=$(($sum + ($emphBas * 3)))
fi

if [ -f ../filesys/base/syn-read.result ] && [ "$(echo $(head -n 1 ../filesys/base/syn-read.result) | grep "PASS")" ]; then
	sumBas=$(($sumBas + 4))
	sum=$(($sum + ($emphBas * 4)))
fi

if [ -f ../filesys/base/syn-remove.result ] && [ "$(echo $(head -n 1 ../filesys/base/syn-remove.result) | grep "PASS")" ]; then
	sumBas=$(($sumBas + 2))
	sum=$(($sum + ($emphBas * 2)))
fi

if [ -f ../filesys/base/syn-write.result ] && [ "$(echo $(head -n 1 ../filesys/base/syn-write.result) | grep "PASS")" ]; then
	sumBas=$(($sumBas + 4))
	sum=$(($sum + ($emphBas * 4)))
fi

if [ -f ../filesys/extended/dir-mkdir.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-mkdir.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 1))
	sum=$(($sum + ($emFunc * 1)))
fi

if [ -f ../filesys/extended/dir-mk-tree.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-mk-tree.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 3))
	sum=$(($sum + ($emFunc * 3)))
fi

if [ -f ../filesys/extended/dir-rmdir.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-rmdir.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 1))
	sum=$(($sum + ($emFunc * 1)))
fi

if [ -f ../filesys/extended/dir-rm-tree.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-rm-tree.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 3))
	sum=$(($sum + ($emFunc * 3)))
fi

if [ -f ../filesys/extended/dir-vine.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-vine.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 5))
	sum=$(($sum + ($emFunc * 5)))
fi

if [ -f ../filesys/extended/grow-create.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-create.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 1))
	sum=$(($sum + ($emFunc * 1)))
fi

if [ -f ../filesys/extended/grow-seq-sm.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-seq-sm.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 1))
	sum=$(($sum + ($emFunc * 1)))
fi

if [ -f ../filesys/extended/grow-seq-lg.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-seq-lg.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 3))
	sum=$(($sum + ($emFunc * 3)))
fi

if [ -f ../filesys/extended/grow-sparse.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-sparse.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 3))
	sum=$(($sum + ($emFunc * 3)))
fi

if [ -f ../filesys/extended/grow-two-files.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-two-files.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 3))
	sum=$(($sum + ($emFunc * 3)))
fi

if [ -f ../filesys/extended/grow-tell.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-tell.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 1))
	sum=$(($sum + ($emFunc * 1)))
fi

if [ -f ../filesys/extended/grow-file-size.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-file-size.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 1))
	sum=$(($sum + ($emFunc * 1)))
fi

if [ -f ../filesys/extended/grow-dir-lg.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-dir-lg.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 1))
	sum=$(($sum + ($emFunc * 1)))
fi
if [ -f ../filesys/extended/grow-root-sm.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-root-sm.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 1))
	sum=$(($sum + ($emFunc * 1)))
fi
if [ -f ../filesys/extended/grow-root-lg.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-root-lg.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 1))
	sum=$(($sum + ($emFunc * 1)))
fi

if [ -f ../filesys/extended/syn-rw.result ] && [ "$(echo $(head -n 1 ../filesys/extended/syn-rw.result) | grep "PASS")" ]; then
	sumef=$(($sumef + 5))
	sum=$(($sum + ($emFunc * 5)))
fi

if [ -f ../filesys/extended/dir-empty-name.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-empty-name.result) | grep "PASS")" ]; then
	sumer=$(($sumer + 1))
	sum=$(($sum + ($emRob * 1)))
fi

if [ -f ../filesys/extended/dir-open.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-open.result) | grep "PASS")" ]; then
	sumer=$(($sumer + 1))
	sum=$(($sum + ($emRob * 1)))
fi
if [ -f ../filesys/extended/dir-over-file.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-over-file.result) | grep "PASS")" ]; then
	sumer=$(($sumer + 1))
	sum=$(($sum + ($emRob * 1)))
fi

if [ -f ../filesys/extended/dir-under-file.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-under-file.result) | grep "PASS")" ]; then
	sumer=$(($sumer + 1))
	sum=$(($sum + ($emRob * 1)))
fi

if [ -f ../filesys/extended/dir-rm-cwd.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-rm-cwd.result) | grep "PASS")" ]; then
	sumer=$(($sumer + 3))
	sum=$(($sum + ($emRob * 3)))
fi

if [ -f ../filesys/extended/dir-rm-parent.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-rm-parent.result) | grep "PASS")" ]; then
	sumer=$(($sumer + 2))
	sum=$(($sum + ($emRob * 2)))
fi

if [ -f ../filesys/extended/dir-rm-root.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-rm-root.result) | grep "PASS")" ]; then
	sumer=$(($sumer + 1))
	sum=$(($sum + ($emRob * 1)))
fi

if [ -f ../filesys/extended/syn-rw-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/syn-rw-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/grow-two-files-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-two-files-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/grow-tell-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-tell-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/grow-sparse-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-sparse-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/grow-seq-sm-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-seq-sm-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/grow-seq-lg-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-seq-lg-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/grow-root-sm-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-root-sm-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/grow-root-lg-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-root-lg-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/grow-file-size-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-file-size-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/grow-dir-lg-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-dir-lg-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/grow-create-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/grow-create-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/dir-vine-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-vine-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/dir-under-file-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-under-file-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/dir-rmdir-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-rmdir-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/dir-rm-tree-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-rm-tree-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/dir-rm-root-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-rm-root-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/dir-rm-parent-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-rm-parent-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/dir-rm-cwd-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-rm-cwd-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/dir-over-file-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-over-file-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/dir-open-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-open-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/dir-mkdir-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-mkdir-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/dir-mk-tree-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-mk-tree-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

if [ -f ../filesys/extended/dir-empty-name-persistence.result ] && [ "$(echo $(head -n 1 ../filesys/extended/dir-empty-name-persistence.result) | grep "PASS")" ]; then
	sumep=$(($sumep + 1))
	sum=$(($sum + ($emPer * 1)))
fi

grade=$(calc $sum/100)
grade90=$(calc $(calc $grade*90)/32.65)

echo ""
echo "===> (3/3) Score Summary"
echo ""
echo ""
echo "========================================================"
echo "sum of functionality tests is: $sumFunc (108) emphasis: 5%"
echo "sum of robustness tests is: $sumRob (88)  emphasis: 5%"
echo "sum of base tests is: $sumBas (30) emphasis: 5%"
echo "sum of extended functionality is: $sumef (34) emphasis: 40%"
echo "sum of extended Robustness is: $sumer (10) emphasis: 20%"
echo "sum of extended Persistence is: $sumep (23) emphasis: 25%"
echo "Total sum is: $sum"
echo "Total grade: $grade"

if [ "$grade90" = "" ]; then
	grade90=0
fi

echo "Total grade out of 90: $grade90"
echo "======================================================="
echo ""

echo $grade90 >grade.txt

##########################################################################################
##########################################################################################

# Delete the temporary grading directory.
#rm -rf ~/cs230-grading &> /dev/null

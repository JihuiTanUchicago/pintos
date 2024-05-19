#!/bin/bash

# This grading script is intended to show students what would happen when TA
# grade p1 project.
#
# Do the following steps to run the script:
# - copy this script to src/threads/
#
# - change file attribute to allow script execution
#
#   chmod 700 p1.sh
#
# - run this script to start grading
#
#   ./p1.sh
#
# This script will download pintos-raw and extract it to ~/cs230-grading/pintos,
# copy specific files that you are allowed to modify there, and do the grading
# on ~/cs230-grading/pintos.

cd ../

rm -rf ~/cs230-grading &> /dev/null

wget http://people.cs.uchicago.edu/~ruidanli/cs230/pintos-raw.tgz -P ~/cs230-grading
tar -zxvf ~/cs230-grading/pintos-raw.tgz -C ~/cs230-grading/
rm ~/cs230-grading/pintos-raw.tgz
mv ~/cs230-grading/pintos-raw ~/cs230-grading/pintos

files=(threads/synch.c
       threads/synch.h
       threads/thread.c
       threads/thread.h
       devices/timer.c
       threads/fixed-point.h)

for file in ${files[@]}; do
    cp $file ~/cs230-grading/pintos/src/$file
done

if [ -f ~/Make.config ]
then
    cd ~/cs230-grading/pintos/src/
    # for the CSIL fix
    cp ~/Make.config .
    cp ~/pintos-gdb utils/
fi

cd ~/cs230-grading/pintos/src/threads/

make 2> /dev/null

cd build/

make check 

make check > check_output

calc() {
    echo "scale=4; $1" | bc ;exit
}

sum=0
sumAla=0
sumPri=0
sumMlf=0

indexChecker() {
    sum=0
    if [ "$1" -eq 0 ] || [ "$1" -eq 1 ] || [ "$1" -eq 2 ] || [ "$1" -eq 3 ] || [ "$1" -eq 24 ] 
    then sum=$(($sum+4));
    else if [ "$1" -eq 4 ] || [ "$1" -eq 5 ]
	 then sum=$(($sum+1));
	 else if [ "$1" -eq 6 ] || [ "$1" -eq 14 ] || [ "$1" -eq 13 ] || [ "$1" -eq 15 ] || [ "$1" -eq 16 ] || [ "$1" -eq 7 ] || [ "$1" -eq 8 ] || [ "$1" -eq 9 ] || [ "$1" -eq 10 ] || [ "$1" -eq 11 ] || [ "$1" -eq 12 ] || [ "$1" -eq 20 ] || [ "$1" -eq 23 ]
	      then sum=$(($sum+3));
	      else if [ "$1" -eq 17 ] || [ "$1" -eq 18 ] || [ "$1" -eq 19 ] || [ "$1" -eq 21 ] || [ "$1" -eq 22 ] || [ "$1" -eq 26 ]
		   then sum=$(($sum+5));
		   else if [ "$1" -eq 25 ]
			then sum=$(($sum+2));
			fi
		   fi
	      fi
	 fi
    fi

    if [ "$1" -ge 0 ] && [ "$1" -le 5 ]
    then sumAla=$(($sumAla+$sum)); 
    else if [ "$1" -ge 6 ] && [ "$1" -le 17 ]
	 then sumPri=$(($sumPri+$sum)); 
	 else if [ "$1" -ge 18 ] && [ "$1" -le 26 ]
	      then sumMlf=$(($sumMlf+$sum)); 
		   #sum=$(($sum*40));
	      fi
	 fi
    fi

}

i=0
while read line
do
    pass=$(echo $line | grep "pass");
    if [ -z "$pass" ]
    then

        sum=$sum;

    else            
	indexChecker $i

    fi

    i=$(($i+1));

done < "check_output"
rm check_output

alaGrade=$(calc $(calc $sumAla/18)*20);
priGrade=$(calc $(calc $sumPri/38)*40);
mlfGrade=$(calc $(calc $sumMlf/37)*40);
grade=$(calc $alaGrade+$priGrade);
grade90=$(calc $(calc $grade*90)/60);
grade100=$(calc $(calc $grade*100)/60);


echo ""
echo "sum of alarm tests is: $sumAla (18)     emphasis: 33.3%";
echo "sum of priority tests is: $sumPri (38)  emphasis: 66.7%";
echo "Total grade out of 100: $grade100";
echo "Total grade out of 90: $grade90";
echo ""
echo $grade90 > grade.txt

rm -rf ~/cs230-grading &> /dev/null

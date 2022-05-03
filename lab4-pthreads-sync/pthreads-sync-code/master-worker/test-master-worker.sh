#!/bin/bash
# script takes 4 arguments that are given to the master worker program

gcc -o master-worker master-worker.c -lpthread


for j in {1..1000}; do
	d=`python -c "import random; print(random.randint(1,1000))"`
	b=`python -c "import random; print(random.randint(1,1000))"`
	w=`python -c "import random; print(random.randint(1,1000))"`
	p=`python -c "import random; print(random.randint(1,1000))"`
	echo $d $b $w $p 
	for i in {1..100}; do
		echo "Test" $i;
		./master-worker $d $b $w $p > output;
		out=`awk -f check.awk MAX=$d output`;
		if [ "$out" != "OK. All test cases passed!" ]; then
			echo $out
			exit
		fi
	done;
done;

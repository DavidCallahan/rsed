#!/bin/bash
set -e 
(mkdir -p build && cd build && cmake ../rsed && make -j 8)
set +e

cd tests
RSED=../build/rsed
failed=0
export ENV1=1
export ENV2="ENV2 value"

runTest () {
    if [ -e "$base.in" ] 
    then
	input="$base.in"
    else
	input=/dev/null
    fi
    ARG1=x ARG2=y $RSED $test $OPT $DEBUG a < $input > $base.test-out
}


for test in test*.rsed
do
    base=`basename $test .rsed`
    echo $test 
    runTest
    if [ $? != 0 ]
    then
	echo "non zero exit"
	failed=1
	DEBUG="-dump -debug" runTest
	cat $base.test-out
	exit 1
    fi
    diff $base.test-out $base.out
    if [ $? != 0 ]
    then
	failed=1
	DEBUG="-dump -debug" runTest
	cat $base.test-out
	exit 1
    fi
    rm $base.test-out
done

echo "test1 from stdin"
base=test1
cat $base.rsed | $RSED -script_in -input=$base.in > $base.test-out
if [ $? != 0 ]
then
    echo "non zero exit"
    failed=1
fi
diff $base.test-out $base.out
if [ $? != 0 ]
then
    failed=1
fi
rm $base.test-out



set +e
for test in err*.rsed
do
    base=`basename $test .rsed`
    echo $test
    $RSED $test < $base.in >& $base.test-out
    if [ $? == 0 ]
    then
	echo "zero exit"
	failed=1
    fi
    diff $base.test-out $base.out
    if [ $? != 0 ]
    then
	failed=1
    fi
    rm $base.test-out
done



if [ $failed != 0 ]
then
    echo "test failures"
fi
exit $failed


#!/bin/bash
# set -e 
(mkdir -p build && cd build && cmake ../rsed && make -j 8)

cd tests
RSED=../build/rsed
failed=0
export ENV1=1
export ENV2="ENV2 value"

for test in test*.rsed
do
    base=`basename $test .rsed`
    echo $test
    $RSED $test < $base.in > $base.test-out
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
done
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


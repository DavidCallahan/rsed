#!/bin/bash
UNAME=`uname`
if [ "$UNAME" == "Darwin" ]
then 
    STAT='stat -f %z'
else
    STAT='stat --printf=%s'
fi
set -e 
(mkdir -p build && cd build && cmake ../rsed && make -j 8)
set +e

cd tests
RSED=`pwd`/../build/rsed
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
    if [ -z "INPUT_ARG" ]
    then
	ARG1=x ARG2=y $RSED $test $OPT $DEBUG a  < $input > $base.test-out 2> $base.test-err
    else
	ARG1=x ARG2=y $RSED $test $OPT $DEBUG a -input=$input > $base.test-out 2> $base.test-err
    fi
}

runPass () {
    echo $test $OPT 
    local keep=0
    if  [ -e "$base.env" ] 
    then
        (source "$base.env" ; runTest)
    else
       runTest
    fi
    local rc=$?
    local err=0
    if [ -e "$base.err" ]
    then
        err=`$STAT $base.err`
    fi
    if [ $err == 0 ] 
    then
        if [ $rc != 0 ]
        then
	    echo "non zero exit" $test $OPT
	    failed=1
            keep=1
        fi
    else
        if [ $rc == 0 ]
        then
            echo "unexpected zero exit" $test $OPT
            failed=1
            keep=1
        fi
        diff $base.test-err $base.err > $base.err-diff
        if [ $? != 0 ]
        then
	    failed=1
            keep=1
            head -n 5 < $base.err-diff
	    echo "output error differences" $test $OPT
        fi
    fi
    diff $base.test-out $base.out > $base.diff
    if [ $? != 0 ]
    then
        failed=1
        keep=1
        head -n 5 < $base.diff
	echo "output differences" $test $OPT
    fi
    if [ $keep == 0 ]
    then
       rm -f $base.test-out $base.test-err $base.diff $base.err-diff
    fi
}

for test in test*.rsed
do
    base=`basename $test .rsed`
    runPass
    OPT=-optimize runPass
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

for test in test{25,38}.rsed
do
    base=`basename $test .rsed`
    mkdir -p  save
    OPT=-save_prefix=save/save-$base- runPass
    cp $test $base.out save
    pushd save >& /dev/null
    OPT=-replay_prefix=save-$base- runPass
    popd  >&/dev/null
    rm -rf save
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

set -e
rm -rf ../samples
CAPTURE_RSED_BINARY="$RSED" ./capture-sample.sh > /dev/null
cd ../samples
for test in ctest*.rsed
do
    base=`basename $test .rsed`
    OPT="-replay_prefix=$base-save" runPass
    OPT="-optimize -replay_prefix=$base-save" runPass
done
set +e

if [ -e ../captured ]
then
cd ../captured
for test in ctest*.rsed
do
    base=`basename $test .rsed`
    OPT="-replay_prefix=$base-save" runPass
    OPT="-optimize -replay_prefix=$base-save" runPass
done
fi


if [ $failed != 0 ]
then
    echo "test failures"
fi
exit $failed


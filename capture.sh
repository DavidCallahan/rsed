#!/bin/bash 
C=~dcallahan/projects/rsed/captured
RSED=${CAPTURE_RSED_BINARY-~dcallahan/projects/rsed/install/rsed.bin}

SCRIPT_IN=0
INPUT="<&0"
for i in "$@"
do
case $i in
    -script_in)
            SCRIPT_IN=1
            ;;
    -input=*)
            INPUT="$i"
            ;;
    *)
            SOURCE="$i"
            ;;
esac
done
if [ -z "$RSED_CAPTURE_DIR" ]
then
RSED_CAPTURE_DIR="$C"
fi
mkdir -p $RSED_CAPTURE_DIR

TEST=1
while [ -e "$RSED_CAPTURE_DIR/ctest$TEST.rsed" ]
do
    let TEST=TEST+1
done
STEM="$RSED_CAPTURE_DIR/ctest$TEST"
SCRIPT="$STEM.rsed"
OUT="$STEM.out"
ERR="$STEM.err"
SAVE="-save_prefix=$STEM-save"
ENV="-env_save=$STEM.env"

if [ $SCRIPT_IN == 0 ]
then
    cp "$SOURCE"  $SCRIPT
else
    cat <&0 > $SCRIPT
fi

if [ -z "$INPUT" ]
then
$RSED $ENV $SAVE $SCRIPT <&0 > $OUT 2> $ERR
else
$RSED $ENV $SAVE $SCRIPT $INPUT > $OUT 2> $ERR
fi
if [ $? != 0 ]
then
    echo "Error in $SCRIPT" >&2
    cat $ERR >&2
    exit 1
fi
cat $OUT


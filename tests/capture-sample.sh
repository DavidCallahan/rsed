#!/bin/bash
export CAPTURE_RSED_BINARY="../build/rsed"
export RSED_CAPTURE_DIR="../samples"
mkdir -p ../samples
../capture.sh sample.rsed < sample.stdin
../capture.sh sample.rsed -input=sample.stdin
../capture.sh -script_in -input=sample.stdin < sample.rsed


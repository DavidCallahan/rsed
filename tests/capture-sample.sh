#!/bin/bash
export RSED_CAPTURE_DIR="../samples"
../capture.sh sample.rsed < sample.stdin
../capture.sh sample.rsed -input=sample.stdin
../capture.sh -script_in -input=sample.stdin < sample.rsed


#!/bin/bash

dir=${1:-.}

cd $dir
shopt -s nullglob
while true; do
  for filename in stream*.txt; do
    mv $filename processing_$filename
    cat processing_$filename
  done
  sleep .5
done 
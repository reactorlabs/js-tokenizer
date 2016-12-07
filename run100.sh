#!/bin/bash
for i in {0..99}
do
    ./build/tokenizer 100 $i db100
done

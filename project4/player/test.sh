#!/bin/bash

touch tmp_move.txt
cp input_for_test.txt tmp_input.txt

COUNTER=0

while [  $COUNTER -lt 10 ]; do
  (./leiserchess tmp_input.txt | tail -n 1 | tail -c +5) > tmp_move.txt
  (cat tmp_input.txt | head -n -2) > ttmp
  (cat ttmp) > tmp_input.txt
  rm ttmp
  (cat tmp_move.txt) >> tmp_input.txt
  (echo go depth 4) >> tmp_input.txt
  (echo quit) >> tmp_input.txt
  
  let COUNTER=COUNTER+1
done

cat tmp_input.txt
rm tmp_input.txt
rm tmp_move.txt

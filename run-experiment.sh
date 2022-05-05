#!/bin/sh
set -eu
cc -O1 main.c
(
	echo -e "N\tHASH\tLINEAR\tBSEARCH"
	for n in $(seq 1 500)
	do
		./a.out "$n" 1000000
	done
) | tee results.tsv
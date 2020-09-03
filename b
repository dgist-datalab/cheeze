#!/bin/bash

#gcc -O2 -g -Wall -fsanitize=address user.c -lz
gcc -O3 -s -Wall user.c -lz


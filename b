#!/bin/bash

#gcc -O2 -g -Wall -fsanitize=address -static-libasan user.c
gcc -O3 -s -Wall user.c

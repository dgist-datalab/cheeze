#!/bin/bash

gcc -O2 -g -Wall -fsanitize=address -static-libasan uring.c -luring
#gcc -O3 -s -Wall user.c

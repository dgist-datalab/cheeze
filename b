#!/bin/bash

gcc -O2 -g -Wall -fsanitize=address user.c

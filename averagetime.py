#!/usr/bin/python

import os
import subprocess

N = 100;
averages = []
for i in range(N):
	out = subprocess.Popen(["build/compiler", "-S", "king_ge.tig"], stdout=subprocess.PIPE).communicate()[0]
	lines = [s.strip().split(':') for s in out.split("\n") if s.find(':') > 0]
	if (len(averages) == 0):
		averages = [0]*len(lines)
	else:
		if len(averages) != len(lines):
			print "Suxx!"
			exit(1)
	for i in range(len(averages)):
		averages[i] += int(lines[i][1].strip())
		
for i in range(len(lines)):
	print lines[i][0], averages[i]/N

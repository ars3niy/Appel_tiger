#!/usr/bin/python

import os
import subprocess
import sys

if len(sys.argv) < 2:
    executable = subprocess.Popen(["find", "..", "-type", "f", "-name", "compiler"], stdout=subprocess.PIPE).communicate()[0].strip()
else:
    executable = sys.argv[1]

open(executable).close()

tests = [ \
	["denest.tig", True], \
	["merge.tig", True], \
	["nest.tig", True], \
	["primes.tig", True], \
	["test10.tig", False], \
	["test11.tig", False], \
	["test12.tig", True], \
	["test13.tig", False], \
	["test14.tig", False], \
	["test15.tig", False], \
	["test16.tig", False], \
	["test17.tig", False], \
	["test18.tig", False], \
	["test19.tig", False], \
	["test1.tig", True], \
	["test20.tig", False], \
	["test21.tig", False], \
	["test22.tig", False], \
	["test23.tig", False], \
	["test24.tig", False], \
	["test25.tig", False], \
	["test26.tig", False], \
	["test27.tig", True], \
	["test28.tig", False], \
	["test29.tig", False], \
	["test2.tig", True], \
	["test30.tig", True], \
	["test31.tig", False], \
	["test32.tig", False], \
	["test33.tig", False], \
	["test34.tig", False], \
	["test35.tig", False], \
	["test36.tig", False], \
	["test37.tig", True], \
	["test38.tig", False], \
	["test39.tig", False], \
	["test3.tig", True], \
	["test40.tig", False], \
	["test41.tig", True], \
	["test42.tig", True], \
	["test43.tig", False], \
	["test44.tig", True], \
	["test45.tig", False], \
	["test46.tig", True], \
	["test47.tig", True], \
	["test48.tig", True], \
	["test49.tig", False], \
	["test4.tig", True], \
	["test5.tig", True], \
	["test6.tig", True], \
	["test7.tig", True], \
	["test8.tig", True], \
	["test9.tig", False], \
]

expected_outputs = [ \
	["nest.bin", "F\n"], \
	["denest.bin", "F\n"], \
	["primes.bin", " 2 3 5 7 11 13 17 19 23 29 31 37 41 43 47 53 59 61 67 71 73 79 83 89 97\n"], \
	["echo 11 33 55 777 + 22 44 66 888 + | ./merge.bin", "11 22 33 44 55 66 777 888 \n"], \
]

def add(name, output):
	global tests
	global expected_outputs
	tests += [[name+".tig", True]]
	expected_outputs += [[name+".bin", output]]

add("recursion", open("recursion.out", "r").read())
add("nest2", open("nest2.out", "r").read())
add("emptyrecursion", "")
add("queens", open("queens.out", "r").read())

os.system("rm -f *.bin test.log")

ok = True
for test in tests:
	open("test.log", "a").write("compiling " + test[0] + "\n")
	ret = os.system(executable + " -o " + test[0].replace(".tig", ".bin") + " " + test[0] + ">>test.log 2>>test.log")
	ret = ret/256
	if ret == 0:
		if not test[1]:
			print "Compiling test %s succeeded instead of failing" % (test[0])
			ok = False
	elif ret <= 2:
		if test[1]:
			print "Compiling test %s failed instead of succeeding" % (test[0])
			ok = False
	else:
		print "Test %s failed fatally" % (test[0])
		ok = False

for test in expected_outputs:
	cmd = test[0] + " >&test.out"
	if not cmd.startswith("echo "):
	    cmd = "./" + cmd
	ret = os.system(cmd )
	output = open("test.out").read()
	if (len(test) == 2) and (ret != 0):
		ok = False
		print "Test %s got runtime errer" % (test[0])
	open("test.log", "a").write("running " + test[0] + "\n" + output + "\n")
	if output != test[1]:
		ok = False
		print "Test %s got wrong answer" % (test[0])

multitests = ["neerc2015/king", "neerc2015/landscape"]

for d in multitests:
	source = [s for s in os.listdir(d) if s.endswith(".tig")][0]
	C = reduce(lambda x, y: x + " " + d+"/"+y, [s for s in os.listdir(d) if s.endswith(".c")], "")
	open("test.log", "a").write("compiling " + source + " " + C + "\n")
	ret = os.system(executable + " -o program.bin " + d + "/" + source + " " + C + " >>test.log 2>>test.log")
	if ret != 0:
		print "Failed to compile " + d + "/" + source
		ok = False
	for f in os.listdir(d + "/tests"):
		if not f.endswith(".a"):
			open("test.log", "a").write(d + "/tests/" + f + "\n")
			ret = os.system("./program.bin <" + d + "/tests/" + f + " >test.out 2>>test.log")
			open("test.log", "a").write(open("test.out").read() + "\n")
			if ret != 0:
				print "Runtime error for " + d + "/tests/" + f
				ok = False
			if open("test.out").read() != open(d + "/tests/" + f + ".a").read():
				print "Wrong answer for " + d + "/tests/" + f
				ok = False

os.system("rm -f *.bin")
os.unlink("test.out")

if not ok:
	exit(1)

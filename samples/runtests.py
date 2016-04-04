#!/usr/bin/python

import os

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
	["nest.elf", "F\n"], \
	["denest.elf", "F\n"], \
	["primes.elf", " 2 3 4 5 6 7 8 10 11 13 14 17 19 22 23 26 29 31\n"], \
	["echo 11 33 55 777 + 22 44 66 888 + | ./merge.elf", "11 22 33 44 55 66 777 888 \n"], \
]

def add(name, output):
	global tests
	global expected_outputs
	tests += [[name+".tig", True]]
	expected_outputs += [[name+".elf", output]]

add("recursion", open("recursion.out", "r").read())
add("emptyrecursion", "")
add("queens", open("queens.out", "r").read())

os.system("rm -f *.elf test.log")

ok = True
for test in tests:
	open("test.log", "a").write("compiling " + test[0] + "\n")
	ret = os.system("../build/compiler " + test[0] + ">>test.log 2>>test.log")
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

os.system("rm -f *.elf")
os.unlink("test.out")

if not ok:
	exit(1)

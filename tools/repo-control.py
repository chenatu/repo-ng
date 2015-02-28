#!/usr/bin/env python

import sys, subprocess, getopt, time, threading, fileinput

def main(argv):
	for index in range(len(argv)):
		if argv[index] == '-f':
			function = argv[index + 1]
			break;
	argv.pop(index + 1)
	argv.pop(index)

	if function == '1':
		ndnRepo242(argv)
	if function == '2':
		ndnMultiReqClient(argv)
	if function == '3':
		ndnRepo43234(argv)
	if function == '4':
		ndnCalRes(argv)

def ndnRepo242(argv):
	p1 = subprocess.Popen("ndn-repo-ng")
	p2 = subprocess.Popen("ndn-repo-ng")
	time.sleep(30)
	p3 = subprocess.Popen("ndn-repo-ng")
	p4 = subprocess.Popen("ndn-repo-ng")
	time.sleep(30)
	p3.terminate();
	p4.terminate();
	time.sleep(30)
	p1.terminate()
	p2.terminate()

def ndnMultiReqClient(argv):
	n = 1
	for index in range(len(argv)):
		if argv[index] == '-n':
			n = int(argv[index + 1])
			argv.pop(index + 1)
			argv.pop(index)
			break;
	# d: duration
	for index in range(len(argv)):
		if argv[index] == '-d':
			d = int(argv[index + 1])
			argv.pop(index)
			argv.pop(index + 1)
			break;


	for oIndex in range(len(argv)):
		if argv[oIndex] == '-o':
			filename = argv[oIndex + 1]
			break;
	argv[0] = 'ndnreq'
	process = []
	for i in range(n):
		if 'filename' in locals():
			argv[oIndex + 1] = filename + str(i)
		process.append(subprocess.Popen(argv))
	if 'd' in locals():
		time.sleep(d)
		for p in process:
			p.terminate()

def ndnRepo43234(argv):
	p1 = subprocess.Popen("ndn-repo-ng")
	p2 = subprocess.Popen("ndn-repo-ng")
	p3 = subprocess.Popen("ndn-repo-ng")
	p4 = subprocess.Popen("ndn-repo-ng")
	time.sleep(10)
	p1.terminate();
	time.sleep(10)
	p2.terminate();
	time.sleep(10)
	p1 = subprocess.Popen("ndn-repo-ng")
	time.sleep(10)
	p2 = subprocess.Popen("ndn-repo-ng")
	time.sleep(10)
	p1.terminate()
	p2.terminate()
	p3.terminate()
	p4.terminate()

def ndnCalRes(argv):
	for index in range(len(argv)):
		if argv[index] == '-o':
			filename = argv[index + 1]
			break;

	for index in range(len(argv)):
		if argv[index] == '-n':
			n = int(argv[index + 1])
			break;
	if ('filename' not in locals()) or ('n' not in locals()):
		return

	files = []
	for i in range(n):
		files.append(open(filename + str(i)))
	min_lines = 0;
	for i in range(n):
		num_lines = sum(1 for line in files[i])
		if (min_lines == 0) or (num_lines < min_lines):
			min_lines = num_lines
	logFile = open(filename, 'w')
	speeds = [0] * min_lines
	print('min_lines ' + str(min_lines))
	for file_object in files:
		file_object.seek(0)
		for i in range(min_lines):
			line = file_object.readline()
			speeds[i] = speeds[i] + int((line.split())[4])

		file_object.close()
	for i in range(len(speeds)):
		logFile.write(str(i*1000) + ' ' + str(speeds[i]) + ' ' + '\n')
	logFile.close()


def usage():
	print("repo-control.py")

if __name__ == '__main__':
	main(sys.argv)
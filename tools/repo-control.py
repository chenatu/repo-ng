#!/usr/bin/env python

import sys, subprocess, getopt, time, threading

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

def usage():
	print("repo-control.py")

if __name__ == '__main__':
	main(sys.argv)
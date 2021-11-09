#!/usr/bin/env python2

import socket
import sys
import time
from datetime import datetime
port = 12345

try:
	s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
except socket.error, msg:
	print 'Failed to create socket. Error code: ' + str(msg[0]) + ' , Error message : ' + msg[1]
	sys.exit();
print 'Socket Created'
try:
    s.bind(('', port)) 
except Exception, e:
    alert("Something's wrong with %s. Exception type is %s" % (('127.0.0.1', port), e))

print 'Socket Connected'
c = 1

def getline(s):
	data, addr = s.recvfrom(4096)
	return data

elapsed = 0
delta = 0
prev1 = 0
basetime = time.time()
while True:
	tm = getline(s)
	linetime = time.time()
	elapsed = linetime-basetime
	delta = elapsed-prev1
	sys.stdout.write("[%s %2.6f] " %  (datetime.now().strftime("%F %T.%f"), delta))
	prev1 = elapsed
	newline = 0
	sys.stdout.write(tm)
	sys.stdout.flush()
s.close()


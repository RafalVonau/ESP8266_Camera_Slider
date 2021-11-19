#!/usr/bin/env python2

import socket	#for sockets
import sys	#for exit
import time	#for exit
import argparse
from datetime import datetime


parser = argparse.ArgumentParser(description='Process some arguments.')
parser.add_argument('-l', '--local', action='store_true')
parser.add_argument('-i','--input' ,help='input file')
args = parser.parse_args()

def readline(s):
	msg=''
	chunk='a'
	while chunk != '\r':
		chunk=s.recv(1)
		if chunk == '':
			return msg
		if (chunk != '\n') and (chunk != '\r'):
			msg=msg+chunk
	return msg

def writeline(s, v):
	s.sendall(v);

def sendCommand(s, x):
	if (x != ""):
		print('Send: '+x)
		try :
			writeline(s, x + "\r")
		except socket.error:
			print('Send failed')
			sys.exit()
		reply = readline(s)
		print reply


host = 'slider.local';
port = 2500;
inputfile = "-"

if args.local:
	host="127.0.0.1"

if args.input:
	inputfile = args.input
  
print("Host: "+host);

old_out = sys.stdout

class St_ampe_dOut:
	nl = True
	def write(self, x):
		if x == '\n':
			old_out.write(x)
			self.nl = True
		elif self.nl:
			old_out.write('(%s): %s' % (str(datetime.now().strftime("%H:%M:%S.%f"))[:-3], x))
			self.nl = False
		else:
			old_out.write(x)

sys.stdout = St_ampe_dOut()



#create an INET, STREAMing socket
try:
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
except socket.error:
	print('Failed to create socket')
	sys.exit()
print('Socket Created')
try:
	remote_ip = socket.gethostbyname( host )
except socket.gaierror:
	#could not resolve
	print('Hostname could not be resolved. Exiting')
	sys.exit()
#Connect to remote server
s.connect((remote_ip , port))
s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
s.setblocking(1)
print('Socket Connected to ' + host + ' on ip ' + remote_ip)

file1 = open(inputfile, 'r')
Lines = file1.readlines()

count = 0
# Strips the newline character
for line in Lines:
	count += 1
	sendCommand(s, line.strip())
sendCommand(s,"XX\r")
s.close()
time.sleep(1.0)
file1.close()


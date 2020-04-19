#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import socket

def main(args):
	sock = socket.socket()
	in_i = 0
	crc = 0
	sz = 0
	buf = bytearray()
	sock.bind(('127.0.0.1', 3301))
	sock.listen()
	print('listening on 127.0.0.1:3301')
	(conn, address) = sock.accept()
	while True:
		b = conn.recv(1).hex().upper()
		if(b != ''):
			print(b)
	return 0

if __name__ == '__main__':
	from sys import argv, exit
	exit(main(argv))
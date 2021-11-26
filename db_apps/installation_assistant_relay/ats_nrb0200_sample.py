#!/usr/bin/env python

import socket
import time

class Command:
    def __init__(self, name, duration):
        self.name = name
        self.duration = duration

    def get_name(self):
        return self.name

    def get_duration(self):
        return self.duration

basic_test_commands = [
    Command('enable test', 0),
    Command('show serial', 0),
    Command('show mac', 0),
    Command('show version hw', 0),
    Command('show version sw', 0),
    Command('show ssid', 0),
    Command('show wpa', 0),
    Command('disable test', 0),
    Command('reset', 0),
]

class Executor:
    def __init__(self, addr, port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.addr = (addr, port)

    def run(self, command):
        self.sock.sendto(command, self.addr)
        data, server = self.sock.recvfrom(512)
        return data


if __name__ == '__main__':
    executor = Executor('localhost', 15800)
    for command in basic_test_commands:
        print(executor.run(command.get_name()))
        time.sleep(command.get_duration())

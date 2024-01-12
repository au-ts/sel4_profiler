import socket
import sys
import threading
import os
import struct
import pmu_sample_pb2
import json
from google.protobuf.json_format import MessageToJson
import argparse

# To track if we have sent a message, to ensure json format is correct
first_msg = 0
stop_recv = 0

class ProfilerClient:
    """
    Class encpasulating the logic to control seL4 profiler over network.
    We 
    will also recieve samples on this client from the seL4 profiler.
    """

    def __init__(self, hostname, port):
        self.hostname = hostname
        self.port = port
        self.socket = None
        self.recv_thread = None
        self.f = None

    def send_command(self, cmd):
        """
        Send a command to the client over telnet.

        Params:
            cmd: Command to send
        """
        print("[send_command] : " + cmd)
        self.socket.send((cmd + "\n").encode('utf-8'))

    def get_mappings(self, f):
        """
        Get the mappings from pid to ELF names
        """
        self.send_command("MAPPINGS")
        f.write("{\n")
        f.write("\"pd_mappings\": {\n")
        data = self.socket.recv(4096).decode()
        print(str(data))
        lines = str(data).split("\n")
        for line in lines:
            content = line.split(":")
            f.write(f"\"{content[0]}\": {content[1]},\n")
        f.write("}")
    def connect(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        try:
            self.socket.connect((self.hostname, self.port))
        except OSError:
            sys.stderr.write(f"Can't connect to {self.hostname} on port {repr(self.port)}"
                        + " ... invalid host or port?\n")
            sys.exit(1)
        
        
        data = self.socket.recv(1024).decode()
        print(str(data))


    def recv_samples_thread(self):
        while True:
            # receive data stream. it won't accept data packet greater than 1024 bytes
            # global stop_recv
            try:
                # we first want to get the len from the socket, then read the socket
                # for the size len
                raw_len = self.socket.recv(2).decode()
                len = int(raw_len)
                # int_len = int.from_bytes(raw_len, 'little')
                # len = socket.ntohl(int_len)
                # dataToRead = struct.unpack("L", self.socket.recv(8))[0]    
                # print(f"This is int_len: {int_len}")
                # print(f"This is len: {len}")
                global first_msg
                if (first_msg == 1):
                    self.f.write(",")
                else:
                    first_msg = 1

                data = self.socket.recv(len)
                # Convert this data using protobuf
                sample = pmu_sample_pb2.pmu_sample()

                sample.ParseFromString(data)
                self.f.write(MessageToJson(sample))

            except socket.error:
                global stop_recv
                if stop_recv:
                    break
                else:
                    print("No data on socket")
                    # Dummy command to refresh socket
                    self.send_command("REFRESH")

    def recv_samples(self, f):
        # Start the samples section of the json
        self.socket.settimeout(2)
        global stop_recv
        stop_recv = 0
        self.recv_thread = threading.Thread(target=self.recv_samples_thread, args=())
        self.recv_thread.daemon = True
        self.f = open("samples.json", "a")
        self.recv_thread.start()

    def stop_samples(self):
        global stop_recv
        stop_recv = 1
        # time.sleep(10)
        self.recv_thread.join()
        self.f.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Set IP address and port number')
    parser.add_argument('-ip', dest='client_ip', required='true', help='IP address of the target system')
    parser.add_argument('-port', dest='client_port', required='true', help='Port of the target system',
                        type=int)
    args = parser.parse_args()  

    # Intially, we want to create client object
    profClient = ProfilerClient(args.client_ip, args.client_port)
    print("seL4 Profiler Network Client. Please enter command:")

    f = None

    # Create the recv thread. Start on START command, exit on STOP command.

    # Sit in a loop and wait for user commands
    while True:
        user_input = input("> ").upper()
        if user_input == "CONNECT":
            f = open("samples.json", "w")
            profClient.connect()
            profClient.get_mappings(f)
            f.write(",\n\"samples\": [\n")
            f.close()

        elif user_input == "START":
            f = open("samples.json", "a")
            profClient.send_command("START")
            # TODO: Start this in a seperate thread
            # recvThread = threading.Thread(target=profClient.recv_samples, args=(f))

            profClient.recv_samples(f)
            # recvThread.start()

        elif user_input == "STOP":
            # Stop the recv thread and close file descriptor
            profClient.send_command("STOP")
            profClient.stop_samples()

        elif user_input == "EXIT":
            profClient.send_command("STOP")   
            profClient.stop_samples()
            f = open("samples.json", "a")
            f.write("]\n}\n")
            f.close()
            os._exit(1)
        
        else: 
            print("These are the following valid commands (case-agnostic):\n")
            print("\t1. \"CONNECT\" - This will attempt to connect to the supplied IP address and port. Connect will add the ")
            print("\t2. \"START\" - This will command the seL4 profiler to start measurements and construct samples.json file.")
            print("\t3. \"STOP\" - This will command the seL4 profiler to stop measurements.")
            print("\t4. \"EXIT\" - This will command the seL4 profiler to stop measurements, close the json file and exit this program")








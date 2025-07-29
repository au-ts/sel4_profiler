import socket
import sys
import threading
import os
import struct
import json
import argparse

sys.path.append("../protobuf/python")
import pmu_sample_pb2
from google.protobuf.json_format import MessageToJson

# To track if we have sent a message, to ensure json format is correct
first_msg = 0
stop_recv = 0

class ProfilerClient:
    """
    Class encpasulating the logic to control seL4 profiler over network.
    We
    will also recieve samples on this client from the seL4 profiler.
    """

    def __init__(self, output, hostname, port):
        self.output = open(output, "w")
        self.hostname = hostname
        self.port = port
        self.socket = None
        self.recv_thread = None

    def send_command(self, cmd):
        """
        Send a command to the client over telnet.

        Params:
            cmd: Command to send
        """
        self.socket.send((cmd + "\n").encode('utf-8'))

    def get_mappings(self):
        """
        Get the mappings from pid to ELF names
        """
        self.send_command("MAPPINGS")
        self.output.write("{\n")
        self.output.write("\"elf_tcb_mappings\": {\n")
        data = self.socket.recv(4096).decode()
        mappings = str(data).rstrip()
        print(mappings)
        lines = mappings.split("\n")
        if (len(lines) == 0):
            print("NC|ERR: No mappings provided. Please ensure the correct mappings are placed within the elf_tcb_mappings field.")
        for i in range(0, len(lines)):
            content = lines[i].split(":")
            if (len(content) != 2):
                print("NC|ERR: Bad mapping string format. Please check outputted samples.json file.")
            else:
                self.output.write(f"\"{content[0]}\": {content[1]}")
                if (i == len(lines) - 1):
                    self.output.write("\n")
                else:
                    self.output.write(",\n")
        self.output.write("}")
    def connect(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
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
                global first_msg
                if (first_msg == 1):
                    self.output.write(",")
                else:
                    first_msg = 1

                data = self.socket.recv(len)
                # Convert this data using protobuf
                sample = pmu_sample_pb2.pmu_sample()

                sample.ParseFromString(data)
                self.output.write(MessageToJson(sample, including_default_value_fields=True))

            except socket.error:
                global stop_recv
                if stop_recv == 1:
                    # If we have been given a command to stop, refresh socket to 
                    # get any remaining buffers from the profiler before terminating
                    # this thread.
                    stop_recv = 2
                    self.send_command("REFRESH")
                elif stop_recv == 2:
                    break
                else:
                    # Dummy command to refresh socket
                    self.send_command("REFRESH")

    def recv_samples(self):
        # Start the samples section of the json
        self.socket.settimeout(2)
        global stop_recv
        stop_recv = 0
        self.recv_thread = threading.Thread(target=self.recv_samples_thread, args=())
        self.recv_thread.daemon = True
        self.recv_thread.start()

    def stop_samples(self):
        global stop_recv
        stop_recv = 1
        # Check that a thread has actually been created
        if self.recv_thread is not None:
            self.recv_thread.join()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Set IP address and port number')
    parser.add_argument('--ip', dest='client_ip', required='true', help='IP address of the target system')
    parser.add_argument('--port', dest='client_port', required='true', help='Port of the target system',
                        type=int)
    parser.add_argument('--output', dest='output', help='File to output sample data to',
                        default="samples.json")
    args = parser.parse_args()

    # Intially, we want to create client object
    client = ProfilerClient(args.output, args.client_ip, args.client_port)
    print("netconn. Please enter command:")

    # Create the recv thread. Start on START command, exit on STOP command.

    # Sit in a loop and wait for user commands
    while True:
        user_input = input("> ").upper()
        if user_input == "CONNECT":
            client.connect()
            client.get_mappings()
            client.output.write(",\n\"samples\": [\n")

        elif user_input == "START":
            if client.socket is not None:
                client.send_command("START")
                client.recv_samples()
                print("Started profiling!")
            else:
                print("ERROR: attempted START command before CONNECT command called")

        elif user_input == "STOP":
            if client.socket is not None:
                # Stop the recv thread and close file descriptor
                client.send_command("STOP")
                client.send_command("REFRESH")
                client.stop_samples()
                print("Stopped profiling!")
            else:
                print("ERROR: attempted STOP command before CONNECT command called")

        elif user_input == "EXIT" or user_input == "q":
            if client.socket is not None:
                client.send_command("STOP")
                client.send_command("REFRESH")
                client.stop_samples()
                client.output.write("]\n}\n")
                client.output.close()
                print("Finished profiling, exiting netconn tool")
            exit(0)

        else:
            print("These are the following valid commands (case-agnostic):\n")
            print("\t1. \"CONNECT\" - This will attempt to connect to the supplied IP address and port. Connect will add the ")
            print("\t2. \"START\" - This will command the seL4 profiler to start measurements and construct samples output file.")
            print("\t3. \"STOP\" - This will command the seL4 profiler to stop measurements.")
            print("\t4. \"EXIT\" - This will command the seL4 profiler to stop measurements, close the samples output file and exit this program")

# simulate serial management message generation
# for testing different type of messages

import os
#import select.select as select
import struct
import time

FIFO_M_TO_S =  "/tmp/fifo_mgmt_spm"
FIFO_S_TO_M = "/tmp/fifo_spm_mgmt"

REQUEST = 0
RESPONSE = 1
INDICATION = 2

HELLO_MESSAGE = 0
KEEP_ALIVE_MESSAGE = 1
LED_CTRL_MESSAGE = 2
BATTERY_STATUS_MESSAGE = 3
KEY_STATES_MESSAGE = 4
CHANGE_BAUDRATE_MESSAGE = 5
NEW_BAUDRATE_MESSAGE = 6
AUTHENTICATION_MESSAGE =7

HELLO_RESPONSE = 0
SUPPORT_BAUD_RATE = 5
LISTING_TCP_PORT = 6
DEVICE_NAME = 2

LED_CTRL_TLV_CTRL = 1

try:
    os.mkfifo (FIFO_M_TO_S, mode=0o666)
except OSError:
    pass

try:
    os.mkfifo (FIFO_S_TO_M, mode=0o666)
except OSError:
    pass

# swap sequence with real management process to avoid dead lock
fd_fifo_out = os.open(FIFO_S_TO_M, os.O_WRONLY)
fd_fifo_in = os.open(FIFO_M_TO_S, os.O_RDONLY)


#While True:
#    checkMessageFromFifoIn()
#    decodeMessage()
def parseMessage(message, length):
    "prase service message to different tlvs"
    mgm_msg = {}
    unpack_format_str = "!B{:d}s".format(length-1)
    #print(unpack_format_str)
    msg_type, tlvs = struct.unpack(unpack_format_str, message)

    mgm_msg['type'] = msg_type & 0x03
    mgm_msg['id'] = (msg_type >> 2) & 0x3f
    print("type: {} id: {}".format(mgm_msg['type'], mgm_msg['id']))
    if mgm_msg['id'] == KEEP_ALIVE_MESSAGE and mgm_msg['type'] == REQUEST:
        mgm_msg['tlvs'] = ''
    else:
        mgm_msg['tlvs'] = parseTlv(tlvs, mgm_msg["id"])
    return mgm_msg

def parseTlv(tlvs, id):
    i = 0
    msg = [{}]
    pdus = tlvs
    while len(pdus) >0:
        unpack_format_str = "!1B1B{:d}s".format(len(pdus)-2)
        print(unpack_format_str, pdus)
        tlv_type, tlv_len, tmpstr= struct.unpack(unpack_format_str,pdus)
        print(tlv_type, tlv_len, tmpstr)
        msg[i]["type"] = tlv_type
        msg[i]["len"] = tlv_len
        if id==HELLO_MESSAGE:
            if msg[i]["type"] == SUPPORT_BAUD_RATE:
                unpack_format_str="!{:d}I{:d}s".format(int(tlv_len/4),len(tmpstr)-tlv_len)
            elif msg[i]["type"] == LISTING_TCP_PORT:
                unpack_format_str="!{:d}H{:d}s".format(int(tlv_len/2),len(tmpstr)-tlv_len)
            elif msg[i]["type"] == DEVICE_NAME:
                unpack_format_str="!{:d}s{:d}s".format(int(tlv_len),len(tmpstr)-tlv_len)
            else:
                unpack_format_str="!{:d}s{:d}s".format(int(tlv_len),len(tmpstr)-tlv_len)
        elif id == BATTERY_STATUS_MESSAGE:
            unpack_format_str="!{:d}B{:d}s".format(int(tlv_len),len(tmpstr)-tlv_len)
        elif id == LED_CTRL_MESSAGE:
            unpack_format_str="!{:d}B{:d}s".format(int(tlv_len),len(tmpstr)-tlv_len)
        print("tlv unpack str:{}, raw: {}".format(unpack_format_str, tmpstr))
        tlv_data, pdus = struct.unpack(unpack_format_str,tmpstr)
        print("unpacked data:{}, left: {}".format(tlv_data, pdus))
        msg[i]['pdu'] = tlv_data
        msg.append({})
        i= i+1
    return msg

def send_hello_response_message():
    """ Simple Hello without baudrate change """
    tlv_data_pdu = 0
    tlv_data_len = 1
    tlv_data_id  = HELLO_RESPONSE
    mgm_header   = HELLO_MESSAGE << 2 | RESPONSE

    mgm_msg = struct.pack("1B1B1B1B", mgm_header, tlv_data_id, tlv_data_len, tlv_data_pdu)
    print(mgm_msg)
    fifo_len_header = len(mgm_msg)
    pack_format_str = "!1H{:d}s".format(fifo_len_header)
    mgm_msg = struct.pack(pack_format_str, fifo_len_header, mgm_msg)
    print(mgm_msg)
    print(os.write(fd_fifo_out, mgm_msg))

def send_keepalive_response():
    mgm_header   = KEEP_ALIVE_MESSAGE << 2 | RESPONSE
    mgm_msg = struct.pack("!1H1B", 1, mgm_header)
    print(mgm_msg)
    print(os.write(fd_fifo_out, mgm_msg))

def send_led_control_req():

    mgm_header   = LED_CTRL_MESSAGE << 2 | REQUEST
    tlv_data_id  = LED_CTRL_TLV_CTRL
    tlv_data_pdu_ledid = 0         #  led id
    tlv_data_pdu_color = 1         #  1byte
    tlv_data_pdu_interval = 5500   #  2 bytes units ms

    tlv_data_pdu = struct.pack("!1B1B1H", tlv_data_pdu_ledid, tlv_data_pdu_color, tlv_data_pdu_interval)
    tlv_data_len = len(tlv_data_pdu)
    pack_str = "1B1B1B{:d}s".format(tlv_data_len)
    mgm_msg = struct.pack(pack_str, mgm_header, tlv_data_id, tlv_data_len,tlv_data_pdu)
    print(mgm_msg)
    fifo_len_header = len(mgm_msg)
    pack_format_str = "!1H{:d}s".format(fifo_len_header)
    mgm_msg = struct.pack(pack_format_str, fifo_len_header, mgm_msg)
    print(mgm_msg)
    print(os.write(fd_fifo_out, mgm_msg))
    print("sent led control request")



while True:
    print("\n")
    data = os.read(fd_fifo_in,2)   # will block here
    print(data)
    data_len = struct.unpack("!1H", data)[0]
    print(data_len)
    message = os.read(fd_fifo_in, data_len)
    if message:
        print(message)
    mgm_msg = parseMessage(message, data_len)
    if mgm_msg['id'] == HELLO_MESSAGE:
        print("Got Hello message")
        send_hello_response_message()
    elif mgm_msg['id'] == KEEP_ALIVE_MESSAGE:
        print("Got keepalive request")
        send_keepalive_response()
        print("Sent keepalive response")
    elif mgm_msg["id"] == LED_CTRL_MESSAGE:
        print("Got LED control response")
    elif mgm_msg["id"] == BATTERY_STATUS_MESSAGE:
        print("Got Battery indication response")

    send_led_control_req()
    time.sleep(30)
    #time.sleep(20)  #testing connection lost
#TODO: statemachine need to simulate the Titan
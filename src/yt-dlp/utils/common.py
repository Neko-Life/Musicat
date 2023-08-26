import sys
import binascii as ba


def printerr(*args, **kwargs):
    return print(*args, file=sys.stderr, **kwargs)


def str_to_bytes(stri: str = ""):
    return bytes(stri, encoding='utf-8')


def decode_bytes(byt: bytes = b''):
    return byt.decode('utf-8')


def create_dir_name(stri: str = ""):
    return decode_bytes(ba.hexlify(str_to_bytes(stri)))

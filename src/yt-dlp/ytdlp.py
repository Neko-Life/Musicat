import sys
import os
from utils.common import printerr  # , create_dir_name

# printerr(ytdlp_dir)

# first page = 25
# +1 page: 49
DEFAULT_PLAYLIST_ENTRY_PER_PAGE = 25

# if len(sys.argv) < 3:
#     printerr(r'args: <music_folder_path> <url>')
#     exit(1)

argvlen = len(sys.argv)
if argvlen < 2:
    printerr(
        'Usage: python ytdlp.py <url> [OPTIONS...]\nOptions:\n\t--ytdlp-dir <path>\n\t--max-entries <int>\tDefault',
        DEFAULT_PLAYLIST_ENTRY_PER_PAGE, '\n\t--outfile </path/to/outfile.opus>')
    exit(1)

MAX_ENTRIES = DEFAULT_PLAYLIST_ENTRY_PER_PAGE
OUTFILE = False


def exitNoArgVal(arg):
    printerr("No argument value provided for", arg)
    exit(1)


def exitInvArgVal(arg, val):
    printerr("Invalid argument provided for", arg, "with value", val)
    exit(1)


# ARG_ROOT_PATH = sys.argv[1]
# ARG_URL = sys.argv[2]

LIB_PATH = ''
ARG_URL = ''

skipNext = False
for i in range(1, argvlen):
    arg = sys.argv[i]
    nextIdx = i + 1

    argVal = None

    if nextIdx <= (argvlen - 1):
        argVal = sys.argv[nextIdx]

    if arg == "--max-entries":
        if not argVal or not len(argVal):
            exitNoArgVal(arg)

        if not argVal.isdigit():
            exitInvArgVal(arg, argVal)
        skipNext = True

        MAX_ENTRIES = int(argVal)
    elif (arg == "--ytdlp-dir"):
        if not argVal or not len(argVal):
            exitNoArgVal(arg)
        skipNext = True

        LIB_PATH = argVal
    elif (arg == "--outfile"):
        if not argVal or not len(argVal):
            exitNoArgVal(arg)
        skipNext = True

        OUTFILE = argVal
    elif not skipNext:
        ARG_URL = arg
    else:
        skipNext = False

if not len(LIB_PATH):
    ytdlp_dir = (os.getenv('YTDLP_DIR')
                 or (os.path.dirname(os.path.abspath(__file__)) +
                     r'/../../libs/yt-dlp/'))
    LIB_PATH = ytdlp_dir

printerr("LIB_PATH:", LIB_PATH)
printerr("MAX_ENTRIES:", MAX_ENTRIES)
printerr("OUTFILE:", OUTFILE)
printerr("ARG_URL:", ARG_URL)

sys.path.insert(0, LIB_PATH)
from utils.ytdlp_run import run

run(ARG_URL, MAX_ENTRIES, 1, OUTFILE)

# test_url = 'https://www.youtube.com/watch?v=dQw4w9WgXcQ'
# https://www.youtube.com/watch?v=YXZH-eBtmqQ
# test_outfile = 'out.opus'
# run(test_url, MAX_ENTRIES, 1, test_outfile)

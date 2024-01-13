import sys
import os
import json
from utils.common import printerr  # , create_dir_name

ytdlp_dir = (os.getenv('YTDLP_DIR')
             or (os.path.dirname(os.path.abspath(__file__)) +
                 r'/../../libs/yt-dlp/'))

# printerr(ytdlp_dir)

LIB_PATH = ytdlp_dir

sys.path.insert(0, LIB_PATH)

import yt_dlp

# first page = 25
# +1 page: 49
DEFAULT_PLAYLIST_ENTRY_PER_PAGE = 25
# True will download the whole playlist (usually 6k+ entries)
# then proceed to get detailed info for each entries!
DEFAULT_YTDLP_PROCESS_ARG = False

# if len(sys.argv) < 3:
#     printerr(r'args: <music_folder_path> <url>')
#     exit(1)

argvlen = len(sys.argv)
if argvlen < 2:
    printerr(r'args: <url> [OPTIONS...]')
    exit(1)

max_entries = DEFAULT_PLAYLIST_ENTRY_PER_PAGE


def exitNoArgVal(arg):
    printerr("No argument value provided for", arg)
    exit(1)


def exitInvArgVal(arg, val):
    printerr("Invalid argument provided for", arg, "with value", val)
    exit(1)


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

        max_entries = int(argVal)

# ARG_ROOT_PATH = sys.argv[1]
# ARG_URL = sys.argv[2]
ARG_URL = sys.argv[1]

printerr("ARG_URL:", ARG_URL)

# ℹ️ See help(yt_dlp.YoutubeDL) for a list of
# available options and public functions
ydl_opts = {'logtostderr': True}

with yt_dlp.YoutubeDL(ydl_opts) as ydl:
    try:
        info = ydl.extract_info(ARG_URL,
                                download=False,
                                process=DEFAULT_YTDLP_PROCESS_ARG)

        # dir_name = create_dir_name(info['id'])

        # folder_path = ARG_ROOT_PATH + '/' + dir_name

        # printerr(r'Saving to ' + folder_path)

        # if not os.path.exists(folder_path):
        #     os.makedirs(folder_path)

        # ℹ️ ydl.sanitize_info makes the info json-serializable
        sanitized_info = ydl.sanitize_info(info)

        # printerr(json.dumps(sanitized_info))
        # printerr('type: ', info['_type'])

        if (info['_type'] == 'playlist' and info['entries']):
            count = 0
            results = []

            for i, element in enumerate(info['entries']):
                results.append(element)
                count += 1
                if count >= max_entries:
                    break

            sanitized_info['entries'] = results

        print(json.dumps(sanitized_info))

    except Exception as e:
        printerr("ERROR YT_DLP:")
        printerr(e)
        exit(1)

import sys
import os
import json
from utils.common import printerr, create_dir_name

LIB_PATH = os.path.dirname(os.path.abspath(__file__)) + r'/../../libs/yt-dlp/'
sys.path.insert(0, LIB_PATH)

import yt_dlp

if len(sys.argv) < 3:
    printerr(r'args: <music_folder_path> <url>')
    exit(1)

ARG_ROOT_PATH = sys.argv[1]
ARG_URL = sys.argv[2]

# ℹ️ See help(yt_dlp.YoutubeDL) for a list of available options and public functions
ydl_opts = {'logtostderr': True}

with yt_dlp.YoutubeDL(ydl_opts) as ydl:
    try:
        info = ydl.extract_info(ARG_URL, download=False)

        dir_name = create_dir_name(info['id'])

        folder_path = ARG_ROOT_PATH + '/' + dir_name

        printerr(r'Saving to ' + folder_path)

        if not os.path.exists(folder_path):
            os.makedirs(folder_path)

        # ℹ️ ydl.sanitize_info makes the info json-serializable
        print(json.dumps(ydl.sanitize_info(info)))

    except Exception as e:
        print(e)
        exit(1)

import json
from utils.common import printerr
import yt_dlp

def run(url, max_entries, print_stdout, outfile = False):
    # ℹ️ See help(yt_dlp.YoutubeDL) for a list of
    # available options and public functions
    ydl_opts = {
            'logtostderr': True,
            # 'quiet': True
            }

    is_download = outfile != False
    # process if downloading
    should_process = is_download

    if is_download == True:
        ydl_opts = {
                'logtostderr': True,
                # 'quiet': True,
                'skip_download': False,
                'format': 'bestaudio',
                'outtmpl': {'default': outfile },
                'outtmpl_na_placeholder': 'NA',
                'ignoreerrors': True,
                'force_generic_extractor': False,
                'allowed_extractors': ['default'],
                'ratelimit': None,
                'throttledratelimit': 100 * 1024,
                'overwrites': None,
                'retries': 10,
                'file_access_retries': 3,
                'fragment_retries': 10,
                'extractor_retries': 3,
                'concurrent_fragment_downloads': 16,
                'buffersize': 1024,
                'http_chunk_size': 2097152,
                'continuedl': True,
                'noprogress': True,
                'final_ext': 'opus',
                'postprocessors': [
                    {'key': 'FFmpegExtractAudio', 'preferredcodec': 'opus', 'preferredquality': '0', 'nopostoverwrites': False}
                    ]
                }

    # print(ydl_opts)

    with yt_dlp.YoutubeDL(ydl_opts) as ydl:
        try:
            info = ydl.extract_info(url,
                                    download=is_download,
                                    process=should_process)

            # ℹ️ ydl.sanitize_info makes the info json-serializable
            sanitized_info = ydl.sanitize_info(info)

            # printerr(json.dumps(sanitized_info))
            # printerr('type: ', info['_type'])

            if (info['_type'] == 'playlist' and info['entries']):
                count = 0
                results = []

                for i, element in enumerate(info['entries']):
                    results.append(element)
                    # count += 1
                    # if count >= max_entries:
                    #     break

                sanitized_info['entries'] = results

            d = json.dumps(sanitized_info);

            if print_stdout == 1:
                print(d)

            return d

        except Exception as e:
            printerr("ERROR YT_DLP:")
            printerr(e)
            return None

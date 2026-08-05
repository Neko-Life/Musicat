import json
from utils.common import printerr
import yt_dlp

def run(url, max_entries, print_stdout, should_process = False):
    # ℹ️ See help(yt_dlp.YoutubeDL) for a list of
    # available options and public functions
    ydl_opts = {'logtostderr': True}

    with yt_dlp.YoutubeDL(ydl_opts) as ydl:
        try:
            info = ydl.extract_info(url,
                                    download=False,
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
                    count += 1
                    if count >= max_entries:
                        break

                sanitized_info['entries'] = results

            d = json.dumps(sanitized_info);
            if (print_stdout == 1) print(d)
            return d

        except Exception as e:
            printerr("ERROR YT_DLP:")
            printerr(e)
            return None

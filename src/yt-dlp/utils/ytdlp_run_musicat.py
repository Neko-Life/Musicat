from utils.common import printerr
import utils.ytdlp_run
import musicat

def worker_run(id, url, max_entries, print_stdout, outfile = False):
    try:
        res = utils.ytdlp_run.run(url, max_entries, print_stdout, outfile)
        if isinstance(res, str):
            musicat.callback(id, res)
        else:
            musicat.callback(id, "")
        return res
    except Exception:
        musicat.callback(id, "")
        return None


def run(id, url, max_entries, print_stdout, outfile = False):
    try:
        return worker_run(id, url, max_entries, print_stdout, outfile)
    except Exception:
        musicat.callback(id, "")
        return None
    

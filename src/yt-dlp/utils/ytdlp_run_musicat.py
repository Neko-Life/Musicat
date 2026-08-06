from utils.common import printerr
import utils.ytdlp_run

# !TODO: !!! FIXME: THIS DOESN'T WORK, INCAPABLE OF ASYNC !!!
import concurrent.futures
# from threading import Thread
# from multiprocessing import Process

import musicat

executor = concurrent.futures.ThreadPoolExecutor(thread_name_prefix='mc/ytdlp')

# executor = None

def worker_run(id, url, max_entries, print_stdout, outfile = False):
    try:
        res = utils.ytdlp_run.run(url, max_entries, print_stdout, outfile)
        if isinstance(res, str):
            musicat.callback(id, res)
        else:
            musicat.callback(id, "")
    except Exception:
        musicat.callback(id, "")


def run(id, url, max_entries, print_stdout, outfile = False):
    # t = Thread(target=worker_run, name=id, args=[id, url, max_entries, print_stdout, outfile])
    # t.start()

    try:
        f = executor.submit(worker_run, id, url, max_entries, print_stdout, outfile).result()
    except Exception:
        musicat.callback(id, "")

    # concurrent.futures.wait([f], 0)

    # global executor
    # if executor == None:
    #     executor = Pool(8)
    # executor.apply_async(worker_run, id, url, max_entries, print_stdout, outfile)

    # Process(target=worker_run, args=(id, url, max_entries, print_stdout, outfile)).start()
    # Process(target=print, args=(id, url, max_entries, print_stdout, outfile)).start()

    # worker_run(id, url, max_entries, print_stdout, outfile)

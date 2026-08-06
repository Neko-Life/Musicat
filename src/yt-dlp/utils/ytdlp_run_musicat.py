from utils.common import printerr
import utils.ytdlp_run

# import concurrent.futures
# from threading import Thread
from multiprocessing import Pool

import musicat

executor = concurrent.futures.ThreadPoolExecutor(thread_name_prefix='musicat_worker')

# executor = None

def worker_run(id, url, max_entries, print_stdout, outfile = False):
    res = utils.ytdlp_run.run(url, max_entries, print_stdout, outfile)
    if isinstance(res, str):
        musicat.callback(id, res)
    else:
        musicat.callback(id, "")


def run(id, url, max_entries, print_stdout, outfile = False):
    # t = Thread(target=worker_run, name=id, args=[id, url, max_entries, print_stdout, outfile])
    # t.start()

    f = executor.submit(worker_run, id, url, max_entries, print_stdout, outfile).result()
    # concurrent.futures.wait([f], 0)

    # global executor
    # if executor == None:
    #     executor = Pool(8)
    # executor.apply_async(worker_run, id, url, max_entries, print_stdout, outfile)

    # worker_run(id, url, max_entries, print_stdout, outfile)

import cppimport

from multiprocessing.pool import Pool as MPool

import bz2
from urllib import request
import sys
import os
import time
from datetime import datetime, timedelta

channels = ['conda-forge', 'r', 'anaconda']
what_to_get = "opencv ==3.4.2"

url_template = 'https://conda.anaconda.org/{}/linux-64/repodata.json.bz2'

def ensure_dir(file_path):
    directory = os.path.dirname(file_path)
    if not os.path.exists(directory):
        os.makedirs(directory)

def download(args):
    channel, url, idx = args
    print("Checking: ", args)
    cache_file = os.path.join('./__cache__/', channel + '.json')

    has_cache = os.path.isfile(cache_file)
    if has_cache:
        print(datetime.fromtimestamp(os.path.getmtime(cache_file)))
        if datetime.now() - datetime.fromtimestamp(os.path.getmtime(cache_file)) > timedelta(hours=24):
            print("Cache invalidated...")
            has_cache = False

    if has_cache == False:
        req = request.urlopen(url)
        print("Downloading ", cache_file)
        with open(cache_file, 'w') as ftmp:
            ftmp.write(bz2.decompress(req.read()).decode('utf-8'))

    return True

def download_all(channels):
    global repos
    repos = {channel: {} for channel in channels}

    channel_args = [(channel, url_template.format(channel), i) for i, channel in enumerate(channels)]
    mpool = MPool(8)
    result = mpool.map(download, channel_args)
    return True

repos = download_all(channels)

api = cppimport.imp('interface')

if os.path.isfile('./installed.json'):
    print("Using installed.json from this folder")
    installed_json = './installed.json'
else:
    print("Couldn't find installed.json. Generate with `conda list --json > installed.json` from the command line.")
    installed_json = ""

channel_json = ['./__cache__/{}.json'.format(c) for c in channels]

result = api.solve(channel_json, installed_json,what_to_get)
print(result)
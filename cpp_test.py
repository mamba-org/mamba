import cppimport

from multiprocessing.pool import Pool as MPool

import bz2
from urllib import request
import sys
import json
import os
import time
from datetime import datetime, timedelta

from conda.core.subdir_data import SubdirData
from conda.models.channel import Channel
from conda.core.link import UnlinkLinkTransaction, PrefixSetup
from conda.cli.install import handle_txn
from conda.core.prefix_data import PrefixData
from conda.resolve import MatchSpec

api = cppimport.imp('interface')
# import interface as api

prefix = '/home/wolfv/miniconda3/'
channels = ['conda-forge', 'r', 'anaconda']
# what_to_get = ["opencv ==3.4.2"]
# what_to_get = ["conda-build"]
what_to_get = ["xtensor-r", "r-base ==3.5.1"]
url_template = 'https://conda.anaconda.org/{}/linux-64/repodata.json.bz2'


installed = sorted(PrefixData(prefix, pip_interop_enabled=True).iter_records(),
                   key=lambda x: x.name)


def ensure_dir(file_path):
    directory = os.path.dirname(file_path)
    if not os.path.exists(directory):
        os.makedirs(directory)

ensure_dir("./__cache__/")

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

installed_list_json = []
for prec in installed:
    installed_list_json.append(prec.dist_fields_dump())

with open('./installed.json', 'w') as fo:
    json.dump(installed_list_json, fo)

if os.path.isfile('./installed.json'):
    print("Using installed.json from this folder")
    installed_json = './installed.json'
else:
    print("Couldn't find installed.json. Generate with `conda list --json > installed.json` from the command line.")
    installed_json = ""

channel_json = [(c, './__cache__/{}.json'.format(c)) for c in channels]

to_link, to_unlink = api.solve(channel_json, installed_json, what_to_get)

print(to_link, to_unlink)

channel_data = {}
for c in channels:
    chan = Channel.from_channel_name(c)
    chan.platform = 'linux-64'
    channel_data[c] = SubdirData(chan).load()

print(channel_data)

to_unlink_records = []
to_link_records = []

for c, pkg in to_unlink:
    for p in installed:
        if p['name'] == pkg:
            to_unlink_records.append(p)

for c, pkg, json_ in to_link:
    for p in channel_data[c]._package_records:
        if p['fn'] == pkg:
            to_link_records.append(p)

pref_setup = PrefixSetup(
    target_prefix = prefix,
    unlink_precs  = to_unlink_records,
    link_precs    = to_link_records,
    remove_specs  = [],
    update_specs  = [MatchSpec(spec) for spec in what_to_get]
)

conda_transaction = UnlinkLinkTransaction(pref_setup)

class Args():
    name = None

handle_txn(conda_transaction, prefix, Args(), True)
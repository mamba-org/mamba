import sys
CONDA_PATH = '/home/wolfv/Programs/conda/'

sys.path.insert(0, CONDA_PATH)
# orig, sys.path = sys.path, [CONDA_PATH]
# import conda
# from tests import helpers
# sys.path = orig + [CONDA_PATH]

# i, r = helpers.get_index_r_1()
# print(i, r)

from conda.models import match_spec
from conda.models import version as cndversion
M = match_spec.MatchSpec('test >=2.5*, <3.5')

from multiprocessing.pool import Pool as MPool

import bz2
import sys
import solv
import json
from packaging import version
from urllib import request
import pandas as pd
import numpy as np
import re
# from clint.textui import progress

pool = solv.Pool()

# what is installed in the system
installed = pool.add_repo('installed')
pool.installed = installed

def normalize_version(version, build_number, build):
    vsplit = version.split('.')
    while len(vsplit) < 3:
        vsplit.append('0')
    return '.'.join(vsplit + [str(build_number)]) + '-' + build


def add_installed(f, installed_pool):
    parsed = json.load(open(f, 'r'))
    for pkg in parsed:
        v = normalize_version(pkg['version'], pkg['build_number'], pkg['build_string'])
        # print(pkg['name'], v)
        s = installed_pool.add_solvable()
        s.name = pkg['name']
        s.evr = v
        # s.arch = sel_pkg.get('target-triplet')

add_installed('installed.json', installed)

def parse_to_rel(d):
    pkg = d.split(' ')
    if len(pkg) > 1:
        # print(d)
        name = pkg[0]
        constraint = pkg[1]

        if len(pkg) > 2:
            feature = pkg[2]
            name = "{}[{}]".format(name, feature)
        # print(constraint)
        # Implement OR operator :)

        constraints = constraint.split(',')
        pcs = []
        for c in constraints:
            if len(c) and c[0] == '<':
                rel = solv.REL_LT
                if c[1] == '=':
                    rel |= solv.REL_EQ
                    target_version = c[2:]
                else:
                    target_version = c[1:]

            elif len(c) and c[0] == '>':
                rel = solv.REL_GT
                if c[1] == '=':
                    target_version = c[2:]
                    rel |= solv.REL_EQ
                else:
                    target_version = c[1:]
            else:
                if '*' in c:
                    rel1 = solv.REL_GT | solv.REL_EQ
                    rel2 = solv.REL_LT
                    fill_value = 0
                    v1, v2 = [], []

                    for el in c.split('.'):
                        matches = re.match(r'(\d+)?(\D+)?', el).groups()
                        # print(el, v1, v2)
                        if matches[0] is None:
                            if matches[1] == '*' and len(v2) > 0:
                                # increment previous
                                v2[-1] += 1

                                # if len(v2) < 3:
                                #     v1.append(fill_value)
                                #     v2.append(fill_value)
                            else:
                                # only * as version number
                                return [pool.str2id(name)]
                        elif matches[0] is not None and matches[1] == '*':
                            v1.append(int(matches[0]))
                            v2.append(int(matches[0]) + 1)
                        elif matches[1] is None:
                            v1.append(int(matches[0]))
                            v2.append(int(matches[0]))

                    # print(c, '.'.join([str(x) for x in v1]), '.'.join([str(x) for x in v2]))

                    r1 = pool.rel2id(pool.str2id(name), pool.str2id('.'.join([str(x) for x in v1])), rel1)
                    r2 = pool.rel2id(pool.str2id(name), pool.str2id('.'.join([str(x) for x in v2])), rel2)
                    return [pool.rel2id(r1, r2, solv.REL_WITH)]
                else:
                    vsplit = c.split('.')
                    if len(vsplit) == 4:
                        rel = solv.REL_EQ
                        return [pool.rel2id(pool.str2id(name), pool.str2id(c), rel)]
                    else:
                        # fix for build numbers as 4th güggel
                        while len(vsplit) < 4:
                            vsplit.append('0')
                        if len(vsplit) > 4:
                            print("WHAAAAAAAAAAAT ", vsplit)
                        rel1 = solv.REL_GT | solv.REL_EQ
                        rel2 = solv.REL_LT

                        r1 = pool.rel2id(pool.str2id(name), pool.str2id('.'.join([x for x in vsplit])), rel1)
                        vsplit[-1] = '9999'
                        r2 = pool.rel2id(pool.str2id(name), pool.str2id('.'.join([x for x in vsplit])), rel2)
                        return [pool.rel2id(r1, r2, solv.REL_WITH)]

                start = 0

                if len(c) and c[0] == '=':
                    start = 1
                    if c[1] == '=':
                        start = 2

                target_version = c[start:]

            pcs.append(pool.rel2id(pool.str2id(name), pool.str2id(target_version), rel))

        if len(pcs) == 2:
            pcs = [pool.rel2id(pcs[0], pcs[1], solv.REL_WITH)]
        return pcs
    else:
        return [pool.str2id(d)]


def parse_json(json_str, channel):
    repo_json = json.loads(json_str)
    label, channel = channel
    print("Parsing JSON ", label)
    packages = repo_json.get('packages')
    if packages is None:
        packages = repo_json
    df = pd.DataFrame(packages).T
    try:
        all_features = df['features'].dropna().unique()
    except:
        all_features = []
    print("all features: ", all_features)

    print(df)

    channel['main'] = pool.add_repo(label)

    for f in all_features:
        channel[f] = pool.add_repo(label + '[' + f + ']')

    for name, package_df in df.groupby(['name']):

        for vers, package_v in package_df.groupby(['version', 'build']):
            sel_pkg = package_v.loc[package_v['build_number'].astype(int).idxmax()]

            additional_name = None
            if len(all_features) and type(sel_pkg['features']) is str:
                s = channel[sel_pkg['features']].add_solvable()
                additional_name = "{}[{}]".format(name, sel_pkg['features'])
            else:
                s = channel['main'].add_solvable()

            if type(sel_pkg['build']) is str:
                additional_name = "{}[{}]".format(name, sel_pkg['build'])

            # print("Adding ", sel_pkg['name'], sel_pkg['version'], sel_pkg['build_number'])
            # print(sel_pkg['name'])
            s.name = sel_pkg['name']

            s.evr = normalize_version(sel_pkg['version'], sel_pkg['build_number'], sel_pkg['build'])
            s.arch = sel_pkg.get('target-triplet')

            if s.name == 'unixodbc':
                print(s.name, s.evr)
            s.add_deparray(solv.SOLVABLE_PROVIDES, pool.rel2id(pool.str2id(s.name), pool.str2id(s.evr), solv.REL_EQ))
            for d in sel_pkg['depends']:
                pcs = parse_to_rel(d)
                for p in pcs:
                    s.add_deparray(solv.SOLVABLE_REQUIRES, p)
            if additional_name:
                # print("additional name: ", additional_name)
                s.add_deparray(solv.SOLVABLE_PROVIDES, pool.rel2id(pool.str2id(additional_name), pool.str2id(s.evr), solv.REL_EQ))

    print("Done")

    # for key, package in repo_json['packages'].items():
    #     s = channel.add_solvable()
    #     s.name = package['name']
    #     s.evr = package['version']
    #     s.arch = package.get('target-triplet')

    #     s.add_deparray(solv.SOLVABLE_PROVIDES, pool.rel2id(pool.str2id(s.name), pool.str2id(s.evr), solv.REL_EQ))
    #     for d in package['depends']:
    #         parse_to_rel(d)
    #         s.add_deparray(solv.SOLVABLE_REQUIRES, pool.str2id(d.split(' ')[0]))

progress = []
progress_bars = []

url_template = 'https://conda.anaconda.org/{}/linux-64/repodata.json.bz2'

def ensure_dir(file_path):
    directory = os.path.dirname(file_path)
    if not os.path.exists(directory):
        os.makedirs(directory)

import os
import time
from datetime import datetime, timedelta

repos = {}

def download(args):
    channel, url, idx = args
    print("Downloading: ", args)
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

    if (channels[0] == 'xtest'):
        c = channels[0]
        with open(c + '.json', 'r') as f:
            parse_json(f.read(), (c, repos[c]))
        return repos
    channel_args = [(channel, url_template.format(channel), i) for i, channel in enumerate(channels)]
    mpool = MPool(8)
    result = mpool.map(download, channel_args)
    for c in repos:
    #     if os.path.isfile('__cache__/' + c + '.solv'):
    #         repos[c].add_solv('__cache__/' + c + '.solv')
    #         with open('__cache__/' + c + '.json', 'r') as f:
    #             repos[c].read(f)
    #     else:
    #         # with open(c + '.json', 'r') as f:
    #         #     parse_json(f.read(), (c, repos[c]))
            with open('__cache__/' + c + '.json', 'r') as f:
                parse_json(f.read(), (c, repos[c]))

    # print(result)
    return repos

channels = ['conda-forge', 'r', 'anaconda']
# channels = ['r']
# channels = ['xtest']
# channels = ['xtest']
repos = download_all(channels)

def internalize_repos(repos):
    for key, repo in repos.items():
        # print(key, repo)
        for subkey, subrepo in repo.items():
            subrepo.internalize()
            # with open('__cache__/' + key + '.solv', 'wb') as f:
            #     xf = solv.xfopen_fd(None, f.fileno())
            #     repo.write(xf)

def get_solver(specs, jobs, features):
    for x in repos.values():
        for r in x.values():
            print("PRIO: {}, SUB: {}".format(r.priority, r.subpriority))
    for r in repos.values():
        for k in r.keys():
            if k in features:
                print("Setting priority to 100", r[k])
                r[k].priority = 100
            else:
                pass
                # if k != 'main':
                #     r[k].free()

    internalize_repos(repos)
    pool.createwhatprovides()
    solver = pool.Solver()
    for s in specs:
        jobs.append(pool.Job(solv.Job.SOLVER_INSTALL | solv.Job.SOLVER_SOLVABLE_NAME, parse_to_rel(s)[0]))

    return solver


# instl = ['iopro 1.4*', 'python 2.7*', 'numpy 1.7*']

# instl = ['xtensor']
instl = ['hdf5 >=1.10']

jobs = []
# jobs.append(pool.Job(solv.Job.SOLVER_INSTALL | solv.Job.SOLVER_SOLVABLE_NAME, pool.str2id("r-rcpp")))
# jobs.append(pool.Job(solv.Job.SOLVER_INSTALL | solv.Job.SOLVER_SOLVABLE_NAME, pool.rel2id(pool.str2id("r-rcpp"), pool.str2id('1.0.0'), solv.REL_EQ)))
# jobs.append(pool.Job(solv.Job.SOLVER_INSTALL | solv.Job.SOLVER_SOLVABLE_NAME, pool.str2id("jupyter")))
# jobs.append(pool.Job(solv.Job.SOLVER_INSTALL | solv.Job.SOLVER_SOLVABLE_NAME, parse_to_rel("iopro 1.4*")[0]))
# jobs.append(pool.Job(solv.Job.SOLVER_INSTALL | solv.Job.SOLVER_SOLVABLE_NAME, parse_to_rel("python 2.7*")[0]))
# jobs.append(pool.Job(solv.Job.SOLVER_INSTALL | solv.Job.SOLVER_SOLVABLE_NAME, parse_to_rel("numpy 1.7*")[0]))
# instl = ['iopro', 'python 2.7*', 'numpy 1.5*']

# solver = get_solver(instl, jobs, features=['mkl'])
solver = get_solver(instl, jobs, features=[])

problems = solver.solve(jobs)
if problems:
    for p in problems:
        print("problem:", p)
    sys.exit(1)
transaction = solver.transaction()

# xls = [p for p in transaction.newsolvables()]
# print("LEN: ", len(xls))

yml_export = """
name: {env_name}
channels:
{channels}
dependencies:
{dependencies}
"""

yml_channels = ''
for c in channels:
    yml_channels += '- {}\n'.format(c)
yml_deps = ''
for p in transaction.newsolvables():
    yml_deps += '- {}::{}={}\n'.format(p.repo.name, p.name, p.evr)

print(yml_export.format(env_name='test', channels=yml_channels, dependencies='\n'.join(sorted(yml_deps.split('\n')))))

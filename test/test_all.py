import subprocess


def get_glibc_version():
    try:
        output = subprocess.check_output(['ldd', '--version'])
    except:
        return
    output.splitlines()
    version = output.splitlines()[0].split()[-1]
    return version.decode('ascii')


def run(exe, channels, package):
    cmd = [exe, 'create', '-n', 'xxx', '--override-channels', '--strict-channel-priority', '--dry-run']
    for channel in channels:
        cmd += ['-c', channel]
    cmd.append(package)
    subprocess.run(cmd, check=True)


def run_mamba_conda(channels, package):
    run('conda', channels, package)
    run('mamba', channels, package)


def test_all():
    version = get_glibc_version()
    with open('test/channel_a/linux-64/repodata.tpl') as f:
        repodata = f.read()
    with open('test/channel_a/linux-64/repodata.json', 'w') as f:
        if version is not None:
            glibc_placeholder = ', "__glibc=' + version + '"'
        else:
            glibc_placeholder = ''
        repodata = repodata.replace('GLIBC_PLACEHOLDER', glibc_placeholder)
        f.write(repodata)

    channels = ['./test/channel_b', './test/channel_a']
    package = 'a'
    run_mamba_conda(channels, package)

    package = 'b'
    run_mamba_conda(channels, package)

    channels = channels[::-1]
    run_mamba_conda(channels, package)

import subprocess
import shutil
import os


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


def add_glibc_virtual_package():
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


def copy_channels_osx():
    for channel in ['a', 'b']:
        if not os.path.exists(f'test/channel_{channel}/osx-64'):
            shutil.copytree(f'test/channel_{channel}/linux-64', f'test/channel_{channel}/osx-64')
            with open(f'test/channel_{channel}/osx-64/repodata.json') as f:
                repodata = f.read()
            with open(f'test/channel_{channel}/osx-64/repodata.json', 'w') as f:
                repodata = repodata.replace('linux', 'osx')
                f.write(repodata)


def test_all():
    add_glibc_virtual_package()
    copy_channels_osx()

    channels = ['./test/channel_b', './test/channel_a']
    package = 'a'
    run_mamba_conda(channels, package)

    package = 'b'
    run_mamba_conda(channels, package)

    channels = channels[::-1]
    run_mamba_conda(channels, package)

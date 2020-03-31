import subprocess


def run(exe, channels, package):
    cmd = [exe, 'create', '-n', 'xxx', '--override-channels', '--strict-channel-priority', '--dry-run']
    for channel in channels:
        cmd += ['-c', channel]
    cmd.append(package)
    subprocess.run(cmd, check=True, capture_output=True)


def run_mamba_conda(channels, package):
    run('conda', channels, package)
    run('mamba', channels, package)


def test_all():
    channels = ['./test/channel_b', './test/channel_a']
    package = 'a'
    run_mamba_conda(channels, package)

    package = 'b'
    run_mamba_conda(channels, package)

    channels = channels[::-1]
    run_mamba_conda(channels, package)

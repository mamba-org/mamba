import subprocess

from .helpers import get_umamba

def info(*args):
    umamba = get_umamba()
    cmd = [umamba, "info"] + [arg for arg in args if arg]

    try:
        p = subprocess.run(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
        )
    except subprocess.CalledProcessError as e:
        print(f"Command {cmd} failed with stderr: {e.stderr.decode()}")
        print(f"Command {cmd} failed with stdout: {e.stdout.decode()}")
        raise e
    return p.stdout.decode(), p.stderr.decode()


def test_log_level():
    stdout, stderr = info()
    assert not "debug" in stdout
    assert not "debug" in stderr
    assert not "trace" in stdout
    assert not "trace" in stderr
    for arg in ["--log-level=debug", "-vv"]:
        stdout, stderr = info(arg)
        assert "debug" in stderr
        assert not "debug" in stdout
        assert not "trace" in stderr
        assert not "trace" in stdout
    for arg in ["--log-level=trace", "-vvv"]:
        stdout, stderr = info(arg)
        assert "debug" in stderr
        assert not "debug" in stdout
        assert "trace" in stderr
        assert not "trace" in stdout

import subprocess
import uuid


def get_lines(std_pipe):
    '''Generator that yields lines from a standard pipe as they are printed.'''
    for line in iter(std_pipe.readline, ''):
        yield line
    #std_pipe.close()


class Shell:

    def __init__(self):
        self.process = subprocess.Popen(['bash'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True)
        self.sentinel = '__command_done__'
        self.echo_sentinel = 'echo ' + self.sentinel + '\n'

    def execute(self, commands):
        if type(commands) == str:
            commands = [commands]
        for cmd in commands:
            if not cmd.endswith('\n'):
                cmd += '\n'
            self.process.stdin.write(cmd)
            self.process.stdin.flush()
        self.process.stdin.write(self.echo_sentinel)
        self.process.stdin.flush()

        out = ''
        for line in get_lines(self.process.stdout):
            if not self.sentinel in line:
                print(line, end='')
                out = line[:-1]
            else:
                break

        return out

    def exit(self):
        self.process.kill()


class Environment:

    def __init__(self):
        self.shell = Shell()
        self.name = 'env_' + str(uuid.uuid1())
        self.shell.execute('MAMBA=$CONDA_PREFIX/bin/mamba')
        self.shell.execute('conda create -q -y -n ' + self.name)
        self.shell.execute('CONDA_BASE=$(conda info --base)')
        self.shell.execute('source $CONDA_BASE/etc/profile.d/conda.sh')
        self.shell.execute('conda activate ' + self.name)

    def __enter__(self):
        return self.shell

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.shell.execute('conda deactivate')
        self.shell.execute(f'conda remove -q -y --name {self.name} --all')
        self.shell.exit()

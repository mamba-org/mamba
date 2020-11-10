# micromamba

Micromamba is a small, pure-C++ executable with enough functionalities to _bootstrap_ fully functional conda-environments.
Currently, given micromamba's early stage, it's main usage is in continous integration pipelines: since it's a single executable, based on mamba, it bootstraps _fast_.

```{admonition} Warning
:class: warning

Note that micromamba is still in early development and not considered ready-for-general-usage yet! It has been used with great succes in CI systems, though, and we encourage this usage.
```

## Installation

`micromamba` works in the bash & zsh shell on Linux & OS X.
It's completely statically linked, which allows you to drop it in some place and just execute it.

Note: it's advised to use micromamba in containers & CI only.

Download and unzip the executable (from the official conda-forge package):

### Linux / Basic Usage

Ensure that basic utilities are installed. We need `wget` and `tar` with support for `bzip2`.
Also you need a glibc based system like Ubuntu, Fedora or Centos (Alpine Linux does not work natively).

The following magic URL always returns the latest available version of micromamba, and the `bin/micromamba` part is automatically extracted using `tar`.

```sh
wget -qO- https://micro.mamba.pm/api/micromamba/linux-64/latest | tar -xvj bin/micromamba
```

```{admonition} Warning
:class: info

Additional builds are available for linux-aarch64 (ARM64) and linux-ppc64le. Just modify `linux-64` in the URL above to match the desired architecture.
```

After extraction is completed, we can use the micromamba binary.

If you want to quickly use micromamba in an ad-hoc usecase, you can run

```
export MAMBA_ROOT_PREFIX=/some/prefix  # optional, defaults to ~/micromamba
eval "$(./bin/micromamba shell hook -s posix)"
```

This shell hook modifies your shell variables to include the micromamba command.

If you want to persist these changes, you can automatically write them to your `.bashrc` (or `.zshrc`) by running `./micromamba shell init ... `.
This also allows you to choose a custom MAMBA_ROOT_ENVIRONMENT, which is where the packages and repodata cache will live.

```sh
./bin/micromamba shell init -s bash -p ~/micromamba  # this writes to your .bashrc file
# sourcing the bashrc file incorporates the changes into the running session.
# better yet, restart your terminal!
source ~/.bashrc
```

Now you can activate the base environment and install new packages, or create other environments.

```sh
micromamba activate  # this activates the base environment
micromamba install python=3.6 jupyter -c conda-forge
# or
micromamba create -n new_prefix xtensor -c conda-forge
micromamba activate new_prefix
```

## OS X

Micromamba has OS X support as well. Instructions are largely the same:

```sh
curl -Ls https://micro.mamba.pm/api/micromamba/osx-64/latest | tar -xvj bin/micromamba
mv bin/micromamba ./micromamba

# directly execute the hook
eval "$(./bin/micromamba shell hook -s posix)"

# ... or shell init
./micromamba shell init -s zsh -p ~/micromamba
source ~/.zshrc
micromamba activate
micromamba install python=3.6 jupyter -c conda-forge
```

## Windows

Micromamba also has Windows support! For Windows, we recommend powershell. Below are the commands to get micromamba installed in `PowerShell`.

```pwsh
Invoke-Webrequest -URI https://micromamba.snakepit.net/api/micromamba/win-64/latest -OutFile micromamba.tar.bz2
C:\PROGRA~1\7-Zip\7z.exe x micromamba.tar.bz2 -aoa
C:\PROGRA~1\7-Zip\7z.exe x micromamba.tar -ttar -aoa -r Library\bin\micromamba.exe

# set a custom root prefix
$Env:MAMBA_ROOT_PREFIX="C:\Your\Root\Prefix"

# Invoke the hook
.\micrommaba.exe shell hook -s powershell | Out-String | Invoke-Expression

# ... or initialize the shell
.\micrommaba.exe shell init -s powershell -p C:\Your\Root\Prefix

micromamba create -f ./test/env_win.yaml -y
micromamba activate ...
```

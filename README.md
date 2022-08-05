![mamba header image](docs/assets/mamba_header.png)

## The Fast Cross-Platform Package Manager

<table>
<thead align="center" cellspacing="10">
  <tr>
    <th colspan="3" align="center" border="">part of mamba-org</th>
  </tr>
</thead>
<tbody>
  <tr background="#FFF">
    <td align="center">Package Manager <a href="https://github.com/mamba-org/mamba">mamba</a></td>
    <td align="center">Package Server <a href="https://github.com/mamba-org/quetz">quetz</a></td>
    <td align="center">Package Builder <a href="https://github.com/mamba-org/boa">boa</a></td>
  </tr>
</tbody>
</table>

# mamba

[![Build Status](https://github.com/mamba-org/mamba/workflows/CI/badge.svg)](https://github.com/mamba-org/mamba/actions)
[![Join the Gitter Chat](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/mamba-org/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![docs](https://readthedocs.org/projects/mamba/badge/?version=latest&style=flat)](https://mamba.readthedocs.io/en/latest)

Mamba is a reimplementation of the conda package manager in C++.

- parallel downloading of repository data and package files using multi-threading
- libsolv for much faster dependency solving, a state of the art library used in the RPM package manager of Red Hat, Fedora and OpenSUSE
- core parts of mamba are implemented in C++ for maximum efficiency

At the same time, mamba utilize the same command line parser, package installation and deinstallation code and transaction verification routines as `conda` to stay as compatible as possible.

Mamba is part of a bigger ecosystem to make scientific packaging more sustainable. You can read our [announcement blog post](https://medium.com/@QuantStack/open-software-packaging-for-science-61cecee7fc23).
The ecosystem also consists of `quetz`, an open source conda package server and `boa`, a fast conda package builder.

### Installation

It's advised to install mamba from conda-forge. If you already have conda, install mamba into the base environment:

```
conda install mamba -n base -c conda-forge
```

otherwise it's best to start with [Miniconda](https://docs.conda.io/en/latest/miniconda.html).
If you want to experiment with the latest software, you can also try micromamba (more below).

Another alternative (if you want/need to avoid Anaconda entirely) is to use [Mambaforge](https://github.com/conda-forge/miniforge#mambaforge) via the [Miniforge](https://github.com/conda-forge/miniforge) distribution.

### Installing conda packages with `mamba`

Now you are ready to install packages with

```bash
mamba install xtensor-r -c conda-forge
```

for example.

### Additional features

Mamba comes with features on top of stock conda.
To efficiently query repositories and query package dependencies you can use `mamba repoquery`.

Here are some examples:

`mamba repoquery search xtensor` will show you all available xtensor packages. You can also specify more constraints on this search query, for example `mamba repoquery search "xtensor>=0.18"`

`mamba repoquery depends xtensor` will show you a tree view of the dependencies of xtensor.

```
$ mamba repoquery depends --tree xtensor

xtensor == 0.21.5
  ├─ libgcc-ng [>=7.3.0]
  │ ├─ _libgcc_mutex [0.1 conda_forge]
  │ └─ _openmp_mutex [>=4.5]
  │   ├─ _libgcc_mutex already visited
  │   └─ libgomp [>=7.3.0]
  │     └─ _libgcc_mutex already visited
  ├─ libstdcxx-ng [>=7.3.0]
  └─ xtl [>=0.6.9,<0.7]
    ├─ libgcc-ng already visited
    └─ libstdcxx-ng already visited
```

And you can ask for the inverse, which packages depend on some other package (e.g. `ipython`) using `whoneeds`.

```
$ mamba repoquery whoneeds ipython

 Name            Version Build          Channel
──────────────────────────────────────────────────
 ipykernel       5.2.1   py37h43977f1_0 installed
 ipywidgets      7.5.1   py_0           installed
 jupyter_console 6.1.0   py_1           installed

```

With the `--tree` (or `-t`) flag, you can get the same information in a tree.

## micromamba

A guide on how to install `micromamba` can be found in the [official documentation](https://mamba.readthedocs.io/en/latest/installation.html#micromamba).

### Development installation

Please refer to the instructions given by the [official documentation](https://mamba.readthedocs.io/en/latest/developer_zone/build_locally.html#).

### Support us

For questions, you can also join us on the [QuantStack Chat](https://gitter.im/QuantStack/Lobby) or on the [conda channel](https://gitter.im/conda/conda) (note that this project is not officially affiliated with `conda` or Anaconda Inc.).

### License

We use a shared copyright model that enables all contributors to maintain the copyright on their contributions.

This software is licensed under the BSD-3-Clause license. See the [LICENSE](LICENSE) file for details.

# mamba, an experiment to make conda faster

[![Build Status](https://github.com/TheSnakePit/mamba/workflows/CI/badge.svg)](https://github.com/TheSnakePit/mamba/actions)
[![Join the Gitter Chat](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/QuantStack/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

Mamba is a reimplementation of the bits which are somewhat slow in conda. Mamba uses:

- parallel downloading of JSON files using multiprocessing, and reusing conda's caches
- libsolv for speedy dependency solving, a state of the art library used in the package manager of Fedora and others

At the same time, mamba re-uses a lot of conda's codebase to parse the command line and execute the transaction (installation & deinstallation of packages).

### Installation

***From `conda-forge`:***

```
conda install mamba -c conda-forge
```

***From Source:***

Make sure to have the following requirements in your conda environment:

- `conda install cmake compilers pybind11 libsolv libarchive libcurl nlohmann_json pip -c conda-forge`

If you build mamba in a different environment than base, you must also install conda in
that environment:

- `conda install conda -c conda-forge`

For a local (dev) build, run `pip install -e .`. This will build and install mamba
in the conda environment.

Now you are ready to install packages with `mamba install xtensor-r -c conda-forge` for example.

### Additional features

Mamba comes with features on top of stock conda.
To efficiently query repositories and query package dependencies you can use `mamba repoquery`.

Here are some examples:

`mamba repoquery search xtensor` will show you all available xtensor packages. You can also specify more constraints on this search query, for example `mamba repoquery search "xtensor>=0.18"`

`mamba repoquery depends xtensor` will show you a tree view of the dependencies of xtensor.

```
$ mamba repoquery depends xtensor

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

### Development installation

You first need to install the mamba dependencies:

```bash
conda install -c conda-forge python=3.7 pybind11 nlohmann_json cmake
```

For the C++ tests, you need Google Tests installed (e.g. `conda install gtest`).
To build the program using CMake, the following line needs to be used:

```bash
cmake .. \
  -DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX \
  -DPYTHON_EXECUTABLE=$CONDA_PREFIX/bin/python3 \
  -DPYTHON_LIBRARIES=$CONDA_PREFIX/lib/libpython3.7m.so \
  -DENABLE_TESTS=ON
```

### Support us

For questions, you can also join us on [QuantStack Chat](https://gitter.im/QuantStack/Lobby) or on the [conda channel](https://gitter.im/conda/conda) (note that this project is not officially affiliated with `conda` or Anaconda Inc.).


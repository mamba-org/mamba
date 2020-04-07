# mamba, an experiment to make conda faster

[![Build Status](https://github.com/QuantStack/mamba/workflows/CI/badge.svg)](https://github.com/QuantStack/mamba/actions)

**Still in BETA! Don't use mamba to install something in valuable environments!** 

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

- `conda install pybind11 libsolv nlohmann_json pip -c conda-forge`

If you build mamba in a different environment than base, you must also install conda in
that environment:

- `conda install conda -c conda-forge`

For a local (dev) build, run `pip install -e .`. This will build and install mamba
in the conda environment.

Now you are ready to install packages with `mamba install xtensor-r -c conda-forge` for example.

### Support us

If you want to stay up to date with Mamba development, follow [@wuoulf](https://twitter.com/wuoulf) on Twitter.
For questions, you can also join us on [QuantStack Chat](https://gitter.im/QuantStack/Lobby) or on the [conda channel](https://gitter.im/conda/conda) (note that this project is not officially affiliated with `conda` or Anaconda Inc.).


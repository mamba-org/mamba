# mamba, an experiment to make conda faster

**Still in BETA! Don't use mamba to install something in valuable environments!** 

Mamba is a reimplementation of the bits which are somewhat slow in conda. Mamba uses:

- parallel downloading of JSON files using multiprocessing, and reusing conda's caches
- simdjson, a hyper high performance library in C++ to parse the repodata.json (which is getting larger and larger, e.g. for conda-forge)
- libsolv for speedy dependency solving, a state of the art library used in the package manager of Fedora and others

At the same time, mamba re-uses a lot of conda's codebase to parse the command line and execute the transaction (installation & deinstallation of packages).

### Installation

***From `conda-forge`:***
```
conda install mamba -c conda-forge
```

***From Source:***

Make sure to have the following requirements in your conda environment:

- `conda install pybind11 libsolv conda cmake`


After that, `cd include`, `mkdir build`, `cd build`, `cmake ..`, `make`, switch back to the mamba root folder with `cd ../../`.
Now you are ready to install packages with `./bin/mamba install xtensor-r` for example.
Channels, and strict priority are currently supported.

We know how to implement feature support, but it's not done in this C++ port, yet.

### Support us

If you want to stay up to date with Mamba development, follow [@wuoulf](https://twitter.com/wuoulf) on Twitter.
For questions, you can also join us on [QuantStack Chat](https://gitter.im/QuantStack/Lobby) or on the [conda channel](https://gitter.im/conda/conda) (note that this project is not officially affiliated with `conda` or Anaconda Inc.).


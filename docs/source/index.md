Welcome to mamba's documentation!
=================================

Mamba is a fast, robust, and cross-platform package manager that runs on Windows, OS X and Linux (ARM64 and PPC64LE included).
Mamba is fully compatible with `conda` packages and supports most of conda's commands. In fact, mamba was conceived as a _drop-in_ replacement for conda, offering higher speed and more reliable environment solutions.

Mamba comes with a C++ core (for speed and efficiency), and a clean Python API on top. The core of mamba uses:

- `libsolv` to solve dependencies (the same library used in RedHat dnf and other Linux package managers)
- `curl` for reliable and fast downloads
- `libarchive` to extract the packages

To get started with `mamba`, it is currently advised to set up `miniconda` first. You find a installer for miniconda here: https://docs.conda.io/en/latest/miniconda.html
Once you have miniconda successfully set up, you can use `conda` to install `mamba`. Just type `conda install mamba -n base -c conda-forge` (this will install mamba into your _base_ conda environment -- mamba has to live in the base environment side-by-side with conda).

### Micromamba

```{admonition} Warning
:class: warning

Note that micromamba is still in early development and not considered ready-for-general-usage yet! It has been used with great succes in CI systems, though, and we encourage this usage.
```

An alternative to miniconda is micromamba. Micromamba is a statically linked, self-contained executable that comes with all the necessary bits and pieces to initialize and activate conda environments.


```{toctree}
:maxdepth: 2
:caption: "Contents:"

python_api
micromamba

```


Indices and tables
==================

* {ref}`Index <genindex>`
* {ref}`Search <search>`

<!-- * {ref}`modindex <modindex>` -->

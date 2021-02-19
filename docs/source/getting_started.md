Getting started with mamba
==========================

For new users
-------------

We strongly recommend to start from `mambaforge`, a community project of the conda-forge community.
You can download `mambaforge` for [Windows, macOS and Linux under this link](https://github.com/conda-forge/miniforge#mambaforge).
Mambaforge comes with the popular `conda-forge` channel preconfigured, but you can modify the configuration to use any channel you like.

After successful installation, you can use the mamba commands as described in [How to Use](how_to_use).


For conda users
---------------

If you are already a conda users, very good! Using mamba will feel natural.

To get mamba, install it _into the base environment_ from the `conda-forge` channel:

```
conda install mamba -n base -c conda-forge
```

There is little difference between using conda & mamba. You can swap almost all commands:

- `mamba install ...`
- `mamba create -n ... -c ... ...`
- `mamba list`

The one difference is that you should still use `conda` for activation and deactivation.

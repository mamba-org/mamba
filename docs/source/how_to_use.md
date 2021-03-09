How to use mamba
================

If you already know `conda`, great, you already know `mamba`! mamba is a drop-in replacement and uses the same commands and configuration options as conda.

For everyone else, here is a short overview over how to use mamba:

Create an environment
---------------------

Mamba handles multiple environments. The initial environment is called the _base_ environment and in general, you should not install packages into the base environment. Rather it's good practice to create new environments whenever you work on a specific project. Mamba has advanced file deduplication to keep the memory footprint of having multiple environments low.

The `mamba create` command creates a new environment. Mamba's environments are similar to virtual environments known from Python's `virtualenv` and similar software, but more powerful since mamba also manages _native_ dependencies and generalizes the virtual environment concept to many programming languages.

You can create a virtual environment with the name `nameofmyenv` by calling

```
mamba create -n nameofmyenv <list of packages>
```

After this process has finished, you can _activate_ the virtual environment by calling `conda activate <nameofmyenv>`.
For example, to install JupyterLab from the `conda-forge` channel and then run it, you could use the following commands:

```
mamba create -n myjlabenv jupyterlab -c conda-forge
conda activate myjlabenv  # activate our environment
jupyter lab               # this will start up jupyter lab and open a browser
```

Once an environment is activated, `mamba install` can be used to install further packages into the environment.

```
conda activate myjlabenv
mamba install bqplot  # now you can use bqplot in myjlabenv
```

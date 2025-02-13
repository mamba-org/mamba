.. _conda_compatible:

================
Conda Campatible
================

To provide full conda environment detection and execution ability to some development tools (e.g., ``VScode``), mamba introduced a new feature named ``conda campatible``.

What is it
==========

``conda compatible`` adds a new argument ``--conda-info-compat``. When calling ``mamba`` with this argument,
``mamba --conda-info-compat info`` and ``mamba --conda-info-compat info --json`` will print output in a conda-like format.

Alternatively, you can add this configuration to ``.mambarc`` as follows:

.. code::

  channels:
  - defaults
  show_channel_urls: true
  default_channels:
  - https://mirrors.example.com/anaconda/pkgs/main
  - https://mirrors.example.com/anaconda/pkgs/r
  - https://mirrors.example.com/anaconda/pkgs/msys2

  conda_info_compat: true


What is it used for
===================

In previous versions of ``mamba``, it provides different APIs from ``conda``, 
which meant that development tools couldn't detect virtual enviroments created by ``mamba`` as they could with ``conda``.

.. note:: 
  So far, the tested and confirmed compatible development tools: ``VScode``
  
Now, you can use ``mamba`` as if it were ``conda`` to manage the virtual environment when using development tools!

What should I do
================

VScode
******
1. Initialize your shell to use ``mamba`` as usual, then add ``conda_info_compat`` to ``.mambarc`` as mentioned earlier.
2. Create an executable shell script named ``conda`` in a directory exists in your $PATH (e.g., ``/usr/local/bin``), and write the following code:

.. code:: 

  #!/bin/bash
  #  
  # Vscode will detect the version of conda. For technial reasons, 
  # mamba can't simplily return a fake conda version when executed in conda_info_compatible mode.
  # So we have to return the fake version by using shell script.
  if [ "$1" == "--version" ]; then
      echo "23.11.0" 
  else
      $MAMBA_EXE "$@" # Created by 'mamba shell init' in your shellrc file
      # If you don't want to modify .mambarc file, you can pass `--conda-info-compat` in here.
      # $MAMBA_EXE --conda-info-compat "$@"
  fi

3. Now ``VScode`` can partially use ``mamba`` as if it were ``conda``. But you can't replace ``mamba`` with ``conda`` in your daily routine, because ``mamba`` is a shell function but ``conda`` is an executable file.  
4. So you should add an ``alias`` command to the bottom of your  ``shellrc file`` (e.g., .bashrc) to directly execute ``conda`` in shell: ``alias conda=mamba`` 

.. note:: 
  If you don't need to execute ``conda`` instead of ``mamba``, you can just stop after create ``conda`` script.


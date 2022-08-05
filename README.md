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

At the same time, mamba utilizes the same command line parser, package installation and deinstallation code and transaction verification routines as `conda` to stay as compatible as possible.

Mamba is part of a bigger ecosystem to make scientific packaging more sustainable. You can read our [announcement blog post](https://medium.com/@QuantStack/open-software-packaging-for-science-61cecee7fc23).
The ecosystem also consists of `quetz`, an open source conda package server and `boa`, a fast conda package builder.

## Installation

Please refer to the [Mamba and Micromamba installation guide](https://mamba.readthedocs.io/en/latest/installation.html) in the documentation.

## Additional features in Mamba and Micromamba

Mamba and Micromamba come with features on top of stock conda.

### `repoquery`

To efficiently query repositories and query package dependencies you can use `mamba repoquery` or `micromamba repoquery`.
See the [repoquery documentation](https://mamba.readthedocs.io/en/latest/user_guide/mamba.html#repoquery) for details.

### Installing lock files

TODO

### More features?

## micromamba

micromamba is a small, pure-C++ reimplementation of Mamba/Conda. It strives to be a full replacement for Mamba and Conda. As such, it doesn't use any Conda code (in fact it doesn't require Python at all).

See the [documentation on Micromamba](https://mamba.readthedocs.io/en/latest/user_guide/micromamba.html) for details.

## Development installation

Please refer to the instructions given by the [official documentation](https://mamba.readthedocs.io/en/latest/developer_zone/build_locally.html#).

## Support us

For questions, you can also join us on the [QuantStack Chat](https://gitter.im/QuantStack/Lobby) or on the [conda channel](https://gitter.im/conda/conda) (note that this project is not officially affiliated with `conda` or Anaconda Inc.).

## License

We use a shared copyright model that enables all contributors to maintain the copyright on their contributions.

This software is licensed under the BSD-3-Clause license. See the [LICENSE](LICENSE) file for details.

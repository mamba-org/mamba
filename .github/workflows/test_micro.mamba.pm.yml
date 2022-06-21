name: Test micro.mamba.pm

on:
  schedule:
    - cron:  '0 10 * * *'

jobs:
  test_micro_mamba_pm:
    runs-on: ${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
    steps:
      - shell: bash
        run: |
          function platform_name () {
            local x
            x=$(uname)-$(uname -m)
            x=${x,,}
            x=${x//darwin/osx}
            x=${x//_x86/}
            echo $x
          }
          curl -Ls https://micro.mamba.pm/api/micromamba/$(platform_name)/latest | tar -xvj bin/micromamba
          bin/micromamba info

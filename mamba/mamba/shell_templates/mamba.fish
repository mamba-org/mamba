# Based on conda.fish which is licensed as below
#
# Copyright (C) 2012 Anaconda, Inc
# SPDX-License-Identifier: BSD-3-Clause

set -gx MAMBA_EXE (dirname $CONDA_EXE)/mamba

function mamba --inherit-variable CONDA_EXE --inherit-variable MAMBA_EXE
  if test (count $argv) -lt 1 || contains -- --help $argv
    $MAMBA_EXE $argv
  else
    set -l cmd $argv[1]
    set -e argv[1]
    switch $cmd
      case activate deactivate
        eval ($CONDA_EXE shell.fish $cmd $argv)
      case install update upgrade remove uninstall
        $MAMBA_EXE $cmd $argv || return $status
        and eval ($CONDA_EXE shell.fish reactivate)
      case '*'
        $MAMBA_EXE $cmd $argv
    end
  end
end

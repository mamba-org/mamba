R"MAMBARAW(
@REM Copyright (C) 2012 Anaconda, Inc
@REM SPDX-License-Identifier: BSD-3-Clause
@REM The file name is conda_hook.bat rather than conda-hook.bat because conda will see
@REM the latter as a 'conda hook' command.

@IF DEFINED MAMBA_SHLVL GOTO :EOF

@FOR %%F in ("%~dp0") do @SET "__mambabin_dir=%%~dpF"
@SET "__mambabin_dir=%__mambabin_dir:~0,-1%"
@SET "PATH=%__mambabin_dir%;%PATH%"
@SET "MAMBA_BAT=%__mambabin_dir%\mamba.bat"
@FOR %%F in ("%__mambabin_dir%") do @SET "__mamba_root=%%~dpF"
__MAMBA_INSERT_MAMBA_EXE__
@SET __mambabin_dir=
@SET __mamba_root=

@DOSKEY micromamba="%MAMBA_BAT%" $*

@SET MAMBA_SHLVL=0
)MAMBARAW"

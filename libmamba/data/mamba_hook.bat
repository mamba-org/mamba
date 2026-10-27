@REM Copyright (C) 2021 QuantStack
@REM SPDX-License-Identifier: BSD-3-Clause
@REM This file is derived from conda_hook.bat

@IF DEFINED CONDA_SHLVL GOTO :EOF

@FOR %%F in ("%~dp0") do @SET "__mambabin_dir=%%~dpF"
@SET "__mambabin_dir=%__mambabin_dir:~0,-1%"
@SET "PATH=%__mambabin_dir%;%PATH%"
@SET "MAMBA_BAT=%__mambabin_dir%\__MAMBA_INSERT_BAT_NAME__"
@FOR %%F in ("%__mambabin_dir%") do @SET "__mamba_root=%%~dpF"
@SET "MAMBA_EXE=%__mamba_root%Library\bin\mamba.exe"
@SET __mambabin_dir=
@SET __mamba_root=

@REM @DOSKEY does not work with delayed evaluation
@REM @DOSKEY after the first usage of a macro whose name is defined with a variable
@REM Therefore no magic here, just grep and replace when generating the final file
@DOSKEY __MAMBA_INSERT_EXE_NAME__="%MAMBA_BAT%" $*

@SET CONDA_SHLVL=0

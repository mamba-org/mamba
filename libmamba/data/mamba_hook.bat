@REM Copyright (C) 2021 QuantStack
@REM SPDX-License-Identifier: BSD-3-Clause
@REM This file is derived from conda_hook.bat

@IF DEFINED CONDA_SHLVL GOTO :EOF

@FOR %%F in ("%~dp0") do @SET "__mambabin_dir=%%~dpF"
@SET "__mambabin_dir=%__mambabin_dir:~0,-1%"
@SET "PATH=%__mambabin_dir%;%PATH%"
@SET "MAMBA_BAT=%__mambabin_dir%\mamba.bat"
@FOR %%F in ("%__mambabin_dir%") do @SET "__mamba_root=%%~dpF"
__MAMBA_INSERT_MAMBA_EXE__
@SET __mambabin_dir=
@SET __mamba_root=

@echo off
@REM We need to define an alias with the same name as the executable to be called by the user.
@REM Get the base filename of MAMBA_EXE
@FOR %%A in ("%MAMBA_EXE%") do (
    @set "__mamba_filename=%%~nxA"
    @REM Remove .exe extension from the filename
    @SET "__mamba_name=!__mamba_filename:%~x1=!"
    @REM Define correct alias depending on the executable name
    @set "__mamba_cmd=call ""%MAMBA_BAT%"" $*"
    @DOSKEY !__mamba_name!=!__mamba_cmd!
)
@SET CONDA_SHLVL=0

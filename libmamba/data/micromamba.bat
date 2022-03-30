@REM Copyright (C) 2012 Anaconda, Inc
@REM SPDX-License-Identifier: BSD-3-Clause

@REM echo _CE_CONDA is %_CE_CONDA%
@REM echo _CE_M is %_CE_M%
@REM echo CONDA_EXE is %CONDA_EXE%

@REM @SET "MAMBA_EXE=%~dp0..\Scripts\micromamba.exe"
__MAMBA_INSERT_MAMBA_EXE__
__MAMBA_INSERT_ROOT_PREFIX__
@IF [%1]==[activate]   "%~dp0_mamba_activate" %*
@IF [%1]==[deactivate] "%~dp0_mamba_activate" %*

@SETLOCAL EnableDelayedExpansion
@REM This is the standard user case.  This script is run in root\condabin.
@REM FOR %%A IN ("%~dp0.") DO @SET _sysp=%%~dpA
@REM IF NOT EXIST "!_sysp!\Scripts\micromamba.exe" @SET "_sysp=!_sysp!..\"
@SET PATH=!MAMBA_ROOT_PREFIX!;!MAMBA_ROOT_PREFIX!\Library\mingw-w64\bin;!MAMBA_ROOT_PREFIX!\Library\usr\bin;!MAMBA_ROOT_PREFIX!\Library\bin;!MAMBA_ROOT_PREFIX!\Scripts;!MAMBA_ROOT_PREFIX!\bin;%PATH%
@CALL %MAMBA_EXE% %*
@ENDLOCAL

@IF %errorlevel% NEQ 0 EXIT /B %errorlevel%

@IF [%1]==[install]   "%~dp0_mamba_activate" reactivate
@IF [%1]==[update]    "%~dp0_mamba_activate" reactivate
@IF [%1]==[upgrade]   "%~dp0_mamba_activate" reactivate
@IF [%1]==[remove]    "%~dp0_mamba_activate" reactivate
@IF [%1]==[uninstall] "%~dp0_mamba_activate" reactivate

@EXIT /B %errorlevel%

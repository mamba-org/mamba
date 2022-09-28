@REM Copyright (C) 2012 Anaconda, Inc
@REM SPDX-License-Identifier: BSD-3-Clause

@REM echo _CE_CONDA is %_CE_CONDA%
@REM echo _CE_M is %_CE_M%
@REM echo CONDA_EXE is %CONDA_EXE%

@IF NOT DEFINED _CE_CONDA (
  @SET _CE_M=
  @SET "CONDA_EXE=%~dp0..\Scripts\conda.exe"
)
@IF [%1]==[activate]   "%~dp0_conda_activate" %*
@IF [%1]==[deactivate] "%~dp0_conda_activate" %*

@SET MAMBA_EXES="%~dp0..\Scripts\mamba.exe"
@CALL %MAMBA_EXES% %*

@IF %errorlevel% NEQ 0 EXIT /B %errorlevel%

@IF [%1]==[install]   "%~dp0_conda_activate" reactivate
@IF [%1]==[update]    "%~dp0_conda_activate" reactivate
@IF [%1]==[upgrade]   "%~dp0_conda_activate" reactivate
@IF [%1]==[remove]    "%~dp0_conda_activate" reactivate
@IF [%1]==[uninstall] "%~dp0_conda_activate" reactivate

@EXIT /B %errorlevel%

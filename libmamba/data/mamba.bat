@REM Copyright (C) 2012 Anaconda, Inc
@REM SPDX-License-Identifier: BSD-3-Clause

@REM Replaced by mamba executable with the MAMBA_EXE and MAMBA_ROOT_PREFIX variable pointing
@REM to the correct locations.
__MAMBA_DEFINE_MAMBA_EXE__
__MAMBA_DEFINE_ROOT_PREFIX__

@IF [%1]==[activate]   "%~dp0__MAMBA_INSERT_ACTIVATE_BAT_NAME__" %*
@IF [%1]==[deactivate] "%~dp0__MAMBA_INSERT_ACTIVATE_BAT_NAME__" %*

@CALL "%MAMBA_EXE%" %*

@IF %errorlevel% NEQ 0 EXIT /B %errorlevel%

@IF [%1]==[install]   "%~dp0__MAMBA_INSERT_ACTIVATE_BAT_NAME__" reactivate
@IF [%1]==[update]    "%~dp0__MAMBA_INSERT_ACTIVATE_BAT_NAME__" reactivate
@IF [%1]==[upgrade]   "%~dp0__MAMBA_INSERT_ACTIVATE_BAT_NAME__" reactivate
@IF [%1]==[remove]    "%~dp0__MAMBA_INSERT_ACTIVATE_BAT_NAME__" reactivate
@IF [%1]==[uninstall] "%~dp0__MAMBA_INSERT_ACTIVATE_BAT_NAME__" reactivate
@IF [%1]==[self-update] @CALL DEL /f %MAMBA_EXE%.bkup

@EXIT /B %errorlevel%

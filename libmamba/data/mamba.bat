@REM Copyright (C) 2012 Anaconda, Inc
@REM SPDX-License-Identifier: BSD-3-Clause

@REM Replaced by mamba executable with the MAMBA_EXE and MAMBA_ROOT_PREFIX variable pointing
@REM to the correct locations.
@SET "CURDIR=%CD%"
@CD /d "%~dp0.."
@SET "MAMBA_ROOT_PREFIX=%CD%"
@SET "MAMBA_EXE=%CD%\Library\bin\mamba.exe"
@CD /d "%CURDIR%"

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

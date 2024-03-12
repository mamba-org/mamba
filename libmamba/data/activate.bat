@REM Copyright (C) 2021 QuantStack
@REM SPDX-License-Identifier: BSD-3-Clause

@CALL "%~dp0..\condabin\mamba_hook.bat"

@REM Replaced by mamba executable with the MAMBA_EXE variable pointing to the correct location.
__MAMBA_INSERT_MAMBA_EXE__

@REM We need to know the name of the executable, either mamba or micromamba
@REM Get the base filename of MAMBA_EXE
@FOR %%A in ("%MAMBA_EXE%") do set "__mamba_filename=%%~nxA"
@REM Remove .exe extension from the filename
@SET "__mamba_name=!__mamba_filename:%~x1=!"

!__mamba_name! activate %*

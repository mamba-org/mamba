param([parameter(Position=0,Mandatory=$false)] [Hashtable] $MambaModuleArgs=@{})

## AFTER PARAM ################################################################

# Defaults from before we had arguments.
if (-not $MambaModuleArgs.ContainsKey('ChangePs1')) {
    $MambaModuleArgs.ChangePs1 = $True
}

## ENVIRONMENT MANAGEMENT ######################################################

<#
    .SYNOPSIS
        Activates a conda environment, placing its commands and packages at
        the head of $Env:PATH.

    .EXAMPLE
        Enter-MambaEnvironment base

    .EXAMPLE
        etenv base

    .NOTES
        This command does not currently support activating environments stored
        in a non-standard location.
#>
function Enter-MambaEnvironment {
    begin {
        $activateCommand = (& $Env:MAMBA_EXE shell activate -s powershell $Args | Out-String);
        Write-Verbose "[micromamba shell activate --shell powershell $Args]`n$activateCommand";
        Invoke-Expression -Command $activateCommand;
    }

    process {}

    end {}
}

<#
    .SYNOPSIS
        Deactivates the current conda environment, if any.

    .EXAMPLE
        Exit-MambaEnvironment

    .EXAMPLE
        exenv
#>
function Exit-MambaEnvironment {
    [CmdletBinding()]
    param();

    begin {
        $deactivateCommand = (& $Env:MAMBA_EXE shell deactivate -s powershell | Out-String);

        # If deactivate returns an empty string, we have nothing more to do,
        # so return early.
        if ($deactivateCommand.Trim().Length -eq 0) {
            return;
        }
        Write-Verbose "[micromamba shell deactivate -s powershell]`n$deactivateCommand";
        Invoke-Expression -Command $deactivateCommand;
    }
    process {}
    end {}
}

## MAMBA WRAPPER ###############################################################

<#
    .SYNOPSIS
        conda is a tool for managing and deploying applications, environments
        and packages.

    .PARAMETER Command
        Subcommand to invoke.

    .EXAMPLE
        conda install toolz
#>
function Invoke-Mamba() {
    # Don't use any explicit args here, we'll use $args and tab completion
    # so that we can capture everything, INCLUDING short options (e.g. -n).
    if ($Args.Count -eq 0) {
        # No args, just call the underlying mamba executable.
        & $Env:MAMBA_EXE;
    }
    else {
        $Command = $Args[0];
        if ($Args.Count -ge 2) {
            $OtherArgs = $Args[1..($Args.Count - 1)];
        } else {
            $OtherArgs = @();
        }
        switch ($Command) {
            "activate" {
                Enter-MambaEnvironment @OtherArgs;
            }
            "deactivate" {
                Exit-MambaEnvironment;
            }
            "self-update" {
                & $Env:MAMBA_EXE $Command @OtherArgs;
                $MAMBA_EXE_BKUP = $Env:MAMBA_EXE + ".bkup";
                if (Test-Path $MAMBA_EXE_BKUP) {
                    Remove-Item $MAMBA_EXE_BKUP
                }
            }
            default {
                # There may be a command we don't know want to handle
                # differently in the shell wrapper, pass it through
                # verbatim.
                & $Env:MAMBA_EXE $Command @OtherArgs;

                # reactivate environment
                if (@("install", "update", "remove").contains($Command))
                {
                    $currentEnv = $Env:CONDA_DEFAULT_ENV;
                    Exit-MambaEnvironment;
                    Enter-MambaEnvironment $currentEnv;
                }
            }
        }
    }
}

## TAB COMPLETION ##############################################################

$MicromambaAutocompleteScriptblock = {
    param($wordToComplete, $commandAst, $cursorPosition)
    $RemainingArgs = $commandAst.ToString().Split()
    $OneRemainingArgs = $RemainingArgs[1..$RemainingArgs.Length]
    if (-not $wordToComplete) {
        $OneRemainingArgs += '""'
    }
    $MMOUT = & $Env:MAMBA_EXE completer @OneRemainingArgs
    $MMLIST = $MMOUT.trim() -split '\s+'
    foreach ($el in $MMLIST) {
        [System.Management.Automation.CompletionResult]::new(
            $el, $el, 'ParameterValue', $el)
    }
}

## PROMPT MANAGEMENT ###########################################################

<#
    .SYNOPSIS
        Modifies the current prompt to show the currently activated conda
        environment, if any.
#>
if ($MambaModuleArgs.ChangePs1) {
    # We use the same procedure to nest prompts as we did for nested tab completion.
    if (Test-Path Function:\prompt) {
        Rename-Item Function:\prompt MambaPromptBackup
    } else {
        function MambaPromptBackup() {
            # Restore a basic prompt if the definition is missing.
            "PS $($executionContext.SessionState.Path.CurrentLocation)$('>' * ($nestedPromptLevel + 1)) ";
        }
    }

    function global:prompt() {
        if ($Env:CONDA_PROMPT_MODIFIER) {
            $Env:CONDA_PROMPT_MODIFIER | Write-Host -NoNewline
        }
        MambaPromptBackup;
    }
}

## ALIASES #####################################################################

New-Alias micromamba Invoke-Mamba -Force
Register-ArgumentCompleter -Native -CommandName micromamba -ScriptBlock $MicromambaAutocompleteScriptblock

## EXPORTS ###################################################################

if ($null -eq $Env:CONDA_SHLVL) {
    $Env:PATH = "$Env:MAMBA_ROOT_PREFIX\condabin;" + $Env:PATH
}

Export-ModuleMember `
    -Alias * `
    -Function Invoke-Mamba, Enter-MambaEnvironment, Exit-MambaEnvironment, prompt

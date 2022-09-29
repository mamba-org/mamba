param([parameter(Position=0,Mandatory=$false)] [Hashtable] $MambaModuleArgs=@{})

# Defaults from before we had arguments.
if (-not $MambaModuleArgs.ContainsKey('ChangePs1')) {
    $MambaModuleArgs.ChangePs1 = $True
}

## ENVIRONMENT MANAGEMENT ######################################################

<#
    .SYNOPSIS
        Obtains a list of valid conda environments.

    .EXAMPLE
        Get-CondaEnvironment

    .EXAMPLE
        genv
#>
function Get-CondaEnvironment {
    [CmdletBinding()]
    param();

    begin {}

    process {
        # NB: the JSON output of conda env list does not include the names
        #     of each env, so we need to parse the fragile output instead.
        & $Env:CONDA_EXE $Env:_CE_M $Env:_CE_CONDA env list | `
            Where-Object { -not $_.StartsWith("#") } | `
            Where-Object { -not $_.Trim().Length -eq 0 } | `
            ForEach-Object {
                $envLine = $_ -split "\s+";
                $Active = $envLine[1] -eq "*";
                [PSCustomObject] @{
                    Name = $envLine[0];
                    Active = $Active;
                    Path = if ($Active) {$envLine[2]} else {$envLine[1]};
                } | Write-Output;
            }
    }

    end {}
}

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
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)][switch]$Stack,
        [Parameter(Position=0)][string]$Name
    );

    begin {
        If ($Stack) {
            $activateCommand = (& $Env:MAMBA_EXE shell activate -s powershell --stack $Name | Out-String);
        } Else {
            $activateCommand = (& $Env:MAMBA_EXE shell activate -s powershell $Name | Out-String);
        }

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

            default {
                # There may be a command we don't know want to handle
                # differently in the shell wrapper, pass it through
                # verbatim.
                & $Env:MAMBA_EXE $Command @OtherArgs;
            }
        }
    }
}

## TAB COMPLETION ##############################################################
# We borrow the approach used by posh-git, in which we override any existing
# functions named TabExpansion, look for commands we can complete on, and then
# default to the previously defined TabExpansion function for everything else.

if (Test-Path Function:\TabExpansion) {
    # Since this technique is common, we encounter an infinite loop if it's
    # used more than once unless we give our backup a unique name.
    Rename-Item Function:\TabExpansion CondaTabExpansionBackup
}

function Expand-CondaEnv() {
    param(
        [string]
        $Filter
    );

    $ValidEnvs = Get-CondaEnvironment;
    $ValidEnvs `
        | Where-Object { $_.Name -like "$filter*" } `
        | ForEach-Object { $_.Name } `
        | Write-Output;
    $ValidEnvs `
        | Where-Object { $_.Path -like "$filter*" } `
        | ForEach-Object { $_.Path } `
        | Write-Output;

}

function Expand-CondaSubcommands() {
    param(
        [string]
        $Filter
    );

    $ValidCommands = Invoke-Mamba shell.powershell commands;

    # Add in the commands defined within this wrapper, filter, sort, and return.
    $ValidCommands + @('activate', 'deactivate') `
        | Where-Object { $_ -like "$Filter*" } `
        | Sort-Object `
        | Write-Output;

}

function TabExpansion($line, $lastWord) {
    $lastBlock = [regex]::Split($line, '[|;]')[-1].TrimStart()

    switch -regex ($lastBlock) {
        # Pull out conda commands we recognize first before falling through
        # to the general patterns for conda itself.
        "^micromamba activate (.*)" { Expand-CondaEnv $lastWord; break; }
        "^etenv (.*)" { Expand-CondaEnv $lastWord; break; }

        # If we got down to here, check arguments to conda itself.
        "^conda (.*)" { Expand-CondaSubcommands $lastWord; break; }

        # Finally, fall back on existing tab expansion.
        default {
            if (Test-Path Function:\CondaTabExpansionBackup) {
                CondaTabExpansionBackup $line $lastWord
            }
        }
    }
}

## PROMPT MANAGEMENT ###########################################################

<#
    .SYNOPSIS
        Modifies the current prompt to show the currently activated conda
        environment, if any.
    .EXAMPLE
        Add-CondaEnvironmentToPrompt

        Causes the current session's prompt to display the currently activated
        conda environment.
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
New-Alias genv Get-CondaEnvironment -Force
New-Alias etenv Enter-MambaEnvironment -Force
New-Alias exenv Exit-MambaEnvironment -Force

## EXPORTS ###################################################################

Export-ModuleMember `
    -Alias * `
    -Function `
        Invoke-Mamba, `
        Get-CondaEnvironment, `
        Enter-CondaEnvironment, Exit-CondaEnvironment, `
        TabExpansion, prompt

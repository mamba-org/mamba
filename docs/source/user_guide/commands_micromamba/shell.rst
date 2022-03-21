.. _commands_micromamba/shell:

``shell``
=========


Generate shell init scripts

**Usage:** ``micromamba shell [OPTIONS] action [prefix]`` 

**Positionals:**

REQUIRED 

action TEXT:{init,hook,activate,deactivate,reactivate}



The action to complete

TEXT 

prefix The root prefix to configure (for init and hook), and the prefix to activate for activate, either by name or by path


**Options:**

``-h,--help`` 

Print this help message and exit

``-s,--shell`` 

TEXT:{bash,cmd.exe,fish,posix,powershell,xonsh,zsh}

A 

shell type

``--stack`` 

Stack the environment being activated

``-p,-n,--prefix,--name`` TEXT 

The root prefix to configure (for init and hook), and the prefix to activate for activate, either by name or by path


**Configuration options:**

``--rc-file`` TEXT 

... Paths to the configuration files to use

``--no-rc`` 

Disable the use of configuration files

``--no-env`` 

Disable the use of environment variables


**Global options:**

``-v,--verbose`` ``-v,`` ``-vvv)`` 

Set verbosity (higher verbosity with multiple e.g.

``--log-level`` OR    ENUM:value in {critical->5,debug->1,error->4,info->2,off->6,trace->0,warning->3}  {5,1,4,2,6,0,3} 

Set the log level

``-q,--quiet`` 

Set quiet mode (print less output)

``-y,--yes`` 

Automatically answer yes on prompted questions

``--json`` 

Report all output as json

``--offline`` 

Force use cached repodata

``--dry-run`` 

Only display what would have been done

``--experimental`` 

Enable experimental features


.. _commands_micromamba/prepend:

``prepend``
===========

**Version: 0.22.:**



Version: 0.22.0

**Usage:** ``micromamba [OPTIONS] [SUBCOMMAND]``

**Options:**

``-h,--help``

Print this help message and exit

``--version``
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


**Prefix options:**

``-r,--root-prefix`` TEXT

Path to the root prefix

``-p,--prefix`` TEXT

Path to the target prefix

``-n,--name`` TEXT

Name of the target prefix


**Subcommands:**



shell Generate shell init scripts



create Create new environment



install Install packages in active environment



update Update packages in active environment



repoquery Find and analyze packages in active environment or channels



remove Remove packages from active environment



list List packages in active environment



package Extract a package or bundle files into an archive



clean Clean package cache



config Configuration of micromamba



info Information about micromamba



constructor Commands to support using micromamba in constructor



env List environments



activate Activate an environment



run Run an executable in an environment



search Find packages in active environment or channels

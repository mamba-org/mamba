.. _commands_micromamba/update:

``update``
==========


Update packages in active environment

**Usage:** ``micromamba update [OPTIONS] [specs...]`` 

**Positionals:**

TEXT 

specs ... Specs to update in the environment


**Options:**

``-h,--help`` 

Print this help message and exit

``-c,--channel`` TEXT 

... Define the list of channels

``--override-channels`` 

Override channels

``--channel-priority`` OR    ENUM:value in {disabled->0,flexible->1,strict->2}  {0,1,2} 

Define the channel priority ('strict' or 'disabled')

``--channel-alias`` TEXT 

The prepended url location to associate with channel names

``--strict-channel-priority`` 

Enable strict channel priority

``--no-channel-priority`` 

Disable channel priority

``-f,--file`` TEXT 

... File (yaml, explicit or plain)

``--no-pin,--pin{false}`` 

Ignore pinned packages

``--no-py-pin,--py-pin{false}`` 

Do not automatically pin Python

``--pyc,--no-pyc{false}`` PYC 

Defines if files will be compiled or not

``--allow-uninstall,--no-allow-uninstall{false}`` 

Allow uninstall when installing or updating packages. Default is true.

``--allow-downgrade,--no-allow-downgrade{false}`` 

Allow downgrade when installing packages. Default is false.

``--allow-softlinks,--no-allow-softlinks{false}`` 

Allow to use soft-links when hard-links are not possible

``--always-softlink,--no-always-softlink{false}`` 

Use soft-link instead of hard-link

``--always-copy,--no-always-copy{false}`` 

Use copy instead of hard-link

``--extra-safety-checks,--no-extra-safety-checks{false}`` 

Run extra verifications on packages

``--lock-timeout`` UINT 

LockFile timeout

``--shortcuts,--no-shortcuts{false}`` 

Install start-menu shortcuts on Windows (not implemented on Linux / macOS)

``--safety-checks`` OR    ENUM:value in {disabled->0,enabled->2,warn->1}  {0,2,1} 

Safety checks policy ('enabled', 'warn', or 'disabled')

``--verify-artifacts`` 

Run verifications on packages signatures

``--platform`` TEXT 

The platform description

``--no-deps`` WILL 

Do not install dependencies. This lead to broken environments and inconsistent behavior. Use at your own risk

``--only-deps`` 

Only install dependencies

``--prune,--no-prune{false}`` 

Prune dependencies (default)

``-a,--all`` 

Update all packages in the environment


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


**Network options:**

``--ssl-verify`` TEXT SSL HTTPS 

Verify certificates for requests

``--ssl-no-revoke`` SSL 

certificate revocation checks

``--cacert-path`` TEXT SSL 

Path (file or directory) certificate(s)

``--repodata-ttl`` UINT 

Repodata time-to-live

``--retry-clean-cache`` 

If solve fails, try to fetch updated repodata


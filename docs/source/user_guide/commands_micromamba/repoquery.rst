.. _commands_micromamba/repoquery:

``repoquery``
=============


Find and analyze packages in active environment or channels

**Usage:** ``micromamba repoquery [OPTIONS] query_type specs...``

**Positionals:**

REQUIRED

query_type TEXT:{search,depends,whoneeds}



The type of query (search, depends or whoneeds)

TEXT REQUIRED

specs ... Specs to search


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

``-t,--tree``

Show result as a tree

``--pretty``

Pretty print result (only for search)

``--local,--remote{false}``

Use installed data or remote repositories

``--platform`` TEXT

The platform description


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

.. _commands_micromamba/repoquery:

``repoquery``
===========

Usage: ``micromamba repoquery [OPTIONS] query_type [specs...]``


**Positionals:**

``query_type TEXT:{search,depends,whoneeds} REQUIRED``

The type of query (search, depends or whoneeds).

``specs TEXT ...``

Specs to repoquery into the environment.


**Options:**

``-h,--help``

Print a help message and exit.

``-c,--channel TEXT ...``

Define the list of channels.

``--override-channels``

Override channels.

``--channel-priority TEXT:{disabled,flexible,strict}``

Define the channel priority ('strict' or 'disabled').

``--channel-alias TEXT``

The prepended url location to associate with channel names.

``--strict-channel-priority``

Enable strict channel priority.

``--no-channel-priority``

Disable channel priority.

``-t,--tree``

Show result as a tree.

``--pretty``

Pretty print result (only for search).

``--local,--remote{false}``

Use installed data or remote repositories.

``--platform TEXT``

The platform description.


**Configuration options:**

``--rc-file TEXT``

Paths to the configuration files to use.

``--no-rc``

Disable the use of configuration files.

``--no-env``

Disable the use of environment variables.


**Global options:**

``-v,--verbose``

Set verbosity (higher verbosity with multiple -v, e.g. -vvv).

``--log-level TEXT:{critical,error,warning,info,debug,trace,off}``

Set the log level.

``-q,--quiet``

Set quiet mode (print less output).

``-y,--yes``

Automatically answer yes on prompted questions.

``--json``

Report all output as json.

``--offline``

Force use cached repodata.

``--dry-run``

Only display what would have been done.

``--experimental``

Enable experimental features.


**Prefix options:**

``-r,--root-prefix TEXT``

Path to the root prefix.

``-p,--prefix TEXT``

Path to the target prefix.

``-n,--name TEXT``

Name of the target prefix.


**Network options:**

``--ssl-verify TEXT``

Verify SSL certificates for HTTPS requests.

``--ssl-no-revoke``

SSL certificate revocation checks.

``--cacert-path TEXT``

Path (file or directory) SSL certificate(s).

``--repodata-ttl INT``

Repodata time-to-live.

``--retry-clean-cache``

If solve fails, try to fetch updated repodata.

.. _commands_micromamba/install:

``install``
===========

Usage: ``micromamba install [OPTIONS] [specs...]``


**Positionals:**

``specs TEXT ...``

Specs to install into the environment.


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

``-f,--file TEXT ...``

File (yaml, explicit or plain).

``--no-pin,--pin{false}``

Ignore pinned packages.

``--no-py-pin,--py-pin{false}``

Do not automatically pin Python.

``--pyc,--no-pyc{false}``

Defines if PYC files will be compiled or not.

``--allow-uninstall,--no-allow-uninstall{false}``

Allow uninstall when installing or updating packages. Default is true.

``--allow-downgrade,--no-allow-downgrade{false}``

Allow downgrade when installing packages. Default is false.

``--allow-softlinks,--no-allow-softlinks{false}``

Allow to use soft-links when hard-links are not possible.

``--always-softlink,--no-always-softlink{false}``

Use soft-link instead of hard-link.

``--always-copy,--no-always-copy{false}``

Use copy instead of hard-link.

``--extra-safety-checks,--no-extra-safety-checks{false}``

Run extra verifications on packages.

``--lock-timeout INT``

LockFile timeout.

``--shortcuts,--no-shortcuts{false}``

Install start-menu shortcuts on Windows (not implemented on Linux / macOS).

``--safety-checks TEXT:{disabled,enabled,warn}``

Safety checks policy ('enabled', 'warn', or 'disabled').

``--verify-artifacts``

Run verifications on packages signatures.

``--platform TEXT``

The platform description.

``--freeze-installed``

Freeze already installed 


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

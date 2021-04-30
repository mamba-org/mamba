0.12.1 (Apr 30, 2021)
=====================

New features
- [micromamba] env list subcommand

Bug fixes
- [micromamba] fix multiple shell init with cmd.exe
- [micromamba] fix activate with --stack option
- [libmamba] only try loading ssl certificates when needed
- [micromamba] remove target_prefix checks when activating
- [micromamba] allow 'ultra-dry' config checks in final build

0.12.0 (Apr 26, 2021)
=====================

New features
- [libmamba] add experimental shell autocompletion (@wolfv) #900
- [libmamba] add token handling (@wolfv) #886
- [libmamba] add experimental pip support in spec files (@wolfv) #885

Bug fixes
- [libmamba] ignore failing pyc compilation for noarch packages (@wolfv) #904 #905
- [libmamba] fix string wrapping in error message (@bdice) #902
- [libmamba] fix cache error during remove operation (@adriendelsalle) #901
- [libmamba] add constraint with pinning during update operation (@wolfv) #892
- [libmamba] fix shell activate prefix check (@ashwinvis) #889
- [libmamba] make prefix mandatory for shell init (@adriendelsalle) #896
- [mamba] fix `env update` command (@ScottWales) #891

General improvements
- [libmamba] use lockfile, fix channel not loaded logic (@wolfv) #903
- [libmamba] make root_prefix warnings more selective (@adriendelsalle) #899
- [libmamba] house-keeping in python tests (@adriendelsalle) #898
- [libmamba] modify mamba/micromamba specific guards (@adriendelsalle) #895
- [libmamba] add simple lockfile mechanism (@wolfv) #894
- [libmamba] deactivate ca-certificates search when using offline mode (@adriendelsalle) #893

0.11.3 (Apr 21, 2021)
====================

- [libmamba] make platform rc configurable #883
- [libmamba] expand user home in target and root prefixes #882
- [libmamba] avoid memory effect between operations on target_prefix #881
- [libmamba] fix unnecessary throwing target_prefix check in `clean` operation #880
- [micromamba] fix `clean` flags handling #880
- [libmamba] C-API teardown on error #879

0.11.2 (Apr 21, 2021)
====================

- [libmamba] create "base" env only for install operation #875
- [libmamba] remove confirmation prompt of root_prefix in shell init #874
- [libmamba] improve overrides between target_prefix and env_name #873
- [micromamba] fix use of `-p,--prefix` and spec file env name #873

0.11.1 (Apr 20, 2021)
====================

- [libmamba] fix channel_priority computation #872

0.11.0 (Apr 20, 2021)
====================

- [libmamba] add experimental mode that unlock edge features #858
- [micromamba] add `--experimental` umamba flag to enable experimental mode #858
- [libmamba] improve base env creation #860
- [libmamba] fix computation of weakly canonical target prefix #859
- update libsolv dependency in env-dev.yml file, update documentation (thanks @Aratz) #843
- [libmamba] handle package cache in secondary locations, fix symlink errors (thanks wenjuno) #856
- [libmamba] fix CI cURL SSL error on macos with Darwin backend (thanks @wolfv) #865
- [libmamba] improve error handling in C-API by catching and returning an error code #862
- [libmamba] handle configuration lifetime (single operation configs) #863
- [libmamba] enable ultra-dry C++ tests #868
- [libmamba] migrate `config` operation implem from `micromamba` to `libmamba` API #866
- [libmamba] add capapbility to set CLI config from C-API #867

0.10.0 (Apr 16, 2021)
====================

- [micromamba] allow creation of empty env (without specs) #824 #827
- [micromamba] automatically create empy `base` env at new root prefix #836
- [micromamba] add remove all CLI flags `-a,--all` #824
- [micromamba] add dry-run and ultra-dry-run tests to increase coverage and speed-up CI #813 #845
- [micromamba] allow CLI to override spec file env name (create, install and update) #816
- [libmamba] split low-level and high-level API #821 #824
- [libmamba] add a C high-level API #826
- [micromamba] support `__linux` virtual package #829
- [micromamba] improve the display of solver problems #822
- [micromamba] improve info sub-command with target prefix status (active, not found, etc.) #825
- [mamba] Change pybind11 to a build dependency (thanks @maresb) #846
- [micromamba] add shell detection for shell sub-command #839
- [micromamba] expand user in shell prefix sub-command #831
- [micromamba] refactor explicit specs install #824
- [libmamba] improve configuration (refactor API, create a loading sequence) #840
- [libmamba] support cpp-filesystem breaking changes on Windows fs #849
- [libmamba] add a simple context debugging (thanks @wolf) #820
- [libmamba] improve C++ test suite #830
- fix CI C++ tests (unix/libmamba) and Python tests (win/mamba) wrongly successful #853

0.9.2 (Apr 1, 2021)
====================

- [micromamba] fix unc url support (thanks @adament)
- [micromamba] add --channel-alias as cli option to micromamba (thanks @adriendelsalle)
- [micromamba] fix --no-rc and environment yaml files (thanks @adriendelsalle)
- [micromamba] handle spec files in update and install subcommands (thanks @adriendelsalle)
- add simple context debugging, dry run tests and other test framework improvements

0.9.1 (Mar 26, 2021)
====================

- [micromamba] fix remove command target_prefix selection
- [micromamba] improve target_prefix fallback for CLI, add tests (thanks @adriendelsalle)

0.9.0 (Mar 25, 2021)
====================

- [micromamba] use strict channels priority by default
- [micromamba] change config precedence order: API>CLI>ENV>RC
- [micromamba] `config list` sub command optional display of sources, defaults, short/long descriptions and groups
- [micromamba] prevent crashes when no bashrc or zshrc file found (thanks @wolfv)
- add support for UNC file:// urls (thanks @adament)
- add support for use_only_tar_bz2 (thanks @tl-hbk @wolfv)
- add pinned specs for env update (thanks @wolfv)
- properly adhere to run_constrains (thanks @wolfv)

0.8.2 (Mar 12, 2021)
====================

- [micromamba] fix setting network options before explicit spec installation
- [micromamba] fix python based tests for windows


0.8.1 (Mar 11, 2021)
====================

- use stoull (instead of stoi) to prevent overflow with long package build numbers (thanks @pbauwens-kbc)
- [micromamba] fixing OS X certificate search path
- [micromamba] refactor default root prefix, make it configurable from CLI (thanks @adriendelsalle)
- [micromamba] set ssl backend, use native SSL if possible (thanks @adriendelsalle)
- [micromamba] sort json transaction, and add UNLINK field
- [micromamba] left align log messages
- [micromamba] libsolv log messages to stderr (thanks @mariusvniekerk)
- [micromamba] better curl error messages


0.8.0 (Mar 5, 2021)
===================

- [micromamba] condarc and mambarc config file reading (and config subcommand) (thanks @adriendelsalle)
- [micromamba] support for virtual packages (thanks @adriendelsalle)
- [micromamba] set ssl backend, use native SSL if possible
- [micromamba] add python based testing framework for CLI
- [micromamba] refactor CLI and micromamba main file (thanks @adriendelsalle)
- [micromamba] add linking options (--always-copy etc.) (thanks @adriendelsalle)
- [micromamba] fix multiple prefix replacements in binary files
- [micromamba] fix micromamba clean (thanks @phue)
- [micromamba] change package validation settings to --safety-checks and --extra-safety-checks
- [micromamba] add update subcommand (thanks @adriendelsalle)
- [micromamba] support pinning packages (including python minor version) (thanks @adriendelsalle)
- [micromamba] add try/catch to WinReg getStringValue (thanks @SvenAdler)
- [micromamba] add ssl-no-revoke option for more conda-compatibility (thanks @tl-hbk)
- [micromamba] die when no ssl certificates are found (thanks @wholtz)
- [docs] add explanation for base env install (thanks @ralexx) and rename changelog to .md (thanks @kevinheavey)
- [micromamba] compare cleaned URLs for cache invalidation
- [micromamba] add regex handling to list command


0.7.14 (Feb 12, 2021)
=====================

- [micromamba] better validation of extracted directories
- [mamba] add additional tests for authentication and simple repodata server
- make LOG_WARN the default log level, and move some logs to INFO
- [micromamba] properly replace long shebangs when linking
- [micromamba] add quote for shell for prefixes with spaces
- [micromamba] add clean functionality
- [micromamba] always make target prefix path absolute


0.7.13 (Feb 4, 2021)
====================

- [micromamba] Immediately exit after printing version (again)

0.7.12 (Feb 3, 2021)
====================

- [micromamba] Improve CTRL+C signal handling behavior and simplify code
- [micromamba] Revert extraction to temporary directory because of invalid cross-device links on Linux
- [micromamba] Clean up partially extracted archives when CTRL+C interruption occured

0.7.11 (Feb 2, 2021)
====================

- [micromamba] use wrapped call when compiling noarch Python code, which properly calls chcp for Windows
- [micromamba] improve checking the pkgs cache
- [mamba] fix authenticated URLs (thanks @wenjuno)
- first extract to temporary directory, then move to final pkgs cache to prevent corrupted extracted data

0.7.10 (Jan 22, 2021)
====================

- [micromamba] properly fix PATH when linking, prevents missing vcruntime140.dll
- [mamba] add virtual packages when creating any environment, not just on update (thanks @cbalioglu)

0.7.9 (Jan 19, 2021)
====================

- [micromamba] fix PATH when linking

0.7.8 (Jan 14, 2021)
====================

- [micromamba] retry on corrupted repodata
- [mamba & micromamba] fix error handling when writing repodata

0.7.6 (Dec 22, 2020)
====================

- [micromamba] more console flushing for std::cout consumers

0.7.6 (Dec 14, 2020)
====================

- [mamba] more arguments for repodata.create_pool

0.7.5 (Dec 10, 2020)
====================

- [micromamba] better error handling for YAML file reading, allows to pass in `-n` and `-p` from command line
- [mamba & micromamba] ignore case of HTTP headers
- [mamba] fix channel keys are without tokens (thanks @s22chan)

0.7.4 (Dec 5, 2020)
====================

- [micromamba] fix noarch installation for explicit environments

0.7.3 (Nov 20, 2020)
====================

- [micromamba] fix installation of noarch files with long prefixes
- [micromamba] fix activation on windows with whitespaces in root prefix (thanks @adriendelsalle)
- [micromamba] add `--json` output to micromamba list

0.7.2 (Nov 18, 2020)
====================

- [micromamba] explicit specs installing should be better now
	- empty lines are ignored
	- network settings are correctly set to make ssl verification work
- New Python repoquery API for mamba
- Fix symlink packing for mamba package creation and transmute
- Do not keep tempfiles around

0.7.1 (Nov 16, 2020)
====================

- Handle LIBARCHIVE_WARN to not error, instead print warning (thanks @obilaniu)

0.7.0 (Nov 12, 2020)
====================

- Improve activation and deactivation logic for micromamba
- Switching `subprocess` implementation to more tested `reproc++`
- Fixing Windows noarch entrypoints generation with micromamba
- Fix pre-/post-link script running with micromamba to use micromamba activation logic
- Empty environment creation skips all repodata downloading & solving
- Fix micromamba install when environment is activated (thanks @isuruf)
- Micromamba now respects the $CONDA_SUBDIR environment variable (thanks @mariusvniekerk)
- Fix compile time warning (thanks @obilaniu)
- Fixed wrong CondaValueError import statement in mamba.py (thanks @saraedum)

0.6.5 (Oct 2020)
================

- Fix code signing for Apple Silicon (osx-arm64) @isuruf

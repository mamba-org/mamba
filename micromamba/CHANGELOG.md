# micromamba 2.0.6 (February 04, 2025)

Enhancements:

- Add reverse flag to list command by @SandrineP in https://github.com/mamba-org/mamba/pull/3705
- Add md5 flag to list command by @SandrineP in https://github.com/mamba-org/mamba/pull/3773
- add canonical flag to list command by @SandrineP in https://github.com/mamba-org/mamba/pull/3777

Bug fixes:

- Correctly populate lists of `MatchSpec` in `MTransaction`'s history by @Hind-M in https://github.com/mamba-org/mamba/pull/3724
- Honour `CONDA_ENVS_PATH` again by @jjerphan in https://github.com/mamba-org/mamba/pull/3725
- Improve CUDA version detection by @jjerphan in https://github.com/mamba-org/mamba/pull/3700
- Support installation using explicit url by @Hind-M in https://github.com/mamba-org/mamba/pull/3710
- Improve display of environment activation message by @Hind-M in https://github.com/mamba-org/mamba/pull/3715
- Adapt warnings shown when several channels are used by @jjerphan in https://github.com/mamba-org/mamba/pull/3720
- Add a hint on cache corruption by @jjerphan in https://github.com/mamba-org/mamba/pull/3736
- Support more condarc paths by @SandrineP in https://github.com/mamba-org/mamba/pull/3695
- Always add `root_prefix/envs` in `envs_dirs` by @Hind-M in https://github.com/mamba-org/mamba/pull/3692
- Options args enhancement by @Hind-M in https://github.com/mamba-org/mamba/pull/3722
- Support globs in `MatchSpec` build strings by @jjerphan in https://github.com/mamba-org/mamba/pull/3735
- Don't encode URLs for `mamba env export --explicit` by @maresb in https://github.com/mamba-org/mamba/pull/3745
- Handle `git+https` pip urls by @Hind-M in https://github.com/mamba-org/mamba/pull/3764
- Uncomment no more failing test by @Hind-M in https://github.com/mamba-org/mamba/pull/3767
- Use CA certificates from `conda-forge::ca-certificates` by @jjerphan in https://github.com/mamba-org/mamba/pull/3765
- Add explicit flag to list command by @SandrineP in https://github.com/mamba-org/mamba/pull/3760
- Fix dependency and `subdir` in repoquery `whoneeds` by @Hind-M in https://github.com/mamba-org/mamba/pull/3743
- Use `LOG_DEBUG` for CUDA version detection by @jjerphan in https://github.com/mamba-org/mamba/pull/3757
- Add missing thread and undefined sanitizers CMake options by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3753
- Factor handling of `GetModuleFileNameW` by @jjerphan in https://github.com/mamba-org/mamba/pull/3785
- Adapt root prefix determination by @jjerphan in https://github.com/mamba-org/mamba/pull/3782
- Remove pip warning for `PIP_NO_PYTHON_VERSION_WARNING` by @Hind-M in https://github.com/mamba-org/mamba/pull/3770
- Use `libmamba`'s installation instead of `mamba`'s as a fallback by @jjerphan in https://github.com/mamba-org/mamba/pull/3792
- Fix typo in Windows workflows by @jjerphan in https://github.com/mamba-org/mamba/pull/3793
- Rerun pytest tests on `main` in case of failures by @jjerphan in https://github.com/mamba-org/mamba/pull/3769

CI fixes and doc:

- Use a portable web request for Windows by @jjerphan in https://github.com/mamba-org/mamba/pull/3704
- Add prettier pre-commit hook by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3663
- Document slight differences for environment export by @jjerphan in https://github.com/mamba-org/mamba/pull/3697
- Unique Release Tag by @Klaim in https://github.com/mamba-org/mamba/pull/3732
- Update Linux installation script for Nushell by @deephbz in https://github.com/mamba-org/mamba/pull/3721
- `update_changelog.py` now can also take input as cli parameters by @Klaim in https://github.com/mamba-org/mamba/pull/3731

Maintenance:

- `list` refactoring by @SandrineP in https://github.com/mamba-org/mamba/pull/3768
- Correctly exclude json files in clang-format by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3749
- Fix build status badge by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3755
- Don't exclude Changelog files from typos-conda by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3748
- Update pre-commit hooks by by @mathbunnyru https://github.com/mamba-org/mamba/pull/3746

# micromamba 2.0.6.rc3 (February 04, 2025)

Enhancement:

- add canonical flag to list command by @SandrineP in https://github.com/mamba-org/mamba/pull/3777

Bug fixes:

- Use `libmamba`'s installation instead of `mamba`'s as a fallback by @jjerphan in https://github.com/mamba-org/mamba/pull/3792

Maintenance:

- Fix typo in Windows workflows by @jjerphan in https://github.com/mamba-org/mamba/pull/3793
- Rerun pytest tests on `main` in case of failures by @jjerphan in https://github.com/mamba-org/mamba/pull/3769

# micromamba 2.0.6.rc2 (January 31, 2025)

Enhancements:

- [all] Add md5 flag to list command by @SandrineP in https://github.com/mamba-org/mamba/pull/3773

Bug fixes:

- [all] Factor handling of `GetModuleFileNameW` by @jjerphan in https://github.com/mamba-org/mamba/pull/3785
- [all] Adapt root prefix determination by @jjerphan in https://github.com/mamba-org/mamba/pull/3782
- [all] Remove pip warning for `PIP_NO_PYTHON_VERSION_WARNING` by @Hind-M in https://github.com/mamba-org/mamba/pull/3770

# micromamba 2.0.6.rc1 (January 28, 2025)

Enhancements:

- Add reverse flag to list command by @SandrineP in https://github.com/mamba-org/mamba/pull/3705

Bug fixes:

- Support globs in `MatchSpec` build strings by @jjerphan in https://github.com/mamba-org/mamba/pull/3735
- Don't encode URLs for `mamba env export --explicit` by @maresb in https://github.com/mamba-org/mamba/pull/3745
- Handle `git+https` pip urls by @Hind-M in https://github.com/mamba-org/mamba/pull/3764
- Uncomment no more failing test by @Hind-M in https://github.com/mamba-org/mamba/pull/3767
- Use CA certificates from `conda-forge::ca-certificates` by @jjerphan in https://github.com/mamba-org/mamba/pull/3765
- Add explicit flag to list command by @SandrineP in https://github.com/mamba-org/mamba/pull/3760
- Fix dependency and `subdir` in repoquery `whoneeds` by @Hind-M in https://github.com/mamba-org/mamba/pull/3743
- Use `LOG_DEBUG` for CUDA version detection by @jjerphan in https://github.com/mamba-org/mamba/pull/3757
- Add missing thread and undefined sanitizers CMake options by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3753

Maintenance:

- `list` refactoring by @SandrineP in https://github.com/mamba-org/mamba/pull/3768
- Correctly exclude json files in clang-format by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3749
- Fix build status badge by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3755
- Don't exclude Changelog files from typos-conda by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3748
- Update pre-commit hooks by by @mathbunnyru https://github.com/mamba-org/mamba/pull/3746

# micromamba 2.0.6.rc0 (January 14, 2025)

Bug fixes:

- Correctly populate lists of `MatchSpec` in `MTransaction`'s history by @Hind-M in https://github.com/mamba-org/mamba/pull/3724
- Honour `CONDA_ENVS_PATH` again by @jjerphan in https://github.com/mamba-org/mamba/pull/3725
- Improve CUDA version detection by @jjerphan in https://github.com/mamba-org/mamba/pull/3700
- Support installation using explicit url by @Hind-M in https://github.com/mamba-org/mamba/pull/3710
- Improve display of environment activation message by @Hind-M in https://github.com/mamba-org/mamba/pull/3715
- Adapt warnings shown when several channels are used by @jjerphan in https://github.com/mamba-org/mamba/pull/3720
- Add a hint on cache corruption by @jjerphan in https://github.com/mamba-org/mamba/pull/3736
- Support more condarc paths by @SandrineP in https://github.com/mamba-org/mamba/pull/3695
- Always add `root_prefix/envs` in `envs_dirs` by @Hind-M in https://github.com/mamba-org/mamba/pull/3692
- Options args enhancement by @Hind-M in https://github.com/mamba-org/mamba/pull/3722

CI fixes and doc:

- Use a portable web request for Windows by @jjerphan in https://github.com/mamba-org/mamba/pull/3704
- Add prettier pre-commit hook by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3663
- Document slight differences for environment export by @jjerphan in https://github.com/mamba-org/mamba/pull/3697
- Unique Release Tag by @Klaim in https://github.com/mamba-org/mamba/pull/3732
- Update Linux installation script for Nushell by @deephbz in https://github.com/mamba-org/mamba/pull/3721
- `update_changelog.py` now can also take input as cli parameters by @Klaim in https://github.com/mamba-org/mamba/pull/3731

# micromamba 2.0.5 (December 12, 2024)

Enhancements:

- `micromamba/mamba --version` displays pre-release version names + establishes pre-release versions name scheme by @Klaim in https://github.com/mamba-org/mamba/pull/3639

Bug fixes:

- fix: Skip empty lines in environment spec files by @jjerphan in https://github.com/mamba-org/mamba/pull/3662
- Handle `.tar.gz` in pkg url by @Hind-M in https://github.com/mamba-org/mamba/pull/3640
- fix: Effectively apply dry-run on installation from PyPI by @jjerphan in https://github.com/mamba-org/mamba/pull/3644
- fix: Handle environment with empty or absent `dependencies` by @jjerphan in https://github.com/mamba-org/mamba/pull/3657
- fix: Reintroduce the `uninstall` command by @jjerphan in https://github.com/mamba-org/mamba/pull/3650

CI fixes and doc:

- Introducing mamba Guru on Gurubase.io by @kursataktas in https://github.com/mamba-org/mamba/pull/3612
- build: Remove server by @jjerphan in https://github.com/mamba-org/mamba/pull/3685
- docs: Clarify installation of lock file by @jjerphan in https://github.com/mamba-org/mamba/pull/3686
- maint: Add pre-commit typos back by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3682
- maint: Cleanup CMake files and delete not compiled files by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3667
- maint: Add pyupgrade pre-commit hook by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3671
- docs: Adapt shell completion subsection by @jjerphan in https://github.com/mamba-org/mamba/pull/3672
- maint: Restructure docs configuration file and improve docs pages by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3615
- docs: Remove installation non-recommendation by @jjerphan in https://github.com/mamba-org/mamba/pull/3656

# micromamba 2.0.5.rc0 (December 09, 2024)

Enhancements:

- `micromamba/mamba --version` displays pre-release version names + establishes pre-release versions name scheme by @Klaim in https://github.com/mamba-org/mamba/pull/3639

Bug fixes:

- fix: Skip empty lines in environment spec files by @jjerphan in https://github.com/mamba-org/mamba/pull/3662
- Handle `.tar.gz` in pkg url by @Hind-M in https://github.com/mamba-org/mamba/pull/3640
- fix: Effectively apply dry-run on installation from PyPI by @jjerphan in https://github.com/mamba-org/mamba/pull/3644
- fix: Handle environment with empty or absent `dependencies` by @jjerphan in https://github.com/mamba-org/mamba/pull/3657
- fix: Reintroduce the `uninstall` command by @jjerphan in https://github.com/mamba-org/mamba/pull/3650

CI fixes and doc:

- maint: Cleanup CMake files and delete not compiled files by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3667
- maint: Add pyupgrade pre-commit hook by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3671
- docs: Adapt shell completion subsection by @jjerphan in https://github.com/mamba-org/mamba/pull/3672
- maint: Restructure docs configuration file and improve docs pages by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3615
- docs: Remove installation non-recommendation by @jjerphan in https://github.com/mamba-org/mamba/pull/3656

# micromamba 2.0.4 (November 22, 2024)

Enhancements:

- feat: List PyPI packages in environment export by @jjerphan in https://github.com/mamba-org/mamba/pull/3623
- pip packages support with `list` by @Hind-M in https://github.com/mamba-org/mamba/pull/3565

Bug fixes:

- fix: Return JSON on environment creation dry run by @jjerphan in https://github.com/mamba-org/mamba/pull/3627
- maint: Address compiler warnings by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3605
- fix: Export `'channels'` as part of environments' export by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3587
- Remove Taskfile from `environment-dev-extra.yml` by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3597
- fixed incorrect syntax in static_build.yml by @Klaim in https://github.com/mamba-org/mamba/pull/3592
- fix: Correct `mamba env export --json --from-history` by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3590
- Fix test on windows by @Hind-M in https://github.com/mamba-org/mamba/pull/3555
- fix: JSON output for environment export by @jjerphan in https://github.com/mamba-org/mamba/pull/3559
- fix: Support `conda env export` `no-builds` flag by @jjerphan in https://github.com/mamba-org/mamba/pull/3563
- fix: Export the environment prefix in specification by @jjerphan in https://github.com/mamba-org/mamba/pull/3562
- Fix relative path in local channel by @Hind-M in https://github.com/mamba-org/mamba/pull/3540
- Correctly rename test to be run by @Hind-M in https://github.com/mamba-org/mamba/pull/3545
- Create empty base prefix with `env update` by @Hind-M in https://github.com/mamba-org/mamba/pull/3519
- fix: Use POSIX-compliant scripts by @jjerphan in https://github.com/mamba-org/mamba/pull/3522
- maint: Clarify `env` subcommand documentation in help menu (cont'd) by @jjerphan in https://github.com/mamba-org/mamba/pull/3539
- maint: Clarify `env` subcommand documentation in help menu by @jjerphan in https://github.com/mamba-org/mamba/pull/3502
- fix: Adapt `test_env_update_pypi_with_conda_forge` by @jjerphan in https://github.com/mamba-org/mamba/pull/3537

CI fixes and doc:

- ci: add brew toolchain test by @henryiii in https://github.com/mamba-org/mamba/pull/3625
- doc: show how to use advanced match specs in yaml spec by @corneliusroemer in https://github.com/mamba-org/mamba/pull/3384
- Doc: how to install specific Micromamba version by @truh in https://github.com/mamba-org/mamba/pull/3517
- doc: Homebrew currently only installs micromamba v1 by @corneliusroemer in https://github.com/mamba-org/mamba/pull/3499
- maint: Add dependabot config for GitHub workflows/actions by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3614
- maint: Unify `cmake` calls in workflows, build win static builds in p… by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3616
- docs: Update pieces of documentation after the release of mamba 2 by @jjerphan in https://github.com/mamba-org/mamba/pull/3610
- Update pre-commit hooks except clang-format by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3599
- Force spinx v6 in readthedocs by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3586
- Fix doc by @Hind-M in https://github.com/mamba-org/mamba/pull/3568
- [windows-vcpkg] Replace deprecated openssl with crypto feature with latest libarchive by @Hind-M in https://github.com/mamba-org/mamba/pull/3556
- maint: Unpin libcurl<8.10 by @jjerphan in https://github.com/mamba-org/mamba/pull/3548
- dev: Remove the use of Taskfile by @jjerphan in https://github.com/mamba-org/mamba/pull/3544
- Upgraded CI to micromamba 2.0.2 by @JohanMabille in https://github.com/mamba-org/mamba/pull/3497

# micromamba 2.0.4alpha3 (November 21, 2024)

Enhancements:

- feat: List PyPI packages in environment export by @jjerphan in https://github.com/mamba-org/mamba/pull/3623

Bug fixes:

- maint: Address compiler warnings by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3605
- fix: Export `'channels'` as part of environments' export by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3587

CI fixes and doc:

- doc: show how to use advanced match specs in yaml spec by @corneliusroemer in https://github.com/mamba-org/mamba/pull/3384
- Doc: how to install specific Micromamba version by @truh in https://github.com/mamba-org/mamba/pull/3517
- doc: Homebrew currently only installs micromamba v1 by @corneliusroemer in https://github.com/mamba-org/mamba/pull/3499
- maint: Add dependabot config for GitHub workflows/actions by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3614
- maint: Unify `cmake` calls in workflows, build win static builds in p… by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3616
- docs: Update pieces of documentation after the release of mamba 2 by @jjerphan in https://github.com/mamba-org/mamba/pull/3610

# micromamba 2.0.4alpha2 (November 14, 2024)

Bug fixes:

- Remove Taskfile from `environment-dev-extra.yml` by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3597

CI fixes and doc:

- Update pre-commit hooks except clang-format by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3599
- Force spinx v6 in readthedocs by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3586

# micromamba 2.0.4alpha1 (November 12, 2024)

Bug fixes:

- fixed incorrect syntax in static_build.yml by @Klaim in https://github.com/mamba-org/mamba/pull/3592
- fix: Correct `mamba env export --json --from-history` by @mathbunnyru in https://github.com/mamba-org/mamba/pull/3590

# micromamba 2.0.4alpha0 (November 12, 2024)

# micromamba 2.0.3 (November 05, 2024)

Enhancements:

- pip packages support with `list` by @Hind-M in https://github.com/mamba-org/mamba/pull/3565

Bug fixes:

- Fix test on windows by @Hind-M in https://github.com/mamba-org/mamba/pull/3555
- fix: JSON output for environment export by @jjerphan in https://github.com/mamba-org/mamba/pull/3559
- fix: Support `conda env export` `no-builds` flag by @jjerphan in https://github.com/mamba-org/mamba/pull/3563
- fix: Export the environment prefix in specification by @jjerphan in https://github.com/mamba-org/mamba/pull/3562
- Fix relative path in local channel by @Hind-M in https://github.com/mamba-org/mamba/pull/3540
- Correctly rename test to be run by @Hind-M in https://github.com/mamba-org/mamba/pull/3545
- Create empty base prefix with `env update` by @Hind-M in https://github.com/mamba-org/mamba/pull/3519
- fix: Use POSIX-compliant scripts by @jjerphan in https://github.com/mamba-org/mamba/pull/3522
- maint: Clarify `env` subcommand documentation in help menu (cont'd) by @jjerphan in https://github.com/mamba-org/mamba/pull/3539
- maint: Clarify `env` subcommand documentation in help menu by @jjerphan in https://github.com/mamba-org/mamba/pull/3502
- fix: Adapt `test_env_update_pypi_with_conda_forge` by @jjerphan in https://github.com/mamba-org/mamba/pull/3537

CI fixes and doc:

- Fix doc by @Hind-M in https://github.com/mamba-org/mamba/pull/3568
- [windows-vcpkg] Replace deprecated openssl with crypto feature with latest libarchive by @Hind-M in https://github.com/mamba-org/mamba/pull/3556
- maint: Unpin libcurl<8.10 by @jjerphan in https://github.com/mamba-org/mamba/pull/3548
- dev: Remove the use of Taskfile by @jjerphan in https://github.com/mamba-org/mamba/pull/3544
- Upgraded CI to micromamba 2.0.2 by @JohanMabille in https://github.com/mamba-org/mamba/pull/3497

# micromamba 2.0.2 (October 02, 2024)

Bug fixes:

- fix: Handle `MatchSpec` with brackets when parsing environments' history by @jjerphan in https://github.com/mamba-org/mamba/pull/3490
- Fix `channel` and `base_url` in `list` cmd by @Hind-M in https://github.com/mamba-org/mamba/pull/3488

CI fixes and doc:

- Rollback to micromamba 1.5.10 in CI by @JohanMabille in https://github.com/mamba-org/mamba/pull/3491

# micromamba 2.0.1 (September 30, 2024)

Bug fixes:

- --full-name option for list by @JohanMabille in https://github.com/mamba-org/mamba/pull/3485
- fix: Support for PEP 440 "Compatible Releases" (operator `~=` for `MatchSpec`) by @jjerphan in https://github.com/mamba-org/mamba/pull/3483
- Added --copy flag to create and install commands by @JohanMabille in https://github.com/mamba-org/mamba/pull/3474

CI fixes and doc:

- doc: add github links to documentation by @timhoffm in https://github.com/mamba-org/mamba/pull/3471

# micromamba 2.0.0 (September 25, 2024)

Enhancements:

- Support CONDA_DEFAULT_ENV by @SylvainCorlay in https://github.com/mamba-org/mamba/pull/3445
- Remove cctools patch from feedstock in CI by @JohanMabille in https://github.com/mamba-org/mamba/pull/3442
- test: Adapt test_explicit_export_topologically_sorted by @jjerphan in https://github.com/mamba-org/mamba/pull/3377
- build: Support fmt 11 by @jjerphan in https://github.com/mamba-org/mamba/pull/3368
- OCI/Conda mapping by @Hind-M in https://github.com/mamba-org/mamba/pull/3310
- [OCI - Mirrors] Add tests and doc by @Hind-M in https://github.com/mamba-org/mamba/pull/3307
- Add checking typos to pre-commit by @Hind-M in https://github.com/mamba-org/mamba/pull/3278
- Update pre-commit hooks" by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3252
- Refactor os utilities by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3248
- [mamba-content-trust] Add integration test by @Hind-M in https://github.com/mamba-org/mamba/pull/3234
- Custom resolve complex MatchSpec in Solver by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3233
- [mamba content trust] Enable verifying packages signatures by @Hind-M in https://github.com/mamba-org/mamba/pull/3192
- Subdir renaming by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3214
- Expected in specs parse API by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3207
- Added HTTP Mirrors by @JohanMabille in https://github.com/mamba-org/mamba/pull/3178
- Use expected for specs parsing by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3201
- Rename MPool into solver::libsolv::Database by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3180
- Automate releases (`CHANGELOG.md` updating) by @Hind-M in https://github.com/mamba-org/mamba/pull/3179
- Simplify MPool Interface by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3177
- Clean libsolv use in Transaction by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3171
- Rewrite Query with Pool functions (wrapping libsolv) by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3168
- Remove hard coded mamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3069
- Support multiple env yaml specs by @jchorl in https://github.com/mamba-org/mamba/pull/2993
- Duplicate reposerver to isolate micromamba tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3071
- Some future proofing MatchSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3082
- Remove micromamba shell init -p by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3092
- Clean PackageInfo interface by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3103
- Move PackageInfo in specs:: by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3109
- Move util_random.hpp > util/random.hpp by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3129
- Refactor test_remove.py to use fixture by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3131
- MRepo refactor by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3118
- Explicit transaction duplicate code by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3138
- Solver Request by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3141
- Split Solver and Unsolvable by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3156
- Solver sort deps by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3163
- Context: not a singleton by @Klaim in https://github.com/mamba-org/mamba/pull/2615
- Add env update by @Hind-M in https://github.com/mamba-org/mamba/pull/2827
- Adding locks for cache directories by @rmittal87 in https://github.com/mamba-org/mamba/pull/2811
- Refactor tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2829
- No ugly kenum by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2831
- Add Nushell activation support by cvanelteren in https://github.com/mamba-org/mamba/pull/2693
- Channel cleanup by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2832
- Don't force MSVC_RUNTIME by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2861
- Add comments in micromamba repoquery by @Hind-M in https://github.com/mamba-org/mamba/pull/2863
- Fix Posix shell on Windows by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2803
- Use CMake targets for reproc by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2883
- Add mamba tests by @Hind-M in https://github.com/mamba-org/mamba/pull/2877
- Filesystem library by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2879
- Add multiple queries to repoquery search by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2897
- Add ChannelSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2870
- Make some fixture local by @JohanMabille in https://github.com/mamba-org/mamba/pull/2919
- Added PackageFetcher by @JohanMabille in https://github.com/mamba-org/mamba/pull/2917
- Resolve ChannelSpec into a Channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2899
- Refactor win encoding conversion by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2939
- Move reposerver tests to micromamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2941
- Remove mamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2942
- Dev workflow by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2948
- Add refactor getenv setenv unsetenv by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2944
- Explicit and smart CMake target by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2935
- Rename env functions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2954
- Update dependencies on OSX by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2976
- Channel initialization by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2953
- Refactor env directories by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2983
- Migrate expand/shrink_home by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2990
- Refactor env::which by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2997
- Migrate Channel::make_channel to resolve multi channels by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2986
- Move core/channel > specs/channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3000
- Remove ChannelContext context capture by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3015
- Default to hide credentials by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3017
- Refactor (some) OpenSSL functions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3024
- Default to conda-forge channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3035
- Plug ChannelSpec in MatchSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3046
- Change MatchSpec::parse to named constructor by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3048
- Added mamba as dynamic build of micromamba by @JohanMabille in https://github.com/mamba-org/mamba/pull/3060

Bug fixes:

- fix: Handle extra white-space in `MatchSpec` by @jjerphan in https://github.com/mamba-org/mamba/pull/3456
- Fix `test_env_update_pypi_with_conda_forge` by @Hind-M in https://github.com/mamba-org/mamba/pull/3459
- fix: Environment removal confirmation by @jjerphan in https://github.com/mamba-org/mamba/pull/3450
- Fix test in osx by @Hind-M in https://github.com/mamba-org/mamba/pull/3448
- Add fallback to root prefix by @Hind-M in https://github.com/mamba-org/mamba/pull/3435
- Fixed micromamba static build after cctools and ld64 upgrade on conda… by @JohanMabille in https://github.com/mamba-org/mamba/pull/3436
- fix: PyPI support for `env update` by @jjerphan in https://github.com/mamba-org/mamba/pull/3419
- Update mamba.sh.in script by @SylvainCorlay in https://github.com/mamba-org/mamba/pull/3422
- test: Adapt `test_remove_orphaned` unlinks by @jjerphan in https://github.com/mamba-org/mamba/pull/3417
- fix: Reduce logging system overhead by @jjerphan in https://github.com/mamba-org/mamba/pull/3416
- Define `etc/profile.d/mamba.sh` and install it by @jjerphan in https://github.com/mamba-org/mamba/pull/3413
- Add posix to supported shells by @jjerphan in https://github.com/mamba-org/mamba/pull/3412
- Replaces instances of -p with --root-prefix in documentation by @SylvainCorlay in https://github.com/mamba-org/mamba/pull/3411
- [micromamba] Fix behavior of `env update` (to mimic conda) by @Hind-M in https://github.com/mamba-org/mamba/pull/3396
- Attempt to fix `test_proxy_install` by @Hind-M in https://github.com/mamba-org/mamba/pull/3324
- Fix `test_no_python_pinning` by @Hind-M in https://github.com/mamba-org/mamba/pull/3321
- Split `ContextOptions::enable_logging_and_signal_handling` into 2 different options by @Klaim in https://github.com/mamba-org/mamba/pull/3329
- Fix test_no_python_pinning by @Hind-M in https://github.com/mamba-org/mamba/pull/3319
- Fix release scripts by @Hind-M in https://github.com/mamba-org/mamba/pull/3306
- Fix VersionSpec equal and glob by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3269
- Use conda-forge feedstock for static builds by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3249
- Mamba 2.0 name fixes by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3225
- Make Taskfile.dist.yml Windows-compatible by @carschandler in https://github.com/mamba-org/mamba/pull/3219
- Remove unmaintained and broken pytest-lazy-fixture by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3193
- Fix URL encoding in repodata.json by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3076
- gracefully handle conflicting names in yaml specs by @jchorl in https://github.com/mamba-org/mamba/pull/3083
- add manually given .tar.bz2 / .conda packages to solver pool by @0xbe7a in https://github.com/mamba-org/mamba/pull/3164
- Fix linking on Windows when Scripts folder is missing by @dalcinl in https://github.com/mamba-org/mamba/pull/2825
- Fix win test micro.mamba.pm by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2888
- Add CI test for local channels by @Hind-M in https://github.com/mamba-org/mamba/pull/2854
- Fixed "micromamba package transmute names files going from .conda -> .tar.bz2 incorrectly" by @mariusvniekerk in https://github.com/mamba-org/mamba/issues/2911
- Fix micromamba test dependency conda-package-handling by @rominf in https://github.com/mamba-org/mamba/pull/2945
- Add cmake-format by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2962
- removed dependency on conda-index by @JohanMabille in https://github.com/mamba-org/mamba/pull/2964

CI fixes and doc:

- Fix wrong version of miniforge in doc by @Hind-M in https://github.com/mamba-org/mamba/pull/3462
- Remove cctools patch removal in CI by @Hind-M in https://github.com/mamba-org/mamba/pull/3451
- docs: Specify `CMAKE_INSTALL_PREFIX` by @jjerphan in https://github.com/mamba-org/mamba/pull/3438
- docs: Adapt "Solving Package Environments" section by @jjerphan in https://github.com/mamba-org/mamba/pull/3326
- [win-64] Remove workaround by @Hind-M in https://github.com/mamba-org/mamba/pull/3398
- [win-64] Add constraint on fmt by @Hind-M in https://github.com/mamba-org/mamba/pull/3400
- Unpin cryptography, python, and add make to environment-dev.yml by @jaimergp in https://github.com/mamba-org/mamba/pull/3352
- ci: Unpin libcxx <18 by @jjerphan in https://github.com/mamba-org/mamba/pull/3375
- chore(ci): bump github action versions by @corneliusroemer in https://github.com/mamba-org/mamba/pull/3350
- doc(more_concepts.rst): improve clarity by @corneliusroemer in https://github.com/mamba-org/mamba/pull/3357
- Temporarily disabled no_python_pinning test on Windows by @JohanMabille in https://github.com/mamba-org/mamba/pull/3322
- Fix CI failure on win-64 by @Hind-M in https://github.com/mamba-org/mamba/pull/3315
- Test with xtensor-python instead of unmaintained xframe by @JohanMabille in https://github.com/mamba-org/mamba/pull/3286
- Small changelog additions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3254
- Fixed a spelling mistake in micromamba-installation.rst by @codeblech in https://github.com/mamba-org/mamba/pull/3236
- Typos in dev_environment.rst by @jd-foster in https://github.com/mamba-org/mamba/pull/3235
- Add MatchSpec doc and fix errors by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3224
- Document specs::Channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3077
- Fix --override-channels docs by @jonashaag in https://github.com/mamba-org/mamba/pull/3084
- Add 2.0 changes draft by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3091
- Add Breathe for API documentation by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3087
- Add instructions for gnu coreutils on OSX by @benmoss in https://github.com/mamba-org/mamba/pull/3111
- Warning around manual install and add ref to conda-libmamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3119
- Add MacOS DNS issue logging by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3130
- Add CI merge groups by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3068
- Build micromamba win with feedstock by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2859
- Update GitHub Actions steps to open Issues for failed scheduled jobs by @jdblischak in https://github.com/mamba-org/mamba/pull/2884
- Fix Ci by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2889
- Mark Anaconda channels as unsupported by @jonashaag in https://github.com/mamba-org/mamba/pull/2904
- Fix nodefaults in documentation by @jonashaag in https://github.com/mamba-org/mamba/pull/2809
- Improve install instruction by @jonashaag in https://github.com/mamba-org/mamba/pull/2908
- Simplify and correct development documentation by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2975
- Add install from source instructions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2977
- update readme install link by @artificial-agent in https://github.com/mamba-org/mamba/pull/2980
- Fail fast except on debug runs by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2985

# micromamba 2.0.0rc6 (September 20, 2024)

Enhancements:

- Support CONDA_DEFAULT_ENV by @SylvainCorlay in https://github.com/mamba-org/mamba/pull/3445

Bug fixes:

- fix: Handle extra white-space in `MatchSpec` by @jjerphan in https://github.com/mamba-org/mamba/pull/3456
- Fix `test_env_update_pypi_with_conda_forge` by @Hind-M in https://github.com/mamba-org/mamba/pull/3459
- fix: Environment removal confirmation by @jjerphan in https://github.com/mamba-org/mamba/pull/3450
- Fix test in osx by @Hind-M in https://github.com/mamba-org/mamba/pull/3448

CI fixes and doc:

- Fix wrong version of miniforge in doc by @Hind-M in https://github.com/mamba-org/mamba/pull/3462
- Remove cctools patch removal in CI by @Hind-M in https://github.com/mamba-org/mamba/pull/3451

# micromamba 2.0.0rc5 (September 13, 2024)

Enhancements:

- Remove cctools patch from feedstock in CI by @JohanMabille in https://github.com/mamba-org/mamba/pull/3442

Bug fixes:

- Add fallback to root prefix by @Hind-M in https://github.com/mamba-org/mamba/pull/3435
- Fixed micromamba static build after cctools and ld64 upgrade on conda… by @JohanMabille in https://github.com/mamba-org/mamba/pull/3436
- fix: PyPI support for `env update` by @jjerphan in https://github.com/mamba-org/mamba/pull/3419
- Update mamba.sh.in script by @SylvainCorlay in https://github.com/mamba-org/mamba/pull/3422

CI fixes and doc:

- docs: Specify `CMAKE_INSTALL_PREFIX` by @jjerphan in https://github.com/mamba-org/mamba/pull/3438

# micromamba 2.0.0rc4 (August 29, 2024)

Bug fixes:

- test: Adapt `test_remove_orphaned` unlinks by @jjerphan in https://github.com/mamba-org/mamba/pull/3417
- fix: Reduce logging system overhead by @jjerphan in https://github.com/mamba-org/mamba/pull/3416

# micromamba 2.0.0rc3 (August 26, 2024)

Bug fixes:

- Define `etc/profile.d/mamba.sh` and install it by @jjerphan in https://github.com/mamba-org/mamba/pull/3413
- Add posix to supported shells by @jjerphan in https://github.com/mamba-org/mamba/pull/3412
- Replaces instances of -p with --root-prefix in documentation by @SylvainCorlay in https://github.com/mamba-org/mamba/pull/3411

CI fixes and doc:

- docs: Adapt "Solving Package Environments" section by @jjerphan in https://github.com/mamba-org/mamba/pull/3326

# micromamba 2.0.0rc2 (August 19, 2024)

Enhancements:

- test: Adapt test_explicit_export_topologically_sorted by @jjerphan in https://github.com/mamba-org/mamba/pull/3377
- build: Support fmt 11 by @jjerphan in https://github.com/mamba-org/mamba/pull/3368

Bug fixes:

- [micromamba] Fix behavior of `env update` (to mimic conda) by @Hind-M in https://github.com/mamba-org/mamba/pull/3396

CI fixes and doc:

- [win-64] Remove workaround by @Hind-M in https://github.com/mamba-org/mamba/pull/3398
- [win-64] Add constraint on fmt by @Hind-M in https://github.com/mamba-org/mamba/pull/3400
- Unpin cryptography, python, and add make to environment-dev.yml by @jaimergp in https://github.com/mamba-org/mamba/pull/3352
- ci: Unpin libcxx <18 by @jjerphan in https://github.com/mamba-org/mamba/pull/3375

# micromamba 2.0.0rc1 (July 26, 2024)

CI fixes and doc:

- chore(ci): bump github action versions by @corneliusroemer in https://github.com/mamba-org/mamba/pull/3350
- doc(more_concepts.rst): improve clarity by @corneliusroemer in https://github.com/mamba-org/mamba/pull/3357

# micromamba 2.0.0rc0 (July 08, 2024)

Bug fixes:

- Attempt to fix `test_proxy_install` by @Hind-M in https://github.com/mamba-org/mamba/pull/3324
- Fix `test_no_python_pinning` by @Hind-M in https://github.com/mamba-org/mamba/pull/3321
- Split `ContextOptions::enable_logging_and_signal_handling` into 2 different options by @Klaim in https://github.com/mamba-org/mamba/pull/3329

CI fixes and doc:

- Temporarily disabled no_python_pinning test on Windows by @JohanMabille in https://github.com/mamba-org/mamba/pull/3322

# micromamba 2.0.0beta3 (June 14, 2024)

Enhancements:

- OCI/Conda mapping by @Hind-M in https://github.com/mamba-org/mamba/pull/3310
- [OCI - Mirrors] Add tests and doc by @Hind-M in https://github.com/mamba-org/mamba/pull/3307

Bug fixes:

- Fix test_no_python_pinning by @Hind-M in https://github.com/mamba-org/mamba/pull/3319
- Fix release scripts by @Hind-M in https://github.com/mamba-org/mamba/pull/3306

CI fixes and doc:

- Fix CI failure on win-64 by @Hind-M in https://github.com/mamba-org/mamba/pull/3315

# micromamba 2.0.0beta2 (May 29, 2024)

Enhancements:

- Add checking typos to pre-commit by @Hind-M in https://github.com/mamba-org/mamba/pull/3278

# micromamba 2.0.0beta1 (May 04, 2024)

Enhancements:

- Update pre-commit hooks" by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3252
- Refactor os utilities by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3248
- Add integration test by @Hind-M in https://github.com/mamba-org/mamba/pull/3234
- Custom resolve complex MatchSpec in Solver by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3233
- Subdir renaming by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3214
- Expected in specs parse API by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3207

Bug fixes:

- Fix VersionSpec equal and glob by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3269
- Use conda-forge feedstock for static builds by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3249
- Mamba 2.0 name fixes by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3225
- Make Taskfile.dist.yml Windows-compatible by @carschandler in https://github.com/mamba-org/mamba/pull/3219

CI fixes and doc:

- Test with xtensor-python instead of unmaintained xframe by @JohanMabille in https://github.com/mamba-org/mamba/pull/3286
- Small changelog additions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3254
- Fixed a spelling mistake in micromamba-installation.rst by @codeblech in https://github.com/mamba-org/mamba/pull/3236
- Typos in dev_environment.rst by @jd-foster in https://github.com/mamba-org/mamba/pull/3235
- Add MatchSpec doc and fix errors by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3224

# micromamba 2.0.0beta0 (April 04, 2024)

Enhancements:

- Update pre-commit hooks" by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3252
- Refactor os utilities by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3248

CI fixes and doc:

- Small changelog additions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3254

# micromamba 2.0.0alpha4 (March 26, 2024)

Enhancements:

- Add integration test by @Hind-M in https://github.com/mamba-org/mamba/pull/3234
- Custom resolve complex MatchSpec in Solver by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3233
- Subdir renaming by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3214
- Expected in specs parse API by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3207

Bug fixes:

- Use conda-forge feedstock for static builds by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3249
- Mamba 2.0 name fixes by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3225
- Make Taskfile.dist.yml Windows-compatible by @carschandler in https://github.com/mamba-org/mamba/pull/3219

CI fixes and doc:

- Fixed a spelling mistake in micromamba-installation.rst by @codeblech in https://github.com/mamba-org/mamba/pull/3236
- Typos in dev_environment.rst by @jd-foster in https://github.com/mamba-org/mamba/pull/3235
- Add MatchSpec doc and fix errors by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3224

# micromamba 2.0.0alpha3 (February 28, 2024)

Enhancements:

- Added HTTP Mirrors by @JohanMabille in https://github.com/mamba-org/mamba/pull/3178
- Use expected for specs parsing by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3201
- Rename MPool into solver::libsolv::Database by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3180
- Automate releases (`CHANGELOG.md` updating) by @Hind-M in https://github.com/mamba-org/mamba/pull/3179
- Simplify MPool Interface by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3177
- Clean libsolv use in Transaction by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3171
- Rewrite Query with Pool functions (wrapping libsolv) by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3168

Bug fixes:

- Remove unmaintained and broken pytest-lazy-fixture by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3193

CI fixes and doc:

# micromamba 2.0.0alpha2 (February 02, 2024)

Enhancements:

- Remove hard coded mamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3069
- Support multiple env yaml specs by @jchorl in https://github.com/mamba-org/mamba/pull/2993
- Duplicate reposerver to isolate micromamba tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3071
- Some future proofing MatchSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3082
- Remove micromamba shell init -p by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3092
- Clean PackageInfo interface by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3103
- Move PackageInfo in specs:: by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3109
- Move util_random.hpp > util/random.hpp by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3129
- Refactor test_remove.py to use fixture by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3131
- MRepo refactor by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3118
- Explicit transaction duplicate code by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3138
- Solver Request by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3141
- Split Solver and Unsolvable by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3156
- Solver sort deps by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3163

Bug fixes:

- Fix URL encoding in repodata.json by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3076
- gracefully handle conflicting names in yaml specs by @jchorl in https://github.com/mamba-org/mamba/pull/3083
- add manually given .tar.bz2 / .conda packages to solver pool by @0xbe7a in https://github.com/mamba-org/mamba/pull/3164

CI fixes and doc:

- Document specs::Channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3077
- Fix --override-channels docs by @jonashaag in https://github.com/mamba-org/mamba/pull/3084
- Add 2.0 changes draft by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3091
- Add Breathe for API documentation by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3087
- Add instructions for gnu coreutils on OSX by @benmoss in https://github.com/mamba-org/mamba/pull/3111
- Warning around manual install and add ref to conda-libmamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3119
- Add MacOS DNS issue logging by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3130

# micromamba 2.0.0alpha1 (December 18, 2023)

CI fixes and doc:

- Add CI merge groups by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3068

# micromamba 2.0.0alpha0 (December 14, 2023)

Enhancements:

- Context: not a singleton by @Klaim in https://github.com/mamba-org/mamba/pull/2615
- Add env update by @Hind-M in https://github.com/mamba-org/mamba/pull/2827
- Adding locks for cache directories by @rmittal87 in https://github.com/mamba-org/mamba/pull/2811
- Refactor tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2829
- No ugly kenum by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2831
- Add Nushell activation support by cvanelteren in https://github.com/mamba-org/mamba/pull/2693
- Channel cleanup by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2832
- Don't force MSVC_RUNTIME by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2861
- Add comments in micromamba repoquery by @Hind-M in https://github.com/mamba-org/mamba/pull/2863
- Fix Posix shell on Windows by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2803
- Use CMake targets for reproc by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2883
- Add mamba tests by @Hind-M in https://github.com/mamba-org/mamba/pull/2877
- Filesystem library by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2879
- Add multiple queries to repoquery search by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2897
- Add ChannelSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2870
- Make some fixture local by @JohanMabille in https://github.com/mamba-org/mamba/pull/2919
- Added PackageFetcher by @JohanMabille in https://github.com/mamba-org/mamba/pull/2917
- Resolve ChannelSpec into a Channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2899
- Refactor win encoding conversion by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2939
- Move reposerver tests to micromamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2941
- Remove mamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2942
- Dev workflow by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2948
- Add refactor getenv setenv unsetenv by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2944
- Explicit and smart CMake target by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2935
- Rename env functions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2954
- Update dependencies on OSX by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2976
- Channel initialization by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2953
- Refactor env directories by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2983
- Migrate expand/shrink_home by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2990
- Refactor env::which by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2997
- Migrate Channel::make_channel to resolve multi channels by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2986
- Move core/channel > specs/channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3000
- Remove ChannelContext context capture by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3015
- Default to hide credentials by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3017
- Refactor (some) OpenSSL functions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3024
- Default to conda-forge channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3035
- Plug ChannelSpec in MatchSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3046
- Change MatchSpec::parse to named constructor by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3048
- Added mamba as dynamic build of micromamba by @JohanMabille in https://github.com/mamba-org/mamba/pull/3060

Bug fixes:

- Fix linking on Windows when Scripts folder is missing by @dalcinl in https://github.com/mamba-org/mamba/pull/2825
- Fix win test micro.mamba.pm by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2888
- Add CI test for local channels by @Hind-M in https://github.com/mamba-org/mamba/pull/2854
- Fixed "micromamba package transmute names files going from .conda -> .tar.bz2 incorrectly" by @mariusvniekerk in https://github.com/mamba-org/mamba/issues/2911
- Fix micromamba test dependency conda-package-handling by @rominf in https://github.com/mamba-org/mamba/pull/2945
- Add cmake-format by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2962
- removed dependency on conda-index by @JohanMabille in https://github.com/mamba-org/mamba/pull/2964

CI fixes and doc:

- Build micromamba win with feedstock by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2859
- Update GitHub Actions steps to open Issues for failed scheduled jobs by @jdblischak in https://github.com/mamba-org/mamba/pull/2884
- Fix Ci by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2889
- Mark Anaconda channels as unsupported by @jonashaag in https://github.com/mamba-org/mamba/pull/2904
- Fix nodefaults in documentation by @jonashaag in https://github.com/mamba-org/mamba/pull/2809
- Improve install instruction by @jonashaag in https://github.com/mamba-org/mamba/pull/2908
- Simplify and correct development documentation by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2975
- Add install from source instructions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2977
- update readme install link by @artificial-agent in https://github.com/mamba-org/mamba/pull/2980
- Fail fast except on debug runs by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2985

# micromamba 2.0.0alpha0 (December 14, 2023)

Enhancements:

- Context: not a singleton by @Klaim in https://github.com/mamba-org/mamba/pull/2615
- Add env update by @Hind-M in https://github.com/mamba-org/mamba/pull/2827
- Adding locks for cache directories by @rmittal87 in https://github.com/mamba-org/mamba/pull/2811
- Refactor tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2829
- No ugly kenum by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2831
- Add Nushell activation support by cvanelteren in https://github.com/mamba-org/mamba/pull/2693
- Channel cleanup by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2832
- Don't force MSVC_RUNTIME by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2861
- Add comments in micromamba repoquery by @Hind-M in https://github.com/mamba-org/mamba/pull/2863
- Fix Posix shell on Windows by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2803
- Use CMake targets for reproc by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2883
- Add mamba tests by @Hind-M in https://github.com/mamba-org/mamba/pull/2877
- Filesystem library by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2879
- Add multiple queries to repoquery search by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2897
- Add ChannelSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2870
- Make some fixture local by @JohanMabille in https://github.com/mamba-org/mamba/pull/2919
- Added PackageFetcher by @JohanMabille in https://github.com/mamba-org/mamba/pull/2917
- Resolve ChannelSpec into a Channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2899
- Refactor win encoding conversion by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2939
- Move reposerver tests to micromamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2941
- Remove mamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2942
- Dev workflow by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2948
- Add refactor getenv setenv unsetenv by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2944
- Explicit and smart CMake target by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2935
- Rename env functions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2954
- Update dependencies on OSX by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2976
- Channel initialization by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2953
- Refactor env directories by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2983
- Migrate expand/shrink_home by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2990
- Refactor env::which by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2997
- Migrate Channel::make_channel to resolve multi channels by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2986
- Move core/channel > specs/channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3000
- Remove ChannelContext context capture by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3015
- Default to hide credentials by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3017
- Refactor (some) OpenSSL functions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3024
- Default to conda-forge channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3035
- Plug ChannelSpec in MatchSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3046
- Change MatchSpec::parse to named constructor by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3048
- Added mamba as dynamic build of micromamba by @JohanMabille in https://github.com/mamba-org/mamba/pull/3060

Bug fixes:

- Fix linking on Windows when Scripts folder is missing by @dalcinl in https://github.com/mamba-org/mamba/pull/2825
- Fix win test micro.mamba.pm by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2888
- Add CI test for local channels by @Hind-M in https://github.com/mamba-org/mamba/pull/2854
- Fixed "micromamba package transmute names files going from .conda -> .tar.bz2 incorrectly" by @mariusvniekerk in https://github.com/mamba-org/mamba/issues/2911
- Fix micromamba test dependency conda-package-handling by @rominf in https://github.com/mamba-org/mamba/pull/2945
- Add cmake-format by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2962
- removed dependency on conda-index by @JohanMabille in https://github.com/mamba-org/mamba/pull/2964

CI fixes and doc:

- Build micromamba win with feedstock by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2859
- Update GitHub Actions steps to open Issues for failed scheduled jobs by @jdblischak in https://github.com/mamba-org/mamba/pull/2884
- Fix Ci by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2889
- Mark Anaconda channels as unsupported by @jonashaag in https://github.com/mamba-org/mamba/pull/2904
- Fix nodefaults in documentation by @jonashaag in https://github.com/mamba-org/mamba/pull/2809
- Improve install instruction by @jonashaag in https://github.com/mamba-org/mamba/pull/2908
- Simplify and correct development documentation by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2975
- Add install from source instructions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2977
- update readme install link by @artificial-agent in https://github.com/mamba-org/mamba/pull/2980
- Fail fast except on debug runs by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2985

# micromamba 1.5.1 (September 05, 2023)

Enhancements:

- Speed up tests (a bit) by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2776
- Restore \_\_linux=0 test by @jonashaag in https://github.com/mamba-org/mamba/pull/2778
- Enable Link Time Optimization by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2742
- Clearer output from micromamba search by @delsner in https://github.com/mamba-org/mamba/pull/2782
- Windows path manipulation and other cleanups by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2801
- Implement --md5 and --channel-subdir for non-explicit env export by @jonashaag in https://github.com/mamba-org/mamba/pull/2672

Bug fixes:

- Fix extra argument in self-update reinit by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2787

CI fixes and doc:

- Split GHA workflow by @JohanMabille in https://github.com/mamba-org/mamba/pull/2779
- Use Release build mode in Windows CI by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2785
- Fix wrong command description by @Hind-M in https://github.com/mamba-org/mamba/pull/2804

# micromamba 1.5.0 (August 24, 2023)

Enhancements:

- Refactor test_repoquery to use new fixtures by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2691
- Remove warnings from test_activation by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2727
- Refactor test_shell by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2726
- Fix and improve static builds by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2755
- Enable pytest color output by @jonashaag in https://github.com/mamba-org/mamba/pull/2759
- Isolate URL object by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2744
- Fix warnings by @Hind-M in https://github.com/mamba-org/mamba/pull/2760

Bug fixes:

- Fix wrong activated PATH in micromamba shell by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2722
- Fix config list sources by @Hind-M in https://github.com/mamba-org/mamba/pull/2756
- Fix \_\_linux virtual package default version by jonashaag in https://github.com/mamba-org/mamba/pull/2749
- Strong pin in test by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2774
- Revert failing test by @jonashaag in https://github.com/mamba-org/mamba/pull/2777

CI fixes and doc:

- Update troubleshooting.rst by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2675
- Ignore format changes in git blame by @jonashaag in https://github.com/mamba-org/mamba/pull/2690
- Add command to docs for completeness by @danpf in https://github.com/mamba-org/mamba/pull/2717
- fix: Correct a command in installation.rst by @wy-luke in https://github.com/mamba-org/mamba/pull/2703
- Split Mamba and Micromamba installation docs by @jonashaag in https://github.com/mamba-org/mamba/pull/2719
- fix: Shell completion section title missing by @wy-luke in https://github.com/mamba-org/mamba/pull/2764
- Add Debug build type by @Hind-M in https://github.com/mamba-org/mamba/pull/2762

# micromamba 1.4.9 (July 13, 2023)

Bug fixes:

- Added upper bound to fmt to avoid weird failure on ci (windows only) by @JohanMabille in https://github.com/mamba-org/mamba/pull/2671

# micromamba 1.4.8 (July 11, 2023)

Enhancements:

- No profile.d fallback in rc files by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2649

CI fixes and doc;

- Update installation docs by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2654

# micromamba 1.4.7 (July 06, 2023)

# micromamba 1.4.6 (June 30, 2023)

CI fixes and doc:

- Document micromamba support for conda-lock spec files by @mfisher87 in https://github.com/mamba-org/mamba/pull/2621

# micromamba 1.4.5 (June 27, 2023)

Enhancements:

- No singleton: ChannelContext, ChannelBuilder and channel cache by @Klaim in https://github.com/mamba-org/mamba/pull/2455
- Micromamba tests improvements by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2517
- Common CMake presets by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2532
- No singleton: configuration by @Klaim in https://github.com/mamba-org/mamba/pull/2541
- Remove banner by @jonashaag in https://github.com/mamba-org/mamba/pull/2298
- Add topological sort explicit export tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2618

Bug fixes:

- Use subsub commands for micromamba shell by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2527
- Fix umamba tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2540
- Honor envs_dirs by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2538
- Fix wrong download url for custom channels by @Hind-M in https://github.com/mamba-org/mamba/pull/2596
- Fix --force-reinstall by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2601

CI fixes and doc:

- Use only vcpkg for static windows build by @pavelzw in https://github.com/mamba-org/mamba/pull/2520
- update the umamba GHA link by @ocefpaf in https://github.com/mamba-org/mamba/pull/2542
- Extend troubleshooting docs by @jonashaag in https://github.com/mamba-org/mamba/pull/2569
- Try new vcpkg by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2572
- Update pre-commit hooks by @jonashaag in https://github.com/mamba-org/mamba/pull/2586
- Move GHA to setup-micromamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2545
- Switch linters to setup-micromamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2600
- Switch to setup-micromamba by @pavelzw in https://github.com/mamba-org/mamba/pull/2610
- Fix broken ref directives in docs by @mfisher87 in https://github.com/mamba-org/mamba/pull/2620

# micromamba 1.4.4 (May 16, 2023)

Bug fixes:

- fix: let the new executable run the shell init script by @ruben-arts in https://github.com/mamba-org/mamba/pull/2529

# micromamba 1.4.3 (May 15, 2023)

Enhancements:

- Context structuring by @Hind-M in https://github.com/mamba-org/mamba/pull/2432
- Resume Context structuring by @Hind-M in https://github.com/mamba-org/mamba/pull/2460
- cleanup: fix pytest warnings by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2490
- Use libsolv wrappers in MPool and MRepo by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2453
- add bearer token authentication by @wolfv in https://github.com/mamba-org/mamba/pull/2512

CI fixes and doc:

- Extend issue template by @jonashaag in https://github.com/mamba-org/mamba/pull/2310

# micromamba 1.4.2 (April 06, 2023)

Enhancements:

- Refactor test_create, test_proxy, and test_env for test isolation by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2416
- Remove const ref to string_view in codebase by @Hind-M in https://github.com/mamba-org/mamba/pull/2440

CI fixes and doc:

- Fixes typos by @nsoranzo in https://github.com/mamba-org/mamba/pull/2419
- Remove outdated micromamba experimental warning by @jonashaag in https://github.com/mamba-org/mamba/pull/2430
- Migrated to doctest by @JohanMabille in https://github.com/mamba-org/mamba/pull/2436

# micromamba 1.4.1 (March 28, 2023)

Enhancements:

- add option to relocate prefix by @DerThorsten in https://github.com/mamba-org/mamba/pull/2385

# micromamba 1.4.0 (March 22, 2023)

Enhancements:

- Implemented recursive dependency printout in repoquery by @timostrunk in https://github.com/mamba-org/mamba/pull/2283
- Aggressive compilation warnings by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2304
- Fine tune clang-format by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2290
- Only full shared or full static builds by @JohanMabille in https://github.com/mamba-org/mamba/pull/2342
- Fixed repoquery commands working with installed packages only by @Hind-M in https://github.com/mamba-org/mamba/pull/2330
- Added micromamba server by @wolfv in https://github.com/mamba-org/mamba/pull/2185

Bug fixes:

- Fixed `micromamba env export` to get channel name instead of full url by @Hind-M in https://github.com/mamba-org/mamba/pull/2260

CI fixes & docs:

- Added missing dependency in local recipe by @wolfv in https://github.com/mamba-org/mamba/pull/2334
- Fixed Conda Lock Path by @function in https://github.com/mamba-org/mamba/pull/2393

# micromamba 1.3.1 (February 09, 2023)

A bugfix release for 1.3.0!

Bug fixes:

- fix up single download target perform finalization to make lockfile download work by @wolfv in https://github.com/mamba-org/mamba/pull/2274
- use CONDA_PKGS_DIRS even in explicit installation trasactions by @hmaarrfk in https://github.com/mamba-org/mamba/pull/2265
- fix rename or remove by @wolfv in https://github.com/mamba-org/mamba/pull/2276
- fix `micromamba shell` for base environment, and improve behavior when `auto_activate_base` is true by @jonashaag, @Hind-M and @wolfv https://github.com/mamba-org/mamba/pull/2272

Docs:

- - add micromamba docker image by @wholtz in https://github.com/mamba-org/mamba/pull/2266
- - added biweekly meetings information to README by @JohanMabille in https://github.com/mamba-org/mamba/pull/2275
- - change docs to homebrew/core by @pavelzw in https://github.com/mamba-org/mamba/pull/2278

# micromamba 1.3.0 (February 03, 2023)

Enhancements:

- add functionality to download lockfiles from internet by @wolfv in https://github.com/mamba-org/mamba/pull/2229
- Stop run command when given prefix does not exist by @Hind-M in https://github.com/mamba-org/mamba/pull/2257
- Install pip deps like conda by @michalsieron in https://github.com/mamba-org/mamba/pull/2241
- switch to repodata.state.json format from cep by @wolfv in https://github.com/mamba-org/mamba/pull/2262

Bug fixes:

- Fix temporary file renaming by @jonashaag in https://github.com/mamba-org/mamba/pull/2242

CI fixes & docs:

- use proper recipe also on macOS by @wolfv in https://github.com/mamba-org/mamba/pull/2225
- Update micromamba installation docs for Windows by @Tiksagol in https://github.com/mamba-org/mamba/pull/2218
- docs: defaults should not be used with conda-forge by @jonashaag in https://github.com/mamba-org/mamba/pull/2181
- fix tests for pkg_cache by @wolfv in https://github.com/mamba-org/mamba/pull/2259
- Fix Windows static builds by @jonashaag in https://github.com/mamba-org/mamba/pull/2228

# micromamba 1.2.0 (January 16, 2023)

This release contains some speed improvements: download repodata faster as zstd encoded files (configure using
`repodata_use_zst: true` in your `~/.mambarc` file). Also, `.conda` file extraction is now faster, a prefix
with spaces works better thanks to a new "shebang" style and the `micromamba package compress` and `transmute`
commands produce better conda packages.

Enhancements:

- Make tarballs look more similar to conda-package-handling by @wolfv in #2177, #2217
- Use new shebang style by @wolfv in #2211
- Faster conda decompress by @wolfv in #2200
- Initial repodata.zst support by @wolfv & @jonashaag in #2113

Bug fixes:

- log warnings but ignore cyclic symlinks by @wolfv in #2212
- Report failure when packages to remove don't exist. (#2131) by @Klaim in #2132
- Fix micromamba shell completion when running 'shell hook' directly by @TomiBelan in #2137
- Don't create a prefix which is missing conda-meta by @maresb in #2162
- Fix `custom_channels` parsing by @XuehaiPan in #2207
- Fix #1783: Add `micromamba env create` by @jonashaag in #1790

CI fixes & docs:

- - Improve build env cleanup by @jonashaag in #2213
- - Run conda_nightly once per week by @jonashaag in #2147
- - Update doc by @Hind-M in #2156
- - Use Conda canary in nightly tests by @jonashaag in #2180
- - Explicitly point to libmamba test data independently of cwd by @AntoinePrv in #2158
- - Add bug report issue template by @jonashaag in #2182
- - Downgrade curl to fix micromamba on macOS x64 by @wolfv in #2205
- - Use conda-forge micromamba feedstock instead of a fork by @JohanMabille in #2206
- - Update pre-commit versions by @jonashaag in #2178
- - Use local meta.yaml by @wolfv in #2214
- - Remove feedstock patches by @wolfv in #2216
- - Fixed static dependency order by @JohanMabille in #2201

# micromamba 1.1.0 (November 25, 2022)

Some bugfixes for 1.0 and experimental release of the new solver messages

Bug fixes

- Fix fish scripts (thanks @JafarAbdi, @raj-magesh, @jonashaag) #2101
- fix direct hook for powershell #2122
- fixes for ssl init and static build #2076

Enhancements

- Handle non leaf conflicts (thanks @AntoinePrv) #2133
- Bind SAT error messages to python (thanks @AntoinePrv) #2127
- Nitpicking error messages (thanks @AntoinePrv) #2121
- Tree error message improvements (thanks @AntoinePrv) #2093
- Tree error message (thanks @AntoinePrv) #2064
- Add experimental flag for error messages (thanks @AntoinePrv) #2080
- Handle non leaf conflicts (thanks @AntoinePrv) #2133
- ci: Update pre-commit-config #2092
- docs: Add warning to manual install instructions #2100
- docs: Consistently use curl for fetching files #2126

# micromamba 1.0.0 (November 01, 2022)

Our biggest version number yet! Finally a 1.0 release :)

New notable micromamba features include:

- - improved shell scripts with autocompletion available in PowerShell, xonsh, fish, bash and zsh
- - `micromamba shell -n someenv`: enter a sub-shell without modifying the system
- - `micromamba self-update`: micromamba searches for updates and installs them if available

(you can also downgrade using `--version 0.26.0` for example)

Bug fixes:

- ignore "Permission denied" in `env::which` (thanks @Rafflesiaceae) #2067
- Link micromamba with static libc++.a and system libc++abi (thanks @isuruf) #2069
- Fix an infinite loop in replace_all() when the search string is empty (thanks @tsibley)
- Do not crash when permissions cannot be changed, instead log warning (thanks @hwalinga)

Enhancements:

- Add `micromamba env remove` (thanks @Hind-M) #2002
- add self-update functionality (#2023)
- order dependencies alphabetically from `micromamba env export` (thanks @torfinnberset) #2063
- Fix ci deprecation warnings, upload conda-bld artifacts for failed builds #2058, #2062
- Explicitly define SPDLOG_FMT_EXTERNAL and use spdlog header only use external fmt (thanks @AntoinePrv) #2060, #2048
- Fix CI by pointing to updated feedstock and fixing update tests (thanks @AntoinePrv) #2055
- Add authentication with urlencoded @ to proxy test (#2024) @AdrianFreundQC
- better test isolation (thanks @AntoinePrv) #1903
- Test special characters in basic auth (thanks @jonashaag) #2012

# micromamba 0.27.0 (October 04, 2022)

Bug fixes:

- fix lockfiles relying on PID (thanks @Klaim) #1915
- fix error condition in micromamba run to not print warning every time #1985
- fix error when getting size of directories (thanks @Klaim) #1982
- fix crash when installing pip packages from env files (thanks @Klaim) #1978

Enhancements:

- add cross-compiled builds to CI (thanks @pavelzw) #1976, #1989

# micromamba 0.26.0 (September 30, 2022)

Bug fixes:

- fix fish scripts (thanks @jonashaag) #1975
- fix issues with `micromamba ps` #1953
- add symlinks and empty directories to archive for `micromamba package compress` #1955
- add `micromamba info --licenses` to print licenses of used OSS (thanks @jonashaag) #1933
- proper quoting in `micromamba run` (thanks @jonashaag) #1936
- install pip dependencies and by category for YAML lockfiles (thanks @jvansanten) #1908 #1917
- fix update for packages with explicit channels (thanks @AntoinePrv) #1864

Enhancements:

- only call compinit once to fix oh-my-zsh (thanks @AntoinePrv) #1911
- CI: add fully static micromamba build (thanks @jonashaag) #1821
- allow configuring proxies (thanks @AdrianFreundQC) #1841

# micromamba 0.25.1 (July 29, 2022)

Bug fixes:

- fix issue where pip installation was broken on Windows @Klaim #1828

# micromamba 0.25.0 (July 26, 2022)

Bug fixes:

- fix pip execution in environments with spaces (thanks @chaubold) #1815
- Fix `shell init --dry-run` (thanks @jonashaag) #1767
- Change exit(1) to throw exceptions instead (thanks @jonashaag) #1792

Enhancements:

- add handling of different tokens for channels on same host (thanks @AntoinePrv) #1784
- better test isolation (thanks @AntoinePrv) #1791
- Add deinit shell command (thanks @pavelzw) #1781
- Add nodefaults handling to libmamba (thanks @AdrianFreundQC) #1773
- Fix micromamba Windows download instructions (thanks @jonashaag) #1793
- Better error message if root prefix is not a directory #1792
- Make `--use-index-cache` option work (thanks @AdrianFreundQC) #1762
- Test improvements (thanks @AntoinePrv) #1777, #1778

# micromamba 0.24.0 (June 01, 2022)

Bug fixes:

- constructor now uses proper (patched) repodata to create repodata_record.json files #1698
- use fmt::format for pretty printing in `micromamba search --pretty` #1710

# micromamba 0.23.3 (May 20, 2022)

Bug fixes

- Fix summing behavior of `-v` flags #1690
- fix curl callback to not exit anymore but report a proper error #1684
- fix up explicit installation by using proper variables #1677

Improvements

- make clean_force_pkgs respect `-y` flag (thanks @Patricol) #1686

# micromamba 0.23.2 (May 12, 2022)

Bug fixes

- Fix a bug with platform replacement in URLs #1670

# micromamba 0.23.1 (May 11, 2022)

Bug fixes

- Fix powershell unset of env vars (thanks @chawyeshu) #1668
- Fix thread clean up and singleton destruction order (thanks @Klaim) #1666, #1620
- Show reason for multi-download failure (thanks @syslaila) #1652

# micromamba 0.23.0 (April 21, 2022)

This release uses tl::expected for some improvements in the error handling.
We also cleaned the API a bit and did some refactorings to make the code compile faster and clean up headers.

Bug fixes

- Do not clean env when running post-link scripts (fixes Qt install on Windows) #1630
- Fix powershell activation in strict mode (thanks @mkessler) #1633

Enhancements

- Add `micromamba auth login / logout` commands
- Add support for new conda-lock yml file format (thanks @Klaim) #1577
- Use cli11 2.2.0 #1626
- Use sscache to speed up builds (thanks @jonashaag) #1606
- Upgrade black
- Use bin2header to inline the various scripts (thanks @jonashaag) #1601
- Fix JSON output issues (thanks @Klaim) #1600
- Refactor the include chain, headers cleanup (thanks @JohanMabille) #1588, #1592, #1590
- Refactor error handling (thanks @JohanMabille) #1579
- Add tests for micromamba run (thanks @Klaim) #1564
- Also complete for micromamba deactivate #1577

# micromamba 0.22.0 (February 25, 2022)

Bug fixes

- Add noarch recompilation step for mamba and micromamba #1511
- Add `--force-reinstall`, `--only-deps` and `--no-deps` to micromamba #1531
- Tolerate `PATH` being unset better (thanks @chrisburr) #1532

Improvements

- Add `--label` option to micromamba run and automatically generate process names otherwise (thanks @Klaim) #1491, #1530, #1529
- Add `search` as an alias for `repoquery search` (thanks @JohanMabille) #1510
- Fix `repoquery search` not working outside activated environment (thanks @JohanMabille) #1510
- Refactor configuration system (thanks @JohanMabille) #1500
- Fix segfault on Linux with "fake" micromamba activate command #1496

# micromamba 0.21.2 (February 14, 2022)

Bug fixes

- Properly attach stdin for `micromamba run` #1488

# micromamba 0.21.1 (February 11, 2022)

Bug fixes

- Revert removal of environment variables when running pip (thanks @austin1howard) #1477

Improvements

- Add `micromamba config --json` (thanks @JohanMabille) #1484
- Adjustments for the progress bars, make better visible on light backgrounds #1458
- Micromamba run add `--clean-env` and `-e,--env` handling to pass in environment variables #1464
- Disable banner with `micromamba run` #1474

# micromamba 0.21.0 (February 07, 2022)

Bug fixes

- fix crash with missing CONDARC file (thanks @jonashaag) #1417
- fix `micromamba --log-level` (thanks @jonashaag) #1417
- Fix erroneous error print when computing SHA256 of missing symlink #1412
- Add `-n` flag handling to `micromamba activate` #1411
- Refactor configuration loading and create file if it doesn't exist when setting values #1420
- Improve shell scripts when ZSH_VERSION is unbound #1440
- Return error code when pip install fails from environment.yml #1442

Improvements

- Update pre-commit versions (thanks @jonashaag) #1417
- Use clang-format from pypi (thanks @chrisburr) #1430
- Incremental ccache updates (thanks @jonashaag) #1445
- Substitute environment vars in .condarc files (thanks @jonashaag) #1423
- Speed up noarch compilation (thanks @chrisburr) #1422
- New fancy progress bars! (thanks @adriendelsalle) #1426, #1350
- Add `micromamba run` command (thanks @JohanMabille) #1380, #1395, #1406, #1438, #1434
- Add `-f` for `micromamba clean` command (thanks @JohanMabille) #1449
- Add improved `micromamba update --all` #1318
- Add `micromamba repoquery` command #1318

# micromamba 0.20.0 (January 25, 2022)

Bug fixes

- Fix micromamba init & conda init clobber (thanks @maresb) #1357
- Rename mamba.sh to micromamba.sh for better compatibility between mamba & micromamba (thanks @maresb) #1355
- Print activate error to stderr (thanks @maresb) #1351

Improvements

- Only print micromamba version and add library versions to `info` command #1372
- Implement activate as a micromamba subcommand for better error messages (thanks @maresb) #1360
- Experimental was logged twice (thanks @baszalmstra) #1360
- Store platform when creating env with `--platform=...` (thanks @adriendelsalle) #1381

# micromamba 0.19.1 (December 08, 2021)

Bug fixes

- Fix lockfiles in Unicode prefix (@wolfv) #1319

Improvements

- Add `micromamba clean --trash` command to remove `*.mamba_trash` files (@wolfv) #1319

# micromamba 0.19.0 (November 30, 2021)

Bug fixes

- Better Unicode support on Windows (@wolfv) #1306
- Do not set higher prio to arch vs noarch (@wolfv) #1312
- Add shell_completion, changeps1 and env_prompt as RC settings, remove auto-activate-base CLI flag (@wolfv) #1304

# micromamba 0.18.2 (November 24, 2021)

Bug fixes

- Fix CMake config for micromamba fully statically linked on Windows (@adriendelsalle) #1297
- Fix shell activation regression (@adriendelsalle) #1289

# 0.18.1 (November 19, 2021)

Bug fixes

- Fix default log level, use warning everywhere (@adriendelsalle) #1279

# 0.18.0 (November 17, 2021)

New features

- Parallel packages extraction using subproc (@jonashaag @adriendelsalle) #1195
- Improve bash completion (activate sub-command, directories completion) (@adriendelsalle) #1234
- Add channel URLs to info (@jonashaag) #1235
- Make pyc compilation configurable using `--pyc,--no-pyc` flags (@adriendelsalle) #1249
- Add `--log-level` option to control log level independently of verbosity (@adriendelsalle) #1255
- Add zsh completion (@adriendelsalle) #1269
- Add info JSON output and `--json` CLI flag (@adriendelsalle) #1271

Bug fixes

- Init all powershell profiles (@adriendelsalle) #1226
- Fix multiple activations in Windows bash (@adriendelsalle) #1228

Docs

- Document fish support (@izahn) #1216

General improvements

- Split projects, improve CMake options (@adriendelsalle) #1219 #1243

# 0.17.0 (October 13, 2021)

API Breaking changes:

The Transaction and the Subdir interface have slightly changed (no more explicit setting of the writable
packages dir is necessary, this value is taken directly from the MultiPackagesCache now)

- improve listing of (RC-) configurable values in `micromamba` #1210 (thanks @adriendelsalle)
- Improve micromamba lockfiles and add many tests #1193 (thanks @adriendelsalle)
- Support multiple package caches in micromamba (thanks @adriendelsalle) #1109
- Order explicit envs in micromamba (also added some text to the docs about libsolv transactions) #1198
- Add `micromamba package` subcommand to extract, create and transmute packages #1187
- Improve micromamba configuration to support multi-stage loading of RC files (thanks @adriendelsalle) #1189 #1190 #1191 #1188
- Add handling of `CONDA_SAFETY_CHECKS` to micromamba #1143 (thanks @isuruf)
- Improve mamba messaging by adding a space #1186 (thanks @wkusnierczyk)
- Add support for `custom_multichannels` #1142
- micromamba: expose setting for `add_pip_as_python_dependency` #1203
- stop displaying banner when running `mamba list` #1184 (thanks @madhur-thandon)

# 0.16.0 (September 27, 2021)

- Add a User-Agent header to all requests (mamba/0.16.0) (thanks @shankerwangmiao)
- Add `micromamba env export (--explicit)` to micromamba
- Do not display banner with `mamba list` (thanks @madhur-tandon)
- Use directory of environment.yml as cwd when creating environment (thanks @marscher & @adriendelsalle)
- Improve outputs
- content-trust: Add Python bindings for content-trust API
- content-trust: Load PkgMgr definitions from file
- content-trust: Improve HEAD request fallback handling
- export Transaction.find_python_version to Python
- Continue `shell init` when we can't create the prefix script dir (thanks @maresb)
- Implement support for `fish` shell in `micromamba` (thanks @soraxas)
- Add constraint with pin when updating
- Expose methods for virtual packages to Python (thanks @madhur-tandon)

# 0.15.3 (August 18, 2021)

- change token regex to work with edge-cases (underscores in user name) (#1122)
- only pin major.minor version of python for update --all (#1101, thanks @mparry!)
- add mamba init to the activate message (#1124, thanks @isuruf)
- hide tokens in logs (#1121)
- add lockfiles for repodata and pkgs download (#1105, thanks @jaimergp)
- log actual SHA256/MD5/file size when failing to avlidate (#1095, thanks @johnhany97)
- Add mamba.bat in front of PATH (#1112, thanks @isuruf)
- Fix mamba not writable cache errors (#1108)

# 0.15.2 (July 16, 2021)

- micromamba autocomplete now ready for usage (#1091)
- improved file:// urls for windows to properly work (#1090)

# 0.15.1 (July 15, 2021)

New features:

- add `mamba init` command and add mamba.sh (#1075, thanks @isuruf & #1078)
- add flexible channel priority option in micromamba CLI (#1087)
- improved autocompletion for micromamba (#1079)

Bug fixes:

- improve "file://" URL handling, fix local channel on Windows (#1085)
- fix CONDA_SUBDIR not being used in mamba (#1084)
- pass in channel_alias and custom_channels from conda to mamba (#1081)

# 0.15.0 (July 9, 2021)

Big changes:

- improve solutions by inspecting dependency versions as well (libsolv PR:
  https://github.com/openSUSE/libsolv/pull/457) @wolfv
- properly implement strict channel priority (libsolv PR:
  https://github.com/openSUSE/libsolv/pull/459) @adriendelsalle
  - Note that this changes the meaning of strict and flexible priority as the
    previous implementation did not follow conda's semantics. Mamba now has
    three modes, just like conda: strict, flexible and disabled. Strict will
    completely disregard any packages from lower-priority channels if a
    package of the same name exists in a higher priority channel. Flexible
    will use packages from lower-priority channels if necessary to fulfill
    dependencies or explicitly requested (e.g. by version number). Disabled
    will use the highest version number, irregardless of the channel order.
- allow subdir selection as part of the channel: users can now specify an
  explicit list of subdirs, for example:

      `-c mychannel[linux-static64, linux-64, noarch]`

  to pull in repodata and packages from these three subdirs.
  Thanks for the contribution, @afranchuk! #1033

New features

- remove orphaned packages such as dependencies of explicitly installed
  packages (@adriendelsalle) #1040
- add a diff character before package name in transaction table to improve
  readability without coloration (@adriendelsalle) #1040
- add capability to freeze installed packages during `install` operation using
  `--freeze-installed` flag (@adriendelsalle) #1048
- Hide tokens and basic http auth secrets in log messages (#1061)
- Parse and use explicit platform specifications (thanks @afranchuk) (#1033)
- add pretty print to repoquery search (thanks @madhur-tandon) (#1018)
- add docs for package resolution

Bug fixes:

- Fix small output issues (#1060)
- More descriptive incorrect download error (thanks @AntoinePrv) #1066
- respect channel specific pins when updating (#1045)
- keep track features in PackageInfo class (#1046)

# 0.14.1 (June 25, 2021)

New features

- [micromamba] add remove command, to remove keys of vectors (@marimeireles)
  #1011

Bug fixes

- [micromamba] fixed in config prepend and append sequence (@adriendelsalle)
  #1023
- fix bug when username has @ (@madhur-tandon) #1025
- fix wrong update spec in history (@madhur-tandon) #1028
- [mamba] silent pinned packages using JSON output (@adriendelsalle) #1031

# 0.14.0 (June 16, 2021)

New features

- [micromamba] add `config set`, `get`, `append` and `prepend`, `remove`
  (@marimeireles) #838
- automatically include `pip` in conda dependencies when having pip packages to
  install (@madhur-tandon) #973
- add experimental support for artifacts verification (@adriendelsalle)
  #954,#955,#956,#963,#965,#970,#972,#978
- [micromamba] shell init will try attempt to enable long paths support on
  Windows (@wolfv) #975
- [micromamba] if `menuinst` json files are present, micromamba will create
  shortcuts in the start menu on Windows (@wolfv) #975
- Improve python auto-pinning and add --no-py-pin flag to micromamba
  (@adriendelsalle) #1010
- [micromamba] Fix constructor invalid repodata_record (@adriendelsalle) #1007
- Refactor log levels for linking steps (@adriendelsalle) #1009
- [micromamba] Use a proper requirements.txt file for pip installations #1008

Bug fixes

- fix double-print int transaction (@JohanMabille) #952
- fix strip function (@wolfv) #974
- [micromamba] expand home directory in `--rc-file` (@adriendelsalle) #979
- [micromamba] add yes and no as additional ways of answering a prompt
  (@ibebrett) #989
- fix long paths support on Windows (@adriendelsalle) #994

General improvement

- remove duplicate snippet (@madhur-tandon) #957
- add `trace` log level (@adriendelsalle) #988

Docs

- concepts, user guide, configuration, update installation and build locally
  (@adriendelsalle) #953
- advance usage section, linking (@adriendelsalle) #998
- repo, channel, subdir, repodata, tarball (@adriendelsalle) #1004
- artifacts verification (@adriendelsalle) #1000

# 0.13.1 (May 17, 2021)

Bug fixes

- [micromamba] pin only minor python version #948
- [micromamba] use openssl certs when not linking statically #949

# 0.13.0 (May 12, 2021)

New features

- [mamba & micromamba] aggregated progress bar for package downloading and
  extraction (thanks @JohanMabille) #928

Bug fixes

- [micromamba] fixes for micromamba usage in constructor #935
- [micromamba] fixes for the usage of lock files #936
- [micromamba] switched from libsodium to openssl for ed25519 signature
  verification #933

Docs

- Mention mambaforge in the README (thanks @s-pike) #932

# 0.12.3 (May 10, 2021)

New features

- [libmamba] add free-function to use an existing conda root prefix
  (@adriendelsalle) #927

General improvements

- [micromamba] fix a typo in documentation (@cjber) #926

# 0.12.2 (May 03, 2021)

New features

- [micromamba] add initial framework for TUF validation (@adriendelsalle) #916
  #919
- [micromamba] add channels from specs to download (@wolfv) #918

# 0.12.1 (Apr 30, 2021)

New features

- [micromamba] env list subcommand (@wolfv) #913

Bug fixes

- [micromamba] fix multiple shell init with cmd.exe (@adriendelsalle) #915
- [micromamba] fix activate with --stack option (@wolfv) #914
- [libmamba] only try loading ssl certificates when needed (@adriendelsalle)
  #910
- [micromamba] remove target_prefix checks when activating (@adriendelsalle)
  #909
- [micromamba] allow 'ultra-dry' config checks in final build (@adriendelsalle)
  #912

# 0.12.0 (Apr 26, 2021)

New features

- [libmamba] add experimental shell autocompletion (@wolfv) #900
- [libmamba] add token handling (@wolfv) #886
- [libmamba] add experimental pip support in spec files (@wolfv) #885

Bug fixes

- [libmamba] ignore failing pyc compilation for noarch packages (@wolfv) #904
  #905
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
- [libmamba] deactivate ca-certificates search when using offline mode
  (@adriendelsalle) #893

# 0.11.3 (Apr 21, 2021)

- [libmamba] make platform rc configurable #883
- [libmamba] expand user home in target and root prefixes #882
- [libmamba] avoid memory effect between operations on target_prefix #881
- [libmamba] fix unnecessary throwing target_prefix check in `clean` operation
  #880
- [micromamba] fix `clean` flags handling #880
- [libmamba] C-API teardown on error #879

# 0.11.2 (Apr 21, 2021)

- [libmamba] create "base" env only for install operation #875
- [libmamba] remove confirmation prompt of root_prefix in shell init #874
- [libmamba] improve overrides between target_prefix and env_name #873
- [micromamba] fix use of `-p,--prefix` and spec file env name #873

# 0.11.1 (Apr 20, 2021)

- [libmamba] fix channel_priority computation #872

# 0.11.0 (Apr 20, 2021)

- [libmamba] add experimental mode that unlock edge features #858
- [micromamba] add `--experimental` umamba flag to enable experimental mode
  #858
- [libmamba] improve base env creation #860
- [libmamba] fix computation of weakly canonical target prefix #859
- update libsolv dependency in env-dev.yml file, update documentation (thanks
  @Aratz) #843
- [libmamba] handle package cache in secondary locations, fix symlink errors
  (thanks wenjuno) #856
- [libmamba] fix CI cURL SSL error on macos with Darwin backend (thanks @wolfv)
  #865
- [libmamba] improve error handling in C-API by catching and returning an error
  code #862
- [libmamba] handle configuration lifetime (single operation configs) #863
- [libmamba] enable ultra-dry C++ tests #868
- [libmamba] migrate `config` operation implem from `micromamba` to `libmamba`
  API #866
- [libmamba] add capapbility to set CLI config from C-API #867

# 0.10.0 (Apr 16, 2021)

- [micromamba] allow creation of empty env (without specs) #824 #827
- [micromamba] automatically create empty `base` env at new root prefix #836
- [micromamba] add remove all CLI flags `-a,--all` #824
- [micromamba] add dry-run and ultra-dry-run tests to increase coverage and
  speed-up CI #813 #845
- [micromamba] allow CLI to override spec file env name (create, install and
  update) #816
- [libmamba] split low-level and high-level API #821 #824
- [libmamba] add a C high-level API #826
- [micromamba] support `__linux` virtual package #829
- [micromamba] improve the display of solver problems #822
- [micromamba] improve info sub-command with target prefix status (active, not
  found, etc.) #825
- [mamba] Change pybind11 to a build dependency (thanks @maresb) #846
- [micromamba] add shell detection for shell sub-command #839
- [micromamba] expand user in shell prefix sub-command #831
- [micromamba] refactor explicit specs install #824
- [libmamba] improve configuration (refactor API, create a loading sequence)
  #840
- [libmamba] support cpp-filesystem breaking changes on Windows fs #849
- [libmamba] add a simple context debugging (thanks @wolf) #820
- [libmamba] improve C++ test suite #830
- fix CI C++ tests (unix/libmamba) and Python tests (win/mamba) wrongly
  successful #853

# 0.9.2 (Apr 1, 2021)

- [micromamba] fix unc url support (thanks @adamant)
- [micromamba] add --channel-alias as cli option to micromamba (thanks
  @adriendelsalle)
- [micromamba] fix --no-rc and environment yaml files (thanks @adriendelsalle)
- [micromamba] handle spec files in update and install subcommands (thanks
  @adriendelsalle)
- add simple context debugging, dry run tests and other test framework
  improvements

# 0.9.1 (Mar 26, 2021)

- [micromamba] fix remove command target_prefix selection
- [micromamba] improve target_prefix fallback for CLI, add tests (thanks
  @adriendelsalle)

# 0.9.0 (Mar 25, 2021)

- [micromamba] use strict channels priority by default
- [micromamba] change config precedence order: API>CLI>ENV>RC
- [micromamba] `config list` sub command optional display of sources, defaults,
  short/long descriptions and groups
- [micromamba] prevent crashes when no bashrc or zshrc file found (thanks
  @wolfv)
- add support for UNC file:// urls (thanks @adamant)
- add support for use_only_tar_bz2 (thanks @tl-hbk @wolfv)
- add pinned specs for env update (thanks @wolfv)
- properly adhere to run_constrains (thanks @wolfv)

# 0.8.2 (Mar 12, 2021)

- [micromamba] fix setting network options before explicit spec installation
- [micromamba] fix python based tests for windows

# 0.8.1 (Mar 11, 2021)

- use stoull (instead of stoi) to prevent overflow with long package build
  numbers (thanks @pbauwens-kbc)
- [micromamba] fixing OS X certificate search path
- [micromamba] refactor default root prefix, make it configurable from CLI
  (thanks @adriendelsalle)
- [micromamba] set ssl backend, use native SSL if possible (thanks
  @adriendelsalle)
- [micromamba] sort json transaction, and add UNLINK field
- [micromamba] left align log messages
- [micromamba] libsolv log messages to stderr (thanks @mariusvniekerk)
- [micromamba] better curl error messages

# 0.8.0 (Mar 5, 2021)

- [micromamba] condarc and mambarc config file reading (and config subcommand)
  (thanks @adriendelsalle)
- [micromamba] support for virtual packages (thanks @adriendelsalle)
- [micromamba] set ssl backend, use native SSL if possible
- [micromamba] add python based testing framework for CLI
- [micromamba] refactor CLI and micromamba main file (thanks @adriendelsalle)
- [micromamba] add linking options (--always-copy etc.) (thanks
  @adriendelsalle)
- [micromamba] fix multiple prefix replacements in binary files
- [micromamba] fix micromamba clean (thanks @phue)
- [micromamba] change package validation settings to --safety-checks and
  --extra-safety-checks
- [micromamba] add update subcommand (thanks @adriendelsalle)
- [micromamba] support pinning packages (including python minor version)
  (thanks @adriendelsalle)
- [micromamba] add try/catch to WinReg getStringValue (thanks @SvenAdler)
- [micromamba] add ssl-no-revoke option for more conda-compatibility (thanks
  @tl-hbk)
- [micromamba] die when no ssl certificates are found (thanks @wholtz)
- [docs] add explanation for base env install (thanks @ralexx) and rename
  changelog to .md (thanks @kevinheavey)
- [micromamba] compare cleaned URLs for cache invalidation
- [micromamba] add regex handling to list command

# 0.7.14 (Feb 12, 2021)

- [micromamba] better validation of extracted directories
- [mamba] add additional tests for authentication and simple repodata server
- make LOG_WARN the default log level, and move some logs to INFO
- [micromamba] properly replace long shebangs when linking
- [micromamba] add quote for shell for prefixes with spaces
- [micromamba] add clean functionality
- [micromamba] always make target prefix path absolute

# 0.7.13 (Feb 4, 2021)

- [micromamba] Immediately exit after printing version (again)

# 0.7.12 (Feb 3, 2021)

- [micromamba] Improve CTRL+C signal handling behavior and simplify code
- [micromamba] Revert extraction to temporary directory because of invalid
  cross-device links on Linux
- [micromamba] Clean up partially extracted archives when CTRL+C interruption
  occurred

# 0.7.11 (Feb 2, 2021)

- [micromamba] use wrapped call when compiling noarch Python code, which
  properly calls chcp for Windows
- [micromamba] improve checking the pkgs cache
- [mamba] fix authenticated URLs (thanks @wenjuno)
- first extract to temporary directory, then move to final pkgs cache to
  prevent corrupted extracted data

# 0.7.10 (Jan 22, 2021)

- [micromamba] properly fix PATH when linking, prevents missing
  vcruntime140.dll
- [mamba] add virtual packages when creating any environment, not just on
  update (thanks @cbalioglu)

# 0.7.9 (Jan 19, 2021)

- [micromamba] fix PATH when linking

# 0.7.8 (Jan 14, 2021)

- [micromamba] retry on corrupted repodata
- [mamba & micromamba] fix error handling when writing repodata

# 0.7.6 (Dec 22, 2020)

- [micromamba] more console flushing for std::cout consumers

# 0.7.6 (Dec 14, 2020)

- [mamba] more arguments for repodata.create_pool

# 0.7.5 (Dec 10, 2020)

- [micromamba] better error handling for YAML file reading, allows to pass in
  `-n` and `-p` from command line
- [mamba & micromamba] ignore case of HTTP headers
- [mamba] fix channel keys are without tokens (thanks @s22chan)

# 0.7.4 (Dec 5, 2020)

- [micromamba] fix noarch installation for explicit environments

# 0.7.3 (Nov 20, 2020)

- [micromamba] fix installation of noarch files with long prefixes
- [micromamba] fix activation on windows with whitespaces in root prefix
  (thanks @adriendelsalle)
- [micromamba] add `--json` output to micromamba list

# 0.7.2 (Nov 18, 2020)

- [micromamba] explicit specs installing should be better now
  - empty lines are ignored
  - network settings are correctly set to make ssl verification work
- New Python repoquery API for mamba
- Fix symlink packing for mamba package creation and transmute
- Do not keep tempfiles around

# 0.7.1 (Nov 16, 2020)

- Handle LIBARCHIVE_WARN to not error, instead print warning (thanks @obilaniu)

# 0.7.0 (Nov 12, 2020)

- Improve activation and deactivation logic for micromamba
- Switching `subprocess` implementation to more tested `reproc++`
- Fixing Windows noarch entrypoints generation with micromamba
- Fix pre-/post-link script running with micromamba to use micromamba
  activation logic
- Empty environment creation skips all repodata downloading & solving
- Fix micromamba install when environment is activated (thanks @isuruf)
- Micromamba now respects the $CONDA_SUBDIR environment variable (thanks
  @mariusvniekerk)
- Fix compile time warning (thanks @obilaniu)
- Fixed wrong CondaValueError import statement in mamba.py (thanks @saraedum)

# 0.6.5 (Oct 2020)

- Fix code signing for Apple Silicon (osx-arm64) @isuruf

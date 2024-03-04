2024.03.04
==========

Releases:  libmamba 1.5.7, libmambapy 1.5.7, mamba 1.5.7, micromamba 1.5.7

Bug fixes:

- [mamba] Fix repoquery subcommand for Python 3.11/3.12 by @mbargull in https://github.com/mamba-org/mamba/pull/3170

CI

- [mamba] Run tests for conda=24.1 by @mbargull in https://github.com/mamba-org/mamba/pull/3191

2023.12.21
==========

Releases:  libmamba 1.5.6, libmambapy 1.5.6, mamba 1.5.6, micromamba 1.5.6

Bug fixes:

- [libmamba, libmambapy] Fix canonical name for local channels by @isuruf in https://github.com/mamba-org/mamba/pull/3056

2023.12.11
==========

Releases:  libmamba 1.5.5, libmambapy 1.5.5, mamba 1.5.5, micromamba 1.5.5

Bug fixes:

- [mamba] Backport: Convert absolute import path to relative for better compatibility by @mbargull in https://github.com/mamba-org/mamba/pull/3040

2023.12.05
==========

Releases:  libmamba 1.5.4, libmambapy 1.5.4, mamba 1.5.4, micromamba 1.5.4

Enhancement:

- [libmamba] Backport: Further improve search output by @delsner in https://github.com/mamba-org/mamba/pull/3006

Bug fixes:

- [micromamba] Backport: pytest.skip fix by @delsner in https://github.com/mamba-org/mamba/pull/3007
- [libmamba] Fix libmamba cmake version file by @opoplawski in https://github.com/mamba-org/mamba/pull/3023
- [mamba] Convert absolute import path to relative for better compatibility by @jezdez in https://github.com/mamba-org/mamba/pull/3036

2023.10.30
==========

Releases: libmamba 1.5.3, libmambapy 1.5.3, mamba 1.5.3, micromamba 1.5.3

Enhancement:

- [libmamba] Backport #2927: Allow defaults::* spec by @isuruf in https://github.com/mamba-org/mamba/pull/2936
- [libmamba] Backport #2921: return architecture levels for micromamba by @isuruf in https://github.com/mamba-org/mamba/pull/2936

Bug fixes:

- [libmamba] Backport #2926: Fix channels with slashes regression by @isuruf in https://github.com/mamba-org/mamba/pull/2936
- [libmambapy] Fix ctrl-c handling issues in libmambapy by @jaimergp in https://github.com/mamba-org/mamba/pull/2946
- [libmamba, libmambapy, mamba] Backport #2949: Parse remote_connect_timeout_secs as a double by @jjerphan in https://github.com/mamba-org/mamba/pull/2951

CI fixes and doc:

2023.10.16
==========

Releases: libmamba 1.5.2, libmambapy 1.5.2, mamba 1.5.2, micromamba 1.5.2

Enhancement:

- [libmamba] Build micromamba with /MD by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2862
- [libmamba] Build micromamba win with feedstock by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2859

Bug fixes:

- [mamba] Backport #2878: compatibility fixes for conda 23.9 and conda-libmamba-solver 23.9 by @jaimergp in https://github.com/mamba-org/mamba/pull/2890
- [mamba] implement mamba remove --force via --no-deps functionality (backport #2856) by @jaimergp in https://github.com/mamba-org/mamba/pull/2903
- [libmambapy] fix stubs by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2817

CI fixes and doc:

- [micromamba] Build micromamba win with feedstock by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2859
- [all] Fix CI by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2889

2023.09.05
==========

Releases: libmamba 1.5.1, libmambapy 1.5.1, mamba 1.5.1, micromamba 1.5.1

Enhancements:

- [libmamba] Add scope in util tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2775
- [micromamba] Speed up tests (a bit) by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2776
- [micromamba] Restore \_\_linux=0 test by @jonashaag in https://github.com/mamba-org/mamba/pull/2778
- [libmamba, micromamba] Enable Link Time Optimization by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2742
- [libmamba] Add libsolv namespace callback by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2796
- [all] Clearer output from micromamba search by @delsner in https://github.com/mamba-org/mamba/pull/2782
- [libmamba] add context.register_envs to control whether environments are registered to environments.txt or not by @jaimergp in https://github.com/mamba-org/mamba/pull/2802
- [libmamba, micromamba] Windows path manipulation and other cleanups by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2801
- [libmamba] Bring back repodata_use_zst by @jonashaag in https://github.com/mamba-org/mamba/pull/2790
- [micromamba] Implement --md5 and --channel-subdir for non-explicit env export by @jonashaag in https://github.com/mamba-org/mamba/pull/2672

Bug fixes:

- [libmamba] fix install pin by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2773
- [libmamba] Use generic_string for path on Windows unix shells by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2685
- [libmamba] Fix pins by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2786
- [libmamba] Various fixes by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2800
- [micromamba] Fix extra agrument in self-update reinit by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2787
- [libmamba] Parse subdirs in CLI match specs by @jonashaag in https://github.com/mamba-org/mamba/pull/2799

CI fixes and doc:

- [all] Splitted GHA workflow by @JohanMabille in https://github.com/mamba-org/mamba/pull/2779
- [all] Use Release build mode in Windows CI by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2785
- [micromamba] Fix wrong command description by @Hind-M in https://github.com/mamba-org/mamba/pull/2804

2023.08.23
==========

Releases: libmamba 1.5.0, libmambapy 1.5.0, mamba 1.5.0, micromamba 1.5.0

Enhancements:

- [libmamba] All headers at the top by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2658
- [libmamba] Add boolean expression tree by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2591
- [libmamba] Add VersionSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2502
- [micromamba] Refactor test_repoquery to use new fixtures by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2691
- [libmamba] Use xdg schemas for config saving/reading (minified) by @danpf in https://github.com/mamba-org/mamba/pull/2714
- [micromamba] Remove warnings from test_activation by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2727
- [micromamba] Refactor test_shell by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2726
- [libmamba] specs platform by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2729
- [libmamba] Safe Curl opt in url.cpp by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2734
- [libmamba] Add win-arm64 support by @isuruf in https://github.com/mamba-org/mamba/pull/2745
- [libmamba] Move util_string to utility library by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2739
- [libmamba] Remove get_clean_dirs() by @jonashaag in https://github.com/mamba-org/mamba/pull/2748
- [micromamba] Fix and improve static builds by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2755
- [all] Enable pytest color output by @jonashaag in https://github.com/mamba-org/mamba/pull/2759
- [libmamba, micromamba] Isolate URL object by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2744
- [all] Fix warnings by @Hind-M in https://github.com/mamba-org/mamba/pull/2760
- [libmamba] New apis for downloading by @JohanMabille in https://github.com/mamba-org/mamba/pull/2695

Bug fixes:

- [libmamba] Respect subdir in match spec by @ThomasBlauthQC in https://github.com/mamba-org/mamba/pull/2300
- [libmamba] Fixed move constructor in CURLHandle by @JohanMabille in https://github.com/mamba-org/mamba/pull/2710
- [micromamba] Fix wrong activated PATH in micromamba shell by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2722
- [mamba] Fix Repo missing url by @Hind-M in https://github.com/mamba-org/mamba/pull/2723
- [mamba] Try Revert "Fix Repo missing url" by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2730
- [mamba] fix subcommands handling in recent versions of conda by @jaimergp in https://github.com/mamba-org/mamba/pull/2732
- [libmamba] Remove created prefix if aborted with --platform by @Hind-M in https://github.com/mamba-org/mamba/pull/2738
- [libmamba] Add missing newline in legacy errors by @jaimergp in https://github.com/mamba-org/mamba/pull/2743
- [mamba] Try fix Missing Url error by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2731
- [libmamba] fix: added missing hook_preamble() for powershell hook by @chawyehsu in https://github.com/mamba-org/mamba/pull/2761
- [micromamba] Fix config list sources by @Hind-M in https://github.com/mamba-org/mamba/pull/2756
- [libmamba] Fix fish completion by @soraxas in https://github.com/mamba-org/mamba/pull/2769
- [libmamba, micromamba] Fix \_\_linux virtual package default version by jonashaag in https://github.com/mamba-org/mamba/pull/2749
- [micromamba] Strong pin in test by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2774
- [mamba] fix: only reactivate current environment by @chawyehsu in https://github.com/mamba-org/mamba/pull/2763
- [micromamba] Revert failing test by @jonashaag in https://github.com/mamba-org/mamba/pull/2777

CI fixes and doc:

- [mamba, micromamba] Update troubleshooting.rst by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2675
- [all] Ignore format changes in git blame by @jonashaag in https://github.com/mamba-org/mamba/pull/2690
- [mamba] Put more "not recommended" warnings in the installation instructions by @jonashaag in https://github.com/mamba-org/mamba/pull/2711
- [micromamba] Add command to docs for completeness by @danpf in https://github.com/mamba-org/mamba/pull/2717
- [micromamba] fix: Correct a command in installation.rst by @wy-luke in https://github.com/mamba-org/mamba/pull/2703
- [micromamba] Split Mamba and Micromamba installation docs by @jonashaag in https://github.com/mamba-org/mamba/pull/2719
- [micromamba] fix: Shell completion section title missing by @wy-luke in https://github.com/mamba-org/mamba/pull/2764
- [all] Add Debug build type by @Hind-M in https://github.com/mamba-org/mamba/pull/2762

2023.07.13
==========

Releases: libmamba 1.4.9, libmambapy 1.4.9, mamba 1.4.9, micromamba 1.4.9

Bug fixes:

- [micromamba] Added upper bound to fmt to avoid weird failure on ci (windows only) by @JohanMabille in https://github.com/mamba-org/mamba/pull/2671
- [libmamba] Fixed missing key <channel_name> in channel <channel_list> issue by @JohanMabille in https://github.com/mamba-org/mamba/pull/2668

2023.07.11
==========

Releases: libmamba 1.4.8, libmambapy 1.4.8, mamba 1.4.8, micromamba 1.4.8

Enhancements:

- [libmamba, micromamba] No profile.d fallback in rc files by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2649
- [libmamba] Removed unused function by @Klaim in https://github.com/mamba-org/mamba/pull/2656
- [libmamba] Replace MTransaction::m_remove with Solution by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2603
- [mamba] Improve warning when package record not found by @maresb in https://github.com/mamba-org/mamba/pull/2662

Bug fixes:

- [libmamba] Fixed zst check in MSubdirData by @JohanMabille in https://github.com/mamba-org/mamba/pull/2661

CI fixes and doc;

- [mamba] Force conda-forge in Anaconda install by @jonashaag in https://github.com/mamba-org/mamba/pull/2619
- [mamba, micromamba] Update installation docs by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2654

2023.07.06
==========

Releases: libmamba 1.4.7, libmambapy 1.4.7, mamba 1.4.7, micromamba 1.4.7

Enhancements:

- [libmamba] ZST support to mamba and remove the feature flag by @johnhany97 in https://github.com/mamba-org/mamba/pull/2642
- [libmamba] Add Version::starts_with and Version::compatible_with by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2645
- [libmamba, libmambapy] Create Solver solution by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2584

Bug fixes:

- [libmamba] call init_console to prevent UTF8 errors when extracting packages by @wolfv in https://github.com/mamba-org/mamba/pull/2655
  [libmambapy, mamba] Call init_console in mamba to prevent UTF8 errors when extracting packages by @JohanMabille in https://github.com/mamba-org/mamba/pull/2657

CI fixes and doc:

- [libmambapy] Fixup python-api docs slightly by @HaoZeke in https://github.com/mamba-org/mamba/pull/2285

2023.06.30
==========

Releases: libmamba 1.4.6, libmambapy 1.4.6, mamba 1.4.6, micromamba 1.4.6

Enhancements:

- [libmamba] Channels refactoring/cleaning by @Hind-M in https://github.com/mamba-org/mamba/pull/2537
- [libmamba] Troubleshooting update by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2635
- [libmamba] Direcly call uname for linux detection by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2624

Bug fixes:

- [libmamba] Fix build with older Clang by @pavelzw in https://github.com/mamba-org/mamba/pull/2625
- [libmambapy, mamba] Fixed missing subdirs in mamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2632
- [libmamba] Add missing noarch in PackageInfo serialization by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2641
- [libmamba] Allow --force-reinstall on uninstalled specs by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2636

CI fixes and doc:

- [micromamba] Document micromamba support for conda-lock spec files by @mfisher87 in https://github.com/mamba-org/mamba/pull/2621

2023.06.27
==========

Releases: libmamba 1.4.5, libmambapy 1.4.5, mamba 1.4.5, micromamba 1.4.5

Enhancements:

- [all] No singleton: ChannelContext, ChannelBuilder and channel cache by @Klaim in https://github.com/mamba-org/mamba/pull/2455
- [libmamba, libmambapy] Move problem graph creation to MSolver by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2515
- [libmamba] Add ObjSolver by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2504
- [micromamba] Micromamba tests improvements by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2517
- [libmamba] Use ObjSolver in MSolver by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2544
- [all] Common CMake presets by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2532
- [libmamba] Wrap libsolv Transaction by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2554
- [libmamba] Split the transaction.hpp header by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2564
- [libmamba] Add more tests for channel canonical_name by @Hind-M in https://github.com/mamba-org/mamba/pull/2568
- [libmamba] use ObjTransaction in MTransaction by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2565
- [libmamba] https://github.com/mamba-org/mamba/pull/2590 by @jonashaag in https://github.com/mamba-org/mamba/pull/2590
- [libmamba] Libcurl: Cleaning and comments by @Hind-M in https://github.com/mamba-org/mamba/pull/2534
- [all] No singleton: configuration by @Klaim in https://github.com/mamba-org/mamba/pull/2541
- [libmamba] Added filtering iterators by @JohanMabille in https://github.com/mamba-org/mamba/pull/2594
- [libmamba] Use ObjSolver wrapper in MSolver by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2602
- [all] Remove banner by @jonashaag in https://github.com/mamba-org/mamba/pull/2298
- [libmamba, libmambapy] LockFile behavior on file-locking is now almost independent from Context by @Klaim in https://github.com/mamba-org/mamba/pull/2608
- [micromamba] Add topological sort explict export tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2618
- [libmamba] Small whitespace fix in error messages by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2623

Bug fixes:

- [libmamba, micromamba] Use subsub commands for micromamba shell by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2527
- [micromamba] Fix umamba tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2540
- [mamba] fix different behavior between --version and -V options by @alaniwi in https://github.com/mamba-org/mamba/pull/2539
- [libmamba, micromamba] Honor envs_dirs by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2538
- [libmambapy] Fix stubgens by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2556
- [mamba] Fix server auth test by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2560
- [libmamba] Fixed Windows test build by @JohanMabille in https://github.com/mamba-org/mamba/pull/2585
- [libmamba] Add missing cstdint include to libmamba/src/solv-cpp/solvable.cpp by @maxyvisser in https://github.com/mamba-org/mamba/pull/2587
- [libmamba, micromamba] Fix wrong download url for custom channels by @Hind-M in https://github.com/mamba-org/mamba/pull/2596
- [libmamba, micromamba] Fix --force-reinstall by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2601
- [libmamba] Handle pip <-> python cycle in topo sort by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2613
- [libmamba] Fix add missing pip PREREQ_MARKER by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2612
- [libmamba] Fix lockfiles topological sort by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2616
- [libmamba] Fix mising SAT message on already installed packages by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2622

CI fixes and doc:

- [libmamba] Fixe clang-format by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2531
- [micromamba] Use only vcpkg for static windows build by @pavelzw in https://github.com/mamba-org/mamba/pull/2520
- [all] update the umamba GHA link by @ocefpaf in https://github.com/mamba-org/mamba/pull/2542
- [all] Extend troubleshooting docs by @jonashaag in https://github.com/mamba-org/mamba/pull/2569
- [micromamba] Try new vcpkg by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2572
- [all] Update pre-commit hooks by @jonashaag in https://github.com/mamba-org/mamba/pull/2586
- [all] Move GHA to setup-micromamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2545
- [all] Switch linters to setup-micromamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2600
- [all] Switch to setup-micromamba by @pavelzw in https://github.com/mamba-org/mamba/pull/2610
- [all] Fix broken ref directives in docs by @mfisher87 in https://github.com/mamba-org/mamba/pull/2620

2023.05.16
==========

Releases: libmamba 1.4.4, libmambapy 1.4.4, mamba 1.4.4, micromamba 1.4.4

Bug fixes:

- [micromamba] fix: let the new executable run the shell init script by @ruben-arts in https://github.com/mamba-org/mamba/pull/2529
- [libmambapy] Support future deprecated API for Context by @Hind-M in https://github.com/mamba-org/mamba/pull/2494
- [libmamba] Fix CURLHandle::get_info on 32bit platform by e8035669 in https://github.com/mamba-org/mamba/pull/2528

2023.05.15
==========

Releases: libmamba 1.4.3, libmambapy 1.4.3, mamba 1.4.3, micromamba 1.4.3

Enhancements:

- [libmamba] No Storing Channel\* and MRepo\* in Solvables by @AntoinPrv in https://github.com/mamba-org/mamba/pull/2409
- [libmamba, libmambapy] Remove dead code / attribute by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2454
- [all] Context structuring by @Hind-M in https://github.com/mamba-org/mamba/pull/2432
- [libmamba] Clean up fetch by @Hind-M in https://github.com/mamba-org/mamba/pull/2452
- [libmamba] Wapped curl multi handle by @JohanMabille in https://github.com/mamba-org/mamba/pull/2459
- [libmamba] Remove empty test\_flat\_set.hpp by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2471
- [libmamba] Add doctest printer for pair and vector by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2470
- [libmamba] Add topological sort by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2467
- [mamba] Add mamba version to mamba info output by @Hind-M in https://github.com/mamba-org/mamba/pull/2477
- [libmamba, libmambapy] Store PackageInfo::track\_features as a vector by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2478
- [libmamba] Use topological sort instead of libsolv by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2472
- [libmamba] Remove assign\_or in favor of json::value by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2487
- [all] Resume Context structuring by @Hind-M in https://github.com/mamba-org/mamba/pull/2460
- [micromamba] cleanup: fix pytest warnings by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2490
- [libmamba] Improve micromamba transaction message by @ruben-arts in https://github.com/mamba-org/mamba/pull/2474
- [libmamba] Remove unused raw function in subdirdata by @Hind-M in https://github.com/mamba-org/mamba/pull/2491
- [libmamba] Wrap ::Pool and ::Repo by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2401
- [libmamba] Curl wrapping by @Hind-M in https://github.com/mamba-org/mamba/pull/2468
- [libmamba] Reset fish shell status even if variable not exists by @soraxas in https://github.com/mamba-org/mamba/pull/2509
- [libmamba, libmambapy, micromamba] Use libsolv wrappers in MPool and MRepo by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2453
- [libmamba, micromamba] add bearer token authentication by @wolfv in https://github.com/mamba-org/mamba/pull/2512

Bug fixes:

- [libmamba] fix: parsing of empty track\_features by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2485
- [libmamba] track\_feature typo by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2488
- [libmamba, mamba] Move repoquery python test from libmamba (not run) to mamba by @Hind-M in https://github.com/mamba-org/mamba/pull/2489
- [libmamba] Set log\_level to critical with --json option by @Hind-M in https://github.com/mamba-org/mamba/pull/2484
- [libmamba] Add missing cstdint include for GCC 13 by @alexfikl in https://github.com/mamba-org/mamba/pull/2511
- [libmamba] Forward NETRC environment variable to curl, if exported by @timostrunk in https://github.com/mamba-org/mamba/pull/2497
- [libmamba] Remove wrong $Args in psm1 by @troubadour-hell in https://github.com/mamba-org/mamba/pull/2499
- [libmamba] Avoid using /tmp by @johnhany97 in https://github.com/mamba-org/mamba/pull/2447
- [libmamba] Fixed winreg search by @JohanMabille in https://github.com/mamba-org/mamba/pull/2526

CI fixes and doc:

- [libmamba] Extend troubleshooting docs by @jonashaag in https://github.com/mamba-org/mamba/pull/2451
- [all] Extend issue template by @jonashaag in https://github.com/mamba-org/mamba/pull/2310

2023.04.06
==========

Releases: libmamba 1.4.2, libmambapy 1.4.2, mamba 1.4.2, micromamba 1.4.2

Enhancements:

- [libmamba] Small libsolv improvements by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2399
- [micromamba] Refactor test\_create, test\_proxy, and test\_env for test isolation by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2416
- [libmamba] Improve message after the env creating with micromamba by @xmnlab in https://github.com/mamba-org/mamba/pull/2425
- [libmamba] Use custom function to properly parse matchspec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2433
- [libmamba, micromamba] Remove const ref to string\_view in codebase by @Hind-M in https://github.com/mamba-org/mamba/pull/2440
- [libmamba] Wrap more libcurl calls by @Hind-M in https://github.com/mamba-org/mamba/pull/2421

Bug fixes:

- [libmamba] Fix PKG\_BUILDNUM env variable for post-link scripts by nsoranzo in https://github.com/mamba-org/mamba/pull/2420
- [libmamba] Solve a corner case in the SAT error messages by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2423
- [libmamba] Windows: Fixed environment variables not read as unicode by @Klaim in https://github.com/mamba-org/mamba/pull/2417
- [libmamba] Fix segfault in add\_pin/all\_problems\_structured by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2428
- [mamba] Safely ignores virtual packages in `compute_final_precs` function by @mariusvniekerk in https://github.com/mamba-org/mamba/pull/2424

CI fixes and doc:

- [libmambapy, micromamba] Fixes typos by @nsoranzo in https://github.com/mamba-org/mamba/pull/2419
- [micromamba] Remove outdated micromamba experimental warning by @jonashaag in https://github.com/mamba-org/mamba/pull/2430
- [libmamba] Replaced libtool 2.4.6\_9 with libtool 2.4.7-3 in vcpkg builds by @JohanMabille in https://github.com/mamba-org/mamba/pull/2439
- [all] Migrated to doctest by @JohanMabille in https://github.com/mamba-org/mamba/pull/2436


2023.03.28
==========

Releases: libmamba 1.4.1, libmambapy 1.4.1, mamba 1.4.1, micromamba 1.4.1

Enhancements:

- [libmamba] First version/steps of unraveling fetch code and wrapping libcurl by @Hind-M in https://github.com/mamba-org/mamba/pull/2376
- [libmamba] Parse repodata.json by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2391
- [libmamba] TimeRef is not a singleton anymore by @Klaim in https://github.com/mamba-org/mamba/pull/2396
- [libmamba] Handle url via ChannelBuilder in Repo constructor by @jaimergp in https://github.com/mamba-org/mamba/pull/2398
- [libmamba, micromamba] add option to relocate prefix by @DerThorsten in https://github.com/mamba-org/mamba/pull/2385
- [libmamba] Renamed validate namespace to `mamba::validation` by @Klaim in https://github.com/mamba-org/mamba/pull/2411

Bug fixes:

- [libmamba] Fixed build with older Clang by @ZhongRuoyu in https://github.com/mamba-org/mamba/pull/2397


2023.03.22
==========

Releases: libmamba 1.4.0, libmambapy 1.4.0, mamba 1.4.0, micromamba 1.4.0

Enchancements:

- [all] Implemented recursive dependency printout in repoquery by @timostrunk in https://github.com/mamba-org/mamba/pull/2283
- [libmamba, libmambapy, micromamba] Agressive compilation warnings by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2304
- [all] Fine tune clang-format by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2290
- [libmamba] Added checked numeric cast by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2315
- [libmamba, libmambapy] Activated SAT error messages by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2325
- [libmamba] Added RISC-V support by @dtcxzyw in https://github.com/mamba-org/mamba/pull/2329
- [mamba] Allow the direct installation of both .tar.bz2 and .conda packages by @romain-intel in https://github.com/mamba-org/mamba/pull/2333
- [libmamba, libmambapy] Removed redundant `DependencyInfo` by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2314
- [libmamba] Isolate solv::ObjQueue by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2289
- [libmamba] Removed unused libarchive header in fetch by @hind-M in https://github.com/mamba-org/mamba/pull/2341
- [libmamba] Removed duplicated header by @JohanMabille in https://github.com/mamba-org/mamba/pull/2343
- [libmamba] Cleaned `util_string` by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2339
- [libmamba, libmambapy, micromamba] Only full shared or full static builds by @JohanMabille in https://github.com/mamba-org/mamba/pull/2342
- [libmamba, libmambapy, micromamba] Fixed repoquery commands working with installed packages only by @Hind-M in https://github.com/mamba-org/mamba/pull/2330
- [libmamba] Added a heuristic to better handle the (almost) cyclic Python conflicts by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2318
- [libmamba, libmambapy, mamba] Isolate `PackageInfo` from libsolv from @AntoinePrv in https://github.com/mamba-org/mamba/pull/2340
- [libmamba] Added `strip_if` functions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2344
- [libmamba] Addded conda.rc Options for Existing Remote Settings by @srilman in https://github.com/mamba-org/mamba/pull/2306
- [micromamba] Added micromamba server by @wolfv in https://github.com/mamba-org/mamba/pull/2185
- [libmamba] Hide independent curl code and compression structures in unexposed files by @Hind-M in https://github.com/mamba-org/mamba/pull/2366
- [libmamba] Added `strip_parts` functions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2347
- [libmamba] Added parsing of Conda version by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2373
- [libmamba] Slight refactoring of the utility library by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2387

Bug fixes:

- [libmamba] Fixed invalid reinstall count display by @timostrunk in https://github.com/mamba-org/mamba/pull/2284
- [libmamba] Fixed segmentation fault in case of an invalid package name by @timostrunk in https://github.com/mamba-org/mamba/pull/2287
- [micromamba] Fixed `micromamba env export` to get channel name instead of full url by @Hind-M in https://github.com/mamba-org/mamba/pull/2260
- [mamba] Fixed `mamba init --no-user` by @xylar in https://github.com/mamba-org/mamba/pull/2324
- [libmambapy] Fixed repoquery output of mamba when query format is JSON by @JohanMabille in https://github.com/mamba-org/mamba/pull/2353
- [libmamba] Fixed `to_lower(wchar_t)` and `to_upper(wchar_t)` by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2360
- [libmamba] Fixed undefined-behaviors reported by UBSAN by @klaim in https://github.com/mamba-org/mamba/pull/2384

CI fixes & docs:

- [libmamba] Fixed sign warning in tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2293
- [micromamba] Added missing dependency in local recipe by @wolfv in https://github.com/mamba-org/mamba/pull/2334
- [mamba] `repoquery depends` requires the package to be installed or to specify a channel by @samtygier in https://github.com/mamba-org/mamba/pull/2098
- [libmamba] Structured test directory layout by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2380
- [micromamba] Fixed Conda Lock Path by @funtion in https://github.com/mamba-org/mamba/pull/2393


2023.02.09
==========

Releases: libmamba 1.3.1, libmambapy 1.3.1, mamba 1.3.1, micromamba 1.3.1

A bugfix release for 1.3.0!

Bug fixes:
- [micromamba, libmamba] fix up single download target perform finalization to make lockfile download work by @wolfv in https://github.com/mamba-org/mamba/pull/2274
- [micromamba] use CONDA_PKGS_DIRS even in explicit installation trasactions by @hmaarrfk in https://github.com/mamba-org/mamba/pull/2265
- [libmamba, micromamba] fix rename or remove by @wolfv in https://github.com/mamba-org/mamba/pull/2276
- [libmamba] add channel specific job with new str by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2277
- [libmamba, micromamba] fix `micromamba shell` for base environment, and improve behavior when `auto_activate_base` is true by @jonashaag, @Hind-M and @wolfv https://github.com/mamba-org/mamba/pull/2272

Docs:
- add micromamba docker image by @wholtz in https://github.com/mamba-org/mamba/pull/2266
- added biweekly meetings information to README by @JohanMabille in https://github.com/mamba-org/mamba/pull/2275
- change docs to homebrew/core by @pavelzw in https://github.com/mamba-org/mamba/pull/2278


2023.02.03
==========

Releases: libmamba 1.3.0, libmambapy 1.3.0, mamba 1.3.0, micromamba 1.3.0

Enhancements:
- [libmambapy] add `use_lockfiles` to libmambapy bindings by @jaimergp in https://github.com/mamba-org/mamba/pull/2256
- [micromamba] add functionality to download lockfiles from internet by @wolfv in https://github.com/mamba-org/mamba/pull/2229
- [micromamba] Stop run command when given prefix does not exist by @Hind-M in https://github.com/mamba-org/mamba/pull/2257
- [micromamba] Install pip deps like conda by @michalsieron in https://github.com/mamba-org/mamba/pull/2241
- [libmamba, micromamba] switch to repodata.state.json format from cep by @wolfv in https://github.com/mamba-org/mamba/pull/2262

Bug fixes:
- [micromamba, libmamba] Fix temporary file renaming by @jonashaag in https://github.com/mamba-org/mamba/pull/2242
- [mamba] Fix mamba / conda incompatibility by @jonashaag in https://github.com/mamba-org/mamba/pull/2249

CI fixes & docs:
- [micromamba] use proper recipe also on macOS by @wolfv in https://github.com/mamba-org/mamba/pull/2225
- [micromamba] Update micromamba installation docs for Windows by @Tiksagol in https://github.com/mamba-org/mamba/pull/2218
- [all] docs: defaults should not be used with conda-forge by @jonashaag in https://github.com/mamba-org/mamba/pull/2181
- [all] fix tests for pkg_cache by @wolfv in https://github.com/mamba-org/mamba/pull/2259
- [libmamba] Added missing public dependency to libmambaConfig.cmake by @JohanMabille in https://github.com/mamba-org/mamba/pull/2227
- [libmambapy] Remove unused `get_tarball` function by @Hind-M in https://github.com/mamba-org/mamba/pull/2261
- [mamba] update documentation for mamba activate by @cdeepali in https://github.com/mamba-org/mamba/pull/2176
- [micromamba] Fix Windows static builds by @jonashaag in https://github.com/mamba-org/mamba/pull/2228

2023.01.16
==========

Releases: libmamba 1.2.0, libmambapy 1.2.0, mamba 1.2.0, micromamba 1.2.0

This release contains some speed improvements: download repodata faster as zstd encoded files (configure using
`repodata_use_zst: true` in your `~/.mambarc` file). Also, `.conda` file extraction is now faster, a prefix
with spaces works better thanks to a new "shebang" style and the `micromamba package compress` and `transmute`
commands produce better conda packages.

Enhancements:

- [micromamba, libmamba] Make tarballs look more similar to conda-package-handling by @wolfv in #2177, #2217
- [micromamba, libmamba] Use new shebang style by @wolfv in #2211
- [micromamba, libmamba] Faster conda decompress by @wolfv in #2200
- [micromamba, libmamba] Initial repodata.zst support by @wolfv & @jonashaag in #2113

Bug fixes:

- [micromamba, libmamba] log warnings but ignore cyclic symlinks by @wolfv in #2212
- [mamba] Add Context binding for experimental_sat_error_message by @syslaila in #2143
- [libmamba] Error messages improvements by @AntoinePrv in #2149
- [micromamba, libmamba] Report failure when packages to remove don't exist. (#2131) by @Klaim in #2132
- [libmamba] Fixing typo in solver errors by @shughes-uk in #2168
- [micromamba] Fix micromamba shell completion when running 'shell hook' directly by @TomiBelan in #2137
- [libmamba] Extend `last_write_time` implementation by special-casing file touching by @coroa in #2141
- [libmamba, micromamba] Don't create a prefix which is missing conda-meta by @maresb in #2162
- [libmamba, micromamba, mamba] Fix `custom_channels` parsing by @XuehaiPan in #2207
- [micromamba] Fix #1783: Add `micromamba env create` by @jonashaag in #1790
- [mamba] Use check_allowlist from conda by @duncanmmacleod in #2220

CI fixes & docs:

- Improve build env cleanup by @jonashaag in #2213
- Run conda_nightly once per week by @jonashaag in #2147
- Update doc by @Hind-M in #2156
- Use Conda canary in nightly tests by @jonashaag in #2180
- Expliclity point to libmamba test data independently of cwd by @AntoinePrv in #2158
- Add bug report issue template by @jonashaag in #2182
- Downgrade curl to fix micromamba on macOS x64 by @wolfv in #2205
- Use conda-forge micromamba feedstock instead of a fork by @JohanMabille in #2206
- Update pre-commit versions by @jonashaag in #2178
- Use local meta.yaml by @wolfv in #2214
- Remove feedstock patches by @wolfv in #2216
- Fixed static dependency order by @JohanMabille in #2201


2022.11.25
==========

Releases: libmamba 1.1.0, libmambapy 1.1.0, mamba 1.1.0, micromamba 1.1.0

Some bugfixes for 1.0 and experimental release of the new solver messages

Bug fixes

- [micromamba] Fix fish scripts (thanks @JafarAbdi, @raj-magesh, @jonashaag) #2101
- [mamba] Fix activate/deactivate in fish shell (thanks @psobolewskiPhD) #2081
- [micromamba] fix direct hook for powershell #2122
- [libmamba] Fix libmamba CMake config file after dependency change (thanks @l2dy) #2091
- [micromamba] fixes for ssl init and static build #2076

Enhancements

- [libmamba] Add safe signed/unsigned conversion (thanks @AntoinePrv) #2087
- [libmamba] Move to fmt::terminal_color and other output IO improvements & drop termcolor (thanks @AntoinePrv) #2085
- [libmamba, micromamba] Handle non leaf conflicts (thanks @AntoinePrv) #2133
- [libmamba, micromamba] Bind SAT error messages to python (thanks @AntoinePrv) #2127
- [libmamba, micromamba] Nitpicking error messages (thanks @AntoinePrv) #2121
- [libmamba, micromamba] Tree error message improvements (thanks @AntoinePrv) #2093
- [libmamba, micromamba] Tree error message (thanks @AntoinePrv) #2064
- [libmamba, micromamba] Add experimental flag for error messages (thanks @AntoinePrv) #2080
- [libmamba, micromamba] Handle non leaf conflicts (thanks @AntoinePrv) #2133
- [mamba] fix: Don't print banner in quiet mode (thanks @corneliusroemer) #2097
- [all] ci: Update pre-commit-config #2092
- [all] docs: Add warning to manual install instructions #2100
- [all] docs: Consistently use curl for fetching files #2126


2022.11.01
==========

Releases: libmamba 1.0.0, libmambapy 1.0.0, mamba 1.0.0, micromamba 1.0.0

Our biggest version number yet! Finally a 1.0 release :)

New notable micromamba features include:

- improved shell scripts with autocompletion available in PowerShell, xonsh, fish, bash and zsh
- `micromamba shell -n someenv`: enter a sub-shell without modifying the system
- `micromamba self-update`: micromamba searches for updates and installs them if available
							(you can also downgrade using `--version 0.26.0` for example)

Bug fixes:
- [micromamba, libmamba] ignore "Permission denied" in `env::which` (thanks @Rafflesiaceae) #2067
- [micromamba] Link micromamba with static libc++.a and system libc++abi (thanks @isuruf) #2069
- [libmamba, micromamba] Fix an infinite loop in replace_all() when the search string is empty (thanks @tsibley)
- [mamba, libmambapy] Ensure package record always has subdir (thanks @jaimergp) #2016
- [libmamba, micromamba] Do not crash when permissions cannot be changed, instead log warning (thanks @hwalinga)

Enhancements:
- [libmamba] Rewrite LockFile interface (thanks @Klaim) #2014
- [micromamba] Add `micromamba env remove` (thanks @Hind-M) #2002
- [micromamba] add self-update functionality (#2023)
- [micromamba] order dependencies alphabetically from `micromamba env export` (thanks @torfinnberset) #2063

- [libmambapy] add stubs with pybind11-stubgen (thanks @dholth) #1983
- [mamba] Support for mamba init fish (thanks @dlukes) #2006
- [mamba] Fix Repoquery help text (thanks @BastianZim) #1998

- [all] Fix ci deprecation warnings, upload conda-bld artifacts for failed builds #2058, #2062
- [all] Explicitly define SPDLOG_FMT_EXTERNAL and use spdlog header only use external fmt (thanks @AntoinePrv) #2060, #2048
- [all] Fix CI by pointing to updated feedstock and fixing update tests (thanks @AntoinePrv) #2055
- [all] Add authentication with urlencoded @ to proxy test (#2024) @AdrianFreundQC
- [all] better test isolation (thanks @AntoinePrv) #1903
- [all] Test special characters in basic auth (thanks @jonashaag) #2012

- [libmamba] ProblemsGraph compression (thanks @AntoinePrv) #2019
- [libmamba] vector_set compare function (thanks @AntoinePrv) #2051
- [libmamba] Clean up util_graph header and tests (thanks @AntoinePrv) #2039
- [libmamba] Add string utilities (thanks @AntoinePrv) #
- [libmamba] Dynamic tree walk of the Compressed problem graph
- [libmamba] Creating the initial problems graph (thanks @syslaila) #1891

2022.10.04
==========

Releases: libmamba 0.27.0, libmambapy 0.27.0, mamba 0.27.0, micromamba 0.27.0

Bug fixes:
- [libmamba, micromamba] fix lockfiles relying on PID (thanks @Klaim) #1915
- [micromamba] fix error condition in micromamba run to not print warning every time #1985
- [micromamba] fix error when getting size of directories (thanks @Klaim) #1982
- [micromamba] fix crash when installing pip packages from env files (thanks @Klaim) #1978
- [libmambapy] make compilation with external fmt library work #1987

Enhancements:
- [micromamba] add cross-compiled builds to CI (thanks @pavelzw) #1976, #1989

2022.09.29
==========

Releases: libmamba 0.26.0, libmambapy 0.26.0, mamba 0.26.0, micromamba 0.26.0

Bug fixes:
- [micromamba] fix fish scripts (thanks @jonashaag) #1975
- [micromamba] fix issues with `micromamba ps` #1953
- [libmamba, micromamba] add symlinks and empty directories to archive for `micromamba package compress` #1955
- [mamba] fix mamba.sh and mamba.bat shell scripts to work with conda 22.09 #1952
- [libmamba] increase curl buffer size for faster max download speeds (thanks @jonashaag) #1949
- [micromamba] add `micromamba info --licenses` to print licenses of used OSS (thanks @jonashaag) #1933
- [micromamba] proper quoting in `micromamba run` (thanks @jonashaag) #1936
- [micromamba] install pip dependencies and by category for YAML lockfiles (thanks @jvansanten) #1908 #1917
- [libmamba] fix crash when installing from environment lockfile (thanks @Klaim) #1893
- [micromamba] fix update for packages with explicit channels (thanks @AntoinePrv) #1864
- [libmamba] fix linux version regex (thanks @kelszo) #1852
- [libmamba] remove duplicate console output (thanks @pavelzw) #1845
- [mamba] remove usage of non-existing function (thanks @AntoinePrv) #1835

Enhancements:
- [libmamba] add option to disable file locks globally (thanks @danpf @JohanMabille) #1830
- [libmamba] extend graph class for better solver messaging work (thanks @syslaila @AntoinePrv) #1880
- [micromamba] only call compinit once to fix oh-my-zsh (thanks @AntoinePrv) #1911
- [libmamba] use std::filesystem instead of ghc::filesystem (thanks @Klaim) #1665
- [libmamba] add missing SolverRuleInfo enum entries (thanks @AntoinePrv) #1833
- [micromamba] CI: add fully static micromamba build (thanks @jonashaag) #1821
- [mamba, micromamba] allow configuring proxies (thanks @AdrianFreundQC) #1841


2022.07.29
==========

Releases: micromamba 0.25.1

Bug fixes:
- [micromamba] fix issue where pip installation was broken on Windows @Klaim #1828

2022.07.26
==========

Releases: libmamba 0.25.0, libmambapy 0.25.0, mamba 0.25.0, micromamba 0.25.0

Bug fixes:
- [micromamba] fix pip execution in environments with spaces (thanks @chaubold) #1815
- [mamba] keep Pool alive for the lifetime of the solver (thanks @AntoinePrv) #1804
- [micromamba] Fix `shell init --dry-run` (thanks @jonashaag) #1767
- [mamba] print banner to stderr and do not print banner with `mamba run` (thanks @jonashaag) #1810
- [micromamba] Change exit(1) to throw exceptions instead (thanks @jonashaag) #1792
- [libmamba] Make lockfiles less noisy (thanks @Klaim) #1750
- [libmamba] Make clobber warnings less noisy #1764
- [libmamba] Do not ever log password in plain text (thanks @AntoinePrv) #1776

Enhancements:
- [libmambapy] Add missing SOLVER_RULE_PKG_CONSTRAINS ruleinfo in libmambapy bindings (thanks @syslaila) #1823
- [libmamba, libmambapy] Add safe id2pkginfo (thanks @AntoinePrv) #1822
- [libmambapy] Change PackageInfo value mutability and add named arguments (thanks @AntoinePrv) #1822
- [libmamba, micromamba] add handling of different tokens for channels on same host (thanks @AntoinePrv) #1784
- [all] better test isolation (thanks @AntoinePrv) #1791
- [micromamba] Add deinit shell command (thanks @pavelzw) #1781
- [all] Add nodefaults handling to libmamba (thanks @AdrianFreundQC) #1773
- [micromamba] Fix micromamba Windows download instructions (thanks @jonashaag) #1793
- [libmamba, libmambapy] Add utilities for better error reporting and refactor Queue #1789
- [micromamba] Better error message if root prefix is not a directory #1792
- [libmamba] Do not modify string during sregex iteration (thanks @jonashaag) #1768
- [libmamba] Better error message for invalid `.condarc` file (thanks @jonashaag) #1765
- [libmamba] Tweak is_writable() (thanks @Klaim) #1750
- [libmamba] Allow for external fmt library (thanks @gdolle) #1732
- [libmamba] Remove error message when `touch` fails #1747
- [libmamba] Log the exception that caused configuration parsing failure (thanks @johnhany97) #1755
- [mamba, micromamba] Make `--use-index-cache` option work (thanks @AdrianFreundQC) #1762
- [micromaba] Do not truncate channel url in `micromamba env export` (thanks @nstinus) #1733
- [libmamba] Fix MSVC warnings (thanks @Klaim) #1721
- [all] Test improvements (thanks @AntoinePrv) #1777, #1778


2022.05.31
==========

Releases: libmamba 0.24.0, libmambapy 0.24.0, mamba 0.24.0, micromamba 0.24.0

Bug fixes:
- [micromamba] constructor now uses proper (patched) repodata to create repodata_record.json files #1698
- [libmamba, micromamba] use fmt::format for pretty printing in `micromamba search --pretty` #1710
- [mamba] remove flag from clean subcommand that conflicts with conda (`mamba clean -l`, use `--lock` instead) #1709
- [libmamba] commit fix for compiling with ppc64le on conda-forge #1695


2022.05.20
==========

Releases: libmamba 0.23.3, libmambapy 0.23.3, mamba 0.23.3, micromamba 0.23.3

Bug fixes
- [micromamba] Fix summing behavior of `-v` flags #1690
- [all] fix curl callback to not exit anymore but report a proper error #1684
- [micromamba] fix up explicit installation by using proper variables #1677
- [libmamba] fix channel prefix test (thanks @jonashaag) #1674
- [mamba] fix strict priority for `mamba create env ...` #1688

Improvements
- [micromamba] make clean_force_pkgs respect `-y` flag (thanks @Patricol) #1686
- [libmamba] various Windows / CMake improvements #1683
- [libmamba] various warnings fixed on Windows and Unix #1683, 1691
- [libmamba] fix yaml-cpp linkage #1678


2022.05.12
==========

Releases: micromamba 0.23.2

Bug fixes
- [micromamba] Fix a bug with platform replacement in URLs #1670

2022.05.11
==========

Releases: libmamba 0.23.1, libmambapy 0.23.1, mamba 0.23.1, micromamba 0.23.1

Bug fixes
- [micromamba] Fix powershell unset of env vars (thanks @chawyeshu) #1668
- [all] Fix thread clean up and singleton destruction order (thanks @Klaim) #1666, #1620
- [all] Show reason for multi-download failure (thanks @syslaila) #1652
- [libmamba] Fix platform splitting to work with unknown platforms #1662
- [libmamba] Create prefix before writing the config file #1662
- [libmamba] Retry HTTP request on 413 & 429, respect Retry-After header (thanks @kenodegard) #1661
- [mamba] Add high-level Python API (thanks @martinRenou) #1656
- [libmamba] Initialize curl (thanks @Klaim) #1648
- [libmamba] Replace thread detaching by thread joining before main's end  (thanks @Klaim) #1637


2022.04.21
==========

Releases: libmamba 0.23.0, libmambapy 0.23.0, mamba 0.23.0, micromamba 0.23.0

This release uses tl::expected for some improvements in the error handling.
We also cleaned the API a bit and did some refactorings to make the code compile faster and clean up headers.

Bug fixes
- [micromamba] Do not clean env when running post-link scripts (fixes Qt install on Windows) #1630
- [micromamba] Fix powershell activation in strict mode (thanks @mkessler) #1633

Enhancements
- [micromamba] Add `micromamba auth login / logout` commands
- [micromamba] Add support for new conda-lock yml file format (thanks @Klaim) #1577
- [libmamba, libmambapy] Make user agent configurable through Context
- [micromamba] Use cli11 2.2.0 #1626
- [libmamba] Correct header casing for macOS (thanks @l2dy) #1613
- [libmamba] Log the thrown error when validating cache (thanks @johnhany97) #1608
- [all] Use sscache to speed up builds (thanks @jonashaag) #1606
- [all] Upgrade black
- [micromamba, libmamba] Use bin2header to inline the various scripts (thanks @jonashaag) #1601
- [micromamba] Fix JSON output issues (thanks @Klaim) #1600
- [all] Refactor the include chain, headers cleanup (thanks @JohanMabille) #1588, #1592, #1590
- [mamba] Remove import of init_std_stream_encoding (thanks @jezdez) #1589
- [all] Refactor error handling (thanks @JohanMabille) #1579
- [libmamba] Do not store multi pkgs cache in subdir anymore #1572
- [libmambapy] Add structured problem extraction #1570, #1566
- [micromamba] Add tests for micromamba run (thanks @Klaim) #1564
- [libmamba, libmambapy] Add API to remove repo from pool
- [libmamba] Store channel in subdirdata and libsolv repo appdata
- [libmamba] Remove prefixdata.load() #1555
- [libmamba] Remove prefixdata from solver interface #1550
- [micromamba] Also complete for micromamba deactivate #1577

2022.02.28
==========

Releases: libmamba 0.22.1, libmambapy 0.22.1, mamba 0.22.1

Bug fixes
- [mamba] Properly add `noarch` to package record to force recompilation #1545

2022.02.25
==========

Releases: libmamba 0.22.0, libmambapy 0.22.0, mamba 0.22.0, micromamba 0.22.0

Bug fixes
- [libmamba, mamba, micromamba] Add noarch recompilation step for mamba and micromamba #1511
- [micromamba] Add `--force-reinstall`, `--only-deps` and `--no-deps` to micromamba #1531
- [micromamba] Tolerate `PATH` being unset better (thanks @chrisburr) #1532

Improvements
- [micromamba] Add `--label` option to micromamba run and automatically generate process names otherwise (thanks @Klaim) #1491, #1530, #1529
- [libmamba] Remove compile time warnings by updating deprecated openssl functions #1509
- [micromamba] Add `search` as an alias for `repoquery search` (thanks @JohanMabille) #1510
- [micromamba] Fix `repoquery search` not working outside activated environment (thanks @JohanMabille) #1510
- [micromamba] Refactor configuration system (thanks @JohanMabille) #1500
- [libmamba] Use custom debug callback from libcurl and libsolv (routed through spdlog) #1507
- [libmamba] Refactor Channel implementation (thanks @JohanMabille) #1537
- [libmamba] Hide tokens in libcurl and libsolv as well (and remove need for `--experimental` flag to load tokens) #1538
- [libmamba] Pass through QEMU_LD_PREFIX to subprocesses (thanks @chrisburr) #1533
- [micromamba] Fix segfault on Linux with "fake" micromamba activate command #1496

2022.02.14
==========

Releases: libmamba 0.21.2, libmambapy 0.21.2, mamba 0.21.2, micromamba 0.21.2

Bug fixes
- [libmamba] Fix json read of `_mod` and `_etag` when they are not available #1490
- [micromamba] Properly attach stdin for `micromamba run` #1488

2022.02.11
==========

Releases: libmamba 0.21.1, libmambapy 0.21.1, mamba 0.21.1, micromamba 0.21.1

Bug fixes
- [libmamba] Adjust cache url hashing and header data parsing #1482
- [libmamba] Properly strip header of \r\n before adding to repodata.json cache #1482
- [micromamba] Revert removal of environment variables when running pip (thanks @austin1howard) #1477
- [mamba] Fix undefined transaction when creating env with no specs #1460

Improvements
- [micromamba] Add `micromamba config --json` (thanks @JohanMabille) #1484
- [mamba,micromamba,libmamba] Adjustments for the progress bars, make better visible on light backgrounds #1458
- [mamba] Refer to mamba activate for activation hint #1462
- [micromamba] Micromamba run add `--clean-env` and `-e,--env` handling to pass in environment variables #1464
- [mamba] Mention in help message that `mamba activate` and `deactivate` are supported (thanks @traversaro) #1476
- [micromamba] Disable banner with `micromamba run` #1474

2022.02.07
==========

Releases: libmamba 0.21.0, libmambapy 0.21.0, mamba 0.21.0, micromamba 0.21.0

Bug fixes
- [libmamba] generate PkgMgr role file from its file definition #1408
- [micromamba] fix crash with missing CONDARC file (thanks @jonashaag) #1417
- [micromamba] fix `micromamba --log-level` (thanks @jonashaag) #1417
- [micromamba] Fix erroneous error print when computing SHA256 of missing symlink #1412
- [micromamba] Add `-n` flag handling to `micromamba activate` #1411
- [micromamba] Refactor configuration loading and create file if it doesn't exist when setting values #1420
- [libmamba] Fix a regex segfault in history parsing #1441
- [libmamba] Add test for segfault history parsing #1444 (thanks @jonashaag)
- [micromamba] Improve shell scripts when ZSH_VERSION is unbound #1440
- [micromamba] Return error code when pip install fails from environment.yml #1442

Improvements
- [all] Update pre-commit versions (thanks @jonashaag) #1417
- [all] Use clang-format from pypi (thanks @chrisburr) #1430
- [all] Incremental ccache updates (thanks @jonashaag) #1445
- [micromamba] Substitute environment vars in .condarc files (thanks @jonashaag) #1423
- [micromamba, libmamba] Speed up noarch compilation (thanks @chrisburr) #1422
- [mamba] Include credentials for defaults channel URLs (thanks @wulmer) #1421
- [micromamba, libmamba] New fancy progress bars! (thanks @adriendelsalle) #1426, #1350
- [libmamba] Refactor how we set env vars in the Context #1426
- [micromamba] Add `micromamba run` command (thanks @JohanMabille) #1380, #1395, #1406, #1438, #1434
- [micromamba] Add `-f` for `micromamba clean` command (thanks @JohanMabille) #1449
- [micromamba] Add improved `micromamba update --all` #1318
- [micromamba] Add `micromamba repoquery` command #1318

2022.01.25
==========

Releases: libmamba 0.20.0, libmambapy 0.20.0, mamba 0.20.0, micromamba 0.20.0

Bug fixes
- [libmamba] Close file before retry & deletion when downloading subdir (thanks @xhochy) #1373
- [micromamba] Fix micromamba init & conda init clobber (thanks @maresb) #1357
- [micromamba] Rename mamba.sh to micromamba.sh for better compatibility between mamba & micromamba (thanks @maresb) #1355
- [micromamba] Print activate error to stderr (thanks @maresb) #1351

Improvements
- [micromamba, libmamba] Store platform when creating env with `--platform=...` (thanks @adriendelsalle) #1381
- [libmamba] Add environment variable to disable low speed limit (thanks @xhochy) #1380
- [libmamba] Make max download threads configurable (thanks @adriendelsalle) #1377
- [micromamba] Only print micromamba version and add library versions to `info` command #1372
- [micromamba] Implement activate as a micromamba subcommand for better error messages (thanks @maresb) #1360
- [micromamba] Experimental wass logged twice (thanks @baszalmstra) #1360
- [mamba] Update to Python 3.10 in the example snippet (thanks @jtpio) #1371

2021.12.08
==========

Releases: libmamba 0.19.1, libmambapy 0.19.1, mamba 0.19.1, micromamba 0.19.1

Bug fixes
- [mamba] Fix environment double print and add dry run to `mamba env create` (@wolfv) #1315
- [micromamba] Fix lockfiles in Unicode prefix (@wolfv) #1319
- [libmamba] Fix curl progress callback

Improvements
- [libmamba] Use WinReg from conda-forge
- [libmamba] Add fast path for hide_secrets (thanks @baszalmstra) #1337
- [libmamba] Use the original sha256 hash if a file doesnt change (thanks @baszalmstra) #1338
- [libmamba] Rename files that are in use (and cannot be removed) on Windows (@wolfv) #1319
- [micromamba] Add `micromamba clean --trash` command to remove `*.mamba_trash` files (@wolfv) #1319
- [libmamba] Avoid recomputing SHA256 for symbolic links (@wolfv) #1319
- [libmamba] Improve cleanup of directories in use (@wolfv) #1319
- [libmamba] Fix pyc compilation on Windows (@adriendelsalle) #1340

2021.11.30
==========

Releases: libmamba 0.19.0, libmambapy 0.19.0, mamba 0.19.0, micromamba 0.19.0

Bug fixes
- [all] Better Unicode support on Windows (@wolfv) #1306
- [libmamba, libmambapy] Solver has function to get more solver errors (@wolfv) #1310
- [mamba, micromamba] Do not set higher prio to arch vs noarch (@wolfv) #1312
- [libmamba] Close json repodata file after opening (@wolfv) #1309
- [micromamba] Add shell_completion, changeps1 and env_prompt as RC settings, remove auto-activate-base CLI flag (@wolfv) #1304
- [libmamba] Add bash & zsh shell_completion to activation functions
- [mamba] Use always_yes for `mamba env` subcommand (@wolfv) #1301
- [libmambapy] Remove libmamba from install_requires for libmambapy (@duncanmmacleod) #1303

2021.11.24
==========

Releases: libmamba 0.18.2, libmambapy 0.18.2, mamba 0.18.2, micromamba 0.18.2

Bug fixes
- [mamba, libmamba] Fix use of a read-only cache (@adriendelsalle) #1294
- [mamba, libmamba] Fix dangling LockFiles (@adriendelsalle) #1290
- [micromamba] Fix CMake config for micromamba fully statically linked on Windows (@adriendelsalle) #1297
- [micromamba, libmamba] Fix shell activation regression (@adriendelsalle) #1289

2021.11.19
==========

Releases: libmamba 0.18.1, libmambapy 0.18.1, mamba 0.18.1, micromamba 0.18.1

Bug fixes
- [all] Fix default log level, use warning everywhere (@adriendelsalle) #1279
- [mamba] Fix json output of `info` subcommand when verbose mode is active (@adriendelsalle) #1280
- [libmamba, libmambapy, mamba] Allow mamba to set max extraction threads using `MAMBA_EXTRACT_THREADS` env var (@adriendelsalle) #1281

2021.11.17
==========

Releases: libmamba 0.18.0, libmambapy 0.18.0, mamba 0.18.0, micromamba 0.18.0

New features
- [libmamba, mamba, micromamba] Implement parallel packages extraction using subprocesses (@jonashaag @adriendelsalle) #1195
- [micromamba] Improve bash completion (activate sub-command, directories completion) (@adriendelsalle) #1234
- [libmamba, micromamba] Add channel URLs to info (@jonashaag) #1235
- [libmamba] Read custom_multichannels from .condarc (@jonashaag) #1240
- [libmamba] Improve pyc compilation, make it configurable (@adriendelsalle) #1249
- [micromamba] Make pyc compilation configurable using `--pyc,--no-pyc` flags (@adriendelsalle) #1249
- [libmamba] Use `spdlog` for nicer and configurable logs (@adriendelsalle) #1255
- [micromamba] Add `--log-level` option to control log level independently of verbosity (@adriendelsalle) #1255
- [libmamba] Make show_banner rc and env_var configurable (@adriendelsalle) #1257
- [micromamba] Add zsh completion (@adriendelsalle) #1269
- [mamba] Make mamba env download and extract using `libmamba` (@adriendelsalle) #1270
- [libmamba] Add info JSON output (@adriendelsalle) #1271
- [micromamba] Add `--json` CLI flag to `info` sub-command (@adriendelsalle) #1271

Bug fixes
- [micromamba] Init all powershell profiles (@adriendelsalle) #1226
- [micromamba] Fix multiple activations in Windows bash (@adriendelsalle) #1228
- [libmamba] Fix failing package cache checks (@wolfv) #1237
- [mamba] Use libmamba LockFile, add `clean --lock` flag (@adriendelsalle) #1238
- [libmamba] Improve catching of reproc errors (such as OOM-killed) (@adriendelsalle) #1250
- [libmamba] Fix shell init with relative paths (@adriendelsalle) #1252
- [libmamba] Fix not thrown error in multiple caches logic (@adriendelsalle) #1253

Docs
- [micromamba] Document fish support (@izahn) #1216

General improvements
- [all] Split projects, improve CMake options (@adriendelsalle) #1219 #1243
- [libmamba] Test that a missing file doesn't cause an unlink error (@adriendelsalle) #1251
- [libmamba] Improve logging on YAML errors (@adriendelsalle) #1254
- [mamba] Conditionally import bindings for cross-compiling (@adriendelsalle) #1263

0.17.0 (October 13, 2021)
=========================

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

0.16.0 (September 27, 2021)
===========================

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

0.15.3 (August 18, 2021)
========================

- change token regex to work with edge-cases (underscores in user name) (#1122)
- only pin major.minor version of python for update --all (#1101, thanks @mparry!)
- add mamba init to the activate message (#1124, thanks @isuruf)
- hide tokens in logs (#1121)
- add lockfiles for repodata and pkgs download (#1105, thanks @jaimergp)
- log actual SHA256/MD5/file size when failing to avlidate (#1095, thanks @johnhany97)
- Add mamba.bat in front of PATH (#1112, thanks @isuruf)
- Fix mamba not writable cache errors (#1108)

0.15.2 (July 16, 2021)
======================

- micromamba autocomplete now ready for usage (#1091)
- improved file:// urls for windows to properly work (#1090)

0.15.1 (July 15, 2021)
======================

New features:
- add `mamba init` command and add mamba.sh (#1075, thanks @isuruf & #1078)
- add flexible channel priority option in micromamba CLI (#1087)
- improved autocompletion for micromamba (#1079)

Bug fixes:
- improve "file://" URL handling, fix local channel on Windows (#1085)
- fix CONDA_SUBDIR not being used in mamba (#1084)
- pass in channel_alias and custom_channels from conda to mamba (#1081)

0.15.0 (July 9, 2021)
=====================

Big changes:
- improve solutions by inspecting dependency versions as well (libsolv PR:
  https://github.com/openSUSE/libsolv/pull/457) @wolfv
- properly implement strict channel priority (libsolv PR:
  https://github.com/openSUSE/libsolv/pull/459) @adriendelsalle
	+ Note that this changes the meaning of strict and flexible priority as the
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
- remove orphaned packages such as dependencies of explicitely installed
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


0.14.1 (June 25, 2021)
======================

New features
- [micromamba] add remove command, to remove keys of vectors (@marimeireles)
  #1011

Bug fixes
- [micromamba] fixed in config prepend and append sequence (@adriendelsalle)
  #1023
- fix bug when username has @ (@madhur-tandon) #1025
- fix wrong update spec in history (@madhur-tandon) #1028
- [mamba] silent pinned packages using JSON output (@adriendelsalle) #1031

0.14.0 (June 16, 2021)
======================

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

0.13.1 (May 17, 2021)
=====================

Bug fixes
- [micromamba] pin only minor python version #948
- [micromamba] use openssl certs when not linking statically #949

0.13.0 (May 12, 2021)
=====================

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

0.12.3 (May 10, 2021)
=====================

New features
- [libmamba] add free-function to use an existing conda root prefix
  (@adriendelsalle) #927

General improvements
- [micromamba] fix a typo in documentation (@cjber) #926

0.12.2 (May 03, 2021)
=====================

New features
- [micromamba] add initial framework for TUF validation (@adriendelsalle) #916
  #919
- [micromamba] add channels from specs to download (@wolfv) #918

0.12.1 (Apr 30, 2021)
=====================

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

0.12.0 (Apr 26, 2021)
=====================

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

0.11.3 (Apr 21, 2021)
====================

- [libmamba] make platform rc configurable #883
- [libmamba] expand user home in target and root prefixes #882
- [libmamba] avoid memory effect between operations on target_prefix #881
- [libmamba] fix unnecessary throwing target_prefix check in `clean` operation
  #880
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

0.10.0 (Apr 16, 2021)
====================

- [micromamba] allow creation of empty env (without specs) #824 #827
- [micromamba] automatically create empy `base` env at new root prefix #836
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

0.9.2 (Apr 1, 2021)
====================

- [micromamba] fix unc url support (thanks @adament)
- [micromamba] add --channel-alias as cli option to micromamba (thanks
  @adriendelsalle)
- [micromamba] fix --no-rc and environment yaml files (thanks @adriendelsalle)
- [micromamba] handle spec files in update and install subcommands (thanks
  @adriendelsalle)
- add simple context debugging, dry run tests and other test framework
  improvements

0.9.1 (Mar 26, 2021)
====================

- [micromamba] fix remove command target_prefix selection
- [micromamba] improve target_prefix fallback for CLI, add tests (thanks
  @adriendelsalle)

0.9.0 (Mar 25, 2021)
====================

- [micromamba] use strict channels priority by default
- [micromamba] change config precedence order: API>CLI>ENV>RC
- [micromamba] `config list` sub command optional display of sources, defaults,
  short/long descriptions and groups
- [micromamba] prevent crashes when no bashrc or zshrc file found (thanks
  @wolfv)
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


0.8.0 (Mar 5, 2021)
===================

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
- [micromamba] Revert extraction to temporary directory because of invalid
  cross-device links on Linux
- [micromamba] Clean up partially extracted archives when CTRL+C interruption
  occured

0.7.11 (Feb 2, 2021)
====================

- [micromamba] use wrapped call when compiling noarch Python code, which
  properly calls chcp for Windows
- [micromamba] improve checking the pkgs cache
- [mamba] fix authenticated URLs (thanks @wenjuno)
- first extract to temporary directory, then move to final pkgs cache to
  prevent corrupted extracted data

0.7.10 (Jan 22, 2021)
====================

- [micromamba] properly fix PATH when linking, prevents missing
  vcruntime140.dll
- [mamba] add virtual packages when creating any environment, not just on
  update (thanks @cbalioglu)

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

- [micromamba] better error handling for YAML file reading, allows to pass in
  `-n` and `-p` from command line
- [mamba & micromamba] ignore case of HTTP headers
- [mamba] fix channel keys are without tokens (thanks @s22chan)

0.7.4 (Dec 5, 2020)
====================

- [micromamba] fix noarch installation for explicit environments

0.7.3 (Nov 20, 2020)
====================

- [micromamba] fix installation of noarch files with long prefixes
- [micromamba] fix activation on windows with whitespaces in root prefix
  (thanks @adriendelsalle)
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
- Fix pre-/post-link script running with micromamba to use micromamba
  activation logic
- Empty environment creation skips all repodata downloading & solving
- Fix micromamba install when environment is activated (thanks @isuruf)
- Micromamba now respects the $CONDA_SUBDIR environment variable (thanks
  @mariusvniekerk)
- Fix compile time warning (thanks @obilaniu)
- Fixed wrong CondaValueError import statement in mamba.py (thanks @saraedum)

0.6.5 (Oct 2020)
================

- Fix code signing for Apple Silicon (osx-arm64) @isuruf

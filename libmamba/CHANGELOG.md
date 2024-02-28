libmamba 2.0.0alpha3 (February 28, 2024)
========================================

Enhancements:

- Added HTTP Mirrors by @JohanMabille in https://github.com/mamba-org/mamba/pull/3178
- Use expected for specs parsing by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3201
- Refactor ObjPool to use views in callbacks by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3199
- Add more solver tests and other small features by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3198
- Finalized Solver bindings and add solver doc by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3195
- Add libsolv.Database Bindings and tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3186
- Add (some) solver Database tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3185
- Make libsolv wrappers into standalone library by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3181
- Rename MPool into solver::libsolv::Database by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3180
- Automate releases (`CHANGELOG.md` updating) by @Hind-M in https://github.com/mamba-org/mamba/pull/3179
- Simplify MPool Interface by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3177
- Clean libsolv use in Transaction by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3171
- Rewrite Query with Pool functions (wrapping libsolv) by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3168

Bug fixes:

- Simple logging fix by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3184

CI fixes and doc:

libmamba 2.0.0alpha2 (February 02, 2024)
========================================

Enhancements:

- Support multiple env yaml specs by @jchorl in https://github.com/mamba-org/mamba/pull/2993
- Update shell hook comments by @jonashaag in https://github.com/mamba-org/mamba/pull/3051
- More specs bindings by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3080
- Add VersionSpec::str by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3081
- Some future proofing MatchSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3082
- Reformat string by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3085
- Clean up url_manip by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3086
- Fix VersionSpec free ranges by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3088
- Add parsing utilities by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3090
- Bump MAMBA libsolv file ABI by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3093
- MatchSpec use VersionSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3089
- GlobSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3094
- Add BuildNumberSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3098
- Refactor MatchSpec unlikey data by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3099
- Remove micromamba shell init -p by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3092
- Clean PackageInfo interface by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3103
- NoArchType as standalone enum by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3108
- Move PackageInfo in specs:: by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3109
- Change PackageInfo types by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3113
- Add some PackageInfo tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3115
- Rename ChannelSpec > UndefinedChannel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3117
- Add Channel::contains_package by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3121
- Pool channel match by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3122
- Added mirrored channels by @JohanMabille in https://github.com/mamba-org/mamba/pull/3125
- Move util_random.hpp > util/random.hpp by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3129
- MRepo refactor by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3118
- No M by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3137
- Explcit transaction duplicate code by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3138
- Solver improvements by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3140
- Sort transaction table entries by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3146
- Solver Request by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3141
- Improve Solution usage by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3148
- Refactor solver flags by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3153
- Moved download related files to dedicated folder by @JohanMabille in https://github.com/mamba-org/mamba/pull/3155
- Remove outdated commented code snippet by @jjerphan in https://github.com/mamba-org/mamba/pull/3160
- Implemented support for mirrors by @JohanMabille in https://github.com/mamba-org/mamba/pull/3157
- Split Solver and Unsolvable by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3156
- Proper sorting of display actions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3165
- Solver sort deps by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3163
- Bind solver::libsolv::UnSolvable by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3166
- Improve Query API by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3167

Bug fixes:

- Fix URL enconding in repodata.json by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3076
- gracefully handle conflicting names in yaml specs by @jchorl in https://github.com/mamba-org/mamba/pull/3083
- Fix verbose and strange prefix in Powershell by @pwnfan in https://github.com/mamba-org/mamba/pull/3116
- handle other deps in multiple env files by @jchorl in https://github.com/mamba-org/mamba/pull/3096
- add manually given .tar.bz2 / .conda packages to solver pool by @0xbe7a in https://github.com/mamba-org/mamba/pull/3164

CI fixes and doc:

- Document specs::Channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3077
- Fix --override-channels docs by @jonashaag in https://github.com/mamba-org/mamba/pull/3084
- Add 2.0 changes draft by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3091
- Add Breathe for API documentation by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3087
- Warning around manual install and add ref to conda-libmamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3119
- Add MacOS DNS issue logging by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3130

libmamba 2.0.0alpha1 (December 18, 2023)
========================================


CI fixes and doc:

- Add CI merge groups by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3068

libmamba 2.0.0alpha0 (December 14, 2023)
========================================

Enhancements:

- Context: not a singleton by @Klaim in https://github.com/mamba-org/mamba/pull/2615
- Add CondaURL by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2805
- No ugly kenum by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2831
- Add Nushell activation support by cvanelteren in https://github.com/mamba-org/mamba/pull/2693
- Support $var syntax in .condarc by @jonashaag in https://github.com/mamba-org/mamba/pull/2833
- Handle null and false noarch values by @gabrielsimoes in https://github.com/mamba-org/mamba/pull/2835
- Add CondaURL::pretty_str by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2830
- Channel cleanup by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2832
- Authenfitication split user and password by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2849
- Improved static build error message by @JohanMabille in https://github.com/mamba-org/mamba/pull/2850
- Add local channels test by @Hind-M in https://github.com/mamba-org/mamba/pull/2853
- Don't force MSVC_RUNTIME by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2861
- Build micromamba with /MD by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2862
- Fix Posix shell on Windows by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2803
- Further improve micromamba search output by @delsner in https://github.com/mamba-org/mamba/pull/2823
- Minor Channel refactoring by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2852
- path_to_url percent encoding by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2867
- Change libsolv static lib name by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2876
- Download by @JohanMabille in https://github.com/mamba-org/mamba/pull/2844
- Use CMake targets for reproc by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2883
- Add FindLibsolv.cmake by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2886
- Read repodata.json using nl::json (rerun) by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2753
- Filesystem library by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2879
- Header cleanup filesystem follow-up by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2894
- Add multiple queries to repoquery search by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2897
- Add ChannelSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2870
- Print error code if run fails by @jonashaag in https://github.com/mamba-org/mamba/pull/2848
- Added PackageFetcher by @JohanMabille in https://github.com/mamba-org/mamba/pull/2917
- return architecture levels for micromamba by @isuruf in https://github.com/mamba-org/mamba/pull/2921
- Resolve ChannelSpec into a Channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2899
- Factorize Win user folder function between files by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2925
- Combine dev environments by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2937
- Refactor win encoding conversion by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2939
- Dev workflow by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2948
- Add refactor getenv setenv unsetenv by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2944
- Explicit and smart CMake target by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2935
- Rename env functions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2954
- Environment map by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2967
- Add envrionment cleaner test fixtures by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2973
- Update dependencies on OSX by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2976
- Channel initialization by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2953
- Add weakening_map by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2981
- Refactor env directories by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2983
- Enable new repodata parser by default by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2989
- Allow overriding archspec by @isuruf in https://github.com/mamba-org/mamba/pull/2966
- Add Python-like set operations to flat_set by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2557
- Migrate expand/shrink_home by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2990
- Refactor env::which by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2997
- Migrate Channel::make_channel to resolve multi channels by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2986
- Move core/channel > specs/channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3000
- Remove ChannelContext ctor by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3002
- Improve ChannelContext and Channel by @AntoinePrv in xhttps://github.com/mamba-org/mamba/pull/3003
- Remove ChanelContext context capture by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3015
- Bind Channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3001
- Default to hide credentials by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3017
- Validation QA by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3022
- Refactor (some) OpenSSL functions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3024
- Use std::array<std::byte, ...> by @AntoinePRv in https://github.com/mamba-org/mamba/pull/3037
- Default to conda-forge channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3035
- Remove duplicate function by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3042
- MatchSpec small improvements by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3043
- Plug ChannelSpec in MatchSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3046
- Drop unneeded dependencies by @opoplawski in https://github.com/mamba-org/mamba/pull/3016
- Change MatchSpec::parse to named constructor by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3048
- restore use_default_signal_handler flag for libmambapy by @dholth in https://github.com/mamba-org/mamba/pull/3028

Bug fixes:

- Fix linking on Windows when Scripts folder is missing by @dalcinl in https://github.com/mamba-org/mamba/pull/2825
- added support for empty lines in dependency file in txt format by @rmittal87 in https://github.com/mamba-org/mamba/pull/2812
- Fix local channels location by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2851
- Fixed libmamba tests static build by @JohanMabille in https://github.com/mamba-org/mamba/pull/2855
- Add CI test for local channels by @Hind-M in https://github.com/mamba-org/mamba/pull/2854
- Nushell hotfix by @cvanelteren https://github.com/mamba-org/mamba/pull/2841
- Added missing dependency in libmambaConfig.cmake.in by @JohanMabille in https://github.com/mamba-org/mamba/pull/2916
- Allow defaults::* spec by @isuruf in https://github.com/mamba-org/mamba/pull/2927
- https://github.com/mamba-org/mamba/pull/2929 by @bruchim-cisco in https://github.com/mamba-org/mamba/pull/2929
- Fix channels with slashes regression by @isuruf in https://github.com/mamba-org/mamba/pull/2926
- fix: Parse remote_connect_timeout_secs as a double by @jjerphan in https://github.com/mamba-org/mamba/pull/2949
- Add mirrors by @Hind-M in https://github.com/mamba-org/mamba/pull/2795
- Add cmake-format by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2962
- Fixed move semantics of DownloadAttempt by @JohanMabille in https://github.com/mamba-org/mamba/pull/2963
- Nu 0.87.0 by @cvanelteren in https://github.com/mamba-org/mamba/pull/2984
- fix config precedence for base env by @0xbe7a in https://github.com/mamba-org/mamba/pull/3009
- Fix libmamba cmake version file by @opoplawski in https://github.com/mamba-org/mamba/pull/3013

CI fixes and doc:

- Simplify and correct development documention by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2975
- Add install from source instructions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2977
- update readme install link by @artifical-agent in https://github.com/mamba-org/mamba/pull/2980
- Fail fast except on debug runs by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2985

libmamba 2.0.0alpha0 (December 14, 2023)
========================================

Enhancements:

- Context: not a singleton by @Klaim in https://github.com/mamba-org/mamba/pull/2615
- Add CondaURL by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2805
- No ugly kenum by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2831
- Add Nushell activation support by cvanelteren in https://github.com/mamba-org/mamba/pull/2693
- Support $var syntax in .condarc by @jonashaag in https://github.com/mamba-org/mamba/pull/2833
- Handle null and false noarch values by @gabrielsimoes in https://github.com/mamba-org/mamba/pull/2835
- Add CondaURL::pretty_str by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2830
- Channel cleanup by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2832
- Authenfitication split user and password by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2849
- Improved static build error message by @JohanMabille in https://github.com/mamba-org/mamba/pull/2850
- Add local channels test by @Hind-M in https://github.com/mamba-org/mamba/pull/2853
- Don't force MSVC_RUNTIME by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2861
- Build micromamba with /MD by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2862
- Fix Posix shell on Windows by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2803
- Further improve micromamba search output by @delsner in https://github.com/mamba-org/mamba/pull/2823
- Minor Channel refactoring by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2852
- path_to_url percent encoding by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2867
- Change libsolv static lib name by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2876
- Download by @JohanMabille in https://github.com/mamba-org/mamba/pull/2844
- Use CMake targets for reproc by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2883
- Add FindLibsolv.cmake by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2886
- Read repodata.json using nl::json (rerun) by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2753
- Filesystem library by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2879
- Header cleanup filesystem follow-up by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2894
- Add multiple queries to repoquery search by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2897
- Add ChannelSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2870
- Print error code if run fails by @jonashaag in https://github.com/mamba-org/mamba/pull/2848
- Added PackageFetcher by @JohanMabille in https://github.com/mamba-org/mamba/pull/2917
- return architecture levels for micromamba by @isuruf in https://github.com/mamba-org/mamba/pull/2921
- Resolve ChannelSpec into a Channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2899
- Factorize Win user folder function between files by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2925
- Combine dev environments by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2937
- Refactor win encoding conversion by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2939
- Dev workflow by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2948
- Add refactor getenv setenv unsetenv by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2944
- Explicit and smart CMake target by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2935
- Rename env functions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2954
- Environment map by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2967
- Add envrionment cleaner test fixtures by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2973
- Update dependencies on OSX by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2976
- Channel initialization by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2953
- Add weakening_map by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2981
- Refactor env directories by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2983
- Enable new repodata parser by default by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2989
- Allow overriding archspec by @isuruf in https://github.com/mamba-org/mamba/pull/2966
- Add Python-like set operations to flat_set by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2557
- Migrate expand/shrink_home by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2990
- Refactor env::which by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2997
- Migrate Channel::make_channel to resolve multi channels by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2986
- Move core/channel > specs/channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3000
- Remove ChannelContext ctor by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3002
- Improve ChannelContext and Channel by @AntoinePrv in xhttps://github.com/mamba-org/mamba/pull/3003
- Remove ChanelContext context capture by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3015
- Bind Channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3001
- Default to hide credentials by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3017
- Validation QA by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3022
- Refactor (some) OpenSSL functions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3024
- Use std::array<std::byte, ...> by @AntoinePRv in https://github.com/mamba-org/mamba/pull/3037
- Default to conda-forge channel by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3035
- Remove duplicate function by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3042
- MatchSpec small improvements by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3043
- Plug ChannelSpec in MatchSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3046
- Drop unneeded dependencies by @opoplawski in https://github.com/mamba-org/mamba/pull/3016
- Change MatchSpec::parse to named constructor by @AntoinePrv in https://github.com/mamba-org/mamba/pull/3048
- restore use_default_signal_handler flag for libmambapy by @dholth in https://github.com/mamba-org/mamba/pull/3028

Bug fixes:

- Fix linking on Windows when Scripts folder is missing by @dalcinl in https://github.com/mamba-org/mamba/pull/2825
- added support for empty lines in dependency file in txt format by @rmittal87 in https://github.com/mamba-org/mamba/pull/2812
- Fix local channels location by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2851
- Fixed libmamba tests static build by @JohanMabille in https://github.com/mamba-org/mamba/pull/2855
- Add CI test for local channels by @Hind-M in https://github.com/mamba-org/mamba/pull/2854
- Nushell hotfix by @cvanelteren https://github.com/mamba-org/mamba/pull/2841
- Added missing dependency in libmambaConfig.cmake.in by @JohanMabille in https://github.com/mamba-org/mamba/pull/2916
- Allow defaults::* spec by @isuruf in https://github.com/mamba-org/mamba/pull/2927
- https://github.com/mamba-org/mamba/pull/2929 by @bruchim-cisco in https://github.com/mamba-org/mamba/pull/2929
- Fix channels with slashes regression by @isuruf in https://github.com/mamba-org/mamba/pull/2926
- fix: Parse remote_connect_timeout_secs as a double by @jjerphan in https://github.com/mamba-org/mamba/pull/2949
- Add mirrors by @Hind-M in https://github.com/mamba-org/mamba/pull/2795
- Add cmake-format by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2962
- Fixed move semantics of DownloadAttempt by @JohanMabille in https://github.com/mamba-org/mamba/pull/2963
- Nu 0.87.0 by @cvanelteren in https://github.com/mamba-org/mamba/pull/2984
- fix config precedence for base env by @0xbe7a in https://github.com/mamba-org/mamba/pull/3009
- Fix libmamba cmake version file by @opoplawski in https://github.com/mamba-org/mamba/pull/3013

CI fixes and doc:

- Simplify and correct development documention by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2975
- Add install from source instructions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2977
- update readme install link by @artifical-agent in https://github.com/mamba-org/mamba/pull/2980
- Fail fast except on debug runs by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2985

libmamba 1.5.1 (September 05, 2023)
===================================

Enhancements:

- Add scope in util tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2775
- Enable Link Time Optimization by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2742
- Add libsolv namespace callback by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2796
- Clearer output from micromamba search by @delsner in https://github.com/mamba-org/mamba/pull/2782
- add context.register_envs to control whether environments are registered to environments.txt or not by @jaimergp in https://github.com/mamba-org/mamba/pull/2802
- Windows path manipulation and other cleanups by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2801
- Bring back repodata_use_zst by @jonashaag in https://github.com/mamba-org/mamba/pull/2790

Bug fixes:

- fix install pin by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2773
- Use generic_string for path on Windows unix shells by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2685
- Fix pins by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2786
- Various fixes by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2800
- Parse subdirs in CLI match specs by @jonashaag in https://github.com/mamba-org/mamba/pull/2799

CI fixes and doc:

- Splitted GHA workflow by @JohanMabille in https://github.com/mamba-org/mamba/pull/2779
- Use Release build mode in Windows CI by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2785

libmamba 1.5.0 (August 24, 2023)
================================

Enhancements:

- All headers at the top by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2658
- Add boolean expression tree by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2591
- Add VersionSpec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2502
- Use xdg schemas for config saving/reading (minified) by @danpf in https://github.com/mamba-org/mamba/pull/2714
- specs platform by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2729
- Safe Curl opt in url.cpp by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2734
- Add win-arm64 support by @isuruf in https://github.com/mamba-org/mamba/pull/2745
- Move util_string to utility library by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2739
- Remove get_clean_dirs() by @jonashaag in https://github.com/mamba-org/mamba/pull/2748
- Enable pytest color output by @jonashaag in https://github.com/mamba-org/mamba/pull/2759
- Isolate URL object by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2744
- Fix warnings by @Hind-M in https://github.com/mamba-org/mamba/pull/2760
- New apis for downloading by @JohanMabille in https://github.com/mamba-org/mamba/pull/2695

Bug fixes:

- Respect subdir in match spec by @ThomasBlauthQC in https://github.com/mamba-org/mamba/pull/2300
- Fixed move constructor in CURLHandle by @JohanMabille in https://github.com/mamba-org/mamba/pull/2710
- Remove created prefix if aborted with --platform by @Hind-M in https://github.com/mamba-org/mamba/pull/2738
- Add missing newline in legacy errors by @jaimergp in https://github.com/mamba-org/mamba/pull/2743
- fix: added missing hook_preamble() for powershell hook by @chawyehsu in https://github.com/mamba-org/mamba/pull/2761
- Fix fish completion by @soraxas in https://github.com/mamba-org/mamba/pull/2769
- Fix \_\_linux virtual package default version by jonashaag in https://github.com/mamba-org/mamba/pull/2749

CI fixes and doc:

- Ignore format changes in git blame by @jonashaag in https://github.com/mamba-org/mamba/pull/2690
- Add Debug build type by @Hind-M in https://github.com/mamba-org/mamba/pull/2762

libmamba 1.4.9 (July 13, 2023)
==============================

Bug fixes:

- Fixed missing key <channel_name> in channel <channel_list> issue by @JohanMabille in https://github.com/mamba-org/mamba/pull/2668

libmamba 1.4.8 (July 11, 2023)
==============================

Enhancements:

- No profile.d fallback in rc files by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2649
- Removed unused function by @Klaim in https://github.com/mamba-org/mamba/pull/2656
- Replace MTransaction::m_remove with Solution by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2603

Bug fixes:

- Fixed zst check in MSubdirData by @JohanMabille in https://github.com/mamba-org/mamba/pull/2661

libmamba 1.4.7 (July 06, 2023)
==============================

Enhancements:

- ZST support to mamba and remove the feature flag by @johnhany97 in https://github.com/mamba-org/mamba/pull/2642
- Add Version::starts_with and Version::compatible_with by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2645
- Create Solver solution by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2584

Bug fixes:

- call init_console to prevent UTF8 errors when extracting packages by @wolfv in https://github.com/mamba-org/mamba/pull/2655

libmamba 1.4.6 (June 30, 2023)
==============================

Enhancements:

- Channels refactoring/cleaning by @Hind-M in https://github.com/mamba-org/mamba/pull/2537
- Troubleshooting update by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2635
- Direcly call uname for linux detection by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2624

Bug fixes:

- Fix build with older Clang by @pavelzw in https://github.com/mamba-org/mamba/pull/2625
- Add missing noarch in PackageInfo serialization by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2641
- Allow --force-reinstall on uninstalled specs by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2636

libmamba 1.4.5 (June 27, 2023)
==============================

Enhancements:

- No singleton: ChannelContext, ChannelBuilder and channel cache by @Klaim in https://github.com/mamba-org/mamba/pull/2455
- Move problem graph creation to MSolver by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2515
- Add ObjSolver by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2504
- Use ObjSolver in MSolver by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2544
- Common CMake presets by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2532
- Wrap libsolv Transaction by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2554
- Split the transaction.hpp header by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2564
- Add more tests for channel canonical_name by @Hind-M in https://github.com/mamba-org/mamba/pull/2568
- use ObjTransaction in MTransaction by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2565
- https://github.com/mamba-org/mamba/pull/2590 by @jonashaag in https://github.com/mamba-org/mamba/pull/2590
- Libcurl: Cleaning and comments by @Hind-M in https://github.com/mamba-org/mamba/pull/2534
- No singleton: configuration by @Klaim in https://github.com/mamba-org/mamba/pull/2541
- Added filtering iterators by @JohanMabille in https://github.com/mamba-org/mamba/pull/2594
- Use ObjSolver wrapper in MSolver by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2602
- Remove banner by @jonashaag in https://github.com/mamba-org/mamba/pull/2298
- LockFile behavior on file-locking is now almost independent from Context by @Klaim in https://github.com/mamba-org/mamba/pull/2608
- Small whitespace fix in error messages by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2623

Bug fixes:

- Use subsub commands for micromamba shell by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2527
- Honor envs_dirs by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2538
- Fixed Windows test build by @JohanMabille in https://github.com/mamba-org/mamba/pull/2585
- Add missing cstdint include to libmamba/src/solv-cpp/solvable.cpp by @maxyvisser in https://github.com/mamba-org/mamba/pull/2587
- Fix wrong download url for custom channels by @Hind-M in https://github.com/mamba-org/mamba/pull/2596
- Fix --force-reinstall by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2601
- Handle pip <-> python cycle in topo sort by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2613
- Fix add missing pip PREREQ_MARKER by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2612
- Fix lockfiles topological sort by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2616
- Fix mising SAT message on already installed packages by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2622

CI fixes and doc:

- Fixe clang-format by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2531
- update the umamba GHA link by @ocefpaf in https://github.com/mamba-org/mamba/pull/2542
- Extend troubleshooting docs by @jonashaag in https://github.com/mamba-org/mamba/pull/2569
- Update pre-commit hooks by @jonashaag in https://github.com/mamba-org/mamba/pull/2586
- Move GHA to setup-micromamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2545
- Switch linters to setup-micromamba by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2600
- Switch to setup-micromamba by @pavelzw in https://github.com/mamba-org/mamba/pull/2610
- Fix broken ref directives in docs by @mfisher87 in https://github.com/mamba-org/mamba/pull/2620

libmamba 1.4.4 (May 16, 2023)
=============================

Bug fixes:

- Fix CURLHandle::get_info on 32bit platform by e8035669 in https://github.com/mamba-org/mamba/pull/2528

libmamba 1.4.3 (May 15, 2023)
=============================

Enhancements:

- No Storing Channel\* and MRepo\* in Solvables by @AntoinPrv in https://github.com/mamba-org/mamba/pull/2409
- Remove dead code / attribute by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2454
- Context structuring by @Hind-M in https://github.com/mamba-org/mamba/pull/2432
- Clean up fetch by @Hind-M in https://github.com/mamba-org/mamba/pull/2452
- Wapped curl multi handle by @JohanMabille in https://github.com/mamba-org/mamba/pull/2459
- Remove empty test\_flat\_set.hpp by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2471
- Add doctest printer for pair and vector by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2470
- Add topological sort by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2467
- Store PackageInfo::track\_features as a vector by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2478
- Use topological sort instead of libsolv by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2472
- Remove assign\_or in favor of json::value by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2487
- Resume Context structuring by @Hind-M in https://github.com/mamba-org/mamba/pull/2460
- Improve micromamba transaction message by @ruben-arts in https://github.com/mamba-org/mamba/pull/2474
- Remove unused raw function in subdirdata by @Hind-M in https://github.com/mamba-org/mamba/pull/2491
- Wrap ::Pool and ::Repo by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2401
- Curl wrapping by @Hind-M in https://github.com/mamba-org/mamba/pull/2468
- Reset fish shell status even if variable not exists by @soraxas in https://github.com/mamba-org/mamba/pull/2509
- Use libsolv wrappers in MPool and MRepo by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2453
- add bearer token authentication by @wolfv in https://github.com/mamba-org/mamba/pull/2512

Bug fixes:

- fix: parsing of empty track\_features by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2485
- track\_feature typo by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2488
- Move repoquery python test from libmamba (not run) to mamba by @Hind-M in https://github.com/mamba-org/mamba/pull/2489
- Set log\_level to critical with --json option by @Hind-M in https://github.com/mamba-org/mamba/pull/2484
- Add missing cstdint include for GCC 13 by @alexfikl in https://github.com/mamba-org/mamba/pull/2511
- Forward NETRC environment variable to curl, if exported by @timostrunk in https://github.com/mamba-org/mamba/pull/2497
- Remove wrong $Args in psm1 by @troubadour-hell in https://github.com/mamba-org/mamba/pull/2499
- Avoid using /tmp by @johnhany97 in https://github.com/mamba-org/mamba/pull/2447
- Fixed winreg search by @JohanMabille in https://github.com/mamba-org/mamba/pull/2526

CI fixes and doc:

- Extend troubleshooting docs by @jonashaag in https://github.com/mamba-org/mamba/pull/2451
- Extend issue template by @jonashaag in https://github.com/mamba-org/mamba/pull/2310

libmamba 1.4.2 (April 06, 2023)
===============================

Enhancements:

- Small libsolv improvements by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2399
- Improve message after the env creating with micromamba by @xmnlab in https://github.com/mamba-org/mamba/pull/2425
- Use custom function to properly parse matchspec by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2433
- Remove const ref to string\_view in codebase by @Hind-M in https://github.com/mamba-org/mamba/pull/2440
- Wrap more libcurl calls by @Hind-M in https://github.com/mamba-org/mamba/pull/2421

Bug fixes:

- Fix PKG\_BUILDNUM env variable for post-link scripts by nsoranzo in https://github.com/mamba-org/mamba/pull/2420
- Solve a corner case in the SAT error messages by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2423
- Windows: Fixed environment variables not read as unicode by @Klaim in https://github.com/mamba-org/mamba/pull/2417
- Fix segfault in add\_pin/all\_problems\_structured by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2428

CI fixes and doc:

- Replaced libtool 2.4.6\_9 with libtool 2.4.7-3 in vcpkg builds by @JohanMabille in https://github.com/mamba-org/mamba/pull/2439
- Migrated to doctest by @JohanMabille in https://github.com/mamba-org/mamba/pull/2436

libmamba 1.4.1 (March 28, 2023)
===============================

Enhancements:

- First version/steps of unraveling fetch code and wrapping libcurl by @Hind-M in https://github.com/mamba-org/mamba/pull/2376
- Parse repodata.json by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2391
- TimeRef is not a singleton anymore by @Klaim in https://github.com/mamba-org/mamba/pull/2396
- Handle url via ChannelBuilder in Repo constructor by @jaimergp in https://github.com/mamba-org/mamba/pull/2398
- add option to relocate prefix by @DerThorsten in https://github.com/mamba-org/mamba/pull/2385
- Renamed validate namespace to `mamba::validation by @Klaim in https://github.com/mamba-org/mamba/pull/2411

Bug fixes:

- Fixed build with older Clang by @ZhongRuoyu in https://github.com/mamba-org/mamba/pull/2397

libmamba 1.4.0 (March 22, 2023)
===============================

Enchancements:

- Implemented recursive dependency printout in repoquery  by @timostrunk in https://github.com/mamba-org/mamba/pull/2283
- Agressive compilation warnings by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2304
- Fine tune clang-format by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2290
- Added checked numeric cast by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2315
- Activated SAT error messages by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2325
- Added RISC-V support by @dtcxzyw in https://github.com/mamba-org/mamba/pull/2329
- Removed redundant `DependencyInfo` by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2314
- Isolate solv::ObjQueue by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2289
- Removed unused libarchive header in fetch by @hind-M in https://github.com/mamba-org/mamba/pull/2341
- Removed duplicated header by @JohanMabille in https://github.com/mamba-org/mamba/pull/2343
- Cleaned `util_string` by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2339
- Only full shared or full static builds by @JohanMabille in https://github.com/mamba-org/mamba/pull/2342
- Fixed repoquery commands working with installed packages only by @Hind-M in https://github.com/mamba-org/mamba/pull/2330
- Added a heuristic to better handle the (almost) cyclic Python conflicts by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2318
- Isolate `PackageInfo` from libsolv from @AntoinePrv in https://github.com/mamba-org/mamba/pull/2340
- Added `strip_if` functions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2344
- Addded conda.rc Options for Existing Remote Settings by @srilman in https://github.com/mamba-org/mamba/pull/2306
- Hide independent curl code and compression structures in unexposed files by @Hind-M in https://github.com/mamba-org/mamba/pull/2366
- Added `strip_parts` functions by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2347
- Added parsing of Conda version by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2373
- Slight refactoring of the utility library by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2387

Bug fixes:

- Fixed invalid reinstall count display by @timostrunk in https://github.com/mamba-org/mamba/pull/2284
- Fixed segmentation fault in case of an invalid package name by @timostrunk in https://github.com/mamba-org/mamba/pull/2287
- Fixed `to_lower(wchar_t)` and `to_upper(wchar_t)` by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2360
- Fixed undefined-behaviors reported by UBSAN by @klaim in https://github.com/mamba-org/mamba/pull/2384

CI fixes & docs:

- Fixed sign warning in tests by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2293
- Structured test directory layout by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2380

libmamba 1.3.1 (February 09, 2023)
==================================

A bugfix release for 1.3.0!

Bug fixes:

- fix up single download target perform finalization to make lockfile download work by @wolfv in https://github.com/mamba-org/mamba/pull/2274
- fix rename or remove by @wolfv in https://github.com/mamba-org/mamba/pull/2276
- add channel specific job with new str by @AntoinePrv in https://github.com/mamba-org/mamba/pull/2277
- fix `micromamba shell` for base environment, and improve behavior when `auto_activate_base` is true by @jonashaag, @Hind-M and @wolfv https://github.com/mamba-org/mamba/pull/2272

Docs:

- - add micromamba docker image by @wholtz in https://github.com/mamba-org/mamba/pull/2266
- - added biweekly meetings information to README by @JohanMabille in https://github.com/mamba-org/mamba/pull/2275
- - change docs to homebrew/core by @pavelzw in https://github.com/mamba-org/mamba/pull/2278

libmamba 1.3.0 (February 03, 2023)
==================================

Enhancements:

- switch to repodata.state.json format from cep by @wolfv in https://github.com/mamba-org/mamba/pull/2262

Bug fixes:

- Fix temporary file renaming by @jonashaag in https://github.com/mamba-org/mamba/pull/2242

CI fixes & docs:

- docs: defaults should not be used with conda-forge by @jonashaag in https://github.com/mamba-org/mamba/pull/2181
- fix tests for pkg_cache by @wolfv in https://github.com/mamba-org/mamba/pull/2259
- Added missing public dependency to libmambaConfig.cmake by @JohanMabille in https://github.com/mamba-org/mamba/pull/2227

libmamba 1.2.0 (January 16, 2023)
=================================

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
- Error messages improvements by @AntoinePrv in #2149
- Report failure when packages to remove don't exist. (#2131) by @Klaim in #2132
- Fixing typo in solver errors by @shughes-uk in #2168
- Extend `last_write_time` implementation by special-casing file touching by @coroa in #2141
- Don't create a prefix which is missing conda-meta by @maresb in #2162
- Fix `custom_channels` parsing by @XuehaiPan in #2207

CI fixes & docs:

- - Improve build env cleanup by @jonashaag in #2213
- - Run conda_nightly once per week by @jonashaag in #2147
- - Update doc by @Hind-M in #2156
- - Use Conda canary in nightly tests by @jonashaag in #2180
- - Expliclity point to libmamba test data independently of cwd by @AntoinePrv in #2158
- - Add bug report issue template by @jonashaag in #2182
- - Downgrade curl to fix micromamba on macOS x64 by @wolfv in #2205
- - Use conda-forge micromamba feedstock instead of a fork by @JohanMabille in #2206
- - Update pre-commit versions by @jonashaag in #2178
- - Use local meta.yaml by @wolfv in #2214
- - Remove feedstock patches by @wolfv in #2216
- - Fixed static dependency order by @JohanMabille in #2201

libmamba 1.1.0 (November 25, 2022)
==================================

Some bugfixes for 1.0 and experimental release of the new solver messages

Bug fixes

- Fix libmamba CMake config file after dependency change (thanks @l2dy) #2091

Enhancements

- Add safe signed/unsigned conversion (thanks @AntoinePrv) #2087
- Move to fmt::terminal_color and other output IO improvements & drop termcolor (thanks @AntoinePrv) #2085
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

libmamba 1.0.0 (November 01, 2022)
==================================

Our biggest version number yet! Finally a 1.0 release :)

New notable micromamba features include:

- - improved shell scripts with autocompletion available in PowerShell, xonsh, fish, bash and zsh
- - `micromamba shell -n someenv`: enter a sub-shell without modifying the system
- - `micromamba self-update`: micromamba searches for updates and installs them if available

(you can also downgrade using `--version 0.26.0` for example)

Bug fixes:

- ignore "Permission denied" in `env::which` (thanks @Rafflesiaceae) #2067
- Fix an infinite loop in replace_all() when the search string is empty (thanks @tsibley)
- Do not crash when permissions cannot be changed, instead log warning (thanks @hwalinga)

Enhancements:

- Rewrite LockFile interface (thanks @Klaim) #2014
- Fix ci deprecation warnings, upload conda-bld artifacts for failed builds #2058, #2062
- Explicitly define SPDLOG_FMT_EXTERNAL and use spdlog header only use external fmt (thanks @AntoinePrv) #2060, #2048
- Fix CI by pointing to updated feedstock and fixing update tests (thanks @AntoinePrv) #2055
- Add authentication with urlencoded @ to proxy test (#2024) @AdrianFreundQC
- better test isolation (thanks @AntoinePrv) #1903
- Test special characters in basic auth (thanks @jonashaag) #2012
- ProblemsGraph compression (thanks @AntoinePrv) #2019
- vector_set compare function (thanks @AntoinePrv) #2051
- Clean up util_graph header and tests (thanks @AntoinePrv) #2039
- Add string utilities (thanks @AntoinePrv) #
- Dynamic tree walk of the Compressed problem graph
- Creating the initial problems graph (thanks @syslaila) #1891

libmamba 0.27.0 (October 04, 2022)
==================================

Bug fixes:

- fix lockfiles relying on PID (thanks @Klaim) #1915

libmamba 0.26.0 (September 30, 2022)
====================================

Bug fixes:

- add symlinks and empty directories to archive for `micromamba package compress` #1955
- increase curl buffer size for faster max download speeds (thanks @jonashaag) #1949
- fix crash when installing from environment lockfile (thanks @Klaim) #1893
- fix linux version regex (thanks @kelszo) #1852
- remove duplicate console output (thanks @pavelzw) #1845

Enhancements:

- add option to disable file locks globally (thanks @danpf @JohanMabille) #1830
- extend graph class for better solver messaging work (thanks @syslaila @AntoinePrv) #1880
- use std::filesystem instead of ghc::filesystem (thanks @Klaim) #1665
- add missing SolverRuleInfo enum entries (thanks @AntoinePrv) #1833

libmamba 0.25.0 (July 26, 2022)
===============================

Bug fixes:

- Make lockfiles less noisy (thanks @Klaim) #1750
- Make clobber warnings less noisy #1764
- Do not ever log password in plain text (thanks @AntoinePrv) #1776

Enhancements:

- Add safe id2pkginfo (thanks @AntoinePrv) #1822
- add handling of different tokens for channels on same host (thanks @AntoinePrv) #1784
- better test isolation (thanks @AntoinePrv) #1791
- Add nodefaults handling to libmamba (thanks @AdrianFreundQC) #1773
- Add utilities for better error reporting and refactor Queue #1789
- Do not modify string during sregex iteration (thanks @jonashaag) #1768
- Better error message for invalid `.condarc` file (thanks @jonashaag) #1765
- Tweak is_writable() (thanks @Klaim) #1750
- Allow for external fmt library (thanks @gdolle) #1732
- Remove error message when `touch` fails #1747
- Log the exception that caused configuration parsing failure (thanks @johnhany97) #1755
- Fix MSVC warnings (thanks @Klaim) #1721
- Test improvements (thanks @AntoinePrv) #1777, #1778

libmamba 0.24.0 (June 01, 2022)
===============================

Bug fixes:

- use fmt::format for pretty printing in `micromamba search --pretty` #1710
- commit fix for compiling with ppc64le on conda-forge #1695

libmamba 0.23.3 (May 20, 2022)
==============================

Bug fixes

- fix curl callback to not exit anymore but report a proper error #1684
- fix channel prefix test (thanks @jonashaag) #1674

Improvements

- various Windows / CMake improvements #1683
- various warnings fixed on Windows and Unix #1683, 1691
- fix yaml-cpp linkage #1678

libmamba 0.23.1 (May 11, 2022)
==============================

Bug fixes

- Fix thread clean up and singleton destruction order (thanks @Klaim) #1666, #1620
- Show reason for multi-download failure (thanks @syslaila) #1652
- Fix platform splitting to work with unknown platforms #1662
- Create prefix before writing the config file #1662
- Retry HTTP request on 413 & 429, respect Retry-After header (thanks @kenodegard) #1661
- Initialize curl (thanks @Klaim) #1648
- Replace thread detaching by thread joining before main's end  (thanks @Klaim) #1637

libmamba 0.23.0 (April 21, 2022)
================================

This release uses tl::expected for some improvements in the error handling.
We also cleaned the API a bit and did some refactorings to make the code compile faster and clean up headers.

Enhancements

- Make user agent configurable through Context
- Correct header casing for macOS (thanks @l2dy) #1613
- Log the thrown error when validating cache (thanks @johnhany97) #1608
- Use sscache to speed up builds (thanks @jonashaag) #1606
- Upgrade black
- Use bin2header to inline the various scripts (thanks @jonashaag) #1601
- Refactor the include chain, headers cleanup (thanks @JohanMabille) #1588, #1592, #1590
- Refactor error handling (thanks @JohanMabille) #1579
- Do not store multi pkgs cache in subdir anymore #1572
- Add API to remove repo from pool
- Store channel in subdirdata and libsolv repo appdata
- Remove prefixdata.load() #1555
- Remove prefixdata from solver interface #1550

libmamba 0.22.1 (February 28, 2022)
===================================


libmamba 0.22.0 (February 25, 2022)
===================================

Bug fixes

- Add noarch recompilation step for mamba and micromamba #1511

Improvements

- Remove compile time warnings by updating deprecated openssl functions #1509
- Use custom debug callback from libcurl and libsolv (routed through spdlog) #1507
- Refactor Channel implementation (thanks @JohanMabille) #1537
- Hide tokens in libcurl and libsolv as well (and remove need for `--experimental` flag to load tokens) #1538
- Pass through QEMU_LD_PREFIX to subprocesses (thanks @chrisburr) #1533

libmamba 0.21.2 (February 14, 2022)
===================================

Bug fixes

- Fix json read of `_mod` and `_etag` when they are not available #1490

libmamba 0.21.1 (February 11, 2022)
===================================

Bug fixes

- Adjust cache url hashing and header data parsing #1482
- Properly strip header of \r\n before adding to repodata.json cache #1482

Improvements

- Adjustments for the progress bars, make better visible on light backgrounds #1458

libmamba 0.21.0 (February 07, 2022)
===================================

Bug fixes

- generate PkgMgr role file from its file definition #1408
- Fix a regex segfault in history parsing #1441
- Add test for segfault history parsing #1444 (thanks @jonashaag)

Improvements

- Update pre-commit versions (thanks @jonashaag) #1417
- Use clang-format from pypi (thanks @chrisburr) #1430
- Incremental ccache updates (thanks @jonashaag) #1445
- Speed up noarch compilation (thanks @chrisburr) #1422
- New fancy progress bars! (thanks @adriendelsalle) #1426, #1350
- Refactor how we set env vars in the Context #1426

libmamba 0.20.0 (January 25, 2022)
==================================

Bug fixes

- Close file before retry & deletion when downloading subdir (thanks @xhochy) #1373

Improvements

- Store platform when creating env with `--platform=...` (thanks @adriendelsalle) #1381
- Add environment variable to disable low speed limit (thanks @xhochy) #1380
- Make max download threads configurable (thanks @adriendelsalle) #1377

libmamba 0.19.1 (December 08, 2021)
===================================

Bug fixes

- Fix curl progress callback

Improvements

- Use WinReg from conda-forge
- Add fast path for hide_secrets (thanks @baszalmstra) #1337
- Use the original sha256 hash if a file doesnt change (thanks @baszalmstra) #1338
- Rename files that are in use (and cannot be removed) on Windows (@wolfv) #1319
- Avoid recomputing SHA256 for symbolic links (@wolfv) #1319
- Improve cleanup of directories in use (@wolfv) #1319
- Fix pyc compilation on Windows (@adriendelsalle) #1340

libmamba 0.19.0 (November 30, 2021)
===================================

Bug fixes

- Better Unicode support on Windows (@wolfv) #1306
- Solver has function to get more solver errors (@wolfv) #1310
- Close json repodata file after opening (@wolfv) #1309
- Add bash & zsh shell_completion to activation functions

libmamba 0.18.2 (November 24, 2021)
===================================

Bug fixes

- Fix use of a read-only cache (@adriendelsalle) #1294
- Fix dangling LockFiles (@adriendelsalle) #1290
- Fix shell activation regression (@adriendelsalle) #1289

0.18.1 (November 19, 2021)
==========================

Bug fixes
- Fix default log level, use warning everywhere (@adriendelsalle) #1279
- Allow mamba to set max extraction threads using `MAMBA_EXTRACT_THREADS` env var (@adriendelsalle) #1281

0.18.0 (November 17, 2021)
==========================

New features
- Implement parallel packages extraction using subprocesses (@jonashaag @adriendelsalle) #1195
- Add channel URLs to info (@jonashaag) #1235
- Read custom_multichannels from .condarc (@jonashaag) #1240
- Improve pyc compilation, make it configurable (@adriendelsalle) #1249
- Use `spdlog` for nicer and configurable logs (@adriendelsalle) #1255
- Make show_banner rc and env_var configurable (@adriendelsalle) #1257
- Add info JSON output (@adriendelsalle) #1271

Bug fixes
- Fix failing package cache checks (@wolfv) #1237
- Improve catching of reproc errors (such as OOM-killed) (@adriendelsalle) #1250
- Fix shell init with relative paths (@adriendelsalle) #1252
- Fix not thrown error in multiple caches logic (@adriendelsalle) #1253

General improvements
- Split projects, improve CMake options (@adriendelsalle) #1219 #1243
- Test that a missing file doesn't cause an unlink error (@adriendelsalle) #1251
- Improve logging on YAML errors (@adriendelsalle) #1254

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

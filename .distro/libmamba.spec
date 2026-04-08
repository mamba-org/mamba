# TODO: Sync with downstream version when ready
# https://src.fedoraproject.org/rpms/libmamba/pull-request/2

# Cannot build statically yet
%bcond micromamba 0

Name:           libmamba
Version:        0.0.0
Release:        %autorelease
Summary:        C++ API for mamba depsolving library

License:        BSD-3-Clause
URL:            https://github.com/mamba-org/mamba
Source0:        https://github.com/mamba-org/mamba/archive/%{version}/%{name}-%{version}.tar.gz
# Use Fedora versions of zstd
# Install into /etc/profile.d
Patch0:         libmamba-fedora.patch

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  bzip2-devel
BuildRequires:  fmt-devel
BuildRequires:  gtest-devel
BuildRequires:  json-devel
BuildRequires:  libarchive-devel
BuildRequires:  libcurl-devel
# Need CONDA_ADD_USE_ONLY_TAR_BZ2
BuildRequires:  libsolv-devel
BuildRequires:  openssl-devel
BuildRequires:  reproc-devel
BuildRequires:  cmake(simdjson)
BuildRequires:  spdlog-devel
BuildRequires:  cmake(tl-expected)
BuildRequires:  yaml-cpp-devel
# This is not yet provided by Fedora package
# https://src.fedoraproject.org/rpms/zstd/pull-request/7
#BuildRequires:  cmake(zstd)
BuildRequires:  libzstd-devel

%description
libmamba is a reimplementation of the conda package manager in C++.

* parallel downloading of repository data and package files using multi-
  threading
* libsolv for much faster dependency solving, a state of the art library used
  in the RPM package manager of Red Hat, Fedora and OpenSUSE
* core parts of mamba are implemented in C++ for maximum efficiency


%package        devel
Summary:        Development files for %{name}
License:        MIT
Requires:       %{name}%{?_isa} = %{version}-%{release}
Requires:       cmake-filesystem
Requires:       pkgconfig
Requires:       fmt-devel%{?_isa}
Requires:       json-devel%{?_isa}
Requires:       libsolv-devel%{?_isa}
Requires:       reproc-devel%{?_isa}
Requires:       spdlog-devel%{?_isa}
Requires:       cmake(tl-expected)
Requires:       yaml-cpp-devel%{?_isa}

%description    devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.


%package -n     mamba
Summary:        The Fast Cross-Platform Package Manager
BuildRequires:  cli11-devel
BuildRequires:  pybind11-devel
# Generate a simple man page until https://github.com/mamba-org/mamba/issues/3032 is addressed
BuildRequires:  help2man
Requires:       %{name}%{?_isa} = %{version}-%{release}
%if %{without micromamba}
Obsoletes:      micromamba < %{version}-%{release}
Provides:       micromamba = %{version}-%{release}
%endif

%description -n mamba
mamba is a reimplementation of the conda package manager in C++.

 * parallel downloading of repository data and package files using multi-
   threading
 * libsolv for much faster dependency solving, a state of the art library
   used in the RPM package manager of Red Hat, Fedora and OpenSUSE
 * core parts of mamba are implemented in C++ for maximum efficiency

At the same time, mamba utilizes the same command line parser, package
installation and deinstallation code and transaction verification routines as
conda to stay as compatible as possible.

mamba is part of the conda-forge ecosystem, which also consists of quetz, an
open source conda package server.


%if %{with micromamba}
%package -n     micromamba
Summary:        Tiny version of the mamba package manager
BuildRequires:  cli11-devel
BuildRequires:  pybind11-devel
BuildRequires:  yaml-cpp-static
# Generate a simple man page until https://github.com/mamba-org/mamba/issues/3032 is addressed
BuildRequires:  help2man

%description -n micromamba
micromamba is the statically linked version of mamba.

It can be installed as a standalone executable without any dependencies,
making it a perfect fit for CI/CD pipelines and containerized environments.
%endif


%package -n     python3-libmambapy
Summary:        Python bindings for libmamba
BuildRequires:  python3-devel
BuildRequires:  python3-pip
BuildRequires:  python3-setuptools
BuildRequires:  python3-wheel
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description -n python3-libmambapy
Python bindings for libmamba.


%prep
%autosetup -p1 -n mamba-%{version}
sed -i -e '/cmake/d' -e '/ninja/d' libmambapy/pyproject.toml


%generate_buildrequires
cd libmambapy
%pyproject_buildrequires


%build
%if %{with micromamba}
%global _vpath_builddir %{_vendor}-%{_target_os}-build-micromamba
export CMAKE_MODULE_PATH=%{_libdir}/cmake/yaml-cpp-static
%cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_MODULE_PATH=%{_libdir}/cmake/yaml-cpp-static \
  -DBUILD_LIBMAMBA=ON \
  -DBUILD_LIBMAMBAPY=OFF \
  -DBUILD_MAMBA=OFF \
  -DBUILD_MICROMAMBA=ON \
  -DBUILD_EXE=ON \
  -DBUILD_SHARED=OFF \
  -DBUILD_STATIC=ON
%cmake_build
help2man %{_vpath_builddir}/micromamba/micromamba > %{_vpath_builddir}/micromamba/micromamba.1
%endif
%global _vpath_builddir %{_vendor}-%{_target_os}-build
%cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_LIBMAMBA=ON \
  -DBUILD_LIBMAMBAPY=ON \
  -DBUILD_MAMBA=OFF \
  -DBUILD_MICROMAMBA=OFF \
  -DBUILD_EXE=OFF \
  -DBUILD_SHARED=ON \
  -DBUILD_STATIC=OFF \
  -DENABLE_TESTS=ON \
  -DMAMBA_WARNING_AS_ERROR=OFF
%cmake_build
cmake --install %{__cmake_builddir} --prefix install
cd libmambapy
export SKBUILD_CONFIGURE_OPTIONS="\
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_LIBMAMBA=ON \
  -DBUILD_LIBMAMBAPY=ON \
  -DBUILD_MICROMAMBA=OFF \
  -DBUILD_MAMBA_PACKAGE=OFF \
  -DBUILD_EXE=ON \
  -DBUILD_SHARED=ON \
  -DBUILD_STATIC=OFF \
  -DENABLE_TESTS=ON \
  -Dlibmamba_ROOT=$PWD/../install \
  -DMAMBA_WARNING_AS_ERROR=OFF"
%pyproject_wheel
cd -
%cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_LIBMAMBA=ON \
  -DBUILD_LIBMAMBAPY=ON \
  -DBUILD_MAMBA=ON \
  -DBUILD_MICROMAMBA=OFF \
  -DBUILD_EXE=ON \
  -DBUILD_SHARED=ON \
  -DBUILD_STATIC=OFF \
  -DENABLE_TESTS=ON \
  -DMAMBA_WARNING_AS_ERROR=OFF
%cmake_build
help2man %{_vpath_builddir}/micromamba/mamba > %{_vpath_builddir}/micromamba/mamba.1


%install
%global _vpath_builddir %{_vendor}-%{_target_os}-build
%cmake_install
cd libmambapy
%pyproject_install
%pyproject_save_files libmambapy
cd -
%if %{with micromamba}
%global _vpath_builddir %{_vendor}-%{_target_os}-build-micromamba
%cmake_install
%else
ln -s mamba %{buildroot}%{_bindir}/micromamba
%endif

# Install init scripts
mkdir -p %{buildroot}/etc/profile.d
for shell in csh sh
do
  sed -e 's,\${\?MAMBA_EXE}\?,%{_bindir}/mamba,' < libmamba/data/mamba.$shell > %{buildroot}/etc/profile.d/mamba.$shell
done
mkdir -p %{buildroot}%{_datadir}/fish/vendor_conf.d
sed -e 's,\${\?MAMBA_EXE}\?,%{_bindir}/mamba,' < libmamba/data/mamba.fish > %{buildroot}%{_datadir}/fish/vendor_conf.d/mamba.fish

# man page
mkdir -p %{buildroot}%{_mandir}/man1
cp -p %{_vpath_builddir}/micromamba/mamba.1 %{buildroot}%{_mandir}/man1/

%if %{with micromamba}
# Install init scripts
mkdir -p %{buildroot}/etc/profile.d
for shell in csh sh
do
  sed -e 's,\${\?MAMBA_EXE}\?,%{_bindir}/micromamba,' < libmamba/data/micromamba.$shell > %{buildroot}/etc/profile.d/micromamba.$shell
done
mkdir -p %{buildroot}%{_datadir}/fish/vendor_conf.d
sed -e 's,\${\?MAMBA_EXE}\?,%{_bindir}/micromamba,' < libmamba/data/mamba.fish > %{buildroot}%{_datadir}/fish/vendor_conf.d/micromamba.fish

# man page
mkdir -p %{buildroot}%{_mandir}/man1
cp -p %{_vpath_builddir}/micromamba/micromamba.1 %{buildroot}%{_mandir}/man1/
%endif


%check
%ctest


%files
%license LICENSE
%doc CHANGELOG.md README.md
%{_libdir}/libmamba.so.2
%{_libdir}/libmamba.so.2.*

%files devel
%{_includedir}/mamba/
%{_libdir}/libmamba.so
%{_libdir}/cmake/%{name}/

%files -n mamba
# TODO - better ownership of vendor_conf.d
%dir %{_datadir}/fish/vendor_conf.d
%{_datadir}/fish/vendor_conf.d/mamba.fish
/etc/profile.d/mamba.sh
/etc/profile.d/mamba.csh
%{_bindir}/mamba
%if %{without micromamba}
%{_bindir}/micromamba
%endif
%{_mandir}/man1/mamba.1*

%if %{with micromamba}
%files -n micromamba
# TODO - better ownership of vendor_conf.d
%dir %{_datadir}/fish/vendor_conf.d
%{_datadir}/fish/vendor_conf.d/micromamba.fish
/etc/profile.d/micromamba.sh
/etc/profile.d/micromamba.csh
%{_bindir}/micromamba
%{_mandir}/man1/micromamba.1*
%endif

%files -n python3-libmambapy -f %{pyproject_files}
%doc CHANGELOG.md README.md


%changelog
%autochangelog

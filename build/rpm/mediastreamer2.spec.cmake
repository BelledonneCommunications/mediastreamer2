# -*- rpm-spec -*-

## rpmbuild options
# These 2 lines are here because we can build the RPM for flexisip, in which
# case we prefix the entire installation so that we don't break compatibility
# with the user's libs.
# To compile with bc prefix, use rpmbuild -ba --with bc [SPEC]
%define                 pkg_name        %{?_with_bc:bc-mediastreamer}%{!?_with_bc:mediastreamer}
%{?_with_bc: %define    _prefix         /opt/belledonne-communications}
%define                 srtp            %{?_without_srtp:0}%{?!_without_srtp:1}

%define     pkg_prefix %{?_with_bc:bc-}%{!?_with_bc:}

# re-define some directories for older RPMBuild versions which don't. This messes up the doc/ dir
# taken from https://fedoraproject.org/wiki/Packaging:RPMMacros?rd=Packaging/RPMMacros
%define _datarootdir       %{_prefix}/share
%define _datadir           %{_datarootdir}
%define _docdir            %{_datadir}/doc

%define build_number @PROJECT_VERSION_BUILD@
%if %{build_number}
%define build_number_ext -%{build_number}
%endif



Name:           %{pkg_name}
Version:        @PROJECT_VERSION@
Release:        %build_number%{?dist}
Summary:         Audio/Video real-time streaming

Group:          Applications/Communications
License:        GPL
URL:            http://www.mediastreamer.org
Source0:        %{name}-%{version}%{?build_number_ext}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-buildroot

Requires:	%{pkg_prefix}bctoolbox
Requires:	%{pkg_prefix}ortp

%description
Mediastreamer2 is a GPL licensed library to make audio and video
real-time streaming and processing. Written in pure C, it is based
upon the oRTP library.


%define         video           %{?_without_video:0}%{!?_without_video:1}

BuildRequires: 

%package devel
Summary:       Development libraries for mediastreamer
Group:         Development/Libraries
Requires:      %{name} = %{version}-%{release}

%description    devel
This package contains header files and development libraries needed to
develop programs using the mediastreamer2 library.

%if 0%{?rhel} && 0%{?rhel} <= 7
%global cmake_name cmake3
%define ctest_name ctest3
%else
%global cmake_name cmake
%define ctest_name ctest
%endif

%prep
%setup -n %{name}-%{version}%{?build_number_ext}

%build
%{expand:%%%cmake_name} . -DCMAKE_INSTALL_LIBDIR=%{_lib} -DCMAKE_PREFIX_PATH:PATH=%{_prefix} -DENABLE_VIDEO=%{video} -DENABLE_SRTP=%{srtp} -DENABLE_UNIT_TESTS=no
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%check
#%{ctest_name} -V %{?_smp_mflags}

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_libdir}/*.so.*
%if %{video}
%{_datadir}/images/nowebcamCIF.jpg
%endif

%files devel
%defattr(-,root,root,-)
%{_libdir}/*.a
%{_libdir}/*.so
#%{_libdir}/pkgconfig/*.pc
%{_includedir}
%{_datadir}/Mediastreamer2/cmake/Mediastreamer2Config.cmake
%{_datadir}/Mediastreamer2/cmake/Mediastreamer2ConfigVersion.cmake
%{_datadir}/Mediastreamer2/cmake/Mediastreamer2Targets-noconfig.cmake
%{_datadir}/Mediastreamer2/cmake/Mediastreamer2Targets.cmake
%doc %{_docdir}/*

%changelog
* Thu Jul 13 2017 jehan.monnier <jehan.monnier@linphone.org>
- cmake port
* Mon Aug 19 2013 jehan.monnier <jehan.monnier@linphone.org>
- Initial RPM release.

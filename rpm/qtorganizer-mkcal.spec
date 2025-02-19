Name: qtorganizer-mkcal
Version: 0.1
Release: 1
Summary: a QTOrganizer engine plugin using mKCal
License: BSD
URL:     https://github.com/dcaliste/qtorganizer-mkcal
Source0: %{name}-%{version}.tar.gz
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Organizer)
BuildRequires: pkgconfig(libmkcal-qt5)
BuildRequires: pkgconfig(KF5CalendarCore)
BuildRequires: cmake
BuildRequires: extra-cmake-modules

%description
Provides a plugin to store QOrganizer items on disk
using mKCal SQLite storage.

%package tests
Summary: Unit tests for qtorganizer-mkcal
BuildRequires: pkgconfig(Qt5Test)
Requires: %{name} = %{version}-%{release}

%description tests
This package contains unit tests for mKCal QtOrganizer plugin.

%prep
%setup -q -n %{name}-%{version}

%build
%cmake
make %{?_smp_mflags}

%install
make DESTDIR=%{buildroot} install
install -m 644 -p -D tests/tst_engine %{buildroot}/opt/tests/qtorganizer-mkcal

%files
%defattr(-,root,root,-)
%license LICENSE.BSD
%{_libdir}/qt5/plugins/organizer/*.so

%files tests
%defattr(-,root,root,-)
/opt/tests/qtorganizer-mkcal

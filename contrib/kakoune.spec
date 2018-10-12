Name:           kakoune
Version:        2018.09.04
Release:        1%{?dist}
Summary:        Vim inspired editor

License:        Unlicense
URL:            http://kakoune.org/
Source0:        %{name}-%{version}.tar.bz2

BuildRequires:  ncurses-devel >= 5.3
BuildRequires:  asciidoc
BuildRequires:  gcc-c++
Requires:       ncurses-libs >= 5.3

%description
Kakoune is a code editor heavily inspired by Vim

%prep
%setup -qn %{name}-%{version}

%build
cd src
make %{?_smp_mflags}

%check
cd src
LANG=en_US.utf8 make test

%install
cd src
%make_install PREFIX=/usr

%files
%doc
%{_bindir}/*
%{_mandir}/man1/kak*
%{_datadir}/doc/kak/*
%{_datadir}/kak/*

%changelog
* Fri Oct 12 2018 Jiri Konecny <jkonecny@redhat.com> - v2018.09.04
- Update spec file to a new release

* Sat May 5 2018 Łukasz Jendrysik <scadu@disroot.org> - v2018.04.13
- Use tagged release

* Wed May 11 2016 jkonecny <jkonecny@redhat.com> - 0-208.20160511git84f62e6f
- Add LANG=en_US.UTF-8 to fix tests
- Update to git: 84f62e6f

* Thu Feb 11 2016 jkonecny <jkonecny@redhat.com> - 0-158.20160210git050484eb
- Add new build requires asciidoc
- Use new man pages

* Sat Mar 28 2015 jkonecny <jkonecny@redhat.com> - 0-5.20150328gitd1b81c8f
- Automated git update by dgroc script new hash: d1b81c8f

* Tue Mar 24 2015 Jiri Konecny <jkonecny@redhat.com> 0-1.7eaa697git
- Add tests

* Tue Mar 17 2015 Jiri Konecny <jkonecny@redhat.com> 0-1.12a732dgit
- Create first rpm for kakoune

Name:           kakoune
Version:        0
Release:        5.20150328gitd1b81c8f%{?dist}
Summary:        Vim inspired editor

License:        Unlicense
URL:            https://github.com/mawww/kakoune
Source0:        kakoune-d1b81c8f.tar

BuildRequires:  boost-devel >= 1.50
BuildRequires:  ncurses-devel >= 5.3
Requires:       boost >= 1.50
Requires:       ncurses-libs >= 5.3

%description
Kakoune is a code editor heavily inspired by Vim

%prep
%setup -qn kakoune

%build
cd src
make %{?_smp_mflags}

%check
cd src
make test

%install
cd src
%make_install PREFIX=/usr

%changelog
* Sat Mar 28 2015 jkonecny <jkonecny@redhat.com> - 0-5.20150328gitd1b81c8f
- Automated git update by dgroc script new hash: d1b81c8f

* Tue Mar 24 2015 Jiri Konecny <jkonecny@redhat.com> 0-1.7eaa697git
- Add tests

* Tue Mar 17 2015 Jiri Konecny <jkonecny@redhat.com> 0-1.12a732dgit
- Create first rpm for kakoune

%files
%doc
%{_bindir}/*
%{_datadir}/doc/kak/*
%{_datadir}/kak/*


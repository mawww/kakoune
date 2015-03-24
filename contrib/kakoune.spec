# TODO: automate this to get new GitHub version everytime
%global commit 48007d5de22f57175115fc72fe9cb592e6b0efb2
%global shortcommit %(c=%{commit}; echo ${c:0:7})

Name:           kakoune
Version:        0
Release:        1.%{shortcommit}git%{?dist}
Summary:        Vim inspired editor

License:        Unlicense
URL:            https://github.com/mawww/kakoune
Source0:        https://github.com/mawww/kakoune/archive/%{commit}/kakoune-%{commit}.tar.gz

BuildRequires:  boost-devel >= 1.50
BuildRequires:  ncurses-devel >= 5.3
Requires:       boost >= 1.50
Requires:       ncurses-libs >= 5.3

%description
Kakoune is a code editor heavily inspired by Vim

%prep
%setup -qn %{name}-%{commit}

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
* Tue Mar 24 2015 Jiri Konecny <jkonecny@redhat.com> 0-1.7eaa697git
- Add tests
* Tue Mar 17 2015 Jiri Konecny <jkonecny@redhat.com> 0-1.12a732dgit
- Create first rpm for kakoune

%files
%doc
%{_bindir}/*
%{_datadir}/doc/kak/*
%{_datadir}/kak/*

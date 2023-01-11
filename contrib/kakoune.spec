%bcond_without tests

# Enable LTO. Profit ~8%
%global optflags        %{optflags} -flto
%global build_ldflags   %{build_ldflags} -flto

Name:           kakoune
Version:        2020.09.01
Release:        2%{?dist}
Summary:        Code editor heavily inspired by Vim

License:        Unlicense
URL:            https://kakoune.org/
Source0:        https://github.com/mawww/kakoune/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  asciidoc
BuildRequires:  gcc-c++ >= 7
BuildRequires:  glibc-langpack-en
BuildRequires:  pkgconfig(ncurses) >= 5.3

%description
- Modal editor
- Faster as in fewer keystrokes 
- Multiple selections
- Orthogonal design

Kakoune is a code editor that implements Vi’s "keystrokes as a text editing
language" model. As it’s also a modal editor, it is somewhat similar to the Vim
editor (after which Kakoune was originally inspired).

Kakoune can operate in two modes, normal and insertion. In insertion mode, keys
are directly inserted into the current buffer. In normal mode, keys are used to
manipulate the current selection and to enter insertion mode.

Kakoune has a strong focus on interactivity, most commands provide immediate and
incremental results, while still being competitive (as in keystroke count) with
Vim.

Kakoune works on selections, which are oriented, inclusive ranges of characters.
Selections have an anchor and a cursor. Most commands move both of
them except when extending selections, where the anchor character stays fixed and
the cursor moves around.


%prep
%autosetup -p1

# Use default Fedora build flags
sed -i '/CXXFLAGS += -O3/d' src/Makefile

# Install doc files in proper location
sed -i 's|$(PREFIX)/share/doc/kak|$(PREFIX)/share/doc/%{name}|' src/Makefile


%build
%set_build_flags
pushd src
%make_build
popd


%install
pushd src
%make_install PREFIX=%{_prefix}
popd


%if %{with tests}
%check
%set_build_flags
pushd src
LANG=en_US.utf8 %make_build test
popd
%endif


%files
%license UNLICENSE
%doc README.asciidoc CONTRIBUTING VIMTOKAK
%{_bindir}/kak
%{_datadir}/kak/
%{_mandir}/man1/*
%{_libexecdir}/kak/


%changelog
* Fri Jan 1 2021 Jiri Konecny <jkonecny@redhat.com> - 2020.09.01-2
- Add new libexec dir to the spec file

* Wed Sep 2 2020 Jiri Konecny <jkonecny@redhat.com> - 2020.09.01-1
- Update to 2020.09.01

* Tue Aug 4 2020 Jiri Konecny <jkonecny@redhat.com> - 2020.08.04-1
- Update to 2020.08.04

* Thu Jan 16 2020 Artem Polishchuk <ego.cordatus@gmail.com> - 2020.01.16-1
- Update to 2020.01.16

* Tue Dec 10 2019 Artem Polishchuk <ego.cordatus@gmail.com> - 2019.12.10-1
- Update to 2019.12.10

* Tue Nov 26 2019 Artem Polishchuk <ego.cordatus@gmail.com> - 2019.07.01-4
- Add patch to pass tests with default Fedora build flags

* Fri Nov 22 2019 Artem Polishchuk <ego.cordatus@gmail.com> - 2019.07.01-2
- Packaging fixes

* Wed Apr 24 2019 Jiri Konecny <jkonecny@redhat.com> - v2019.01.20-1
- Add a new build dependency (glibc-langpack-en) required for Fedora 30 and later
- Update version

* Fri Oct 12 2018 Jiri Konecny <jkonecny@redhat.com> - v2018.09.04-1
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

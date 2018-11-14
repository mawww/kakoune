declare-option str-to-str-map filetype_map
declare-option str-list       loaded_lang_files

define-command -hidden -params 1 lazy-load %{ evaluate-commands %sh{
    if [ -n "$1" ]; then
        lang_file=$(printf '%s' "$kak_opt_filetype_map" | grep -o "\b${1}=[^ ]*" | tr -d \' | cut -d '=' -f 2)
        if [ -n "$lang_file" -a -z "$(printf '%s' "$kak_opt_loaded_lang_files" | grep -o "$lang_file")" ]; then
            printf 'set-option -add global loaded_lang_files %s\n' "$lang_file"
            printf 'source %s/lang/%s\n' "$kak_runtime" "$lang_file"
        fi
    fi
} }

hook global BufSetOption filetype=([^*]*) %{
    lazy-load %val{hook_param_capture_1}
}


# core

# asciidoc
set-option -add global filetype_map 'asciidoc=asciidoc.kak'

hook global BufCreate .+\.(a(scii)?doc|asc) %{
    set-option buffer filetype asciidoc
}

# c-family
set-option -add global filetype_map 'c=c-family.kak'
set-option -add global filetype_map 'cpp=c-family.kak'
set-option -add global filetype_map 'objc=c-family.kak'

hook global BufCreate .*\.(cc|cpp|cxx|C|hh|hpp|hxx|H)$ %{
    set-option buffer filetype cpp
}

hook global BufSetOption filetype=c\+\+ %{
    set-option buffer filetype cpp
}

hook global BufCreate .*\.c$ %{
    set-option buffer filetype c
}

hook global BufCreate .*\.h$ %{
    try %{
        execute-keys -draft %{%s\b::\b|\btemplate\h*<lt>|\bclass\h+\w+|\b(typename|namespace)\b|\b(public|private|protected)\h*:<ret>}
        set-option buffer filetype cpp
    } catch %{
        set-option buffer filetype c
    }
}

hook global BufCreate .*\.m %{
    set-option buffer filetype objc
}

# diff
set-option -add global filetype_map 'diff=diff.kak'

hook global BufCreate .*\.(diff|patch) %{
    set-option buffer filetype diff
}

# kakrc
set-option -add global filetype_map 'kak=kakrc.kak'

hook global BufCreate (.*/)?(kakrc|.*.kak) %{
    lazy-load sh
    set-option buffer filetype kak
}

# makefile
set-option -add global filetype_map 'makefile=makefile.kak'

hook global BufCreate .*/?[mM]akefile %{
    set-option buffer filetype makefile
}

# python
set-option -add global filetype_map 'python=python.kak'

hook global BufCreate .*[.](py) %{
    set-option buffer filetype python
}

# sh
set-option -add global filetype_map 'sh=sh.kak'

hook global BufCreate .*\.(z|ba|c|k|mk)?sh(rc|_profile)? %{
    set-option buffer filetype sh
}


# base

# clojure
set-option -add global filetype_map 'clojure=clojure.kak'

hook global BufCreate .*[.](clj|cljc|cljs|cljx|edn) %{
    lazy-load lisp
    set-option buffer filetype clojure
}

# css
set-option -add global filetype_map 'css=css.kak'

hook global BufCreate .*[.](css) %{
    set-option buffer filetype css
}

# d
set-option -add global filetype_map 'd=d.kak'

hook global BufCreate .*\.di? %{
    set-option buffer filetype d
}

# etc
set-option -add global filetype_map 'etc-hosts=etc.kak'
set-option -add global filetype_map 'etc-resolv-conf=etc.kak'
set-option -add global filetype_map 'etc-shadow=etc.kak'
set-option -add global filetype_map 'etc-passwd=etc.kak'
set-option -add global filetype_map 'etc-gshadow=etc.kak'
set-option -add global filetype_map 'etc-group=etc.kak'
set-option -add global filetype_map 'etc-fstab=etc.kak'

hook global BufCreate .*/etc/(hosts|networks|services)  %{ set-option buffer filetype etc-hosts }
hook global BufCreate .*/etc/resolv.conf                %{ set-option buffer filetype etc-resolv-conf }
hook global BufCreate .*/etc/shadow                     %{ set-option buffer filetype etc-shadow }
hook global BufCreate .*/etc/passwd                     %{ set-option buffer filetype etc-passwd }
hook global BufCreate .*/etc/gshadow                    %{ set-option buffer filetype etc-gshadow }
hook global BufCreate .*/etc/group                      %{ set-option buffer filetype etc-group }
hook global BufCreate .*/etc/(fs|m)tab                  %{ set-option buffer filetype etc-fstab }
hook global BufCreate .*/etc/environment                %{ set-option buffer filetype sh }
hook global BufCreate .*/etc/env.d/.*                   %{ set-option buffer filetype sh }
hook global BufCreate .*/etc/profile(\.(csh|env))?      %{ set-option buffer filetype sh }
hook global BufCreate .*/etc/profile\.d/.*              %{ set-option buffer filetype sh }

# fish
set-option -add global filetype_map 'fish=fish.kak'

hook global BufCreate .*[.](fish) %{
    set-option buffer filetype fish
}

# gas
set-option -add global filetype_map 'gas=gas.kak'

hook global BufCreate .*\.(s|S|asm)$ %{
    set-option buffer filetype gas
}

# git
set-option -add global filetype_map 'git-commit=git.kak'

hook global BufCreate .*(COMMIT_EDITMSG|MERGE_MSG) %{
    set-option buffer filetype git-commit
}

hook global BufCreate .*(\.gitconfig|git/config) %{
    set-option buffer filetype ini
}

# go
set-option -add global filetype_map 'go=go.kak'

hook global BufCreate .*\.go %{
    set-option buffer filetype go
}

# haskell
set-option -add global filetype_map 'haskell=haskell.kak'

hook global BufCreate .*[.](hs) %{
    set-option buffer filetype haskell
}

# html
set-option -add global filetype_map 'html=html.kak'

hook global BufCreate .*\.html %{
    lazy-load css
    lazy-load javascript
    set-option buffer filetype html
}

# ini
set-option -add global filetype_map 'ini=ini.kak'

hook global BufCreate .+\.(repo|ini|cfg|properties) %{
    set-option buffer filetype ini
}

# java
set-option -add global filetype_map 'java=java.kak'

hook global BufCreate .*\.java %{
    set-option buffer filetype java
}

# javascript
set-option -add global filetype_map 'javascript=javascript.kak'
set-option -add global filetype_map 'typescript=javascript.kak'

hook global BufCreate .*[.](js)x? %{
    set-option buffer filetype javascript
}

hook global BufCreate .*[.](ts)x? %{
    set-option buffer filetype typescript
}

# json
set-option -add global filetype_map 'json=json.kak'

hook global BufCreate .*[.](json) %{
    set-option buffer filetype json
}

# julia
set-option -add global filetype_map 'julia=julia.kak'

hook global BufCreate .*\.(jl) %{
    set-option buffer filetype julia
}

# lisp
set-option -add global filetype_map 'lisp=lisp.kak'

hook global BufCreate .*[.](lisp) %{
    set-option buffer filetype lisp
}

# lua
set-option -add global filetype_map 'lua=lua.kak'

hook global BufCreate .*[.](lua) %{
    set-option buffer filetype lua
}

# mail
set-option -add global filetype_map 'mail=mail.kak'

hook global BufCreate .+\.eml %{
    set-option buffer filetype mail
}

# markdown
set-option -add global filetype_map 'markdown=markdown.kak'

hook global BufCreate .*[.](markdown|md|mkd) %{
    set-option buffer filetype markdown
}

# mercurial
set-option -add global filetype_map 'hg-commit=mercurial.kak'

hook global BufCreate .*hg-editor-\w+\.txt$ %{
    set-option buffer filetype hg-commit
}

# ocaml
set-option -add global filetype_map 'ocaml=ocaml.kak'

hook global BufCreate .*\.mli? %{
  set-option buffer filetype ocaml
}

# perl
set-option -add global filetype_map 'perl=perl.kak'

hook global BufCreate .*\.p[lm] %{
    set-option buffer filetype perl
}

# restructuredtext
set-option -add global filetype_map 'restructuredtext=restructuredtext.kak'

hook global BufCreate .*[.](rst) %{
    set-option buffer filetype restructuredtext
}

# ruby
set-option -add global filetype_map 'ruby=ruby.kak'

hook global BufCreate .*(([.](rb))|(irbrc)|(pryrc)|(Brewfile)|(Capfile|[.]cap)|(Gemfile|[.]gemspec)|(Guardfile)|(Rakefile|[.]rake)|(Thorfile|[.]thor)|(Vagrantfile)) %{
    set-option buffer filetype ruby
}

# rust
set-option -add global filetype_map 'rust=rust.kak'

hook global BufCreate .*[.](rust|rs) %{
    set-option buffer filetype rust
}

# scala
set-option -add global filetype_map 'scala=scala.kak'

hook global BufCreate .*[.](scala) %{
    set-option buffer filetype scala
}

# sql
set-option -add global filetype_map 'sql=sql.kak'

hook global BufCreate .*/?(?i)sql %{
    set-option buffer filetype sql
}

# swift
set-option -add global filetype_map 'swift=swift.kak'

hook global BufCreate .*\.(swift) %{
    set-option buffer filetype swift
}

# yaml
set-option -add global filetype_map 'yaml=yaml.kak'

hook global BufCreate .*[.](ya?ml) %{
    set-option buffer filetype yaml
}


# extra

# arch-linux
# package build description file
hook global BufCreate (.*/)?PKGBUILD %{
    set-option buffer filetype sh
}

# cabal
set-option -add global filetype_map 'cabal=cabal.kak'

hook global BufCreate .*[.](cabal) %{
    set-option buffer filetype cabal
}

# cmake
set-option -add global filetype_map 'cmake=cmake.kak'

hook global BufCreate .+\.cmake|.*/CMakeLists.txt %{
    set-option buffer filetype cmake
}

hook global BufCreate .*/CMakeCache.txt %{
    set-option buffer filetype ini
}

# coffee
set-option -add global filetype_map 'coffee=coffee.kak'

hook global BufCreate .*[.](coffee) %{
    set-option buffer filetype coffee
}

# cucumber
set-option -add global filetype_map 'cucumber=cucumber.kak'

hook global BufCreate .*[.](feature|story) %{
    set-option buffer filetype cucumber
}

# dart
set-option -add global filetype_map 'dart=dart.kak'

hook global BufCreate .*\.dart %{
    set-option buffer filetype dart
}

# dockerfile
set-option -add global filetype_map 'dockerfile=dockerfile.kak'

hook global BufCreate .*/?Dockerfile(\.\w+)?$ %{
    set-option buffer filetype dockerfile
}

# editorconfig
hook global BufCreate .*[.](editorconfig) %{
    set-option buffer filetype ini
    set-option buffer static_words indent_style indent_size tab_width \
    end_of_line charset insert_final_newline trim_trailing_whitespace root \
    latin1 utf-8 utf-8-bom utf-16be utf-16le lf cr crlf unset space tab
}

# elixir
set-option -add global filetype_map 'elixir=elixir.kak'

hook global BufCreate .*[.](ex|exs) %{
    set-option buffer filetype elixir
}

# elm
set-option -add global filetype_map 'elm=elm.kak'

hook global BufCreate .*[.](elm) %{
    set-option buffer filetype elm
}

# exherbo
set-option -add global filetype_map 'paludis-mirrors-conf=exherbo.kak'
set-option -add global filetype_map 'exheres-0-licence-groups=exherbo.kak'
set-option -add global filetype_map 'exheres-0-metadata=exherbo.kak'
set-option -add global filetype_map 'glep42=exherbo.kak'
set-option -add global filetype_map 'paludis-key-value-conf=exherbo.kak'
set-option -add global filetype_map 'paludis-options-conf=exherbo.kak'
set-option -add global filetype_map 'paludis-mirrors-conf=exherbo.kak'
set-option -add global filetype_map 'paludis-specs-conf=exherbo.kak'

## Repository metadata files
hook global BufCreate .*/metadata/mirrors\.conf         %{ set-option buffer filetype paludis-mirrors-conf }
hook global BufCreate .*/metadata/licence_groups.conf   %{ set-option buffer filetype exheres-0-licence-groups }
hook global BufCreate .*/metadata/options/descriptions/.*\.conf   %{ set-option buffer filetype exheres-0-licence-groups }
hook global BufCreate .*/metadata/.*\.conf              %{ set-option buffer filetype exheres-0-metadata }

## News items
hook global BufCreate .*/metadata/news/.*/.*\.txt %{ set-option buffer filetype glep42 }

## exheres-0, exlib
hook global BufCreate .*\.(exheres-0|exlib) %{ set-option buffer filetype sh }

## Paludis configurations
hook global BufCreate .*/etc/paludis(-.*)?/bashrc                               %{ set-option buffer filetype sh }
hook global BufCreate .*/etc/paludis(-.*)?/general(\.conf.d/.*.conf|\.conf)     %{ set-option buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/licences(\.conf.d/.*.conf|\.conf)    %{ set-option buffer filetype paludis-options-conf }
hook global BufCreate .*/etc/paludis(-.*)?/mirrors(\.conf.d/.*.conf|\.conf)     %{ set-option buffer filetype paludis-mirrors-conf }
hook global BufCreate .*/etc/paludis(-.*)?/options(\.conf.d/.*.conf|\.conf)     %{ set-option buffer filetype paludis-options-conf }
hook global BufCreate .*/etc/paludis(-.*)?/output(\.conf.d/.*.conf|\.conf)      %{ set-option buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/package_(unmask|mask)(\.conf.d/.*.conf|\.conf)     %{ set-option buffer filetype paludis-specs-conf }
hook global BufCreate .*/etc/paludis(-.*)?/platforms(\.conf.d/.*.conf|\.conf)   %{ set-option buffer filetype paludis-specs-conf }
hook global BufCreate .*/etc/paludis(-.*)?/repositories/.*\.conf                %{ set-option buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/repository\.template                 %{ set-option buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/repository_defaults\.conf            %{ set-option buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/specpath\.conf                       %{ set-option buffer filetype paludis-key-value-conf }
hook global BufCreate .*/etc/paludis(-.*)?/suggestions(\.conf.d/.*.conf|\.conf) %{ set-option buffer filetype paludis-specs-conf }

# haml
set-option -add global filetype_map 'haml=haml.kak'

hook global BufCreate .*[.](haml) %{
    lazy-load ruby
    lazy-load coffee
    lazy-load sass
    set-option buffer filetype haml
}

# hbs
set-option -add global filetype_map 'hbs=hbs.kak'

hook global BufCreate .*[.](hbs) %{
    lazy-load css
    lazy-load javascript
    lazy-load html
    set-option buffer filetype hbs
}

# just
set-option -add global filetype_map 'justfile=just.kak'

hook global BufCreate .*/?[jJ]ustfile %{
    lazy-load sh
    set-option buffer filetype justfile
}

# kickstart
set-option -add global filetype_map 'kickstart=kickstart.kak'

hook global BufCreate .*\.ks %{
    lazy-load sh
    set-option buffer filetype kickstart
}

# latex
set-option -add global filetype_map 'latex=latex.kak'

hook global BufCreate .*\.tex %{
    set-option buffer filetype latex
}

# moon
set-option -add global filetype_map 'moon=moon.kak'

hook global BufCreate .*[.](moon) %{
    set-option buffer filetype moon
}

# nim
set-option -add global filetype_map 'nim=nim.kak'

hook global BufCreate .*\.nim(s|ble)? %{
    set-option buffer filetype nim
}

# php
set-option -add global filetype_map 'php=php.kak'

hook global BufCreate .*[.](php) %{
    lazy-load css
    lazy-load javascript
    lazy-load html
    set-option buffer filetype php
}

# pony
set-option -add global filetype_map 'pony=pony.kak'

hook global BufCreate .*[.](pony) %{
    set-option buffer filetype pony
}

# protobuf
set-option -add global filetype_map 'protobuf=protobuf.kak'

hook global BufCreate .*\.proto %{
    set-option buffer filetype protobuf
}

# pug
set-option -add global filetype_map 'pug=pug.kak'

hook global BufCreate .*[.](pug|jade) %{
    lazy-load javascript
    set-option buffer filetype pug
}

# ragel
set-option -add global filetype_map 'ragel=ragel.kak'

hook global BufCreate .*[.](ragel|rl) %{
    set-option buffer filetype ragel
}

# sass
set-option -add global filetype_map 'sass=sass.kak'

hook global BufCreate .*[.](sass) %{
    set-option buffer filetype sass
}

# scheme
set-option -add global filetype_map 'scheme=scheme.kak'

hook global BufCreate (.*/)?(.*\.(scm|ss|sld)) %{
    lazy-load lisp
    set-option buffer filetype scheme
}

# scss
set-option -add global filetype_map 'scss=scss.kak'

hook global BufCreate .*[.](scss) %{
    lazy-load css
    set-option buffer filetype scss
}

# systemd
hook global BufCreate .*/systemd/.+\.(automount|conf|link|mount|network|path|service|slice|socket|target|timer) %{
    set-option buffer filetype ini

    # NOTE: INI files define the commenting character to be `;`, which won't work in `systemd` files
    hook -once buffer BufSetOption comment_line=.+ %{
        set-option buffer comment_line "#"
    }
}

# taskpaper
set-option -add global filetype_map 'taskpaper=taskpaper.kak'

hook global BufCreate .*\.taskpaper %{
    set-option buffer filetype taskpaper
}

# toml
set-option -add global filetype_map 'toml=toml.kak'

hook global BufCreate .*\.(toml) %{
    set-option buffer filetype toml
}

# troff
set-option -add global filetype_map 'troff=troff.kak'

hook global BufCreate .*\.\d+ %{
    set-option buffer filetype troff
}

# tupfile
set-option -add global filetype_map 'tupfile=tupfile.kak'

hook global BufCreate .*/?Tup(file|rules)(\.\w+)?$ %{
    set-option buffer filetype tupfile
}

# void-linux
# Void Linux package template
hook global BufCreate .*/?srcpkgs/.+/template %{
    set-option buffer filetype sh
}

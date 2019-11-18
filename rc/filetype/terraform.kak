# Terraform configuration language
# https://www.terraform.io/docs/configuration/

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](tf|tfvars) %{
  set-option buffer filetype terraform
}


# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=terraform %{
    require-module terraform

    set-option window static_words %opt{terraform_static_words}

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window terraform-.+ }
}


hook -group terraform-highlight global WinSetOption filetype=terraform %{
    add-highlighter window/terraform ref terraform
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/terraform }
}


provide-module terraform %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/terraform regions
add-highlighter shared/terraform/code  default-region group

add-highlighter shared/terraform/comment1 region '#'    '$'  fill comment
add-highlighter shared/terraform/comment2 region '\\'   '$'  fill comment
add-highlighter shared/terraform/comment3 region /\*    \*/  fill comment

# Strings can contain interpolated terraform expressions, which can contain
# strings. Currently, we cannot support nesting of the same type of delimiter,
# so instead we render the full interpolation as a value (otherwise, it
# looks bad).
# See https://github.com/mawww/kakoune/issues/1670
add-highlighter shared/terraform/string  region '"' '(?<!\\)(?:\\\\)*"'  group
add-highlighter shared/terraform/string/fill fill string
add-highlighter shared/terraform/string/inter regex \$\{.+?\} 0:value

add-highlighter shared/terraform/heredoc region -match-capture '<<-?(\w+)' '^\h*(\w+)$' regions
add-highlighter shared/terraform/heredoc/fill default-region fill string
add-highlighter shared/terraform/heredoc/inter region -recurse \{ (?<!\\)(\\\\)*\$\{ \} ref terraform


add-highlighter shared/terraform/code/valueDec regex '\b[0-9]+([kKmMgG]b?)?\b' 0:value
add-highlighter shared/terraform/code/valueHex regex '\b0x[0-9a-f]+([kKmMgG]b?)?\b' 0:value

add-highlighter shared/terraform/code/operators regex [\[\]] 0:operator

add-highlighter shared/terraform/code/field regex '^\h+(\w+)\s*(=)' 1:variable 2:keyword

evaluate-commands %sh{
  blocks="connection content data dynamic locals module output provider
          provisioner resource terraform variable"

  constants="true false null"

  keywords="for for_each if in"

  types="bool list map number object set string tuple"

  var_subs="local module var"

  # Builtin functions
  fun_num="abs ceil floor log max min parseint pow signum"

  fun_str="chomp format formatlist indent join lower regex regexall replace
           split strrev substr title trimspace upper"

  fun_coll="chunklist coalesce coalescelist compact concat contains
            distinct element flatten index keys length lookup
            matchkeys merge range reverse setintersection setproduct
            setunion slice sort transpose values zipmap"

  fun_enc="base64decode base64encode base64gzip csvdecode jsondecode
           jsonencode urlencode yamldecode yamlencode"

  fun_file="abspath dirname pathexpand basename file fileexists fileset
            filebase64 templatefile"

  fun_dt="formatdate timeadd timestamp"

  fun_crypt="base64sha256 base64sha512 bcrypt filebase64sha256
             filebase64sha512 filemd5 filesha1 filesha256 filesha512 md5
             rsadecrypt sha1 sha256 sha512 uuid uuidv5"

  fun_net="cidrhost cidrnetmask cidrsubnet"

  fun_cast="tobool tolist tomap tonumber toset tostring"

  functions="$fun_num $fun_str $fun_coll $fun_enc $fun_file $fun_dt $fun_crypt $fun_net $fun_cast"

  join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

  # Add grammar elements to the static completion list
  printf %s\\n "declare-option str-list terraform_static_words $(join "$blocks $keywords $constants $types $var_subs $functions" ' ')"

  # Highlight grammar elements
  printf %s "
    add-highlighter shared/terraform/code/ regex '\b($(join "$blocks"    '|'))\b[^.]' 1:keyword
    add-highlighter shared/terraform/code/ regex '\b($(join "$keywords"  '|'))\b'     1:keyword
    add-highlighter shared/terraform/code/ regex '\b($(join "$constants" '|'))\b'     1:value
    add-highlighter shared/terraform/code/ regex '\b($(join "$types"     '|'))\b'     1:type
    add-highlighter shared/terraform/code/ regex '\b($(join "$var_subs"  '|'))\b\.'   1:meta
    add-highlighter shared/terraform/code/ regex '\b($(join "$functions" '|'))\s*\('  1:builtin
  "
}

§

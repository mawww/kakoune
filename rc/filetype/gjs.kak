# https://github.com/ember-template-imports/ember-template-imports
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](gjs) %{
    set-option buffer filetype gjs
}

hook global BufCreate .*[.](gts) %{
    set-option buffer filetype gts
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=(gjs|gts) %{
    require-module javascript

    hook window ModeChange pop:insert:.* -group "%val{hook_param_capture_1}-trim-indent" javascript-trim-indent
    hook window InsertChar .* -group "%val{hook_param_capture_1}-indent" javascript-indent-on-char
    hook window InsertChar \n -group "%val{hook_param_capture_1}-insert" javascript-insert-on-new-line
    hook window InsertChar \n -group "%val{hook_param_capture_1}-indent" javascript-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* "
        remove-hooks window %val{hook_param_capture_1}-.+
    "
}

hook -group gjs-highlight global WinSetOption filetype=gjs %{
    require-module gjs
    add-highlighter window/gjs ref gjs
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/gjs }
}

hook -group gts-highlight global WinSetOption filetype=gts %{
    require-module gts
    add-highlighter window/gts ref gts
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/gts }
}

# Modules

provide-module gjs %{
    require-module javascript
    require-module hbs
    require-module html
    maybe-add-hbs-to-html

    add-highlighter "shared/gjs" regions
    add-highlighter "shared/gjs/" default-region ref javascript
    add-highlighter "shared/gjs/hbs" region '<template>' '</template>' ref html
}

provide-module gts %{
    require-module javascript
    require-module hbs
    require-module html
    maybe-add-hbs-to-html

    add-highlighter "shared/gts" regions
    add-highlighter "shared/gts/" default-region ref typescript
    add-highlighter "shared/gts/hbs" region '<template>' '</template>' ref html
}

decl line-flag-list git_diff_flags

def git-diff-update-buffer %{ %sh{
    added_lines=""
    removed_lines=""
    git diff -U0 $kak_bufname | {
        line=0
        flags="0:red:."
        while read; do
            if [[ $REPLY =~ ^---.* ]]; then
                continue
            elif [[ $REPLY =~ ^@@.-[0-9]+(,[0-9]+)?.\+([0-9]+)(,[0-9]+)?.@@.* ]]; then
                line=${BASH_REMATCH[2]}
            elif [[ $REPLY =~ ^\+ ]]; then
                flags="$flags,$line:green:+"
                ((line++))
            elif [[ $REPLY =~ ^\- ]]; then
                flags="$flags,$line:red:-"
            fi
        done
        echo "setb git_diff_flags '$flags'"
    }
}}

def git-diff-show %{ addhl flag_lines black git_diff_flags; git-diff-update-buffer }

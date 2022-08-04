
# Detection
# --------

hook global BufCreate .*\.v %{
    set-option buffer filetype coq
}

# Initialization
# --------------

hook global WinSetOption filetype=coq %{
    require-module coq
    hook window ModeChange pop:insert:.* -group coq-trim-indent coq-trim-indent
    hook window InsertChar \n -group coq-indent coq-copy-indent-on-newline

    set-option window static_words %opt{coq_static_words}
    add-highlighter window/coq ref coq

    hook -once -always window WinSetOption filetype=.* %{
        remove-highlighter window/coq
        remove-hooks window coq-indent 
    }
}

provide-module coq %{

# Syntax
# ------

# This is a `looks sensible' keyword syntax highlighting, far from being correct.
# Note that only the core language and the proof language is supported,
# the Ltac language is not (for now).
     
add-highlighter shared/coq regions

add-highlighter shared/coq/comment region -recurse \Q(* \Q(* \Q*) fill comment
add-highlighter shared/coq/string region (?<!")" (?<!")("")*" fill string

add-highlighter shared/coq/command default-region group

# This is not any lexical convention of coq, simply highlighting used to make
# proofs look better, based on how people usually use notations
add-highlighter shared/coq/command/ regex [`!@#$%^&*-=+\\:\;|<>/]+ 0:operator
add-highlighter shared/coq/command/ regex \(dfs\)|\(bfs\)          0:operator
add-highlighter shared/coq/command/ regex [()\[\]{}]               0:operator

# numeral literals
add-highlighter shared/coq/command/ regex [-]?[0-9][0-9_]*(\.[0-9_]+)?([eE][+-][0-9_]+)? 0:value

evaluate-commands %sh{
    # These are builtin keywords of the Gallina language (without tactics)
    keywords="_ IF Prop SProp Set Type as at by cofix discriminated else end exists exists2 fix for"
    keywords="${keywords} forall fun if in lazymatch let match multimatch return then using where with"
    keywords="${keywords} inside outside"

    # These are (part of) coq top level commands
    commands="Abort About Add Admitted All Arguments Axiom Back BackTo"
    commands="${commands} Canonical Cd Check Coercion CoFixpoint Collection Compute Conjecture Context Contextual Corollary"
    commands="${commands} Declare Defined Definition Delimit Drop End Eval Example Existential Export"
    commands="${commands} Fact Fail File Fixpoint Focus From Function Generalizable Global Goal Grab"
    commands="${commands} Hint Hypotheses Hypothesis Immediate Implicit Import Include Inductive"
    commands="${commands} Lemma Let Library Load LoadPath Local Locate Module No Notation Opaque"
    commands="${commands} Parameter Parameters Primitive Print Proof Property Proposition Pwd Qed Quit"
    commands="${commands} Rec Record Redirect Register Remark Remove Require Reset"
    commands="${commands} Section Search SearchAbout SearchHead SearchPattern SearchRewrite Show Strategy"
    commands="${commands} Test Theorem Time Timeout Transparent Types Universes Undo Unfocus Unfocused Unset Variable Variables"

    # These are (part of) coq's builtin tactics
    tactics="abstract absurd admit all apply assert assert_fails"
    tactics="${tactics} assert_succeeds assumption auto autoapply"
    tactics="${tactics} autorewrite autounfold btauto by case cbn"
    tactics="${tactics} cbv change clear clearbody cofix compare"
    tactics="${tactics} compute congr congruence constructor contradict"
    tactics="${tactics} cut cutrewrite cycle decide decompose dependent"
    tactics="${tactics} destruct discriminate do done double"
    tactics="${tactics} eapply eassert eauto eexact elim elimtype exact exfalso"
    tactics="${tactics} fail field first firstorder fix fold functional"
    tactics="${tactics} generalize guard have hnf idtac induction injection"
    tactics="${tactics} instantiate intro intros intuition inversion"
    tactics="${tactics} inversion_clear lapply lazy last move omega"
    tactics="${tactics} pattern pose progress red refine reflexivity"
    tactics="${tactics} remember rename repeat replace rewrite right ring"
    tactics="${tactics} set setoid_reflexivity setoid_replace setoid_rewrite"
    tactics="${tactics} setoid_symmetry setoid_transitivity simpl simple"
    tactics="${tactics} simplify_eq solve specialize split start stop"
    tactics="${tactics} subst symmetry tauto transitivity trivial try"
    tactics="${tactics} under unfold unify unlock"

    echo declare-option str-list coq_static_words ${keywords} ${commands} ${tactics}

    keywords_regex=$(echo ${keywords} | tr ' ' '|')
    printf %s "
        add-highlighter shared/coq/command/ regex \b(${keywords_regex})\b 0:keyword
    "
    commands_regex=$(echo ${commands} | tr ' ' '|')
    printf %s "
        add-highlighter shared/coq/command/ regex ^[\h\n]*(${commands_regex})\b 0:variable
    "

    tactics_regex=$(echo ${tactics} | tr ' ' '|')
    printf %s "
        add-highlighter shared/coq/command/ regex \b(${tactics_regex})\b 0:keyword
    "
}

# Indentation
# -----------
# Coq's syntax is based heavily on keywords and program structure,
# not based on explicit, unique delimiters, like braces in C-family.
# So it is difficult to properly indent using only regex...
# Hence here only a simple mechanism of copying indent is done.
define-command -hidden coq-copy-indent-on-newline %{
    evaluate-commands -draft -itersel %{
        try %{ execute-keys -draft k x s ^\h+ <ret> y gh j P }
    }
}

define-command -hidden coq-trim-indent %{
    evaluate-commands -no-hooks -draft -itersel %{
        execute-keys x
        # remove trailing white spaces
        try %{ execute-keys -draft s \h + $ <ret> d }
    }
}

}

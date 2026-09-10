# https://wiki.contextgarden.net/Main_Page
# 
# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.(mkiv) %{
    set-option buffer filetype context
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=context %(
    require-module context

    hook window InsertChar \n -group context-indent context-indent-on-new-line
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window context-indent }

)

hook -group context-highlight global WinSetOption filetype=context %{
    add-highlighter window/context ref context
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/context }
}

provide-module context %{

# Highlighters
#‾‾‾‾‾‾‾‾‾‾‾‾
#
add-highlighter shared/context group
#add-highlighter shared/context default-region group
#

# Commands and control sequences for typesetting 
# ‾‾‾‾‾‾‾‾‾‾‾‾
#add-highlighter shared/context/ regex '(\[\w+?=?\w+\])' 1:cyan 2:magenta 3:yellow
#add-highlighter shared/context/ regex '(\[?\w-*\w+\]|\[\w*-|\W)' 1:yellow 2:magenta 3:cyan
add-highlighter shared/context/ regex '(\[?\w*-*\w+\]|\[\w*-|\W?)' 1:yellow 2:magenta 3:cyan
#add-highlighter shared/context/ regex '(\[\w*\W?\w+\])' 1:yellow 2:magenta 3:cyan
#add-highlighter shared/context/ regex '(\w*|\W?\[?\W)' 1:cyan 2:yellow 3:magenta
add-highlighter shared/context/ regex '(\[file?:\w+\])' 1:value 
add-highlighter shared/context/ regex '(\{\\em.*?\})' 1:+i
add-highlighter shared/context/ regex '(\{\\bf.*?\})' 1:+b
add-highlighter shared/context/ regex '(\{\\it.*?\})' 1:+i
add-highlighter shared/context/ regex '(\{\\bi.*?\})' 1:+ib
add-highlighter shared/context/ regex '(\{\\bs.*?\})' 1:+ib
add-highlighter shared/context/ regex '(\\overstrikes\{.*?\})' 1:+s

#for more information about the commands availability and/or implementation please read the ConTeXt documentation
#
add-highlighter shared/context/ regex '(\h*\\start*?\w-*|FLOATtext|FLOWcell|FLOWchart|FORMULAformula|JScode|JSpreamble|LUA|MP|MPclip|MPcode|MPdefinitions|MPenvironment|MPextensions|MPinclusions|MPinitializations|MPpage|MPpositiongraphic|MPpositionmethod|MPrun|PARSEDXML|TEX|TEXpage|XML|align|alignment|allmodes|appendices|asciimode|aside|attachment|attention|backgrounds|backmatter|bar|bbordermatrix|bitmapimage|blockquote|bodymatter|boxedcolumns|btxlabeltext|btxrenderingdefinitions|buffer|but|cases|catcodetable|centeraligned|chapter|characteralign|checkedfences|chemical|chemicaltext|collect|collecting|color|colorintent|coloronly|colorset|columns|columnset|columnsetspan|combination|comment|component|concept|contextcode|contextdefinitioncode|ctxfunction|ctxfunctiondefinition|currentcolor|currentlistentrywrapper|delimited|delimitedtext|displaymath|document|effect|element|embeddedxtable|endnote|enumerate|environment|exceptions|expanded|expandedcollect|extendedcatcodetable|externalfigurecollection|facingfloat|fact|figure|figuretext|fittingpage|fixed|floatcombination|font|fontclass|fontsolution|footnote|formula|formulas|framed|framedcell|framedcontent|framedrow|framedtable|framedtext|frontmatter|goto|graphictext|gridsnapping|hanging|hboxestohbox|hbregister|head|headtext|headtext|help|helptext|hiding|highlight|hyphenation|imath|indentedtext|interaction|interactionmenu|interface|intermezzotext|intertext|item|itemgroup|itemgroupcolumns|itemize|itemize|knockout|labeltext|language|layout|leftaligned|legend|linealignment|linecorrection|linefiller|linenote|linenumbering|lines|linetable|linetablebody|linetablecell|linetablehead|localfootnotes|localfootnotes|localheadsetup|locallinecorrection|localnotes|localsetups|luacode|luaparameterset|makeup|marginblock|marginrule|markedcontent|markedpages|math|mathalignment|mathcases|mathlabeltext|mathmatrix|mathmode|mathstyle|matrices|matrix|maxaligned|mdformula|midaligned|middlealigned|middlemakeup|mixedcolumns|mode|modeset|module|moduletestsection|mpformula|multicolumns|namedsection|namedsubformulas|narrow|narrower|negative|nicelyfilledbox|nointerference|notallmodes|note|notext|notmode|operatortext|opposite|outputstream|overlay|overprint|packed|pagecolumns|pagecomment|pagefigure|pagelayout|pagemakeup|par|paragraph|paragraphs|parbuilder|part|placechemical|placefigure|placefloat|placeformula|placegraphic|placeintermezzo|placelegend|placepairedbox|placetable|positioning|positionoverlay|postponing|prefixtext|processcommacommand|processcommalist|processesassignmentcommand|processesassignmentlist|product|project|protectedcolors|publication|quotation|quote|randomized|rawsetups|readingfile|regime|register|remark|reusableMPgraphic|reusableMPgraphic|rightaligned|ruby|script|sdformula|section|sectionblock|sectionblockenvironment|sectionlevel|setups|shift|sidebar|simplecolumns|specialitem|speech|splitformula|splittext|spread|standardmakeup|staticMPfigure|staticMPgraphic|strictinspectnextcharacter|structurepageregister|style|subformulas|subject|subjectlevel|subsection|subsentence|substack|subsubject|subsubsection|subsubsubject|subsubsubsection|suffixtext|symbolset|table|tabletext|tabulate|tabulatehead|tabulatetail|taglabeltext|texcode|texdefinition|text|textbackgrounds|textcolor|textcolorintent|textflow|textmakeup|textrule|textrule|thematrix|title|tokenlist|tokens|transparent|typescript|typescriptcollection|typing|uniqueMPgraphic|unittext|unpacked|usableMPgraphic|useMPgraphic|usemathstyleparameter|userdata|usingbtxspecification|vboxregister|vboxtohbox|vboxtohboxseparator|viewerlayer|vtopregister|xcell|xcolumn|xgroup|xmldisplayverbatim|xmlinlineverbatim|xmlraw|xmlsetups|xrow|xrowgroup|xtable|xtablebody|xtablefoot|xtablehead|xtablenext)' 1:meta

#add-highlighter shared/context/ regex '(\h*\\stop*?\w+)' 1:meta

add-highlighter shared/context/ regex '(\h*\\stop*?\w-*|FLOATtext|FLOWcell|FLOWchart|FORMULAformula|JScode|JSpreamble|LUA|MP|MPclip|MPcode|MPdefinitions|MPenvironment|MPextensions|MPinclusions|MPinitializations|MPpage|MPpositiongraphic|MPpositionmethod|MPrun|PARSEDXML|TEX|TEXpage|XML|align|alignment|allmodes|appendices|asciimode|aside|attachment|attention|backgrounds|backmatter|bar|bbordermatrix|bitmapimage|blockquote|bodymatter|boxedcolumns|btxlabeltext|btxrenderingdefinitions|buffer|but|cases|catcodetable|centeraligned|chapter|characteralign|checkedfences|chemical|chemicaltext|collect|collecting|color|colorintent|coloronly|colorset|columns|columnset|columnsetspan|combination|comment|component|concept|contextcode|contextdefinitioncode|ctxfunction|ctxfunctiondefinition|currentcolor|currentlistentrywrapper|delimited|delimitedtext|displaymath|document|effect|element|embeddedxtable|endnote|enumerate|environment|exceptions|expanded|expandedcollect|extendedcatcodetable|externalfigurecollection|facingfloat|fact|figure|figuretext|fittingpage|fixed|floatcombination|font|fontclass|fontsolution|footnote|formula|formulas|framed|framedcell|framedcontent|framedrow|framedtable|framedtext|frontmatter|goto|graphictext|gridsnapping|hanging|hboxestohbox|hbregister|head|headtext|headtext|help|helptext|hiding|highlight|hyphenation|imath|indentedtext|interaction|interactionmenu|interface|intermezzotext|intertext|item|itemgroup|itemgroupcolumns|itemize|itemize|knockout|labeltext|language|layout|leftaligned|legend|linealignment|linecorrection|linefiller|linenote|linenumbering|lines|linetable|linetablebody|linetablecell|linetablehead|localfootnotes|localfootnotes|localheadsetup|locallinecorrection|localnotes|localsetups|luacode|luaparameterset|makeup|marginblock|marginrule|markedcontent|markedpages|math|mathalignment|mathcases|mathlabeltext|mathmatrix|mathmode|mathstyle|matrices|matrix|maxaligned|mdformula|midaligned|middlealigned|middlemakeup|mixedcolumns|mode|modeset|module|moduletestsection|mpformula|multicolumns|namedsection|namedsubformulas|narrow|narrower|negative|nicelyfilledbox|nointerference|notallmodes|note|notext|notmode|operatortext|opposite|outputstream|overlay|overprint|packed|pagecolumns|pagecomment|pagefigure|pagelayout|pagemakeup|par|paragraph|paragraphs|parbuilder|part|placechemical|placefigure|placefloat|placeformula|placegraphic|placeintermezzo|placelegend|placepairedbox|placetable|positioning|positionoverlay|postponing|prefixtext|processcommacommand|processcommalist|processesassignmentcommand|processesassignmentlist|product|project|protectedcolors|publication|quotation|quote|randomized|rawsetups|readingfile|regime|register|remark|reusableMPgraphic|reusableMPgraphic|rightaligned|ruby|script|sdformula|section|sectionblock|sectionblockenvironment|sectionlevel|setups|shift|sidebar|simplecolumns|specialitem|speech|splitformula|splittext|spread|standardmakeup|staticMPfigure|staticMPgraphic|strictinspectnextcharacter|structurepageregister|style|subformulas|subject|subjectlevel|subsection|subsentence|substack|subsubject|subsubsection|subsubsubject|subsubsubsection|suffixtext|symbolset|table|tabletext|tabulate|tabulatehead|tabulatetail|taglabeltext|texcode|texdefinition|text|textbackgrounds|textcolor|textcolorintent|textflow|textmakeup|textrule|textrule|thematrix|title|tokenlist|tokens|transparent|typescript|typescriptcollection|typing|uniqueMPgraphic|unittext|unpacked|usableMPgraphic|useMPgraphic|usemathstyleparameter|userdata|usingbtxspecification|vboxregister|vboxtohbox|vboxtohboxseparator|viewerlayer|vtopregister|xcell|xcolumn|xgroup|xmldisplayverbatim|xmlinlineverbatim|xmlraw|xmlsetups|xrow|xrowgroup|xtable|xtablebody|xtablefoot|xtablehead|xtablenext)' 1:meta

add-highlighter shared/context/ regex '(\h*\\define*?\w-*|accent|activecharacter|alternativestyle|anchor|attachment|attribute|background|bar|block|bodyfont|bodyfontenvironment|bodyfontswitch|bodyfont|breakpoint|breakpoints|btx|btxdataset|btxregister|btxrendering|buffer|button|capitals|character|characterkerning|characterspacing|chemical|chemicals|chemicalsymbol|collector|color|colorgroup|color|columnbreak|columnset|columnsetarea|columnsetspan|combination|combinedlist|command|comment|complexorsimple|complexorsimpleempty|complexorsimpleempty|complexorsimple|conversion|conversionset|conversion|counter|dataset|date|delimitedtext|description|dfont|document|effect|enumeration|expandable|expansion|externalfigure|face|facingfloat|fallbackfamily|fallbackfamily|field|fieldbody|fieldbodyset|fieldcategory|fieldstack|figuresymbol|fileconstant|filefallback|filesynonym|filler|firstline|fittingpage|float|float|font|fontalternative|fontfallback|fontfamily|fontfamilypreset|fontfamily|fontfamilypreset|fontfamily|fontfeature|fontfile|fontsize|fontsolution|fontstyle|fontsynonym|font|formula|formulaalternative|formulaframed|framed|framedcontent|framedtable|framedtext|frozenfont|globalcolor|color|globalcolor|graphictypesynonym|gridsnapping|hbox|head|headalternative|help|high|highlight|hspace|hyphenationfeatures|indentedtext|indenting|initial|insertion|interaction|interactionbar|interactionmenu|interfaceconstant|interfaceelement|interfacevariable|interlinespace|intermediatecolor|itemgroup|items|label|labelclass|layer|layerpreset|layerpreset|layout|linefiller|linenote|linenumbering|lines|list|listalternative|listextra|low|lowhigh|lowmidhigh|MPinstance|makeup|marginblock|margindata|marker|marking|mathaccent|mathalignment|mathcases|mathcommand|mathdouble|mathdoubleextensible|mathematics|mathextensible|mathfence|mathfraction|mathframed|mathmatrix|mathornament|mathover|mathoverextensible|mathovertextextensible|mathradical|mathstackers|mathstyle|mathtriplet|mathunder|mathunderextensible|mathundertextextensible|mathunstacked|measure|messageconstant|mixedcolumns|mode|multicolumns|multitonecolor|color|spotcolor|namedcolor|color|namedcolor|namespace|narrower|note|orientation|ornament|ornament|outputroutine|outputroutinecommand|overlay|page|pagebreak|pagechecker|pagecolumns|pageinjection|pageinjectionalternative|pageshift|pagestate|pairedbox|palet|palet|papersize|papersize|paragraph|paragraphs|parallel|parbuilder|periodkerning|placeholder|placement|positioning|prefixset|processcolor|color|processor|profile|program|pushbutton|pushsymbol|reference|referenceformat|register|renderingwindow|resetset|ruby|scale|script|section|sectionblock|sectionlevels|selector|separatorset|shift|sidebar|sort|sorting|spotcolor|startstop|style|styleinstance|subfield|subformula|symbol|synonym|synonyms|systemattribute|systemconstant|systemvariable|TABLEsetup|tabletemplate|tabulate|text|textbackground|textflow|textnote|tokenlist|tooltip|transparency|transparency|transparency|twopasslist|type|typeface|typescriptprefix|typescriptsynonym|typesetting|typing|unit|userdata|userdataalternative|viewerlayer|vspace|vspacing|vspacingamount|xtable|color|gridsnapping|bodyfontenvironment)' 1:meta 

add-highlighter shared/context/ regex '(\h*\\setup*?\w-*|MPinstance|TABLE|align|attachment|background|backgroundOPT|bar|blackrules|bleeding|block|bodyfont|bookmark|btx|btxdataset|btxregister|btxrendering|buffer|button|capitals|caption|characteralign|characterkerning|chemical|clipping|collector|columnset|columnsetarea|columnsetspan|combination|combinedlist|combinedlistOPT|comment|counter|dataset|delimitedtext|description|document|effect|enumeration|expansion|externalfigure|facingfloat|fieldbody|fieldcategory|fieldcontentframed|fieldlabelframed|fieldtotalframed|filler|fillinlines|fillinrules|firstline|fittingpage|fittingpageOPT|float|floatsplitting|fontsolution|formula|formulaframed|framed|framedcontent|framedtext|head|headalternative|help|high|highlight|indentedtext|indenting|initial|insertion|interaction|interactionbar|interactionbarOPT|interactionmenu|interlinespace|itemgroup|items|label|language|layer|layerOPT|layeredtext|layout|linefiller|linenote|linenumbering|lines|linetable|list|listalternative|listextra|low|lowhigh|lowmidhigh|makeup|marginblock|margindata|marginframed|marginrule|marginruleOPT|marking|mathalignment|mathcases|mathematics|mathfence|mathfraction|mathframed|mathmatrix|mathmatrixOPT|mathornament|mathradical|mathstackers|mixedcolumns|mixedcolumnsOPT|multicolumns|multicolumnsOPT|narrower|notation|note|offset|offsetbox|orientation|outputroutine|pagechecker|pagecolumns|pagecolumnsOPT|pageinjection|pageinjectionalternative|pagestate|pairedbox|papersize|paragraph|paragraphs|parallel|periodkerning|periods|placeholder|placement|positionbar|positioning|processor|profile|referenceformat|register|renderingwindow|rotate|ruby|scale|script|sectionblock|selector|shift|sidebar|sorting|startstop|stretched|style|subformula|synonyms|tables|tabulate|textbackground|textflow|textnote|thinrules|tolerance|tooltip|type|typing|unit|userdata|userdataalternative|viewerlayer|xtable|xtableOPT)' 1:meta


add-highlighter shared/context/ regex '(\h*\\use*?\w-*|typescript)' 1:meta 

#add-highlighter shared/context/ regex '(^\\start\.*|chapter|section|subsection|subsubsection|paragraph|tabulate|itemize|enumerate|formula|columns|table|quotation|textbackgrounds|backgrounds)' 1:green 
#
#add-highlighter shared/context/ regex '(^\\stop\.*|chapter|section|subsection|subsubsection|paragraph|tabulate|itemize|enumerate|formula|columns|table|quotation|textbackgrounds|backgrounds)' 1:green

#detection of any % comment string including followed by a command
add-highlighter shared/context/ regex '(\B\h*%.*?$)' 1:comment

#indentation
#
define-command -hidden context-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        try %{ execute-keys -draft <semicolon>K<a-&> }
        try %{ execute-keys -draft kxs(\[.*\])<esc>"ay}
        try %{ execute-keys -draft Z kx <a-k>(^\h*([^\n]+)\])<ret> z i<tab> <esc> <c-r><a><c-r><a><esc>}
		#The following was the tricky part,without additional registers or an if then clause regex, for the simple reason that unlike TeX derivatives such as LaTeX in which groups are defined within \{ and \} respectively and its optional arguments' counterparts are enclosed within \[ and \], in ConTeXt however, brackets are used for both optional and mandatory arguments. So the key element here is to reverse the order, for the selection to reflect it, and it doesn't matter really whether \stop precedes \start or whether the input will attempt to modify it. 
        try %{ execute-keys -draft Z jxs (^\h*\\start)<ret> d <esc> i \stop <esc>}
        # No need to close define group. It inherits most arguments from setup. if setup is needed then presumably the same method applies as here save the arguments that follow as it applies to the rest
        try %{ execute-keys -draft Z jxs (^\h*\\define\.*[^\[.*\]\n]+)<ret> d<esc>}
        try %{ execute-keys -draft Z jxs (^\h*\\setup\.*[^\[.*\]\n]+)<ret> d<esc>}
        # removing trailing \[ \] after the groups/environments so is not processed by the engine
        try %{ execute-keys -draft Z jxs (\h*\[.*\])<ret> d <esc> i <esc>}
    }
}

}


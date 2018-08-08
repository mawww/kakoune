hook global BufCreate .*(sway|i3)/config %{
    set buffer filetype i3
}

add-highlighter shared/i3 regions
add-highlighter shared/i3/code default-region group
add-highlighter shared/i3/double_string region %{"} %{"} group
add-highlighter shared/i3/single_string region %{'} %{'} group
add-highlighter shared/i3/exec region %{(?<=exec)} "$" fill string
add-highlighter shared/i3/comment region "#" "$" fill comment

add-highlighter shared/i3/double_string/ fill string
add-highlighter shared/i3/single_string/ fill string

# Symbols
add-highlighter shared/i3/code/ regex "[+|→]" 0:operator
add-highlighter shared/i3/code/ regex "\$\w+" 0:variable

# keys
add-highlighter shared/i3/code/ regex "\b(Shift|Control|Ctrl|Mod1|Mod2|Mod3|Mod4|Mod5|Mode_switch|Return|Escape|Print)\b" 0:value

# keywords
add-highlighter shared/i3/code/ regex "\b(bind|bindcode|bindsym|assign|new_window|popup_during_fullscreen|font|floating_modifier|default_orientation|workspace_layout|for_window|focus_follows_mouse|bar|position|colors|output|tray_output|workspace_buttons|workspace_auto_back_and_forth|binding_mode_indicator|debuglog|floating_minimum_size|floating_maximum_size|force_focus_wrapping|force_xinerama|force_display_urgency_hint|hidden_state|modifier|new_float|shmlog|socket_path|verbose|mouse_warping|strip_workspace_numbers|focus_on_window_activation|no_focus|set|mode|set_from_resource)\b" 0:keyword
# function keywords
add-highlighter shared/i3/code/ regex "\b(exit|reload|restart|kill|fullscreen|global|layout|border|focus|move|open|split|append_layout|mark|unmark|resize|grow|shrink|show|nop|rename|title_format|sticky)\b" 0:function
add-highlighter shared/i3/code/ regex "\b(exec|exec_always|i3bar_command|status_command)\b" 0:function
# " these are not keywords but we add them for consistency
add-highlighter shared/i3/code/ regex "\b(no|false|inactive)\b" 0:value

# values
add-highlighter shared/i3/code/ regex "\b(1pixel|default|stacked|tabbed|normal|none|tiling|stacking|floating|enable|disable|up|down|horizontal|vertical|auto|up|down|left|right|parent|child|px|or|ppt|leave_fullscreen|toggle|mode_toggle|scratchpad|width|height|top|bottom|client|hide|primary|yes|all|active|window|container|to|absolute|center|on|off|ms|smart|ignore|pixel|splith|splitv|output|true)\b" 0:value
add-highlighter shared/i3/code/ regex "\b(next|prev|next_on_output|prev_on_output|back_and_forth|current|number|none|vertical|horizontal|both|dock|hide|invisible|gaps|smart_gaps|smart_borders|inner|outer|current|all|plus|minus|no_gaps)\b" 0:value

# double-dash arguments
add-highlighter shared/i3/code/ regex "--(release|border|whole-window|toggle|no-startup-id)" 0:attribute

# color
add-highlighter shared/i3/double_string/ regex "#[0-9a-fA-F]{6}" 0:value
add-highlighter shared/i3/single_string/ regex "#[0-9a-fA-F]{6}" 0:value

# attributes
add-highlighter shared/i3/code/ regex "client\.(background|statusline|background|separator|statusline)" 1:attribute
add-highlighter shared/i3/code/ regex "client\.(focused_inactive|focused|unfocused|urgent|inactive_workspace|urgent_workspace|focused_workspace|active_workspace|placeholder)" 1:attribute

hook -group i3-highlight global WinSetOption filetype=i3 %{ add-highlighter window/i3 ref i3 }
hook -group i3-highlight global WinSetOption filetype=(?!i3).* %{ remove-highlighter window/i3 }

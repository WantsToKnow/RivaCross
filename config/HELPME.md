l1: crosshair character
l2: position x
l3: position y
l4: size
l5: color in #RRGGBB or #RRGGBBAA hex format
l6: hotkey mode (optional; 0=disabled, 1=hold, 2=toggle)
l7: hotkey VK code (optional; decimal; mouse left=1 right=2 middle=4 X1=5 X2=6; F1=112 F2=113 ... F12=123)
l8: hotkey modifier mask (optional; 0=none, 1=Ctrl, 2=Shift, 4=Alt)
l9: master toggle VK code (optional; 0=disabled)
l10: master toggle modifier mask (optional; 0=none, 1=Ctrl, 2=Shift, 4=Alt)
l11: fine X offset (optional; signed RTSS units or OCAT pixels)
l12: fine Y offset (optional; signed RTSS units or OCAT pixels)
l13: second hotkey VK code (optional; 0=disabled; either hotkey triggers)
l14: second hotkey modifier mask (optional; 0=none, 1=Ctrl, 2=Shift, 4=Alt)

The second hotkey shares the main hotkey's hold/toggle mode. In hold mode,
the crosshair stays visible while either binding is held. In both GUIs,
master OFF forces the crosshair hidden; master ON resets it to visible in
toggle mode, even if a hotkey previously hid it. Hold mode always requires
either hotkey to be held, including immediately after master ON. Old presets load with
the second hotkey disabled.

Note: Pressing B in the TUI clears the Windows clipboard (3-press confirm).

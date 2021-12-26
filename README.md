# Wii-donut.c
A port of donut.c to the Wii


### Controls

| Action | Wiimote | Gamecube Controller | Console Buttons |
|---|---|---|---|
| Exit (return to HBC/loader) | Home | Start/Pause | Reset |
| Power off console | Power | N/A | Power |
| Toggle debug mode | 1 | Y | N/A |
| Adjust rotation speed (A axis) | D-pad left/right | D-pad left/right | N/A |
| Adjust rotation speed (B axis) | D-pad up/down | D-pad up/down | N/A |
| Reset rotation speed (both axis) | 2 | X | N/A|

### Building

Requires devkitPro/devkitPPC

No external dependencies.

To build for Wii run `make`

To build for Gamecube run `make -f Makefile.gc`

### Acknowledgments

a1k0n for the original basis and explaing the math behind it.

An anonymous Github gister for making the code readable and portable.

The gist this was directly ported from: https://gist.github.com/anonymous/1073922

a1k0n's post: https://www.a1k0n.net/2011/07/20/donut-math.html

The youtube video which brought this to my attention: https://www.youtube.com/watch?v=DEqXNfs_HhY

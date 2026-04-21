# SuperSeriousFinalDotEngine

compile in scripts/build_linux.sh
RGFW wayland seems to block for 1 second on the event loop each time unless there is an actual event and breaks the loop.
Defaulting to using x11 for now.

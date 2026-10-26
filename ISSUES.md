# Known issues / to investigate

## Cursor doesn't revert on landing page
- Log: `WARNING: MacWindowManager::replaceCursor(): Custom cursor signified but not provided!`
- Repro: on the landing page, move the mouse out of the text input box and back over other UI elements.
- Expected: cursor returns to the Star Trek chevron (custom cursor from the cast). Likely the sprite/cast cursor lookup returns a "custom" flag with no cursor data after leaving the text field's I-beam state.
- Status: open, not yet investigated.

## Systems screen (open)
- Defiant: layers not sharing zoom/offset, stray orange nacelle icon, part picture offset from box.
- Resource Budget value and hull-name text blank/wrong.
- Menus displaced from start position after use.

## Not yet tested / not implemented
- QD3D assembly view; Crew, Fleet, Mission screens.

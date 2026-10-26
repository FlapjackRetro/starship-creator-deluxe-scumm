# Known issues / to investigate

## Cursor doesn't revert on landing page
- Log: `WARNING: MacWindowManager::replaceCursor(): Custom cursor signified but not provided!`
- Repro: on the landing page, move the mouse out of the text input box and back over other UI elements.
- Expected: cursor returns to the Star Trek chevron (custom cursor from the cast). Likely the sprite/cast cursor lookup returns a "custom" flag with no cursor data after leaving the text field's I-beam state.
- Status: open, not yet investigated.

## Systems screen (open)
- (fixed in v12) Z buttons reversed, part picture offset from box, naming-text rotation.
- Still check: other classes' alignment, Defiant cutaway scale vs original.
- Resource Budget value and hull-name text blank/wrong.
- Menus displaced from start position after use.

## Not yet tested / not implemented
- QD3D assembly view; Crew, Fleet, Mission screens.

## Systems: selecting a part (v15)
- Fixed: `set the rect of member` on text/field members was read-only, misaligning the part-info title ("Class 7").
- Improved: the screen is blank for a moment while the part-info panel rebuilds (the game's own strip animation + ~90 `updateStage` calls). Unmasked background-transparent bitmaps now use a fast blit path, cutting the blank from ~2 s to <1 s on a slow software renderer. Original shows progressive redraw; ours is a short blank.

## v16: `play frame sprite(n)` (root cause of slow Systems redraws)
`cPartInfo.mShowDisplay` runs `play frame sprite(pflashspr)`, which is a Flash-sprite "play" command, not a movie jump. ScummVM treated it as a frame jump, re-running endSprite/beginSprite/mClose/mArrive for the whole screen on every selection (seconds of blank). It is now a no-op for sprite references. Flash timeline *playback* (as opposed to static frames) is still unimplemented.

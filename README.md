# Starship Creator Deluxe on ScummVM

Work-in-progress port of *Star Trek: Starship Creator Deluxe* (1998, Macromedia Director 5/6) using ScummVM's Director engine, avoiding the bundled QuickTime 3.0.

- Base: ScummVM master @ `2359eb49d56d292117396cb81d41c3fd7171434d`
- Apply: `cd scummvm && git apply ../patches/director-swa-v9.patch` (full patch; includes the files in `new-files/`)
- Run from the parent "Starship Creator" dir: `scummvm --start-movie=ShipMain/Creator.dxr director:trekship`

## Status (patch v9)
Working: startup, class select, structure, naming (hull text via new Flash renderer), palette fixes, SWA audio stubs, Lingo fixes.
Not working: Systems screen cutaway/part pictures (Flash layers created but frames/state not selected; see `mUpdateShip`/`mHandleLayers`), QuickDraw 3D assembly view, displaced menus on Systems, Crew/Fleet/Mission untested.

`new-files/flashxtra.*` is a minimal SWF v1 renderer for the Flash Asset Xtra members. `tools/` has the Xvfb click-driving scripts used for testing.

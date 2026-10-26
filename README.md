# Starship Creator Deluxe on ScummVM

Work-in-progress port of *Star Trek: Starship Creator Deluxe* (1998, Macromedia Director 5/6) using ScummVM's Director engine, avoiding the bundled QuickTime 3.0.

- Base: ScummVM master @ `2359eb49d56d292117396cb81d41c3fd7171434d`
- Apply: `cd scummvm && git apply ../patches/director-swa-v14.patch` (full patch; includes the files in `new-files/`)
- Run from the parent "Starship Creator" dir: `scummvm --start-movie=ShipMain/Creator.dxr director:trekship`

## Status (patch v10)
Working: startup, class select, structure, naming (hull text via new Flash renderer), palette fixes, SWA audio stubs, Lingo fixes.
Working too: Systems screen cutaway and part picture (Flash layers, transparency, view/registration properties). Still missing: Systems text fields (Resource Budget value, status line), hull-name text oddities, QuickDraw 3D assembly view, Crew/Fleet/Mission untested.

`new-files/flashxtra.*` is a minimal SWF v1 renderer for the Flash Asset Xtra members. `tools/` has the Xvfb click-driving scripts used for testing.

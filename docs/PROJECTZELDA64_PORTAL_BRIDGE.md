# ProjectZelda64 Portal Bridge

This is an experimental integration point for the separate `ProjectZelda64` host project.

## Current behavior

When the CVar below is enabled, Shipwright writes a portal event file after OoT loads the Happy Mask Shop entrance:

```text
gProjectZelda64.EnableOoTPortals = 1
```

Trigger:

```text
ENTR_HAPPY_MASK_SHOP_0 = 0x0530
```

Output file:

```text
projectzelda64_portal_event.json
```

Event payload:

```json
{
  "schema": 1,
  "sourceGame": "oot",
  "event": "oot.enter_happy_mask_shop",
  "sourceEntrance": "ENTR_HAPPY_MASK_SHOP_0",
  "sourceEntranceIndex": 1328,
  "targetGame": "mm",
  "targetPortal": "mm.clock_town.clock_tower_door_exterior"
}
```

## Why this does not redirect yet

This first bridge is intentionally safe. It detects and emits the portal event without changing vanilla Shipwright scene loading. ProjectZelda64 can use this to prove file-based host communication before we add any behavior that suppresses the normal Happy Mask Shop load.

The future redirect version should intercept the transition before `gSaveContext.entranceIndex` is committed, suspend OoT in the host, and boot or resume 2S2H at Clock Town / Clock Tower Door exterior.

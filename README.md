# Arcane Sector
A Dragon Sector CTF old-school RPG Game.

## Design
Arcane Sector is a multiplayer old-school 2.5D RPG game. Almost all logic for the game is server-side, and each team gets their own server (Python). The client is implemented in C++ (it's not a setup for a pwn challenge though, C++ is chosen for speed, so the implementation should be as flawless as possible) with an API-agnostic graphics engine (i.e. all graphics are done on frame buffers in memory), and should have at least two versions - <S></S>DL and websock/webRTC.

Initially the players get access only to the web version (video/audio/keyboard/mouse is proxied) and have to get access to a `backup.zip` file containing the client + an incomplete server. Only then MOST of the challenges are solvable.

## Glossary

   * **PC** - Player Character

## Visuals

The game uses a pseudo-3D view typical for old school RPGs like Ishar, Eye of Beholder, Dungeon Master, etc. The pseudo-3D is rendered by an actual software 3D engine, which supports some (but not all) typical 3D operations. For example it can do fog and texturing, but can render only certain types of axis-aligned quads - this is actually enough to render a game of this type while keeping the code relatively short (the whole 3D stuff is actually just 350 lines of code).

The initial idea (that probably will be kept) is that the 3D view will be rendered in 428 x 240 pixels, but will be upscaled to whatever the actual window size (with pixelization).

Some random technicalities about the pseudo-3D:

    * The 3D coordinates are in meters - all the math has been calibrated for this.
    * The ground level is on Y=0m, and the player's eyes are 1.8m above the ground (that's actually -1.8m, since everything above the ground is expressed in negative numbers).
    * The player stands a short step (0.5m) behind the tile center - this actually gives a nice view of the current tile and enough space to e.g. render items on it.

### Rendering order

There are 5 stages of rendering, 4 of which have Z-buffer filled but not checked and cover the floors/ceilings, and the last one which fully uses the Z-buffer and handles all and any vertical surfaces (and also is rendered in reverse order - from from to back - to take advantage of the Z-buffer).

The list below contains information on what is rendered in which stage (note that stage N+1 is rendered on top of stage N).

    * **Stage 0**: Water.
    * **Stage 1**: Grassland, forest ground, wood floor.
    * **Stage 2**: Sand, Rocky roads.
    * **Stage 3**: Dirt roads.
    * **Stage 4**: Building / mountain walls, trees, items.

## Map

Technically there are two maps kept in sync - the server-side map, and the client-side map, both of which are generated using `export-world.py` script from the Tiled (mapeditor.org) map format (with additional preprocessing done on the export).

The common part is a 512 by 768 tile map, where each tile on the map represents a 5m x 5m square (so the main map has about 2.5km x 2.5km in size, or 6.3km<sup>2</sup> of area).

As mentioned in the Visuals section, the player will see the world standing in the middle of a tile (actually half a meter back from the middle), looking in either North, South, West or East direction. We assume that North-South is the Y axis, with North being on the lower values and South on the higher. Similarly West-East is the X axis, with West being lower values and East higher. The tile in the North-West corner of the map is (0, 0).

The full map size is 512 x 768 tiles, with the upper 512 x 512 being the main terrain map, and the bottom 512 x 256 (aka supplementary part) being used for insides of buildings, caves, etc (so entering a building through the door is actually teleporting to the supplementary part).

The client-side map contains additional 8-bit of information per tile about the tile variant (this is tile specific, e.g. which forest tile to use) - this information is automatically generated during the export phase base on a set of preprocessors. Additionally selected static item information (door, cave entry, etc) is also added these 8 bits for some tiles (building walls, cave walls, etc).

The server-side in addition to the base tile plane also contains two lists of objects:

    * The "items" list contains whereabouts of static items (like signs, doors, decorations, etc).
    * The "system objects" list contains information about location-bound system processors/event generators like e.g. monster/item spawns.

### Client-side map additional information

This part contains documentation related to the nature of the 8 bits of additional data in the client-side map file.

#### Forest

    * Bits 0-2: Number of trees in the tile (0 means one tree, 4 means five trees).
    * Bits 3-5: Slot set selector (tree position randomization seed basically).
    * Bits 6-7: Type of forest (pine trees, leaf trees, dead trees or a mix).

## Protocol

### Common parts
Each an every packet begins with these three fields:

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | uint32_t     | sz         | Packet length (excluding the common part)
| 4            | char[4]      | chunk_id   | Chunk ID (4 capital letters)
| 8            | uint64_t     | packet_id  | Response to this packet (if any) will have the same value here
| **10**       | ← **total size**

The packet length cannot be larger than 1 MB.
Note that the packet_id is just echoed in replies (if any) by the server. There is no sanitization needed.

Apart from that the following types are used:

#### string
A data+length string.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | uint16_t     | sz         | String length
| 2            | char[sz]     | text       | String data
| **2+sz**     | ← **total size**

#### item
Any item in the game. A special id `~0` means "no item" (e.g. empty inventory slot, etc) - in this case no other fields exist in the packet.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | uint64_t     | id         | Item unique ID (~0 is a non-existing item)
| 8            | uint8_t      | movable    | 0 - immobile, 1 - movable / can be taken
| 9            | string       | gfx_id     | Which graphics to use to display it
| ?            | string       | name       | Item name (will be displayed as is in the client)
| **?**        | ← **total size**

#### mob
Any mobile character in the game (PC, NPC).

If `visible` is set to 0, the pos_x/pos_y/gfx_id/name fields will NOT be included in the packet.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | uint8_t      | type       | (ignored)
| 1            | uint8_t      | visible    | If 0, it's a signal to the client to stop rendering the mob.
| 2            | uint64_t     | id         | Player or NPC ID in the world
| 10           | uint16_t     | pos_x      | X coordinate of NPC
| 12           | uint16_t     | pos_y      | Y coordinate of NPC
| 14           | string       | gfx_id     | Which graphics to use to display it
| ?            | string       | name       | Name to display on hover
| **14+?**     | ← **total size**

### Login (C→S) "ENTR"
Sent at the very beginning of communication.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | char[32]     | passwd     | Password string
| 20           | uint8_t      | player_id  | Player ID (max 256 PCs)
| **21**       | ← **total size**

### Create PC Request (S→C) "NOPC"
No PC was found, so it needs to be set up now.

*This packet has no additional data.*

### Create PC Response (C→S) "MYPC"
This packet can only be sent in response to NOPC, and should kill the connection in all other cases.

The name has to be at most 32, and the portrait number will be limited to whatever number of portraits we will have available (probably 2).

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | char[32]     | pc_name    | PC's name
| 20           | uint8_t      | portrait   | Portrait to be used.
| **21**       | ← **total size**

### Ready to play the game (S→C) "GAME"
Login and character creation (if any) were successful and the game can begin. Server will transmit a set of standard state packets after this one.

*This packet has no additional data.*

### PC move request (C→S) "MOVE"
Player requests to move in the direction they are facing. Server will check if there are at most 10 moves per second (but client will limit it to 5).

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | uint8_t      | direction  | 0-3 mapped as Forward, Backward, Strafe Left, Strafe Right
| **1**       | ← **total size**


### PC changes looking direction (C→S) "DIRE"
PC changes direction they are facing.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | uint8_t      | direction  | 0-3 mapped as North, South, West, East
| **1**       | ← **total size**

### PC position (S→C) "POSI"
This packet serves a dual purpose - in case of a move request it confirms the current position (if the move was successful), and in other cases it updates the current position.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | uint16_t     | pos_x      | X coordinate of PC
| 2            | uint16_t     | pos_y      | Y coordinate of PC
| 4            | uint8_t      | direction  | 0-3 mapped as North, South, West, East
| **5**        | ← **total size**

### Display text (S→C) "TEXT"
For messages, etc. The length of the packet determines the string size. Default color is chosen by the client.

The text can contain the following special characters:

   * `\x10` - `\x1f` - Colors (in the order of ANSI escape codes [Ubuntu color scheme](https://en.wikipedia.org/wiki/ANSI_escape_code#3/4_bit)).
   * `\x0f` - Default color (as chosen by the client).
   * `\n` - New line + carriage return.
   * `\xff` - Next byte after this character is a spell byte.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | char[sz]     | text       | Text
| **sz**       | ← **total size**

Note: If I ever add another field, I need to change char[sz] to string for the sake of sanity.

### Basic player info (S→C) "INFO"
Basic PC info like HP, sent every time any info changes.

Note that `name` can be empty - this means "no change in name".

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | uint16_t     | hp         | Current health points
| 2            | uint16_t     | hp_max     | Max health points
| 4            | uint16_t     | mana       | Current mana points
| 6            | uint16_t     | mana_max   | Max mana points
| 8            | string       | name       | PC's own name
| **8+?**      | ← **total size**

### PC's inventory (S→C) "INVT"
Player's inventory and equipment.

Note: If inventory or equipment size change, it needs to be updated in code (packets.py PacketSC_INVT and ...).

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | item[8]      | inventory  | Inventory slots
| ?            | item[2]      | equipment  | Mapped as Left Hand, Right Hand.
| **?**        | ← **total size**

### Mobile's update information (S→C) "MOBS"
Sent to update the client about the whereabouts of mobs (NPC, other players).

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | uint16_t     | count      | Number of mobs updated.
| ?            | mob[count]   | moblist    | List of updated mobs.
| **?**        | ← **total size**

### Play an animation on a mobile (S→C) "ANIM"
Play an animation on a PC / NPC. Animation details are known to the client.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | uint8_t      | type       | 0 - PC, 1 - NPC
| 1            | uint64_t     | id         | Player or NPC ID in the world
| 9            | uint8_t      | anim       | Animation to play

### Mobile's update information (S→C) "GRND"
Full info about items lying on the ground in a given position. Server will update the client only about items nearby.

| Offset (hex) | Type / Size     | Name       | Description
| ------------ | --------------- | ---------- | ----------------------------------------
| 0            | uint8_t         | count      | Number of updated ground positions (should be non-zero)
| 1            | itemlist[count] | lists      | Lists with items.
| **5+?**      | ← **total size**

#### Subpacket: itemlist
Item list at a given position.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | uint8_t      | count      | Number of items in this position.
| 1            | uint16_t     | pos_x      | X coordinate of ground
| 3            | uint16_t     | pos_y      | Y coordinate of ground
| ?            | item[count]  | items      | Items (if any)
| **5+?**      | ← **total size**

### Hold item request (C→S) "HOLD"
Pick up an item (the server has to figure out if the item is within the reach of the client and whether it's movable at all). Note that the item can be on the ground, in the inventory, or equipped.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | uint64_t     | item_id    | Item the client wants to pick up.
| **8**        | ← **total size**

### Drop held item (C→S) "DROP"
The client wants to drop the held item in the inventory, in the equipment or on the ground.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 8            | uint8_t      | dst        | Destination mapped as: 0-7 inventory slots, 8-9 left and right hand (equipment), or (any other value) ground where the player is at.
| **8**        | ← **total size**

### Holding (S→C) "HLDI"
The client is holding the item and should display the item as the mouse cursor.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | item         | item       | Item held
| **8**        | ← **total size**

### Use item (C→S) "USEI"
Use a given item (right-clicking on an item does this). For example attacking with a sword is actually using the sword.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | uint64_t     | item_id    | Item to use
| **8**        | ← **total size**

### Select request (S→C) "SLCT"
The client it supposed to allow the user to select something (item, mob or ground). Note that the packet_id is important in this packet.

*This packet has no additional data.*

### Select response (C→S) "THIS"
The selected item / mob or ground. The packet_id has to match.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | uint8_t      | type       | Mapped to Item, Mob, Ground, Empty slot (in this order).
| 1            | uint64_t     | id         | ID of the selected Item or Mob, for Empty slot it's 0-7 (inventory slots), 8-9 left and right hand (equipment); ignored for ground.
| **9**        | ← **total size**

### Cast a spell (C→S) "CAST"
A request to cast a spell.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | uint8_t[8]   | spell      | Spell symbols.
| **8**        | ← **total size**

### Speak (C→S) "SAYS"
Basically a chat message. Length of the packet determines text size. Server side needs to enforce max message length, which is 256 characters, and remove all and any non-ASCII characters, this includes all the runes and color tags, as well as new lines and tabs.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            | char[sz]     | text       | Text
| **sz**       | ← **total size**

### Goodbye (C→S) "GBYE"
Sent when the client is exiting. The server should gracefully close the connection at this point. This is somewhat a workaround for the problem of net.join() hanging for 60 seconds on client-initiated exit (might be related to not thinking about this when implementing packet parsing and then not wanting to re-write the networking code - ups). So it's basically a server-assisted goodbye.

### Ping (C→S) "PING"
Sent every 30 seconds by the client.

*This packet has no additional data.*

### Pong (S→C) "PONG"
Sent in reply to PING.

*This packet has no additional data.*

### Template entry
Just a template entry for me to use while filling the doc.

| Offset (hex) | Type / Size  | Name       | Description
| ------------ | ------------ | ---------- | ----------------------------------------
| 0            |              |            |
| 0            |              |            |
| 0            |              |            |
| 0            |              |            |
| 0            |              |            |
| **1**        | ← **total size**

## Thin client packet protocol

    * TC - Thin Client
    * C - Client/Renderer


Websocket based. Packet IDs:

    * (C→TC) Anything with lowest bit 0 - keyframe (including the ID!)
    * (C→TC) 0x01 - diff frame
    * (C→TC) 0x03 - cursor (16-bit int w, h, hot_x, hot_y, followed by image data; if w and h are 0 it means default cursor)



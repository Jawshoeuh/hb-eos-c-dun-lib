#include <pmdsky.h>
#include <cot.h>
#include "hb-eos-c-dun-lib.h"

#ifndef NDEBUG
    #define HB_DEBUG_PRINT_ABS_FLOOR(abs_floor) \
        DebugPrint(0, "%d", (abs_floor)->num_rooms);\
        for (int16_t i = 0; i < (abs_floor)->num_rooms; ++i) {\
            DebugPrint(0, "Room %d: Start (%d, %d) End (%d, %d)", i, (abs_floor)->room_start_pos[i][HB_X_POS], (abs_floor)->room_start_pos[i][HB_Y_POS], (abs_floor)->room_end_pos[i][HB_X_POS], (abs_floor)->room_end_pos[i][HB_Y_POS]);\
        }\
        char row[HB_DUNGEON_MAX_X + 1]; \
        for (int16_t y = 0; HB_DUNGEON_MAX_Y > y; ++y) {\
            row[0] = '\0'; \
            for (int16_t x = 0; HB_DUNGEON_MAX_X > x; ++x) {\
                switch ((abs_floor)->tiles[x][y].terrain_type) {\
                    default: strcat(row, "?"); break;\
                    case TERRAIN_WALL: strcat(row, "#"); break;\
                    case TERRAIN_NORMAL: strcat(row, "."); break;\
                    case TERRAIN_SECONDARY: strcat(row, "~"); break;\
                    case TERRAIN_CHASM: strcat(row, " "); break;\
                }\
            }\
            DebugPrint(0, "%s", row);\
        }
#else
    #define HB_DEBUG_PRINT_ABS_FLOOR(abs_floor)
#endif

inline bool IsCoordinateValid(const int16_t x, const int16_t y) {
    if (HB_DUNGEON_MAX_X <= x || HB_DUNGEON_MAX_Y <= y || 0 > x || 0 > y) {
        return false;
    }
    return true;
}

inline int CalcActualRoomDensity(int room_density) {
    if (room_density < 0) {
        room_density = abs(room_density);
    } else {
        room_density += DungeonRandInt(3);
    }
    if (room_density > HB_MAX_ROOMS) {
        room_density = HB_MAX_ROOMS;
    } else if (room_density < 2) {
        room_density = 2;
    }
    return room_density;
}

inline bool IsAreaValid(const int16_t x0, const int16_t y0, const int16_t x1, const int16_t y1) {
    if (x0 > x1 || y0 > y1) {
        return false;
    }
    if (HB_DUNGEON_MAX_X <= x0 || HB_DUNGEON_MAX_Y <= y0 || 0 > x0 || 0 > y0) {
        return false;
    }
    if (HB_DUNGEON_MAX_X <= x1 || HB_DUNGEON_MAX_Y <= y1 || 0 > x1 || 0 > y1) {
        return false;
    }
    return true;
}

inline bool DoesAreaOverlap(const int16_t x0a, const int16_t y0a, const int16_t x1a, const int16_t y1a, const int16_t x0b, const int16_t y0b, const int16_t x1b, const int16_t y1b) {
    if (x0a > x1b || x0b > x1a || y0a > y1b || y0b > y1a) {
        return false;
    }
    return true;
}

inline bool IsCoordinateInArea (const int16_t x, const int16_t y, const int16_t x0, const int16_t y0, const int16_t x1, const int16_t y1) {
    if (x < x0 || x >= x1 || y < y0 || y >= y1) {
        return false;
    }
    return true;
}

inline bool IsDirectionCardinal(const enum direction_id dir) {
    return (dir == DIR_DOWN || dir == DIR_RIGHT || dir == DIR_UP || dir == DIR_LEFT);
}

inline enum direction_id MakeDirectionCardinal(enum direction_id dir) {
    // If not a valid cardinal direction... make it one.
    if (DIR_DOWN > dir || DIR_DOWN_LEFT < dir) {
        dir = 2 * DungeonRandInt(4); // DIR_DOWN (0), DIR_RIGHT (2), DIR_UP (4), DIR_LEFT (6)
    }
    // This is equivalent to (dir == DIR_DOWN_RIGHT || dir == DIR_UP_RIGHT ||
    // dir == DIR_UP_LEFT || dir == DIR_DOWN_LEFT
    if (dir & 0x01) { // 0b00000001
        if (DungeonRandInt(2)) {
            dir += 1;
        } else {
            dir -= 1;
        }
        dir = dir & 0x07; // 0b00000111
    }
    return dir;
}

inline enum direction_id MakeDirectionDiagonal(enum direction_id dir) {
    // If not a valid diagonal direction... make it one.
    if (DIR_DOWN > dir || DIR_DOWN_LEFT < dir) {
        // DIR_DOWN_RIGHT (1), DIR_UP_RIGHT (3), DIR_UP_LEFT (5), DIR_DOWN_LEFT (7)
        dir = 2 * DungeonRandInt(4) + 1;
    }
    // This is equivalent to (dir == DIR_DOWN || dir == DIR_UP ||
    // dir == DIR_LEFT || dir == DIR_RIGHT
    if (!(dir & 0x01)) { // 0b00000001
        if (DungeonRandInt(2)) {
            dir += 1;
        } else {
            dir -= 1;
        }
        dir = dir & 0x07; // 0b00000111
    }
    return dir;
}

inline enum direction_id RandomCardinalDirection() {
    // DIR_DOWN (0), DIR_RIGHT (2), DIR_UP (4), DIR_LEFT (6)
    return 2 * DungeonRandInt(4);
}

inline enum direction_id RandomDiagonalDirection() {
    // DIR_DOWN_RIGHT (1), DIR_UP_RIGHT (3), DIR_UP_LEFT (5), DIR_DOWN_LEFT (7)
    return 2 * DungeonRandInt(4) + 1;
}

bool AddRoomToAbstractFloor(int16_t x0, int16_t y0, int16_t x1, int16_t y1, struct abstract_floor *abs_floor) {
    if (NULL == abs_floor || false == IsAreaValid(x0, y0, x1, y1) || HB_MAX_ROOMS <= abs_floor->num_rooms) {
        return false;
    }
    // Check if the area is already occupied by another room by checking the
    // start and end position of existing rooms which is faster than checking
    // every tile.
    int16_t existing_x0, existing_y0, existing_x1, existing_y1;
    for (uint32_t i = 0; i < abs_floor->num_rooms; ++i) {
        // Add a buffer of 2 tiles around existing rooms to prevent new rooms
        // from being directly adjacent to existing rooms.
        existing_x0 = abs_floor->room_start_pos[i][HB_X_POS] - 2;
        existing_y0 = abs_floor->room_start_pos[i][HB_Y_POS] - 2;
        existing_x1 = abs_floor->room_end_pos[i][HB_X_POS] + 2;
        existing_y1 = abs_floor->room_end_pos[i][HB_Y_POS] + 2;
        if (DoesAreaOverlap(x0, y0, x1, y1, existing_x0, existing_y0, existing_x1, existing_y1)) {
            return false;
        }
    }


    for (int32_t x = x0; x < x1; ++x) {
        for (int32_t y = y0; y < y1; ++y) {
            abs_floor->tiles[x][y].terrain_type = TERRAIN_NORMAL;
            abs_floor->tiles[x][y].is_room = true;
        }
    }
    uint32_t room_id = abs_floor->num_rooms;
    abs_floor->room_start_pos[room_id][HB_X_POS] = x0;
    abs_floor->room_start_pos[room_id][HB_Y_POS] = y0;
    abs_floor->room_end_pos[room_id][HB_X_POS] = x1;
    abs_floor->room_end_pos[room_id][HB_Y_POS] = y1;
    abs_floor->num_rooms += 1;

    // Bound check.
    int block_x_start = x0 - 1;
    int block_y_start = y0 - 1;
    int block_x_end = x1;
    int block_y_end = y1;
    for(int x = x0; x < x1; ++x) {
        abs_floor->tiles[x][block_y_start].x_connect_blocked = true;
        abs_floor->tiles[x][block_y_end].x_connect_blocked = true;
    }
    for(int y = y0; y < y1; ++y) {
        abs_floor->tiles[block_x_start][y].y_connect_blocked = true;
        abs_floor->tiles[block_x_end][y].y_connect_blocked = true;
    }
    return true;
}

void InitAbstractFloor(struct abstract_floor *abs_floor) {
    if (NULL == abs_floor) {
        return;
    }
    for (uint32_t x = 0; HB_DUNGEON_MAX_X > x; ++x) {
        for (uint32_t y = 0; HB_DUNGEON_MAX_Y > y; ++y) {
            abs_floor->tiles[x][y].terrain_type = TERRAIN_WALL;
            abs_floor->tiles[x][y].is_room = false;
            abs_floor->tiles[x][y].room_blocked = false;
            abs_floor->tiles[x][y].x_connect_blocked = false;
            abs_floor->tiles[x][y].y_connect_blocked = false;
            abs_floor->tiles[x][y].item_spawn = false;
            abs_floor->tiles[x][y].custom_tile_info = false;
        }
    }
    abs_floor->num_rooms = 0;
}

void ApplyAbstractFloorToFloor(const struct abstract_floor *abs_floor, struct dungeon_grid_cell *grid, const struct floor_properties *floor_props) {
    if (NULL == abs_floor || NULL == grid || NULL == floor_props) {
        return;
    }
    for (uint32_t x = 0; HB_DUNGEON_MAX_X > x; ++x) {
        for (uint32_t y = 0; HB_DUNGEON_MAX_Y > y; ++y) {
            struct tile *tile = GetTileSafe(x, y);
            tile->terrain_type = abs_floor->tiles[x][y].terrain_type;
            if (abs_floor->tiles[x][y].item_spawn) {
                tile->spawn_or_visibility_flags.spawn.f_item = true;
            }
        }
    }
    uint32_t i;
    for (i = 0; i < abs_floor->num_rooms; ++i) {
        CreateRoomInCell(
            abs_floor->room_start_pos[i][HB_X_POS],
            abs_floor->room_start_pos[i][HB_Y_POS],
            abs_floor->room_end_pos[i][HB_X_POS],
            abs_floor->room_end_pos[i][HB_Y_POS],
            i,
            &(grid[i]),
            floor_props->room_flags
        );
    }
    while (i < HB_MAX_ROOMS) {
         grid[i].is_invalid = true;
         ++i;
    }
}

bool CanCarveAbstractTile(const int16_t x, const int16_t y, struct abstract_floor *abs_floor, const enum direction_id dir) {
    if (false == IsCoordinateValid(x, y)) {
        return false;
    }
    if (IsDirectionCardinal(dir) == false) {
        return false;
    }
    struct abstract_tile *curr_tile = &abs_floor->tiles[x][y];
    if ((dir == DIR_LEFT || dir == DIR_RIGHT) && curr_tile->x_connect_blocked) {
        return false;
    }
    if ((dir == DIR_UP || dir == DIR_DOWN) && curr_tile->y_connect_blocked) {
        return false;
    }
    return true;
}

bool TryCarveAbstractTile(const int16_t x, const int16_t y, struct abstract_floor *abs_floor, const enum direction_id dir) {
    struct abstract_tile *curr_tile = &abs_floor->tiles[x][y];
    if ((dir == DIR_LEFT || dir == DIR_RIGHT) && curr_tile->x_connect_blocked) {
        return false;
    }
    if ((dir == DIR_UP || dir == DIR_DOWN) && curr_tile->y_connect_blocked) {
        return false;
    }
    enum terrain_type terrain_type = curr_tile->terrain_type;
    if(TERRAIN_NORMAL != terrain_type) {
        abs_floor->tiles[x][y].terrain_type = TERRAIN_NORMAL;
    }
    abs_floor->tiles[x][y].terrain_type = TERRAIN_NORMAL;
    if (DIR_LEFT == dir || DIR_RIGHT == dir) {
        abs_floor->tiles[x][y + 1].x_connect_blocked = true;
        abs_floor->tiles[x][y - 1].x_connect_blocked = true;
        if (DIR_LEFT == dir) {
            abs_floor->tiles[x - 1][y + 1].x_connect_blocked = true;
            abs_floor->tiles[x - 1][y - 1].x_connect_blocked = true;
        } else {
            abs_floor->tiles[x + 1][y + 1].x_connect_blocked = true;
            abs_floor->tiles[x + 1][y - 1].x_connect_blocked = true;
        }
    } else if (DIR_UP == dir || DIR_DOWN == dir) {
        abs_floor->tiles[x + 1][y].y_connect_blocked = true;
        abs_floor->tiles[x - 1][y].y_connect_blocked = true;
        if (DIR_UP == dir) {
            abs_floor->tiles[x + 1][y - 1].y_connect_blocked = true;
            abs_floor->tiles[x - 1][y - 1].y_connect_blocked = true;
        } else {
            abs_floor->tiles[x + 1][y + 1].y_connect_blocked = true;
            abs_floor->tiles[x - 1][y + 1].y_connect_blocked = true;
        }
    }
    return true;
}

bool TryCarveAbstractTileSafe(const int16_t x, const int16_t y, struct abstract_floor *abs_floor, const enum direction_id dir) {
    if (false == IsCoordinateValid(x, y)) {
        return false;
    }
    if (IsDirectionCardinal(dir) == false) {
        return false;
    }
    if ((DIR_LEFT == dir || DIR_RIGHT == dir) && abs_floor->tiles[x][y].x_connect_blocked) {
        return false;
    }
    if ((DIR_UP == dir || DIR_DOWN == dir) && abs_floor->tiles[x][y].y_connect_blocked) {
        return false;
    }
    enum terrain_type terrain_type = abs_floor->tiles[x][y].terrain_type;
    if(TERRAIN_NORMAL != terrain_type) {
        abs_floor->tiles[x][y].terrain_type = TERRAIN_NORMAL;
    }
    if (DIR_LEFT == dir || DIR_RIGHT == dir) {
        if (HB_DUNGEON_MAX_Y - 1 > y) {
            abs_floor->tiles[x][y + 1].x_connect_blocked = true;
            if (DIR_LEFT == dir && 0 < x) {
                abs_floor->tiles[x - 1][y + 1].x_connect_blocked = true;
            }
            else if (HB_DUNGEON_MAX_X - 1 > x) { // DIR_RIGHT == dir
                abs_floor->tiles[x + 1][y + 1].x_connect_blocked = true;
            }
        }
        if(0 < y) {
            abs_floor->tiles[x][y - 1].x_connect_blocked = true;
            if (DIR_LEFT == dir && 0 < x) {
                abs_floor->tiles[x - 1][y - 1].x_connect_blocked = true;
            }
            else if (DIR_RIGHT == dir && HB_DUNGEON_MAX_X - 1 > x) {
                abs_floor->tiles[x + 1][y - 1].x_connect_blocked = true;
            }
        }   
    } else if (DIR_UP == dir || DIR_DOWN == dir) {
        if (HB_DUNGEON_MAX_X - 1 > x) {
            abs_floor->tiles[x + 1][y].y_connect_blocked = true;
            if (DIR_UP == dir && 0 < y) {
                abs_floor->tiles[x + 1][y - 1].y_connect_blocked = true;
            }
            else if (HB_DUNGEON_MAX_Y - 1 > y) { // DIR_DOWN == dir
                abs_floor->tiles[x + 1][y + 1].y_connect_blocked = true;
            }
        }
        if (0 < x) {
            abs_floor->tiles[x - 1][y].y_connect_blocked = true;
            if (DIR_UP == dir && 0 < y) {
                abs_floor->tiles[x - 1][y - 1].y_connect_blocked = true;
            }
            else if (HB_DUNGEON_MAX_Y - 1 > y) { // DIR_DOWN == dir
                abs_floor->tiles[x - 1][y + 1].y_connect_blocked = true;
            }
        }
    }
    return true;
}

void __attribute__((naked)) CreateRoomInCell(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t room_id, struct dungeon_grid_cell *cell, struct room_flags flags) {
    asm("stmdb sp!,{r4,r5,r6,r7,r8,r9,r10,lr}"); // 8 byte stack alignment
    asm("cmp   r0,r2"); // Check for correct order of arguments.
    asm("cmplt r1,r3");
    asm("ldmgeia sp!,{r4,r5,r6,r7,r8,r9,r10,pc}");
    asm("cmp   r0,#0x0"); // Check lower bounds.
    asm("cmpge r1,#0x0");
    asm("ldmltia sp!,{r4,r5,r6,r7,r8,r9,r10,pc}");
    asm("cmp   r2,#0x38"); // Check upper bounds.
    asm("cmplt r3,#0x20");
    asm("ldmgeia sp!,{r4,r5,r6,r7,r8,r9,r10,pc}");
    asm("ldrb  r8,[sp,#0x20]"); // Room #
    asm("mov   r4,r0");         // x0
    asm("mov   r9,r1");         // y0
    asm("mov   r6,r2");         // x1
    asm("mov   r7,r3");         // y1
    asm("mov   r1,#0x1");
    asm("ldr   r0,[sp,#0x24]"); // Cell Ptr
    asm("strb  r1,[r0,#0xA]");  // is_room = TRUE
    asm("strb  r1,[r0,#0xB]");  // is_connected = TRUE
    asm("strh  r4,[r0,#0x0]");  // Cell Start X
    asm("strh  r9,[r0,#0x2]");  // Cell Start Y
    asm("strh  r6,[r0,#0x4]");  // Cell End X
    asm("strh  r7,[r0,#0x6]");  // Cell End Y
    asm("create_room_in_cell_x_loop:");
    asm("mov   r5,r9");
    asm("create_room_in_cell_y_loop:");
    asm("mov   r0,r4");
    asm("mov   r1,r5");
    asm("bl    GetTileSafe");
    asm("add   r5,r5,#0x1"); // Y Iterate
    asm("ldrh  r1,[r0,#0x0]");
    asm("bic   r1,r1,#0x3");
    asm("orr   r1,r1,#0x1");
    asm("strh  r1,[r0,#0x0]");
    asm("strb  r8,[r0,#0x7]");
    asm("cmp   r5,r7");
    asm("blt   create_room_in_cell_y_loop");
    asm("add   r4,r4,#0x1"); // X Iterate
    asm("cmp   r4,r6");
    asm("blt   create_room_in_cell_x_loop");
    asm("ldr   r0,=FLOOR_GENERATION_STATUS");
    asm("ldr   r0,[r0,#0x18]");
    asm("cmp   r0,#0x0");
    asm("moveq r8,#0x0");
    asm("beq   create_room_in_cell_imperfection_check");
    asm("mov   r0,#0x64");
    asm("bl    DungeonRandInt");
    asm("cmp   r0,#0x50");
    asm("movge r8,#0x0");
    asm("movlt r8,#0x1");
    asm("create_room_in_cell_imperfection_check:");
    asm("ldrb  r7,[sp,#0x28]");
    asm("tst   r7,#0x4"); // Check Imperfect Room Bit
    asm("moveq r9,#0x0");
    asm("beq   create_room_in_cell_store_check");
    asm("cmp   r8,#0x0");
    asm("mov   r9,#0x1");
    asm("beq   create_room_in_cell_store_check");
    asm("mov   r0,#0x64");
    asm("bl    DungeonRandInt");
    asm("cmp   r0,#0x32");
    asm("movlt r9,#0x0");
    asm("movge r8,#0x0");
    asm("create_room_in_cell_store_check:");
    asm("cmp   r8,#0x0");
    asm("cmpeq r9,#0x0");
    asm("ldmeqia sp!,{r4,r5,r6,r7,r8,r9,r10,pc}");
    asm("ldr   r0,[sp,#0x24]"); // Cell Ptr
    asm("strb  r8,[r0,#0x1D]"); // Secondary Structures
    asm("strb  r9,[r0,#0x1C]"); // Imperfect
    asm("ldmia sp!,{r4,r5,r6,r7,r8,r9,r10,pc}");
}

void __attribute__((naked)) CreateRoom(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t room_id) {
    asm("stmdb sp!,{r4,r5,r6,r7,r8,r9,lr}");
    asm("cmp   r0,r2"); // Check for correct order of arguments.
    asm("cmplt r1,r3");
    asm("ldmgeia sp!,{r4,r5,r6,r7,r8,r9,pc}");
    asm("cmp   r0,#0x0 "); // Check lower bounds.
    asm("cmpge r1,#0x0");
    asm("ldmltia sp!,{r4,r5,r6,r7,r8,r9,pc}");
    asm("cmp   r2,#0x38"); // Check upper bounds.
    asm("cmplt r3,#0x20");
    asm("ldmgeia sp!,{r4,r5,r6,r7,r8,r9,pc}");
    asm("ldrb  r8,[sp,#0x1C]"); // Room #
    asm("mov   r4,r0"); // x0
    asm("mov   r9,r1"); // y0
    asm("mov   r6,r2"); // x1
    asm("mov   r7,r3"); // y1
    asm("create_room_x_loop:");
    asm("mov   r5,r9");
    asm("create_room_y_loop:");
    asm("mov   r0,r4");
    asm("mov   r1,r5");
    asm("bl    GetTileSafe");
    asm("add   r5,r5,#0x1"); // Y Iterate
    asm("ldrh  r1,[r0,#0x0]");
    asm("bic   r1,r1,#0x3");
    asm("orr   r1,r1,#0x1");
    asm("strh  r1,[r0,#0x0]");
    asm("strb  r8,[r0,#0x7]");
    asm("cmp   r5,r7");
    asm("blt   create_room_y_loop");
    asm("add   r4,r4,#0x1"); // X Iterate
    asm("cmp   r4,r6");
    asm("blt   create_room_x_loop");
    asm("ldmia sp!,{r4,r5,r6,r7,r8,r9,pc}");
}

void __attribute__((naked)) MergeRoomsHorizontally(int16_t x, int16_t y, int16_t dx, struct dungeon_grid_cell *grid) {
    asm("stmdb sp!,{r4,r5,r6,r7,r8,r9,r10,r11,lr}");
    asm("add   r2,r2,r0");
    asm("mov   r12,#0x1E"); // DUNGEON_GRID_CELL_BYTES
    asm("ldr   r10,=DUNGEON_GRID_COLUMN_BYTES");
    asm("mul   r4,r0,r10"); // X Offset for First Cell
    asm("mul   r5,r1,r12"); // Y Offset for Both Cells
    asm("mul   r6,r2,r10"); // X Offset for Second Cell
    asm("add   r4,r4,r5");
    asm("add   r5,r5,r6");
    asm("add   r4,r3,r4"); // Cell 1 PTR
    asm("add   r5,r3,r5"); // Cell 2 PTR
    asm("ldrsh r6,[r4,#0x0]");  // Cell 1 Start X
    asm("mov   r0,r6");
    asm("ldrsh r7,[r5,#0x4]");  // Cell 2 End   X
    asm("ldrsh r11,[r4,#0x2]"); // Cell 1 Start Y
    asm("mov   r1,r11");
    asm("bl    GetTile"); // MergeRoomsVertically uses GetTile for room ID.
    asm("ldrb  r10,[r0,#0x7]"); // Save Room ID to give tiles.
    asm("ldrsh r0,[r5,#0x2]");  // Cell 2 Start Y
    asm("cmp   r0,r11");
    asm("movlt r11,r0");       // Determine Merged Cell New Start Y
    asm("ldrsh r9,[r4,#0x6]"); // Cell 1 End Y
    asm("ldrsh r1,[r5,#0x6]"); // Cell 2 End Y
    asm("cmp   r1,r9");
    asm("movgt r9,r1");        // Determine Merged Cell New End Y
    asm("merge_rooms_horizontally_x_loop:");
    asm("mov   r8,r11"); // Use r8 to iterate, r11 holds original start of Y
    asm("merge_rooms_horizontally_y_loop:");
    asm("mov   r0,r6");
    asm("mov   r1,r8");
    asm("bl    GetTileSafe");
    asm("add   r8,r8,#0x1"); // Iterate Y Loop
    asm("ldrh  r1,[r0,#0x0]");
    asm("bic   r1,r1,#0x3");
    asm("orr   r1,r1,#0x1");
    asm("strh  r1,[r0,#0x0]");
    asm("strb  r10,[r0,#0x7]");
    asm("cmp   r8,r9");
    asm("blt   merge_rooms_horizontally_y_loop");
    asm("add   r6,r6,#0x1"); // Iterate X Loop
    asm("cmp   r6,r7");
    asm("blt   merge_rooms_horizontally_x_loop");
    asm("mov   r0,#0x0");
    asm("mov   r1,#0x1");
    asm("strh  r7,[r4,#0x4]");  // Save New End   X
    asm("strh  r11,[r4,#0x2]"); // Save New Start Y
    asm("strh  r9,[r4,#0x6]");  // Save New End   Y
    asm("strb  r1,[r4,#0x12]"); // Cell 1 is_merged_room = TRUE
    asm("strb  r1,[r5,#0x12]"); // Cell 2 is_merged_room = TRUE
    asm("strb  r1,[r5,#0x11]"); // Cell 2 was_merged_into_other_room = TRUE
    asm("strb  r0,[r5,#0xB]");  // Cell 2 is_connected = FALSE 
    asm("ldmia sp!,{r4,r5,r6,r7,r8,r9,r10,r11,pc}");
}

/* WARNING: This function is extremely unsafe and can cause a stack overflow.
// Because of this, the function has been squished into
// Generate RecusiveBacktrackingFloor where it has been tested and known to be
// not cause a stack overflow by managing the stack abnormally.
void __attribute__((naked)) GenerateRecursiveBacktrackingMaze(int start_x, int start_y) {
    asm("stmdb sp!,{r4,r5,r6,r7,r8,r9,lr}"); // Warning: 8 byte stack alignment violated
    asm("mov r4,r0");
    asm("mov r5,r1");
    asm("mov r0,#0x4");
    asm("bl DungeonRandInt");
    asm("mov r6,r0");   // Random Starting Direction
    asm("mov r9,#0x0"); // Counter
    asm("maze_recursive_backtracking_loop:");
    asm("mov r7,r4");
    asm("mov r8,r5");
    asm("cmp r6,#0x1");
    asm("addlt r8,r8,#0x3"); // I considered using the directions array
    asm("addeq r7,r7,#0x3"); // however, I only use the four cardinal
    asm("cmp r6,#0x2");      // directions and have to add the number 3
    asm("subeq r7,r7,#0x3"); // which can't be shifted from the original
    asm("subgt r8,r8,#0x3"); // offset of 1. Could probably use my own
    asm("cmp r7,#0x2");      // array of offsets for X/Y? But meh.
    asm("cmpge r8,#0x2");
    asm("blt maze_recursive_backtracking_loop_iter");
    asm("cmp r7,#0x36");
    asm("cmplt r8,#0x1E");
    asm("bge maze_recursive_backtracking_loop_iter");
    asm("mov r0,r7");
    asm("mov r1,r8");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("and r1,r1,#0x3");
    asm("cmp r1,#0x0");
    asm("bne maze_recursive_backtracking_loop_iter");
    asm("bic r1,r1,#0x3");
    asm("orr r1,r1,#0x1");
    asm("strh r1,[r0,#0x0]");
    asm("mov r0,r4");
    asm("mov r1,r5");
    asm("cmp r6,#0x1");
    asm("addlt r1,r1,#0x2");
    asm("addeq r0,r0,#0x2");
    asm("cmp r6,#0x2");
    asm("subeq r0,r0,#0x2");
    asm("subgt r1,r1,#0x2");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0x3");
    asm("orr r1,r1,#0x1");
    asm("strh r1,[r0,#0x0]");
    asm("mov r0,r4");
    asm("mov r1,r5");
    asm("cmp r6,#0x1");
    asm("addlt r1,r1,#0x1");
    asm("addeq r0,r0,#0x1");
    asm("cmp r6,#0x2");
    asm("subeq r0,r0,#0x1");
    asm("subgt r1,r1,#0x1");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0x3");
    asm("orr r1,r1,#0x1");
    asm("strh r1,[r0,#0x0]");
    asm("mov r0,r4");
    asm("mov r1,r5");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0x3");
    asm("orr r1,r1,#0x1");
    asm("strh r1,[r0,#0x0]");
    asm("mov r0,r7");
    asm("mov r1,r8");
    asm("bl GenerateRecursiveBacktrackingMaze");
    asm("maze_recursive_backtracking_loop_iter:");
    asm("add r6,r6,#0x1");
    asm("and r6,r6,#0x3");
    asm("add r9,r9,#0x1");
    asm("cmp r9,#0x4");
    asm("blt maze_recursive_backtracking_loop");
    asm("ldmia sp!,{r4,r5,r6,r7,r8,r9,pc}"); // Warning: 8 byte stack alignment violated
} */

// Note: I was unable to find a good way to port
// GenerateRecusiveBacktrackingFloor and GenerateRecusiveBacktrackingMaze to
// C. Improved/rewritten implementations caused stack overflows. I tried a
// variety of assembly trick or ways to reduce the stack; however remembering
// the bounds x0, y0, x1, y1 is still 0x10 bytes per call. So...
void __attribute__((naked)) GenerateRecursiveBacktrackingFloor(struct floor_properties *floor_props) {
    asm("stmdb sp!,{r4,r5,r6,r7,r8,r9,r10,lr}"); // 8 byte stack alignment
    asm("mov r6,r0"); // r6 = param_1 (Floor Properties)
    asm("mov r0,#0x2"); // Yes, this is a very poor practice, but because the
    asm("mov r1,#0x2"); // maze algorithm is recursive, I need the stack space.
    asm("bl GenerateRecursiveBacktrackingMaze");
    asm("sub sp,sp,#0x1B00");
    asm("mov r0,#0x0"); // Zero (Number)
    asm("add r4,sp,#0xC");
    asm("mov r5,#0x0");
    asm("maze_rb_floor_init_room_check_loop:");
    asm("strb r0,[r4,r5]");
    asm("add r5,r5,#0x1"); // Iterate
    asm("cmp r5,#0x80");
    asm("blt maze_rb_floor_init_room_check_loop");
    asm("mov r0,#0x3");
    asm("bl DungeonRandInt");
    asm("ldrsb r1,[r6,#0x1]"); // Room Density
    asm("cmp r1,#0x0");
    asm("rsblt r10,r1,#0x0"); // If negative, use abs value.
    asm("addge r10,r1,r0");
    asm("cmp r10,#0x4"); // Less than 4 rooms fail the check for enough room
    asm("movlt r10,#0x4"); // tiles. For some reason 3 doesn't work...
    asm("cmp r10,#0xF"); // Could do more rooms, but that would require more work...
    asm("movgt r10,#0xF"); // and 15 is enough anyway I'm sure.
    asm("add r0,sp,#0x94");
    asm("mov r1,#0x1");
    asm("mov r2,r10"); // 3 -> 15
    asm("bl InitDungeonGrid");
    asm("add r4,sp,#0x94");
    asm("mov r5,#0x0");
    asm("maze_rb_floor_room_loop:");
    asm("mov r0,#0x10"); // Random X (0->15)
    asm("bl DungeonRandInt");
    asm("mov r7,r0");
    asm("mov r0,#0x8"); // Random Y (0->7)
    asm("bl DungeonRandInt");
    asm("mov r8,r0");
    asm("add r1,sp,r7");
    asm("add r1,r1,#0xC");
    asm("ldrb r2,[r1,r8, lsl #0x4]"); // sp + 0xC + x + (y * 16)
    asm("cmp r2,#0x0");
    asm("bne maze_rb_floor_room_loop");
    asm("mov r9,#0x1");
    asm("cmp r7,#0x0");
    asm("subgt r0,r1,#0x1"); // Mark this 3x3 area, and the ones around it
    asm("movle r0,r1");      // that a cell can't be placed here.
    asm("cmp r7,#0xF");      // Yes, for the edges I store the same value
    asm("addlt r2,r1,#0x1"); // multiple times into the same slot. It was just
    asm("movge r2,r1");      // a lot more complex to check for each cell
    asm("cmp r8,#0x0");      // coordinate was along an edge and to not store.
    asm("subgt r3,r8,#0x1");
    asm("movle r3,r8");
    asm("cmp r8,#0x7");
    asm("addlt r12,r8,#0x1");
    asm("movge r12,r8");
    asm("strb r9,[r0,r3, lsl #0x4]"); // Left Top
    asm("strb r9,[r0,r8, lsl #0x4]"); // Left Middle
    asm("strb r9,[r0,r12, lsl #0x4]"); // Left Bottom
    asm("strb r9,[r1,r3, lsl #0x4]"); // Middle Top
    asm("strb r9,[r1,r8, lsl #0x4]"); // Middle Middle
    asm("strb r9,[r1,r12, lsl #0x4]"); // Middle Bottom
    asm("strb r9,[r2,r3, lsl #0x4]"); // Right Top
    asm("strb r9,[r2,r8, lsl #0x4]"); // Right Middle
    asm("strb r9,[r2,r12, lsl #0x4]"); // Right Bottom
    asm("mov r3,#0x3");
    asm("mul r0,r7,r3");
    asm("add r0,r0,#0x4"); // (X * 3) + 4
    asm("mul r1,r8,r3");
    asm("add r1,r1,#0x4"); // (Y * 3) + 4
    asm("add r2,r0,#0x3"); // (X * 3) + 4 + 3
    asm("add r3,r1,#0x3"); // (Y * 3) + 4 + 3
    asm("strb r5,[sp,#0x0]");
    asm("str r4,[sp,#0x4]");
    asm("mov r12,#0x0");
    asm("strb r12,[sp,#0x8]"); // No, no, no! ROOM IMPERFECTIONS!
    asm("bl CreateRoomInCell");
    asm("add r4,r4,#0x1E"); // DUNGEON_GRID_CELL_BYTES
    asm("add r5,r5,#0x1");
    asm("cmp r5,r10");
    asm("blt maze_rb_floor_room_loop");
    asm("ldr r7,=FLOOR_GENERATION_STATUS");
    asm("add r0,sp,#0x94");
    asm("ldrsh r3,[r7,#0x10]");
    asm("mov r1,#0x1");
    asm("mov r2,r10");
    asm("bl GenerateMonsterHouse");
    asm("ldrb r3,[r6,#0x13]");
    asm("add r0,sp,#0x94");
    asm("mov r1,#0x1");
    asm("mov r2,r10");
    asm("bl GenerateExtraHallways");
    asm("add r0,sp,#0x94");
    asm("mov r1,#0x1");
    asm("mov r2,r10");
    asm("bl GenerateSecondaryStructures");
    asm("add sp,sp,#0x1B00");
    asm("ldmia sp!,{r4,r5,r6,r7,r8,r9,r10,pc}");
    asm("GenerateRecursiveBacktrackingMaze:");
    asm("stmdb sp!,{r4,r5,r6,r7,r8,r9,lr}"); // Warning: 8 byte stack alignment violated
    asm("mov r4,r0");
    asm("mov r5,r1");
    asm("mov r0,#0x4");
    asm("bl DungeonRandInt");
    asm("mov r6,r0");   // Random Starting Direction
    asm("mov r9,#0x0"); // Counter
    asm("maze_recursive_backtracking_loop:");
    asm("mov r7,r4");
    asm("mov r8,r5");
    asm("cmp r6,#0x1");
    asm("addlt r8,r8,#0x3"); // I considered using the directions array
    asm("addeq r7,r7,#0x3"); // however, I only use the four cardinal
    asm("cmp r6,#0x2");      // directions and have to add the number 3
    asm("subeq r7,r7,#0x3"); // which can't be shifted from the original
    asm("subgt r8,r8,#0x3"); // offset of 1. Could probably use my own
    asm("cmp r7,#0x2");      // array of offsets for X/Y? But meh.
    asm("cmpge r8,#0x2");
    asm("blt maze_recursive_backtracking_loop_iter");
    asm("cmp r7,#0x36");
    asm("cmplt r8,#0x1E");
    asm("bge maze_recursive_backtracking_loop_iter");
    asm("mov r0,r7");
    asm("mov r1,r8");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("and r1,r1,#0x3");
    asm("cmp r1,#0x0");
    asm("bne maze_recursive_backtracking_loop_iter");
    asm("bic r1,r1,#0x3");
    asm("orr r1,r1,#0x1");
    asm("strh r1,[r0,#0x0]");
    asm("mov r0,r4");
    asm("mov r1,r5");
    asm("cmp r6,#0x1");
    asm("addlt r1,r1,#0x2");
    asm("addeq r0,r0,#0x2");
    asm("cmp r6,#0x2");
    asm("subeq r0,r0,#0x2");
    asm("subgt r1,r1,#0x2");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0x3");
    asm("orr r1,r1,#0x1");
    asm("strh r1,[r0,#0x0]");
    asm("mov r0,r4");
    asm("mov r1,r5");
    asm("cmp r6,#0x1");
    asm("addlt r1,r1,#0x1");
    asm("addeq r0,r0,#0x1");
    asm("cmp r6,#0x2");
    asm("subeq r0,r0,#0x1");
    asm("subgt r1,r1,#0x1");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0x3");
    asm("orr r1,r1,#0x1");
    asm("strh r1,[r0,#0x0]");
    asm("mov r0,r4");
    asm("mov r1,r5");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0x3");
    asm("orr r1,r1,#0x1");
    asm("strh r1,[r0,#0x0]");
    asm("mov r0,r7");
    asm("mov r1,r8");
    asm("bl GenerateRecursiveBacktrackingMaze");
    asm("maze_recursive_backtracking_loop_iter:");
    asm("add r6,r6,#0x1");
    asm("and r6,r6,#0x3");
    asm("add r9,r9,#0x1");
    asm("cmp r9,#0x4");
    asm("blt maze_recursive_backtracking_loop");
    asm("ldmia sp!,{r4,r5,r6,r7,r8,r9,pc}"); // Warning: 8 byte stack alignment violated
}

/* The original assembly version of this function has been left in the
// codebase for reference, but it has been rewritten in C. The original
// version only supported generating binary mazes with a DIR_UP_LEFT bias, but
// the new version supports all four diagonal directions.
void __attribute__((naked)) GenerateBinaryTreeMaze(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    asm("stmdb sp!,{r4,r5,r6,r7,r8,r9,lr}");
    asm("mov   r4,r0");
    asm("mov   r8,r1");
    asm("sub   r6,r2,#0x1");
    asm("sub   r7,r3,#0x1");
    asm("maze_binary_tree_x_loop:");
    asm("mov   r5,r8"); // Reinit Y
    asm("maze_binary_tree_y_loop:");
    asm("mov   r0,#0x2");
    asm("bl    DungeonRandInt");
    asm("mov   r9,r0");
    asm("mov   r0,r4");
    asm("mov   r1,r5");
    asm("bl    GetTileSafe");
    asm("ldrh  r1,[r0,#0x0]");
    asm("bic   r1,r1,#0x3");
    asm("orr   r1,r1,#0x1");
    asm("strh  r1,[r0,#0x0]");
    asm("cmp   r9,#0x0");
    asm("addne r0,r4,#0x2");
    asm("moveq r0,r4");
    asm("addeq r1,r5,#0x2");
    asm("movne r1,r5");
    asm("bl    GetTileSafe");
    asm("ldrh  r1,[r0,#0x0]");
    asm("bic   r1,r1,#0x3");
    asm("orr   r1,r1,#0x1");
    asm("strh  r1,[r0,#0x0]");
    asm("cmp   r9,#0x0");
    asm("addne r0,r4,#0x1");
    asm("moveq r0,r4");
    asm("addeq r1,r5,#0x1");
    asm("movne r1,r5");
    asm("bl    GetTileSafe");
    asm("ldrh  r1,[r0,#0x0]");
    asm("bic   r1,r1,#0x3");
    asm("orr   r1,r1,#0x1");
    asm("strh  r1,[r0,#0x0]");
    asm("maze_binary_tree_y_loop_iter:");
    asm("add   r5,r5,#0x2");
    asm("cmp   r5,r7");
    asm("blt   maze_binary_tree_y_loop");
    asm("add   r4,r4,#0x2");
    asm("cmp   r4,r6");
    asm("blt   maze_binary_tree_x_loop");
    asm("ldmia sp!,{r4,r5,r6,r7,r8,r9,pc}");
} */
void GenerateBinaryTreeMaze(const int16_t x0, const int16_t y0, const int16_t x1, const int16_t y1, enum direction_id dir) {
    if (false == IsAreaValid(x0, y0, x1, y1)) {
        return;
    }
    dir = MakeDirectionDiagonal(dir);
    int last_x, last_y, x, y;
    int chance_vertical = y1 - y0;
    int weight = x1 - x0 + chance_vertical;
    struct tile *tile;
    if (DIR_DOWN_RIGHT == dir) {
        last_x = x1 - 1;
        last_y = y1 - 1;
        for (x = x0; x < last_x; x+= 2) {
            for (y = y0; y < last_y; y += 2) {
                tile = GetTileSafe(x, y);
                tile->terrain_type = TERRAIN_NORMAL;
                if (DungeonRandInt(weight) < chance_vertical) {
                    tile = GetTileSafe(x, y + 1);
                    tile->terrain_type = TERRAIN_NORMAL;
                } else {
                    tile = GetTileSafe(x + 1, y);
                    tile->terrain_type = TERRAIN_NORMAL;
                }
            }
        }
    } else if (DIR_DOWN_LEFT == dir) {
        last_x = x0;
        last_y = y1 - 1;
        for (x = x1 - 1; x > last_x; x-= 2) {
            for (y = y0; y < last_y; y += 2) {
                tile = GetTileSafe(x, y);
                tile->terrain_type = TERRAIN_NORMAL;
                if (DungeonRandInt(weight) < chance_vertical) {
                    tile = GetTileSafe(x, y + 1);
                    tile->terrain_type = TERRAIN_NORMAL;
                } else {
                    tile = GetTileSafe(x - 1, y);
                    tile->terrain_type = TERRAIN_NORMAL;
                }
            }
        }
    } else if (DIR_UP_RIGHT == dir) {
        last_x = x1 - 1;
        last_y = y0;
        for (x = x0; x < last_x; x+= 2) {
            for (y = y1 - 1; y > last_y; y -= 2) {
                tile = GetTileSafe(x, y);
                tile->terrain_type = TERRAIN_NORMAL;
                if (DungeonRandInt(weight) < chance_vertical) {
                    tile = GetTileSafe(x, y - 1);
                    tile->terrain_type = TERRAIN_NORMAL;
                } else {
                    tile = GetTileSafe(x + 1, y);
                    tile->terrain_type = TERRAIN_NORMAL;
                }
            }
        }
    } else { // (DIR_UP_LEFT == dir)
        last_x = x0;
        last_y = y0;
        for (x = x1 - 1; x > last_x; x-= 2) {
            for (y = y1 - 1; y > last_y; y -= 2) {
                tile = GetTileSafe(x, y);
                tile->terrain_type = TERRAIN_NORMAL;
                if (DungeonRandInt(weight) < chance_vertical) {
                    tile = GetTileSafe(x, y - 1);
                    tile->terrain_type = TERRAIN_NORMAL;
                } else {
                    tile = GetTileSafe(x - 1, y);
                    tile->terrain_type = TERRAIN_NORMAL;
                }
            }
        }
    }
}
/* Original Assembly Function Was Rewritten For hb-eos-c-dun-lib however, it
   has been left in the codebase commented out to show the original
   implementation.
GenerateSidewinderMaze: // x0, x1, y0, y1
    stmdb sp!,{r3,r4,r5,r6,r7,r8,r9,r10,r11,lr}
    mov r4,r0
    mov r5,r1
    mov r6,r2
    mov r7,r3
    sidewinder_maze_loop_y:
        cmp r5,r7
        bge end_sidewinding
        mov r8,r4
        mov r9,r4
        sidewinder_maze_loop_x:
            cmp r8,r6
            bge sidewinder_carve_north
            mov r0,#0x3
            bl  DungeonRandInt
            cmp r0,#0x2
            blt sidewinder_carve_east
            sidewinder_carve_north:
            mov r0,r9
            add r1,r8,#0x1
            bl  DungeonRandRange
            and r0,r0,#0b11111110
            mov r10,r0
            mov r1,r5
            bl  GetTileSafe
            ldrh r1,[r0,#0x0]
            bic r1,r1,#0x3
            orr r1,r1,#0x1
            strh r1,[r0,#0x0] ; Make hallway.
            mov r0,r10
            sub r1,r5,#0x1
            bl  GetTileSafe
            ldrh r1,[r0,#0x0]
            bic r1,r1,#0x3
            orr r1,r1,#0x1
            strh r1,[r0,#0x0] ; Make hallway.
            mov r0,r10
            sub r1,r5,#0x2
            bl  GetTileSafe
            ldrh r1,[r0,#0x0]
            bic r1,r1,#0x3
            orr r1,r1,#0x1
            strh r1,[r0,#0x0] ; Make hallway.
            add r9,r8,#0x2
            b sidewinder_maze_loop_x_iter
            sidewinder_carve_east:
            mov r0,r8
            mov r1,r5
            bl  GetTileSafe
            ldrh r1,[r0,#0x0]
            bic r1,r1,#0x3
            orr r1,r1,#0x1
            strh r1,[r0,#0x0] ; Make hallway.
            add r0,r8,#0x1
            mov r1,r5
            bl  GetTileSafe
            ldrh r1,[r0,#0x0]
            bic r1,r1,#0x3
            orr r1,r1,#0x1
            strh r1,[r0,#0x0] ; Make hallway.
            add r0,r8,#0x2
            mov r1,r5
            bl  GetTileSafe
            ldrh r1,[r0,#0x0]
            bic r1,r1,#0x3
            orr r1,r1,#0x1
            strh r1,[r0,#0x0] ; Make hallway.
            sidewinder_maze_loop_x_iter:
            add r8,r8,#0x2
            cmp r8,r6
            ble sidewinder_maze_loop_x
        add r5,r5,#0x2
        b sidewinder_maze_loop_y
    end_sidewinding:
    ldmia sp!,{r3,r4,r5,r6,r7,r8,r9,r10,r11,pc} */
void GenerateSidewinderMaze(const int16_t x0, const int16_t y0, const int16_t x1, const int16_t y1, enum direction_id dir, const int32_t weight) {
    if (false == IsAreaValid(x0, y0, x1, y1)) {
        return;
    }
    dir = MakeDirectionCardinal(dir);
    struct tile *tile;
    int x, y, run, rand, last_y, last_x;
    if (DIR_DOWN == dir) {
        last_y = y1 - 1;
        last_x = x1 - 1;
        for (y = y0; y < last_y; y += 2) {
            run = x0;
            for (x = x0; x < last_x; x += 2) {
                tile = GetTileSafe(x, y);
                tile->terrain_type = TERRAIN_NORMAL;
                if(0 == DungeonRandInt(weight)) {
                    // Connect Vertical
                    rand = DungeonRandInt((x - run) / 2);
                    tile = GetTileSafe(run + rand * 2, y + 1);
                    run = x + 2;
                } else {
                    // Connect Horizontal
                    tile = GetTileSafe(x + 1, y);
                }
                tile->terrain_type = TERRAIN_NORMAL;
            }
            tile = GetTileSafe(x, y);
            tile->terrain_type = TERRAIN_NORMAL;
            rand = DungeonRandInt((x - run) / 2);
            tile = GetTileSafe(run + rand * 2, y + 1);
            tile->terrain_type = TERRAIN_NORMAL;
        }
    } else if (DIR_UP == dir) {
        last_y = y0;
        last_x = x1 - 1;
        for (y = y1 - 1; y > last_y; y -= 2) {
            run = x0;
            for (x = x0; x < last_x; x += 2) {
                tile = GetTileSafe(x, y);
                tile->terrain_type = TERRAIN_NORMAL;
                if(0 == DungeonRandInt(weight)) {
                    // Connect Vertical
                    rand = DungeonRandInt((x - run) / 2);
                    tile = GetTileSafe(run + rand * 2, y - 1);
                    run = x + 2;
                } else {
                    // Connect Horizontal
                    tile = GetTileSafe(x + 1, y);
                }
                tile->terrain_type = TERRAIN_NORMAL;
            }
            tile = GetTileSafe(x, y);
            tile->terrain_type = TERRAIN_NORMAL;
            rand = DungeonRandInt((x - run) / 2);
            tile = GetTileSafe(run + rand * 2, y - 1);
            tile->terrain_type = TERRAIN_NORMAL;
        }
    } else if (DIR_RIGHT == dir) {
        last_y = y1 - 1;
        last_x = x1 - 1;
        for (x = x0; x < last_x; x += 2) {
            run = y0;
            for (y = y0; y < last_y; y += 2) {
                tile = GetTileSafe(x, y);
                tile->terrain_type = TERRAIN_NORMAL;
                if(0 == DungeonRandInt(weight)) {
                    // Connect Horizontal
                    rand = DungeonRandInt((y - run) / 2);
                    tile = GetTileSafe(x + 1, run + rand * 2);
                    run = y + 2;
                } else {
                    // Connect Vertical
                    tile = GetTileSafe(x, y + 1);
                }
                tile->terrain_type = TERRAIN_NORMAL;
            }
            tile = GetTileSafe(x, y);
            tile->terrain_type = TERRAIN_NORMAL;
            rand = DungeonRandInt((y - run) / 2);
            tile = GetTileSafe(x + 1, run + rand * 2);
            tile->terrain_type = TERRAIN_NORMAL;
        }
    } else { // DIR_LEFT == dir
        last_y = y1 - 1;
        last_x = x0;
        for (x = x1 - 1; x > last_x; x -= 2) {
            run = y0;
            for (y = y0; y < last_y; y += 2) {
                tile = GetTileSafe(x, y);
                tile->terrain_type = TERRAIN_NORMAL;
                if(0 == DungeonRandInt(weight)) {
                    // Connect Horizontal
                    rand = DungeonRandInt((y - run) / 2);
                    tile = GetTileSafe(x - 1, run + rand * 2);
                    run = y + 2;
                } else {
                    // Connect Vertical
                    tile = GetTileSafe(x, y + 1);
                }
                tile->terrain_type = TERRAIN_NORMAL;
            }
            tile = GetTileSafe(x, y);
            tile->terrain_type = TERRAIN_NORMAL;
            rand = DungeonRandInt((y - run) / 2);
            tile = GetTileSafe(x - 1, run + rand * 2);
            tile->terrain_type = TERRAIN_NORMAL;
        }
    }
}

void GenerateHuntAndKillMaze(const int16_t x0, const int16_t y0, const int16_t x1, const int16_t y1, struct abstract_floor *abs_floor) {
    if (false == IsAreaValid(x0, y0, x1, y1)) {
        return;
    }
    // Pick a valid location for the maze to start. It's not impossible someone
    // has blocked part of the maze area to give it a unique shape.
    struct abstract_tile *tile;
    int start_location_find_attempts = (x1 - x0 + y1 - y0) + 1; // min 1 so init guaranteed
    int current_x = x0;
    int current_y = y0;
    int i;
    for(i = 0; i < start_location_find_attempts; ++i) {
        current_x = x0 + (DungeonRandInt(x1 - x0) / 2) * 2;
        current_y = y0 + (DungeonRandInt(y1 - y0) / 2) * 2;
        tile = &(abs_floor->tiles[current_x][current_y]);
        if (false == (tile->x_connect_blocked & tile->y_connect_blocked)) {
            break;
        }
    }
    if (i == start_location_find_attempts) {
        return; // Give up.
    }
    bool stuck, finding_prey;
    enum direction_id curr_dir, start_dir;
    int middle_x, middle_y, next_x, next_y, x_offset, y_offset, scan_x, scan_y;
    do {
        // Kill/Walk
        do {
            stuck = true;
            tile = &(abs_floor->tiles[current_x][current_y]);
            tile->custom_tile_info = true;
            tile->terrain_type = TERRAIN_NORMAL;
            start_dir = RandomCardinalDirection();
            curr_dir = start_dir;
            do {
                x_offset = DIRECTIONS_XY[curr_dir][HB_X_POS];
                y_offset = DIRECTIONS_XY[curr_dir][HB_Y_POS];
                middle_x = current_x + x_offset;
                middle_y = current_y + y_offset;
                next_x = current_x + 2 * x_offset;
                next_y = current_y + 2 * y_offset;
                if (abs_floor->tiles[next_x][next_y].custom_tile_info) {
                    goto ghakm_dir_iter;
                }
                if (false == IsCoordinateInArea(next_x, next_y, x0, y0, x1, y1)) {
                    goto ghakm_dir_iter;
                }
                if (false == CanCarveAbstractTile(next_x, next_y, abs_floor, curr_dir)) {
                    goto ghakm_dir_iter;
                }
                if (false == CanCarveAbstractTile(middle_x, middle_y, abs_floor, curr_dir)) {
                    goto ghakm_dir_iter;
                }
                TryCarveAbstractTileSafe(middle_x, middle_y, abs_floor, curr_dir);
                TryCarveAbstractTileSafe(next_x, next_y, abs_floor, curr_dir);
                stuck = false;
                break;
                // Label made the code look cleaner and more convenient here.
                // So... oh well here is a label. It doesn't have to be here.
                ghakm_dir_iter:;
                curr_dir = (curr_dir + 2) & 0x07;
            } while (start_dir != curr_dir);
            current_x = next_x;
            current_y = next_y;
        } while (false == stuck);
        // Hunt
        finding_prey = true;
        scan_x = x0;
        while (scan_x < x1) {
            scan_y = y0;
            while (scan_y < y1) {
                tile = &(abs_floor->tiles[scan_x][scan_y]);
                if (tile->custom_tile_info) {
                    for(i = 0; i < 4; ++i) {
                        x_offset = DIRECTIONS_XY[DIR_DOWN + (i << 1)][HB_X_POS];
                        y_offset = DIRECTIONS_XY[DIR_DOWN + (i << 1)][HB_Y_POS];
                        middle_x = scan_x + x_offset;
                        middle_y = scan_y + y_offset;
                        next_x = scan_x + 2 * x_offset;
                        next_y = scan_y + 2 * y_offset;
                        if (abs_floor->tiles[next_x][next_y].custom_tile_info) {
                            continue;
                        }
                        if (false == IsCoordinateInArea(next_x, next_y, x0, y0, x1, y1)) {
                            continue;
                        }
                        if (false == CanCarveAbstractTile(next_x, next_y, abs_floor, curr_dir)) {
                            continue;
                        }
                        if (false == CanCarveAbstractTile(middle_x, middle_y, abs_floor, curr_dir)) {
                            continue;
                        }
                        finding_prey = false;
                    }
                }
                if (false == finding_prey) {
                    break;
                }
                ++scan_y;
            }
            if (false == finding_prey) {
                break;
            }
            ++scan_x;
        }
        current_x = scan_x;
        current_y = scan_y;
    } while (false == finding_prey);
    // Ends if it's still looking for 'prey' after checking all tiles.
}

void GenerateBinaryTreeFloor(struct floor_properties *floor_props, bool easy_solution) {
    struct dungeon_grid_cell grid[15];
    uint8_t stair_pos[2];
    int16_t spawn_x, spawn_y;
    struct tile *tile;
    InitDungeonGrid(grid, 1, 3);
    enum direction_id dir = RandomDiagonalDirection();
    GenerateBinaryTreeMaze(7, 7, 24, 24, dir);
    if (DIR_DOWN_RIGHT == dir) {
        CreateRoomInCell(7, 7, 10, 10, 0, &grid[0], floor_props->room_flags);
        CreateRoomInCell(7, 23, 22, 28, 1, &grid[0], floor_props->room_flags);
        CreateRoomInCell(23, 7, 28, 22, 2, &grid[1], floor_props->room_flags);
        CreateRoomInCell(25, 25, 28, 28, 3, &grid[1], floor_props->room_flags);
        for (int i = 24; i >= 22; i--) {
            tile = GetTile(i, 27);
            tile->terrain_type = TERRAIN_NORMAL;
            tile = GetTile(27, i);
            tile->terrain_type = TERRAIN_NORMAL;
        }
        stair_pos[HB_X_POS] = 7;
        stair_pos[HB_Y_POS] = 7;
        spawn_x = 27;
        spawn_y = 27;
    } else if (DIR_DOWN_LEFT == dir) {
        CreateRoomInCell(21, 7, 24, 10, 0, &grid[0], floor_props->room_flags);
        CreateRoomInCell(3, 7, 8, 22, 1, &grid[0], floor_props->room_flags);
        CreateRoomInCell(9, 23, 24, 28, 2, &grid[1], floor_props->room_flags);
        CreateRoomInCell(3, 25, 6, 28, 3, &grid[1], floor_props->room_flags);
        for (int i = 0; i < 3; ++i) {
            tile = GetTile(3, 22 + i);
            tile->terrain_type = TERRAIN_NORMAL;
            tile = GetTile(6 + i, 27);
            tile->terrain_type = TERRAIN_NORMAL;
        }
        stair_pos[HB_X_POS] = 23;
        stair_pos[HB_Y_POS] = 7;
        spawn_x = 3;
        spawn_y = 27;
    } else if (DIR_UP_RIGHT == dir) {
        CreateRoomInCell(7, 21, 10, 24, 0, &grid[0], floor_props->room_flags);
        CreateRoomInCell(7, 3, 22, 8, 1, &grid[0], floor_props->room_flags);
        CreateRoomInCell(23, 9, 28, 24, 2, &grid[1], floor_props->room_flags);
        CreateRoomInCell(25, 3, 28, 6, 3, &grid[1], floor_props->room_flags);
        for (int i = 0; i < 3; ++i) {
            tile = GetTile(22 + i, 3);
            tile->terrain_type = TERRAIN_NORMAL;
            tile = GetTile(27, 6 + i);
            tile->terrain_type = TERRAIN_NORMAL;
        }
        stair_pos[HB_X_POS] = 7;
        stair_pos[HB_Y_POS] = 23;
        spawn_x = 27;
        spawn_y = 3;
    } else { // (DIR_UP_LEFT == dir)
        CreateRoomInCell(21, 21, 24, 24, 0, &grid[0], floor_props->room_flags);
        CreateRoomInCell(3, 9, 8, 24, 1, &grid[0], floor_props->room_flags);
        CreateRoomInCell(9, 3, 24, 8, 2, &grid[1], floor_props->room_flags);
        CreateRoomInCell(3, 3, 6, 6, 3, &grid[1], floor_props->room_flags);
        for (int i = 6; i < 9; i++) {
            tile = GetTile(i, 3);
            tile->terrain_type = TERRAIN_NORMAL;
            tile = GetTile(3, i);
            tile->terrain_type = TERRAIN_NORMAL;
        }
        stair_pos[HB_X_POS] = 23;
        stair_pos[HB_Y_POS] = 23;
        spawn_x = 3;
        spawn_y = 3;
    }
    if (easy_solution) {
        int temp = spawn_x;
        spawn_x = stair_pos[HB_X_POS];
        stair_pos[HB_X_POS] = temp;
        temp = spawn_y;
        spawn_y = stair_pos[HB_Y_POS];
        stair_pos[HB_Y_POS] = temp;
    }
    SpawnStairs(stair_pos, &(DUNGEON_PTR->gen_info), STAIRS_TYPE_NORMAL);
    DUNGEON_PTR->gen_info.team_spawn_pos.x = spawn_x;
    DUNGEON_PTR->gen_info.team_spawn_pos.y = spawn_y;
    GenerateMazeRoom(grid, 1, 2, floor_props->maze_room_chance);
    GenerateKecleonShop(grid, 1, 2, floor_props->kecleon_shop_spawn_chance);
    GenerateMonsterHouse(grid, 1, 2, floor_props->monster_house_spawn_chance);
    GenerateExtraHallways(grid, 1, 2, floor_props->extra_hallways);
    GenerateRoomImperfections(grid, 1, 2);
    GenerateSecondaryStructures(grid, 1, 2);
}

void GenerateSidewinderFloor(struct floor_properties *floor_props, bool hard_solution) {
    struct dungeon_grid_cell grid[15];
    uint8_t stair_pos[2];
    int16_t spawn_x, spawn_y;
    InitDungeonGrid(grid, 1, 2);
    enum direction_id dir = RandomCardinalDirection();
    int weight = DungeonRandInt(3) + 2;
    GenerateSidewinderMaze(6, 6, 27, 27, dir, weight);
    if (DIR_UP == dir || DIR_DOWN == dir) {
        CreateRoomInCell(6, 2, 27, 7, 0, &grid[0], floor_props->room_flags);
        CreateRoomInCell(6, 24, 27, 29, 1, &grid[1], floor_props->room_flags);
        spawn_x = DungeonRandRange(6, 27);
    } else {
        CreateRoomInCell(2, 6, 7, 27, 0, &grid[0], floor_props->room_flags);
        CreateRoomInCell(24, 6, 29, 27, 1, &grid[1], floor_props->room_flags);
        spawn_y = DungeonRandRange(6, 27);
    }
    if (DIR_UP == dir) {
        spawn_y = 28;
        stair_pos[HB_X_POS] = spawn_x;
        stair_pos[HB_Y_POS] = 2;
    } else if (DIR_DOWN == dir) {
        spawn_y = 2;
        stair_pos[HB_X_POS] = spawn_x;
        stair_pos[HB_Y_POS] = 28;
    } else if (DIR_RIGHT == dir) {
        spawn_x = 2;
        stair_pos[HB_Y_POS] = spawn_y;
        stair_pos[HB_X_POS] = 28;
    } else { // (DIR_LEFT == dir)
        spawn_x = 28;
        stair_pos[HB_Y_POS] = spawn_y;
        stair_pos[HB_X_POS] = 2;
    }
    if (hard_solution) {
        int temp = spawn_x;
        spawn_x = stair_pos[HB_X_POS];
        stair_pos[HB_X_POS] = temp;
        temp = spawn_y;
        spawn_y = stair_pos[HB_Y_POS];
        stair_pos[HB_Y_POS] = temp;
    }
    SpawnStairs(stair_pos, &(DUNGEON_PTR->gen_info), STAIRS_TYPE_NORMAL);
    DUNGEON_PTR->gen_info.team_spawn_pos.x = spawn_x;
    DUNGEON_PTR->gen_info.team_spawn_pos.y = spawn_y;
    GenerateMazeRoom(grid, 1, 2, floor_props->maze_room_chance);
    GenerateKecleonShop(grid, 1, 2, floor_props->kecleon_shop_spawn_chance);
    GenerateMonsterHouse(grid, 1, 2, floor_props->monster_house_spawn_chance);
    GenerateExtraHallways(grid, 1, 2, floor_props->extra_hallways);
    GenerateRoomImperfections(grid, 1, 2);
    GenerateSecondaryStructures(grid, 1, 2);
}

void GenerateCenterMazeFloor(struct floor_properties *floor_props) {
    const int x_start = 2;
    const int y_start = 2;
    const int x_end = 29;
    const int y_end = 29;
    struct abstract_floor abs_floor;
    struct dungeon_grid_cell grid[HB_MAX_ROOMS];
    InitAbstractFloor(&abs_floor);
    int side_room_size[4];
    side_room_size[0] = DungeonRandInt(3) * 2 + 5; // Top Left
    AddRoomToAbstractFloor(x_start, y_start, x_start + side_room_size[0], y_start + side_room_size[0], &abs_floor);
    side_room_size[1] = DungeonRandInt(3) * 2 + 5; // Bottom Left
    AddRoomToAbstractFloor(x_start, y_end - side_room_size[1], x_start + side_room_size[1], y_end, &abs_floor);
    side_room_size[2] = DungeonRandInt(3) * 2 + 5; // Bottom Right
    AddRoomToAbstractFloor(x_end - side_room_size[2], y_end - side_room_size[2], x_end, y_end, &abs_floor);
    side_room_size[3] = DungeonRandInt(3) * 2 + 5; // Top Right
    AddRoomToAbstractFloor(x_end - side_room_size[3], y_start, x_end, y_start + side_room_size[3], &abs_floor);
    GenerateHuntAndKillMaze(x_start + 4, y_start + 4, x_end - 4, y_end - 4, &abs_floor);
    // HB_DEBUG_PRINT_ABS_FLOOR(&abs_floor);
    ApplyAbstractFloorToFloor(&abs_floor, grid, floor_props);
    uint8_t stair_pos[2];
    int16_t spawn_x, spawn_y;
    int spawn_room = DungeonRandInt(4);
    int stair_room = (spawn_room + 0x2) & 0x3;
    stair_pos[HB_X_POS] = DungeonRandRange(abs_floor.room_start_pos[stair_room][HB_X_POS], abs_floor.room_end_pos[stair_room][HB_X_POS]);
    stair_pos[HB_Y_POS] = DungeonRandRange(abs_floor.room_start_pos[stair_room][HB_Y_POS], abs_floor.room_end_pos[stair_room][HB_Y_POS]);
    spawn_x = DungeonRandRange(abs_floor.room_start_pos[spawn_room][HB_X_POS], abs_floor.room_end_pos[spawn_room][HB_X_POS]);
    spawn_y = DungeonRandRange(abs_floor.room_start_pos[spawn_room][HB_Y_POS], abs_floor.room_end_pos[spawn_room][HB_Y_POS]);
    SpawnStairs(stair_pos, &(DUNGEON_PTR->gen_info), STAIRS_TYPE_NORMAL);
    DUNGEON_PTR->gen_info.team_spawn_pos.x = spawn_x;
    DUNGEON_PTR->gen_info.team_spawn_pos.y = spawn_y;
    GenerateMazeRoom(grid, 1, 4, floor_props->maze_room_chance);
    GenerateKecleonShop(grid, 1, 4, floor_props->kecleon_shop_spawn_chance);
    GenerateMonsterHouse(grid, 1, 4, floor_props->monster_house_spawn_chance);
    GenerateExtraHallways(grid, 1, 4, floor_props->extra_hallways);
    GenerateRoomImperfections(grid, 1, 4);
    GenerateSecondaryStructures(grid, 1, 4);
}

void GeneratePSMDMazeFloor(struct floor_properties *floor_props) {
    const int main_rooms_size_x = 5;
    const int main_rooms_size_y = 5;
    const int buffer_y = 5;
    const int main_rooms_start_x = HB_DUNGEON_MAX_X/2 - main_rooms_size_x/2;
    const int main_rooms_start_y = (HB_DUNGEON_MAX_Y)/2 - main_rooms_size_y - (buffer_y)/2 - 1;
    struct abstract_floor abs_floor;
    struct dungeon_grid_cell grid[HB_MAX_ROOMS];
    uint8_t stair_pos[2];
    int16_t spawn_x, spawn_y;
    InitAbstractFloor(&abs_floor);
    int x0 = main_rooms_start_x;
    int y0 = main_rooms_start_y;
    int x1 = x0 + main_rooms_size_x;
    int y1 = y0 + main_rooms_size_y;
    abs_floor.tiles[HB_DUNGEON_MAX_X/2][y0 - 1].terrain_type = TERRAIN_NORMAL;
    AddRoomToAbstractFloor(x0, y0, x1, y1, &abs_floor);
    // Prevent connections on sides of room.
    int end_x = x1 - 1;
    for(int y = y0; y < y1; ++y) {
        abs_floor.tiles[x0][y].x_connect_blocked = true;
        abs_floor.tiles[end_x][y].x_connect_blocked = true;
    }
    abs_floor.tiles[x0][y0].y_connect_blocked = true;
    abs_floor.tiles[end_x][y0].y_connect_blocked = true;
    // Prevent connections between the rooms directly.
    y0 = y1 + buffer_y;
    for (int x = x0; x < x1; ++x) {
        for (int y = y1; y < y0; ++y) {
            abs_floor.tiles[x][y].y_connect_blocked = true;
        }
    }
    y1 = y0 + main_rooms_size_y;
    spawn_y = y0;
    abs_floor.tiles[HB_DUNGEON_MAX_X/2][y1].terrain_type = TERRAIN_NORMAL;
    AddRoomToAbstractFloor(x0, y0, x1, y1, &abs_floor);
    // Prevent connections on sides of room.
    for(int y = y0; y < y1; ++y) {
        abs_floor.tiles[x0][y].x_connect_blocked = true;
        abs_floor.tiles[end_x][y].x_connect_blocked = true;
    }
    int end_y = y1 - 1;
    abs_floor.tiles[x0][end_y].y_connect_blocked = true;
    abs_floor.tiles[x1 - 1][end_y].y_connect_blocked = true;
    // Generate some donuts/circles.
    int num_donuts = DungeonRandInt(3) + 2;
    int rand_x, rand_y, start_y, start_x;
    for (int i = 0; i < num_donuts; ++i) {
        rand_x = 4 + (DungeonRandInt((main_rooms_start_x - 8) >> 1) << 1);
        rand_y = 4 + (DungeonRandInt((HB_DUNGEON_MAX_Y - 8) >> 1) << 1);
        abs_floor.tiles[rand_x][rand_y].x_connect_blocked = true;
        abs_floor.tiles[rand_x][rand_y].y_connect_blocked = true;
        start_x = rand_x - 2;
        start_y = rand_y - 2;
        end_x = start_x + 4;
        end_y = start_y + 4;
        for (int x = start_x; x <= end_x; ++x) {
            abs_floor.tiles[x][start_y].terrain_type = TERRAIN_NORMAL;
            abs_floor.tiles[x][end_y].terrain_type = TERRAIN_NORMAL;
        }
        for (int y = start_y; y <= end_y; ++y) {
            abs_floor.tiles[start_x][y].terrain_type = TERRAIN_NORMAL;
            abs_floor.tiles[end_x][y].terrain_type = TERRAIN_NORMAL;
        }
        abs_floor.tiles[start_x][start_y].item_spawn = true;
        abs_floor.tiles[start_x][end_y].item_spawn = true;
        abs_floor.tiles[end_x][start_y].item_spawn = true;
        abs_floor.tiles[end_x][end_y].item_spawn = true;
    }
    GenerateHuntAndKillMaze(2, 2, 29, 30, &abs_floor);
    // Copy the left side to the right side so it's symmetrical.
    enum terrain_type terrain_type;
    for (int x = 2; x < HB_DUNGEON_MAX_X/2; ++x) {
        for (int y = 2; y < HB_DUNGEON_MAX_Y; ++y) {
            terrain_type = abs_floor.tiles[x][y].terrain_type;
            abs_floor.tiles[(HB_DUNGEON_MAX_X) - x][y].terrain_type = terrain_type;
        }
    }
    // Add an extra room on each side.
    rand_x = 4 + (DungeonRandInt((main_rooms_start_x - 12) >> 1) << 1);
    rand_y = 4 + (DungeonRandInt((HB_DUNGEON_MAX_Y - 8) >> 1) << 1);
    start_x = rand_x - 2;
    start_y = rand_y - 2;
    end_x = start_x + 5;
    end_y = start_y + 5;
    AddRoomToAbstractFloor(start_x, start_y, end_x, end_y, &abs_floor);
    end_x = (HB_DUNGEON_MAX_X) - (rand_x) + 3;
    start_y = rand_y - 2;
    start_x = end_x - 5;
    end_y = start_y + 5;
    AddRoomToAbstractFloor(start_x, start_y, end_x, end_y, &abs_floor);
    // HB_DEBUG_PRINT_ABS_FLOOR(&abs_floor);
    ApplyAbstractFloorToFloor(&abs_floor, grid, floor_props);
    spawn_x = HB_DUNGEON_MAX_X/2;
    stair_pos[HB_X_POS] = spawn_x;
    spawn_x = HB_DUNGEON_MAX_X/2;
    if (0 == DungeonRandInt(2)) {
        stair_pos[HB_Y_POS] = spawn_y - buffer_y - 1;
    } else {
        stair_pos[HB_Y_POS] = spawn_y;
        spawn_y = spawn_y - buffer_y - 1;
    }
    SpawnStairs(stair_pos, &(DUNGEON_PTR->gen_info), STAIRS_TYPE_NORMAL);
    DUNGEON_PTR->gen_info.team_spawn_pos.x = spawn_x;
    DUNGEON_PTR->gen_info.team_spawn_pos.y = spawn_y;
    GenerateMazeRoom(grid, 1, 4, floor_props->maze_room_chance);
    GenerateKecleonShop(grid, 1, 4, floor_props->kecleon_shop_spawn_chance);
    GenerateMonsterHouse(grid, 1, 4, floor_props->monster_house_spawn_chance);
    // GenerateExtraHallways(grid, 1, 4, floor_props->extra_hallways);
    GenerateRoomImperfections(grid, 1, 4);
    GenerateSecondaryStructures(grid, 1, 4);
}
/* The original assembly version of this function has been left in the
// codebase for reference, but it has been rewritten in C.
Generate12RoomOctopus:
    stmdb sp!,{r3,r4,r5,r6,r7,r8,r9,r10,r11,lr}
    sub sp,sp,#0xAE0
    sub sp,sp,#0x1000
    mov r6,r0 ; r6 = param_0 (Floor Properties)
    add r0,sp,#0x44
    add r1,sp,#0x8
    mov r2,#0x6
    mov r3,#0x4
    bl GetGridPositions
    add r0,sp,#0x80
    mov r1,#0x6
    mov r2,#0x4
    bl InitDungeonGrid
    ; CUSTOM ROOM ASSIGNMENTS
    mov r11,#0x1
    ldr r4,=DUNGEON_GRID_COLUMN_BYTES
    mov r5,DUNGEON_GRID_CELL_BYTES
    add r7,sp,#0x80
    mov r8,#0x0
    big_room_octopus_floor_room_x_loop:
    mla r0,r8,r4,r7
    mov r9,#0x0
    big_room_octopus_floor_room_y_loop:
    mla r1,r9,r5,r0
    add r9,r9,#0x1
    strb r11,[r1,#0xA] ; is_room = TRUE
    cmp r9,#0x4
    blt big_room_octopus_floor_room_y_loop
    add r8,r8,#0x1
    cmp r8,#0x6
    blt big_room_octopus_floor_room_x_loop
    add r10,r7,#0x820 ; Invalidate the 4 corner cells;
    strb r11,[r7,#0x8] ; (0,0) 0x8 = 0x008
    strb r11,[r7,#0x62] ; (0,3) 0x5A + 0x8 = 0x062
    strb r11,[r10,#0x10C] ; (5,3) 0x8CA + 0x5A + 0x8 = 0x92C
    strb r11,[r10,#0xB2] ; (5,0) 0x8CA + 0x8 = 0x8D2
    ; CUSTOM ROOM ASSIGNMENTS
    add r1,sp,#0x8
    str r1,[sp,#0x0]
    ldrb r3,[r6,#0xD]
    add r0,sp,#0x80
    mov r1,#0x6
    str r3,[sp,#0x4]
    mov r2,#0x4
    add r3,sp,#0x44
    bl CreateRoomsAndAnchors
    ; CUSTOM GRID CELL CONNECTIONS
    add r0,r7,#0x1D4 ; Connect the legal rooms along the edges to
    add r1,r7,#0x300 ; the rooms in the center. Connect top rooms
    add r2,r7,#0x500 ; down, bottom tops up top, left rooms to the
    add r3,r7,#0x700 ; right and left rooms to the right.
    strb r11,[r0,#0x2] ; (1, 0) 0x1C2 + 0x14 = 0x1D6
    strb r11,[r7,#0x398] ; (2, 0) 0x384 + 0x14 = 0x398
    strb r11,[r2,#0x5A] ; (3, 0) 0x546 + 0x14 = 0x55A
    strb r11,[r2,#0x21C] ; (4, 0) 0x708 + 0x14 = 0x71C
    strb r11,[r0,#0x5B] ; (1, 3) 0x1C2 + 0x5A + 0x13 = 0x22F
    strb r11,[r1,#0xF1] ; (2, 3) 0x384 + 0x5A + 0x13 = 0x3F1
    strb r11,[r2,#0xB3] ; (3, 3) 0x546 + 0x5A + 0x13 = 0x5B3
    strb r11,[r3,#0x75] ; (4, 3) 0x708 + 0x5A + 0x13 = 0x775
    strb r11,[r7,#0x34] ; (0, 1) 0x1E + 0x16 = 0x034
    strb r11,[r7,#0x52] ; (0, 2) 0x3C + 0x16 = 0x052
    strb r11,[r10,#0xDD] ; (5, 1) 0x8CA + 0x1E + 0x15 = 0x8FD
    strb r11,[r10,#0xFB] ; (5, 2) 0x8CA + 0x3C + 0x15 = 0x91B
    ; CUSTOM GRID CELL CONNECTIONS
    add r1,sp,#0x8
    str r1,[sp,#0x0]
    mov r0,#0x1 ; Modified, DISABLE ROOM MERGING
    str r0,[sp,#0x4]
    add r0,sp,#0x80
    mov r1,#0x6
    mov r2,#0x4
    add r3,sp,#0x44
    bl CreateGridCellConnections
    ; CUSTOM ROOM MERGING
    mov r0,#0x1 ; These room merges could be inside a loop
    mov r1,#0x1 ; however, I didn't feel a need to make
    mov r2,#0x1 ; two loops for 4 and 3 merges. Could though.
    add r3,sp,#0x80
    bl MergeRoomsVertically
    mov r0,#0x2
    mov r1,#0x1
    mov r2,#0x1
    add r3,sp,#0x80
    bl MergeRoomsVertically
    mov r0,#0x3
    mov r1,#0x1
    mov r2,#0x1
    add r3,sp,#0x80
    bl MergeRoomsVertically
    mov r0,#0x4
    mov r1,#0x1
    mov r2,#0x1
    add r3,sp,#0x80
    bl MergeRoomsVertically
    mov r0,#0x1
    mov r1,#0x1
    mov r2,#0x1
    add r3,sp,#0x80
    bl MergeRoomsHorizontally
    mov r0,#0x1
    mov r1,#0x1
    mov r2,#0x2
    add r3,sp,#0x80
    bl MergeRoomsHorizontally
    mov r0,#0x1
    mov r1,#0x1
    mov r2,#0x3
    add r3,sp,#0x80
    bl MergeRoomsHorizontally
    ; CUSTOM ROOM MERGING
    ldrb r3,[r6,#9] ; I think generating a maze seems to work however,
    add r0,sp,#0x80 ; the middle room is the only room with the correct
    mov r1,#0x6 ; dimensions to be turned into a maze room if it
    mov r2,#0x4 ; meets the requirements.
    bl GenerateMazeRoom
    ldr r7,=FLOOR_GENERATION_STATUS
    add r0,sp,#0x80
    ldrsh r3,[r7,#0xC]
    mov r1,#0x6
    mov r2,#0x4
    bl GenerateKecleonShop
    add r0,sp,#0x80
    ldrsh r3,[r7,#0x10]
    mov r1,#0x6
    mov r2,#0x4
    bl GenerateMonsterHouse
    ldrb r3,[r6,#0x13]
    add r0,sp,#0x80
    mov r1,#0x6
    mov r2,#0x4
    bl GenerateExtraHallways
    add r0,sp,#0x80
    mov r1,#0x6
    mov r2,#0x4
    bl GenerateRoomImperfections
    add r0,sp,#0x80
    mov r1,#0x6
    mov r2,#0x4
    bl GenerateSecondaryStructures
    add sp,sp,#0xAE0
    add sp,sp,#0x1000
    ldmia sp!,{r3,r4,r5,r6,r7,r8,r9,r10,r11,pc} */
void Generate12RoomOctopusFloor(struct floor_properties *floor_props) {
    struct dungeon_grid_cell grid[6 * HB_GRID_COLS];
    int start_pos_x[16];
    int start_pos_y[16];
    // InitDungeonGrid has a check for the floor size and can ignore the size
    // of the arguments passed for certain sizes... so we must override it for
    // safety reasons to avoid a crash in case the user overwrites the small
    // or medium standard floor layouts with this layout.
    FLOOR_GENERATION_STATUS_PTR->floor_size.val = FLOOR_SIZE_LARGE;
    GetGridPositions(start_pos_x, start_pos_y, 6, 4);
    InitDungeonGrid(grid, 6, 4);
    // Place rooms.
    for (int x = 0; x < 6; ++x) {
        for (int y = 0; y < 4; ++y) {
            grid[(x * HB_GRID_COLS) + y].is_room = true;
        }
    }
    // But not the corners.
    grid[(0 * HB_GRID_COLS) + 0].is_invalid = true;
    grid[(5 * HB_GRID_COLS) + 0].is_invalid = true;
    grid[(0 * HB_GRID_COLS) + 3].is_invalid = true;
    grid[(5 * HB_GRID_COLS) + 3].is_invalid = true;
    CreateRoomsAndAnchors(grid, 6, 4, start_pos_x, start_pos_y, floor_props->room_flags);
    // Connect the rooms.
    for (int x = 0; x < 6; ++x) {
        grid[(x * HB_GRID_COLS) + 0].is_connected_to_bottom = true;
        grid[(x * HB_GRID_COLS) + 3].is_connected_to_top = true;
    }
    for (int y = 0; y < 4; ++y) {
        grid[(0 * HB_GRID_COLS) + y].is_connected_to_right = true;
        grid[(5 * HB_GRID_COLS) + y].is_connected_to_left = true;
    }
    CreateGridCellConnections(grid, 6, 4, start_pos_x, start_pos_y, true);
    // Make the big center room by merging them all together. The vanilla game
    // does this for the bettle layout.
    MergeRoomsVertically(1, 1, 1, grid);
    MergeRoomsVertically(2, 1, 1, grid);
    MergeRoomsVertically(3, 1, 1, grid);
    MergeRoomsVertically(4, 1, 1, grid);
    MergeRoomsHorizontally(1, 1, 1, grid);
    MergeRoomsHorizontally(1, 1, 2, grid);
    MergeRoomsHorizontally(1, 1, 3, grid);
    GenerateMazeRoom(grid, 6, 4, floor_props->maze_room_chance);
    GenerateKecleonShop(grid, 6, 4, floor_props->kecleon_shop_spawn_chance);
    GenerateMonsterHouse(grid, 6, 4, floor_props->monster_house_spawn_chance);
    GenerateExtraHallways(grid, 6, 4, floor_props->extra_hallways);
    GenerateRoomImperfections(grid, 6, 4);
    GenerateSecondaryStructures(grid, 6, 4);
}

void __attribute__((naked)) GenerateHamiltonianCycleFloor(struct floor_properties floor_props) {
    asm("stmdb sp!,{r3,r4,r5,r6,r7,r8,r9,r10,r11,lr}");
    asm("sub sp,sp,#0xAE0");
    asm("sub sp,sp,#0x1000");
    asm("mov r4,r0");
    asm("mov r0,#0"); // next 25 bytes are for keeping track of the connections
    asm("mov r1,#0"); // of the 25 squares (5x5)
    asm("zero_maze_info_loop:");
    asm("str r0,[sp,r1]");
    asm("add r1,#0x4");
    asm("cmp r1,#0x100");
    asm("blt zero_maze_info_loop");
    asm("mov r0,#5");
    asm("bl DungeonRandInt");
    asm("mov r5,r0"); // Random X
    asm("mov r0,#5");
    asm("bl DungeonRandInt");
    asm("mov r6,r0"); // Random Y
    asm("mov r7,#0"); // Zero cells in the frontier.
    asm("add r8,sp,#0x8");
    asm("add r9,sp,#0x24");
    asm("mov r0,#0b11000000");
    asm("mov r1,#5");
    asm("mul r3,r6,r1");
    asm("add r2,r5,r3");
    asm("strb r0,[r8,r2]");
    asm("prim_maze_build_loop:");
    asm("check_add_left_to_frontier:");
    asm("cmp r5,#0"); // Check the left side.
    asm("ble check_add_right_to_frontier");
    asm("mov r1,#5");
    asm("sub r2,r5,#1");
    asm("mul r3,r6,r1");
    asm("add r1,r2,r3");
    asm("ldrb r12,[r8,r1]");
    asm("tst r12,#0b10000000");
    asm("bne check_add_right_to_frontier");
    asm("orr r12,r12,#0b10000000");
    asm("strb r12,[r8,r1]");
    asm("sub r0,r5,#1");
    asm("strb r0,[r9,#0x0]");
    asm("add r7,r7,#1");
    asm("strb r6,[r9,#0x1]");
    asm("add r9,r9,#0x2");
    asm("check_add_right_to_frontier:");
    asm("cmp r5,#4");
    asm("bge check_add_top_to_frontier");
    asm("mov r1,#5");
    asm("add r2,r5,#1");
    asm("mul r3,r6,r1");
    asm("add r1,r2,r3");
    asm("ldrb r12,[r8,r1]");
    asm("tst r12,#0b10000000");
    asm("bne check_add_top_to_frontier");
    asm("orr r12,r12,#0b10000000");
    asm("strb r12,[r8,r1]");
    asm("add r0,r5,#1");
    asm("strb r0,[r9,#0x0]");
    asm("add r7,r7,#1");
    asm("strb r6,[r9,#0x1]");
    asm("add r9,r9,#0x2");
    asm("check_add_top_to_frontier:");
    asm("cmp r6,#0");
    asm("ble check_add_bottom_to_frontier");
    asm("mov r1,#5");
    asm("sub r2,r6,#1");
    asm("mul r3,r2,r1");
    asm("add r1,r5,r3");
    asm("ldrb r12,[r8,r1]");
    asm("tst r12,#0b10000000");
    asm("bne check_add_bottom_to_frontier");
    asm("orr r12,r12,#0b10000000");
    asm("strb r12,[r8,r1]");
    asm("strb r5,[r9,#0x0]");
    asm("add r7,r7,#1");
    asm("sub r0,r6,#1");
    asm("strb r0,[r9,#0x1]");
    asm("add r9,r9,#0x2");
    asm("check_add_bottom_to_frontier:");
    asm("cmp r6,#4");
    asm("bge complete_to_frontier_check");
    asm("mov r1,#5");
    asm("add r2,r6,#1");
    asm("mul r3,r2,r1");
    asm("add r1,r5,r3");
    asm("ldrb r12,[r8,r1]");
    asm("tst r12,#0b10000000");
    asm("bne complete_to_frontier_check");
    asm("orr r12,r12,#0b10000000");
    asm("strb r12,[r8,r1]");
    asm("strb r5,[r9,#0x0]");
    asm("add r7,r7,#1");
    asm("add r0,r6,#1");
    asm("strb r0,[r9,#0x1]");
    asm("add r9,r9,#0x2");
    asm("complete_to_frontier_check:");
    asm("cmp r7,#0");
    asm("beq break_prim_maze_build_loop");
    asm("mov r0,r7");
    asm("bl DungeonRandInt"); // select a random cell in the frontier
    asm("mov r10,r0");
    asm("lsl r3,r10,#1"); // Multiply by 2
    asm("add r2,sp,#0x24");
    asm("ldrb r5,[r2,r3]");
    asm("add r3,#0x1");
    asm("ldrb r6,[r2,r3]");
    asm("mov r0,#5");
    asm("mul r1,r6,r0");
    asm("add r1,r1,r5");
    asm("ldrb r11,[r8,r1]"); // get current connection bitflags
    asm("mov r0,#4");
    asm("bl DungeonRandInt");
    asm("mov r2,r0");
    asm("mov r0,#0");
    asm("cmp r2,#2");
    asm("bgt check_add_left_to_maze");
    asm("beq check_add_right_to_maze");
    asm("cmp r2,#1");
    asm("beq check_add_top_to_maze");
    asm("b check_add_bottom_to_maze");
    asm("check_add_left_to_maze:");
    asm("cmp r5,#0"); // Check the left side.
    asm("ble check_add_right_to_maze");
    asm("mov r1,#5");
    asm("sub r2,r5,#1");
    asm("mul r3,r6,r1");
    asm("add r1,r2,r3");
    asm("ldrb r12,[r8,r1]");
    asm("cmp r12,#0b00000000");
    asm("cmpne r12,#0b10000000");
    asm("beq check_add_right_to_maze");
    asm("orr r12,r12,#0b0100");
    asm("orr r11,r11,#0b0001");
    asm("strb r12,[r8,r1]");
    asm("b cell_added_to_maze");
    asm("check_add_right_to_maze:");
    asm("cmp r5,#4");
    asm("bge check_add_top_to_maze");
    asm("mov r1,#5");
    asm("add r2,r5,#1");
    asm("mul r3,r6,r1");
    asm("add r1,r2,r3");
    asm("ldrb r12,[r8,r1]");
    asm("cmp r12,#0b00000000");
    asm("cmpne r12,#0b10000000");
    asm("beq check_add_top_to_maze");
    asm("orr r12,r12,#0b0001");
    asm("orr r11,r11,#0b0100");
    asm("strb r12,[r8,r1]");
    asm("b cell_added_to_maze");
    asm("check_add_top_to_maze:");
    asm("cmp r6,#0");
    asm("ble check_add_bottom_to_maze");
    asm("mov r1,#5");
    asm("sub r2,r6,#1");
    asm("mul r3,r2,r1");
    asm("add r1,r5,r3");
    asm("ldrb r12,[r8,r1]");
    asm("cmp r12,#0b00000000");
    asm("cmpne r12,#0b10000000");
    asm("beq check_add_bottom_to_maze");
    asm("orr r12,r12,#0b0010");
    asm("orr r11,r11,#0b1000");
    asm("strb r12,[r8,r1]");
    asm("b cell_added_to_maze");
    asm("check_add_bottom_to_maze:");
    asm("cmp r6,#4");
    asm("bge check_add_left_to_maze");
    asm("mov r1,#5");
    asm("add r2,r6,#1");
    asm("mul r3,r2,r1");
    asm("add r1,r5,r3");
    asm("ldrb r12,[r8,r1]");
    asm("cmp r12,#0b00000000");
    asm("cmpne r12,#0b10000000");
    asm("beq check_add_left_to_maze");
    asm("orr r12,r12,#0b1000");
    asm("orr r11,r11,#0b0010");
    asm("strb r12,[r8,r1]");
    // asm("b cell_added_to_maze"); // redundant
    asm("cell_added_to_maze:");
    asm("mov r0,#5");
    asm("mul r1,r6,r0");
    asm("add r1,r1,r5");
    asm("strb r11,[r8,r1]");
    asm("sub r9,r9,#0x2");
    asm("ldrb r0,[r9,#0x0]");
    asm("ldrb r1,[r9,#0x1]");
    asm("lsl r3,r10,#1"); // Multiply by 2
    asm("add r2,sp,#0x24");
    asm("sub r7,r7,#1");
    asm("strb r0,[r2,r3]");
    asm("add r3,#0x1");
    asm("strb r1,[r2,r3]");
    asm("b prim_maze_build_loop");
    asm("break_prim_maze_build_loop:");
    asm("mov r5,#0");
    asm("create_hamiltonian_cycle_loop_x:");
    asm("lsl r10,r5,#2");
    asm("add r10,r10,#3");
    asm("mov r6,#0");
    asm("create_hamiltonian_cycle_loop_y:");
    asm("mov r1,#5");
    asm("lsl r11,r6,#2");
    asm("add r11,r11,#3");
    asm("mul r2,r6,r1");
    asm("add r3,r2,r5");
    asm("ldrb r7,[r8,r3]");
    asm("add r0,r10,#1");
    asm("add r1,r11,#1");
    asm("bl GetTileSafe"); // This tile is always floor.
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0b11");
    asm("orr r1,r1,#0b01");
    asm("strh r1,[r0,#0x0]");
    asm("add r0,r10,#3");
    asm("add r1,r11,#1");
    asm("bl GetTileSafe"); // This tile is always floor.
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0b11");
    asm("orr r1,r1,#0b01");
    asm("strh r1,[r0,#0x0]");
    asm("add r0,r10,#1");
    asm("add r1,r11,#3");
    asm("bl GetTileSafe"); // This tile is always floor.
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0b11");
    asm("orr r1,r1,#0b01");
    asm("strh r1,[r0,#0x0]");
    asm("add r0,r10,#3");
    asm("add r1,r11,#3");
    asm("bl GetTileSafe"); // This tile is always floor.
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0b11");
    asm("orr r1,r1,#0b01");
    asm("strh r1,[r0,#0x0]");
    asm("check_left_connection:");
    asm("tst r7,#0b0001");
    asm("bne connect_left_side");
    asm("add r0,r10,#1");
    asm("add r1,r11,#2");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0b11");
    asm("orr r1,r1,#0b01");
    asm("strh r1,[r0,#0x0]");
    asm("b check_right_connection");
    asm("connect_left_side:");
    asm("add r0,r10,#0");
    asm("add r1,r11,#1");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0b11");
    asm("orr r1,r1,#0b01");
    asm("strh r1,[r0,#0x0]");
    asm("add r0,r10,#0");
    asm("add r1,r11,#3");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0b11");
    asm("orr r1,r1,#0b01");
    asm("strh r1,[r0,#0x0]");
    asm("check_right_connection:");
    asm("tst r7,#0b0100");
    asm("bne check_top_connection");
    asm("add r0,r10,#3");
    asm("add r1,r11,#2");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0b11");
    asm("orr r1,r1,#0b01");
    asm("strh r1,[r0,#0x0]");
    // Redundant connecting right because the connection
    // should be both ways and should be handled by left
    // connection.
    asm("check_top_connection:");
    asm("tst r7,#0b1000");
    asm("bne connect_top_side");
    asm("add r0,r10,#2");
    asm("add r1,r11,#1");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0b11");
    asm("orr r1,r1,#0b01");
    asm("strh r1,[r0,#0x0]");
    asm("b check_bottom_connection");
    asm("connect_top_side:");
    asm("add r0,r10,#1");
    asm("add r1,r11,#0");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0b11");
    asm("orr r1,r1,#0b01");
    asm("strh r1,[r0,#0x0]");
    asm("add r0,r10,#3");
    asm("add r1,r11,#0");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0b11");
    asm("orr r1,r1,#0b01");
    asm("strh r1,[r0,#0x0]");
    asm("check_bottom_connection:");
    asm("tst r7,#0b0010");
    asm("bne check_connection_complete");
    asm("add r0,r10,#2");
    asm("add r1,r11,#3");
    asm("bl GetTileSafe");
    asm("ldrh r1,[r0,#0x0]");
    asm("bic r1,r1,#0b11");
    asm("orr r1,r1,#0b01");
    asm("strh r1,[r0,#0x0]");
    // Redundant connecting bottom because the connection
    // should be both ways and should be handled by top
    // connection.
    asm("check_connection_complete:");
    asm("add r6,r6,#1");
    asm("cmp r6,#5");
    asm("blt create_hamiltonian_cycle_loop_y");
    asm("add r5,r5,#1");
    asm("cmp r5,#5");
    asm("blt create_hamiltonian_cycle_loop_x");
    asm("mov r0,#0x3");
    asm("bl DungeonRandInt");
    asm("ldrsb r1,[r4,#0x1]"); // Room Density
    asm("cmp r1,#0x0");
    asm("rsblt r10,r1,#0x0"); // If negative, use abs value.
    asm("addge r10,r1,r0");
    asm("cmp r10,#0x4");   // Less than 4 rooms fail the check for enough room
    asm("movlt r10,#0x4"); // tiles. For some reason 3 doesn't work...
    asm("cmp r10,#0xF");   // Could do more rooms, but that would require more
    asm("movgt r10,#0xF"); // work... and 15 is enough anyway I'm sure.
    asm("add r0,sp,#0x80");
    asm("mov r1,#0x1");
    asm("mov r2,r10"); // 4 -> 15
    asm("bl InitDungeonGrid");
    asm("mov r0,#0");
    asm("add r6,sp,#0xC");
    asm("init_room_select_loop:");
    asm("strb r0,[r6,r0]");
    asm("add r0,r0,#1");
    asm("cmp r0,#16");
    asm("blt init_room_select_loop");
    asm("mov r5,#0");
    asm("select_room_loop:"); // Randomly select rooms to fill out until
    asm("mov r0,r5");
    asm("mov r1,#16");
    asm("bl DungeonRandRange");
    asm("ldrb r2,[r6,r5]");
    asm("ldrb r1,[r6,r0]");
    asm("strb r1,[r6,r5]");
    asm("strb r2,[r6,r0]");
    asm("add r5,r5,#1");
    asm("cmp r5,r10");
    asm("blt select_room_loop");
    // 00   01 02 03   04
    // 
    // 05              06
    // 07              08
    // 09              10
    // 
    // 11   12 13 14   15
    asm("mov r8,#0");
    asm("add r9,sp,#0x80");
    // Maybe a bit jank? I could also like make a list for the coordinates
    // of each place or maybe like to number them in a different way? IDK
    // should probably number them in a different way and then I could
    // make a 3x3 grid of cords with 4 exceptions for the corners.
    // (or make life easier and ignore the corners).
    asm("create_rooms_loop:");
    asm("ldrb r7,[r6,r8]");
    asm("cmp r7,#0");
    asm("cmpne r7,#5");
    asm("cmpne r7,#7");
    asm("cmpne r7,#9");
    asm("cmpne r7,#11");
    asm("moveq r0,#2");
    asm("moveq r2,#5");
    asm("beq handle_room_y");
    asm("cmp r7,#1");
    asm("cmpne r7,#12");
    asm("moveq r0,#8");
    asm("moveq r2,#11");
    asm("beq handle_room_y");
    asm("cmp r7,#2");
    asm("cmpne r7,#13");
    asm("moveq r0,#12");
    asm("moveq r2,#15");
    asm("beq handle_room_y");
    asm("cmp r7,#3");
    asm("cmpne r7,#14");
    asm("moveq r0,#16");
    asm("moveq r2,#19");
    asm("beq handle_room_y");
    asm("mov r0,#22");
    asm("mov r2,#25");
    asm("handle_room_y:");
    asm("cmp r7,#4");
    asm("movle r1,#2");
    asm("movle r3,#5");
    asm("ble actually_make_room");
    asm("cmp r7,#6");
    asm("movle r1,#8");
    asm("movle r3,#11");
    asm("ble actually_make_room");
    asm("cmp r7,#8");
    asm("movle r1,#12");
    asm("movle r3,#15");
    asm("ble actually_make_room");
    asm("cmp r7,#10");
    asm("movle r1,#16");
    asm("movle r3,#19");
    asm("ble actually_make_room");
    asm("mov r1,#22");
    asm("mov r3,#25");
    asm("actually_make_room:");
    asm("strb r8,[sp,#0x0]");
    asm("str r9,[sp,#0x4]");
    asm("mov r12,#0");
    asm("strb r12,[sp,#0x8]"); // Force no room imperfections.
    asm("bl CreateRoomInCell");
    asm("add r8,r8,#1");
    asm("add r9,r9,#0x1E"); // DUNGEON_GRID_CELL_BYTES
    asm("cmp r8,r10");
    asm("blt create_rooms_loop");
    asm("ldr r7,=FLOOR_GENERATION_STATUS");
    asm("add r0,sp,#0x80");
    asm("ldrsh r3,[r7,#0x10]");
    asm("mov r1,#0x1");
    asm("mov r2,r10");
    asm("bl GenerateMonsterHouse");
    asm("ldrb r3,[r4,#0x13]");
    asm("add r0,sp,#0x80");
    asm("mov r1,#0x1");
    asm("mov r2,r10");
    asm("bl GenerateExtraHallways");
    asm("add r0,sp,#0x80");
    asm("mov r1,#0x1");
    asm("mov r2,r10");
    asm("bl GenerateSecondaryStructures");
    asm("add sp,sp,#0xAE0");
    asm("add sp,sp,#0x1000");
    asm("ldmia sp!,{r3,r4,r5,r6,r7,r8,r9,r10,r11,pc}");
}

/* The original assembly version of this function has been left in the
// codebase for reference, but it has been rewritten in C.
GenerateMiniSpiralFloor:
    stmdb sp!,{r3,r4,r5,r6,r7,r8,r9,r10,r11,lr}
    sub sp,sp,#0xAE0
    sub sp,sp,#0x1000
    mov r6,r0 ; r6 = param_0 (Floor Properties)
    add r0,sp,#0x80
    mov r1,#0x1
    mov r2,#0x5
    bl InitDungeonGrid
    ; CUSTOM ROOM CREATION
    ldrb r7,[r6,#0xD]
    add r4,sp,#0x80
    mov r0,#0x5
    mov r1,#0x8
    bl DungeonRandRange
    mov r5,r0
    mov r0,#0x5
    mov r1,#0x8
    bl DungeonRandRange
    mov r12,#0x0
    add r3,r0,#0x2 ; End Y
    mov r1,#0x2 ; Start Y
    add r2,r5,#0xE ; End X
    mov r0,#0xE ; Start X
    strb r12,[sp,#0x0] ; Room 0 (Top Left)
    str r4,[sp,#0x4]
    strb r7,[sp,#0x8]
    bl CreateRoomInCell
    add r4,r4,DUNGEON_GRID_CELL_BYTES
    mov r0,#0x5
    mov r1,#0x8
    bl DungeonRandRange
    mov r5,r0
    mov r0,#0x5
    mov r1,#0x8
    bl DungeonRandRange
    mov r12,#0x1
    mov r3,#0x1E ; End Y
    sub r1,r3,r0 ; Start Y
    add r2,r5,#0xE ; End X
    mov r0,#0xE ; Start X
    strb r12,[sp,#0x0] ; Room 1 (Bottom Left)
    str r4,[sp,#0x4]
    strb r7,[sp,#0x8]
    bl CreateRoomInCell
    add r4,r4,DUNGEON_GRID_CELL_BYTES
    mov r0,#0x5
    mov r1,#0x8
    bl DungeonRandRange
    mov r5,r0
    mov r0,#0x5
    mov r1,#0x8
    bl DungeonRandRange
    mov r12,#0x2
    add r3,r0,#0x2 ; End Y
    mov r1,#0x2 ; Start Y
    mov r2,#0x2A ; End X
    sub r0,r2,r5 ; Start X
    strb r12,[sp,#0x0] ; Room 2 (Top Right)
    str r4,[sp,#0x4]
    strb r7,[sp,#0x8]
    bl CreateRoomInCell
    add r4,r4,DUNGEON_GRID_CELL_BYTES
    mov r0,#0x5
    mov r1,#0x8
    bl DungeonRandRange
    mov r5,r0
    mov r0,#0x5
    mov r1,#0x8
    bl DungeonRandRange
    mov r12,#0x3
    mov r3,#0x1E ; End Y
    sub r1,r3,r0 ; Start Y
    mov r2,#0x2A ; End X
    sub r0,r2,r5 ; Start X
    strb r12,[sp,#0x0] ; Room 3 (Bottom Right)
    str r4,[sp,#0x4]
    strb r7,[sp,#0x8]
    bl CreateRoomInCell
    add r4,r4,DUNGEON_GRID_CELL_BYTES
    mov r0,#0x7
    mov r1,#0xB
    bl DungeonRandRange
    mov r5,r0
    mov r1,#0xA
    sub r0,r1,r0
    bl DungeonRandInt
    mov r8,r0
    mov r0,#0x7
    mov r1,#0xB
    bl DungeonRandRange
    mov r9,r0
    mov r1,#0xA
    sub r0,r1,r0
    bl DungeonRandInt
    mov r12,#0x4
    add r1,r0,#0xB ; Start Y
    add r3,r1,r9 ; End Y
    add r0,r8,#0x17 ; Start X
    add r2,r0,r5 ; End X
    strb r12,[sp,#0x0] ; Room 4 (Middle)
    str r4,[sp,#0x4]
    strb r7,[sp,#0x8]
    bl CreateRoomInCell
    ; CUSTOM ROOM CREATION
    ; CUSTOM HALLWAY GENERATION
    mov r0,#0x4
    bl DungeonRandInt
    mov r11,r0
    mov r5,#0x0
    add r4,sp,#0x80
    cmp r11,#0x0
    beq mini_spiral_floor_skip_hallway0
    ldrh r0,[r4,#0x2] ; Room 0, Start Y
    ldrh r1,[r4,#0x6] ; Room 0, End Y
    bl DungeonRandRange
    mov r7,r0
    ldrh r0,[r4,#0x3E] ; Room 2, Start Y
    ldrh r1,[r4,#0x42] ; Room 2, End Y
    bl DungeonRandRange
    mov r8,r0
    mov r0,#0x16 ; 22
    mov r1,#0x22 ; 34
    bl DungeonRandRange
    str r0,[sp,#0x4]
    ldrh r0,[r4,#0x4] ; Room 0, End X
    mov r1,r7
    ldrh r2,[r4,#0x3C] ; Room 2, Start X
    mov r3,r8
    strb r5,[sp,#0x0]
    bl CreateHallway ; (Top Left -> Top Right)
    mini_spiral_floor_skip_hallway0:
    cmp r11,#0x1
    beq mini_spiral_floor_skip_hallway1
    ldrh r0,[r4,#0x20] ; Room 1, Start Y
    ldrh r1,[r4,#0x24] ; Room 1, End Y
    bl DungeonRandRange
    mov r7,r0
    ldrh r0,[r4,#0x5C] ; Room 2, Start Y
    ldrh r1,[r4,#0x60] ; Room 2, End Y
    bl DungeonRandRange
    mov r8,r0
    mov r0,#0x16 ; 22
    mov r1,#0x22 ; 34
    bl DungeonRandRange
    str r0,[sp,#0x4]
    ldrh r0,[r4,#0x22] ; Room 1, End X
    mov r1,r7
    ldrh r2,[r4,#0x5A] ; Room 3, Start X
    mov r3,r8
    strb r5,[sp,#0x0]
    bl CreateHallway ; (Bottom Left -> Bottom Right)
    mini_spiral_floor_skip_hallway1:
    mov r5,#0x1
    cmp r11,#0x2
    beq mini_spiral_floor_skip_hallway2
    ldrh r0,[r4,#0x0] ; Room 0, Start X
    ldrh r1,[r4,#0x4] ; Room 0, End X
    bl DungeonRandRange
    mov r7,r0
    ldrh r0,[r4,#0x1E] ; Room 1, Start X
    ldrh r1,[r4,#0x22] ; Room 1, End X
    bl DungeonRandRange
    mov r8,r0
    mov r0,#0xA ; 10
    mov r1,#0x16 ; 22
    bl DungeonRandRange
    str r0,[sp,#0x8]
    mov r0,r7
    ldrh r1,[r4,#0x6] ; Room 0, End Y
    mov r2,r8
    ldrh r3,[r4,#0x20] ; Room 1, Start Y
    strb r5,[sp,#0x0]
    bl CreateHallway ; (Top Left -> Bottom Left)
    mini_spiral_floor_skip_hallway2:
    cmp r11,#0x3
    beq mini_spiral_floor_skip_hallway3
    ldrh r0,[r4,#0x3C] ; Room 2, Start X
    ldrh r1,[r4,#0x40] ; Room 2, End X
    bl DungeonRandRange
    mov r7,r0
    ldrh r0,[r4,#0x5A] ; Room 3, Start X
    ldrh r1,[r4,#0x5E] ; Room 3, End X
    bl DungeonRandRange
    mov r8,r0
    mov r0,#0xA ; 10
    mov r1,#0x16 ; 22
    bl DungeonRandRange
    str r0,[sp,#0x8]
    mov r0,r7
    ldrh r1,[r4,#0x42] ; Room 2, End Y
    mov r2,r8
    ldrh r3,[r4,#0x5C] ; Room 3, Start Y
    strb r5,[sp,#0x0]
    bl CreateHallway ; (Top Right -> Bottom Right)
    mini_spiral_floor_skip_hallway3:
    mov r1,DUNGEON_GRID_CELL_BYTES
    cmp r11,#0x1
    movlt r2,#0x0
    moveq r2,#0x3
    cmp r11,#0x2
    subge r2,r11,#0x1
    mul r4,r2,r1
    add r5,sp,#0x80
    add r4,r4,r5
    ldrh r0,[r4,#0x0] ; Room Start X
    ldrh r1,[r4,#0x4] ; Room End X
    bl DungeonRandRange
    mov r7,r0
    ldrh r0,[r4,#0x2] ; Room Start Y
    ldrh r1,[r4,#0x6] ; Room End Y
    bl DungeonRandRange
    mov r8,r0
    cmp r11,#0x1
    bgt mini_spiral_right_left
    ldrneh r10,[r5,#0x7A] ; Room 4 Start Y
    ldreqh r10,[r5,#0x7E] ; Room 4 End Y
    ldrh r0,[r5,#0x78] ; Room 4 Start X
    ldrh r1,[r5,#0x7C] ; Room 4 End X
    bl DungeonRandRange
    str r0,[sp,#0x4]
    mov r1,r10
    mov r12,#0x0
    b mini_spiral_hallway_finalize
    mini_spiral_right_left:
    cmp r11,#0x2
    ldreqh r10,[r5,#0x78] ; Room 4 Start X
    ldrneh r10,[r5,#0x7C] ; Room 4 End X
    ldrh r0,[r5,#0x7A] ; Room 4 Start Y
    ldrh r1,[r5,#0x7E] ; Room 4 End Y
    bl DungeonRandRange
    mov r1,r0
    str r0,[sp,#0x8]
    mov r0,r10
    mov r12,#0x1
    mini_spiral_hallway_finalize:
    str r12,[sp,#0x0]
    mov r2,r7
    mov r3,r8
    bl CreateHallway
    ; CUSTOM HALLWAY GENERATION
    ldrb r3,[r6,#0x9]
    add r0,sp,#0x80
    mov r1,#0x1
    mov r2,#0x5
    bl GenerateMazeRoom
    ldr r7,=FLOOR_GENERATION_STATUS
    add r0,sp,#0x80
    ldrsh r3,[r7,#0xC]
    mov r1,#0x1
    mov r2,#0x5
    bl GenerateKecleonShop
    add r0,sp,#0x80
    ldrsh r3,[r7,#0x10]
    mov r1,#0x1
    mov r2,#0x5
    bl GenerateMonsterHouse
    ldrb r3,[r6,#0x13]
    add r0,sp,#0x80
    mov r1,#0x1
    mov r2,#0x5
    bl GenerateExtraHallways
    add r0,sp,#0x80
    mov r1,#0x1
    mov r2,#0x5
    bl GenerateRoomImperfections
    add r0,sp,#0x80
    mov r1,#0x1
    mov r2,#0x5
    bl GenerateSecondaryStructures
    add sp,sp,#0xAE0
    add sp,sp,#0x1000
    ldmia sp!,{r3,r4,r5,r6,r7,r8,r9,r10,r11,pc} */
void GenerateMiniSpiralFloor(struct floor_properties *floor_props) {
    // Setup the grid.
    struct dungeon_grid_cell grid[HB_GRID_COLS];
    struct dungeon_grid_cell *room_0 = &grid[0];
    struct dungeon_grid_cell *room_1 = &grid[1];
    struct dungeon_grid_cell *room_2 = &grid[2];
    struct dungeon_grid_cell *room_3 = &grid[3];
    struct dungeon_grid_cell *room_center = &grid[4];
    InitDungeonGrid(grid, 1, 5);

    // Place the rooms.
    int rand_x = DungeonRandRange(5, 8);
    int rand_y = DungeonRandRange(5, 8); // Top Left
    CreateRoomInCell(14, 2, rand_x + 14, rand_y + 2, 0, room_0, floor_props->room_flags);
    rand_x = DungeonRandRange(5, 8);
    rand_y = DungeonRandRange(5, 8); // Bottom Right
    CreateRoomInCell(14, 30 - rand_y, rand_x + 14, 30, 1, room_1, floor_props->room_flags);
    rand_x = DungeonRandRange(5, 8);
    rand_y = DungeonRandRange(5, 8); // Top Right
    CreateRoomInCell(42 - rand_x, 2, 42, rand_y + 2, 2, room_2, floor_props->room_flags);
    rand_x = DungeonRandRange(5, 8);
    rand_y = DungeonRandRange(5, 8); // Bottom Right
    CreateRoomInCell(42 - rand_x, 30 - rand_y, 42, 30, 3, room_3, floor_props->room_flags);
    int center_width = DungeonRandRange(7, 11);
    int center_start_x = DungeonRandInt(10 - center_width) + 23;
    int center_height = DungeonRandRange(7, 11);
    int center_start_y = DungeonRandInt(10 - center_height) + 11;
    CreateRoomInCell(center_start_x, center_start_y, center_start_x + center_width, center_start_y + center_height, 4, room_center, floor_props->room_flags);

    // Create the hallways around the edges.
    int spiral_connector_location = DungeonRandInt(4);
    if (0 != spiral_connector_location) {
        int y0 = DungeonRandRange(room_0->start_y, room_0->end_y);
        int y1 = DungeonRandRange(room_2->start_y, room_2->end_y);
        int mid_x = DungeonRandRange(22, 34);
        CreateHallway(room_0->end_x, y0, room_2->start_x, y1, false, mid_x, 0);
    }
    if (1 != spiral_connector_location) {
        int y0 = DungeonRandRange(room_1->start_y, room_1->end_y);
        int y1 = DungeonRandRange(room_3->start_y, room_3->end_y);
        int mid_x = DungeonRandRange(22, 34);
        CreateHallway(room_1->end_x, y0, room_3->start_x, y1, false, mid_x, 0);
    }
    if (2 != spiral_connector_location) {
        int x0 = DungeonRandRange(room_0->start_x, room_0->end_x);
        int x1 = DungeonRandRange(room_1->start_x, room_1->end_x);
        int mid_y = DungeonRandRange(10, 22);
        CreateHallway(x0, room_0->end_y, x1, room_1->start_y, true, 0, mid_y);
    }
    if (3 != spiral_connector_location) {
        int x0 = DungeonRandRange(room_2->start_x, room_2->end_x);
        int x1 = DungeonRandRange(room_3->start_x, room_3->end_x);
        int mid_y = DungeonRandRange(10, 22);
        CreateHallway(x0, room_2->end_y, x1, room_3->start_y, true, 0, mid_y);
    }

    // Create the hallway connecting the center room to the outer rooms.
    bool vertical;
    int target_x, target_y, source_x, source_y;
    if (spiral_connector_location == 0) {
        target_x = DungeonRandRange(room_0->start_x, room_0->end_x);
        target_y = DungeonRandRange(room_0->start_y, room_0->end_y);
        source_x = DungeonRandRange(room_center->start_x, room_center->end_x);
        source_y = room_center->start_y;
        vertical = false;
    } else if (spiral_connector_location == 1) {
        target_x = DungeonRandRange(room_3->start_x, room_3->end_x);
        target_y = DungeonRandRange(room_3->start_y, room_3->end_y);
        source_x = DungeonRandRange(room_center->start_x, room_center->end_x);
        source_y = room_center->end_y;
        vertical = false;
    } else if (spiral_connector_location == 2) {
        target_x = DungeonRandRange(room_1->start_x, room_1->end_x);
        target_y = DungeonRandRange(room_1->start_y, room_1->end_y);
        source_x = room_center->start_x;
        source_y = DungeonRandRange(room_center->start_y, room_center->end_y);
        vertical = true;
    } else {
        target_x = DungeonRandRange(room_2->start_x, room_2->end_x);
        target_y = DungeonRandRange(room_2->start_y, room_2->end_y);
        source_x = room_center->end_x;
        source_y = DungeonRandRange(room_center->start_y, room_center->end_y);
        vertical = true;
    }
    CreateHallway(source_x, source_y, target_x, target_y, vertical, source_x, source_y);

    // Normal dungeon generation.
    GenerateMazeRoom(grid, 1, 5, floor_props->maze_room_chance);
    GenerateKecleonShop(grid, 1, 5, floor_props->kecleon_shop_spawn_chance);
    GenerateMonsterHouse(grid, 1, 5, floor_props->monster_house_spawn_chance);
    GenerateExtraHallways(grid, 1, 5, floor_props->extra_hallways);
    GenerateRoomImperfections(grid, 1, 5);
    GenerateSecondaryStructures(grid, 1, 5);
}

/* The original assembly version of this function has been left in the
// codebase for reference, but it has been rewritten in C.
GenerateFourCornerSquareFloor:
    stmdb sp!,{r3,r4,r5,r6,r7,r8,r9,r10,r11,lr}
    sub sp,sp,#0xAE0
    sub sp,sp,#0x1000
    mov r6,r0 ; r6 = param_0 (Floor Properties)
    add r0,sp,#0x80
    mov r1,#0x2
    mov r2,#0x2
    bl InitDungeonGrid
    ; CUSTOM ROOM ASSIGNMENT AND GENERATION
    add r4,sp,#0x80
    add r9,r4,#0x100
    mov r5,#0x1
    strb r5,[r4,#0xA] ; (0,0) 0xA = 0x00A
    strb r5,[r4,#0x28] ; (0,1) 0x1E + 0xA = 0x028
    strb r5,[r4,#0x1CC] ; (1,0) 0x1C2 + 0xA = 0x1CC
    strb r5,[r9,#0xEA] ; (1,1) 0x1C2 + 0x1E + 0xA = 0x1EA
    mov r0,#0x5 ; 5
    mov r1,#0xD ; 13
    bl DungeonRandRange ; 5 -> 12
    cmp r0,#0xC ; 12
    orrlt r7,r0,#0x1 ; Odd number bias, like normal generation.
    movge r7,r0
    mov r1,#0x36 ; This loop looks a bit weird, because it fills
    mov r2,#0x1E ; out all corners at once.
    sub r8,r1,r7 ; Start2 X
    sub r5,r2,r7 ; Start2 Y
    mov r10,#0x2 ; Start1 X
    mov r11,#0x2 ; Start1 Y
    add r12,r10,#0xC
    strb r12,[r4,#0x0] ; (0,0) 0x0 = 0x000
    strb r11,[r4,#0x2] ; (0,0) 0x2 = 0x002
    strb r12,[r4,#0x1E] ; (0,1) 0x1E + 0x0 = 0x01E
    strb r5,[r4,#0x20] ; (0,1) 0x1E + 0x2 = 0x020
    sub r12,r8,#0xC
    strb r12,[r9,#0xC2] ; (1,0) 0x1C2 + 0x0 = 0x1C2
    strb r11,[r4,#0x1C4] ; (1,0) 0x1C2 + 0x2 = 0x1C4
    strb r12,[r4,#0x1E0] ; (1,1) 0x1C2 + 0x1E + 0x0 = 0x1E0
    strb r5,[r9,#0xE2] ; (1,1) 0x1C2 + 0x1E + 0x2 = 0x1E2
    add r7,r7,#0x2 ; For End Condition of X & Y Loop
    generate_four_corner_floor_x_loop:
    mov r9,r5 ; Reinitalize Y values.
    mov r11,#0x2
    generate_four_corner_floor_y_loop:
    add r0,r10,#0xC
    mov r1,r11
    bl GetTileSafe
    mov r2,#0x0
    ldrh r1,[r0,#0x0]
    bic r1,r1,#0x3
    orr r1,r1,#0x1
    strh r1,[r0,#0x0]
    strb r2,[r0,#0x7]
    add r0,r10,#0xC
    mov r1,r9
    bl GetTileSafe
    mov r2,#0x1
    ldrh r1,[r0,#0x0]
    bic r1,r1,#0x3
    orr r1,r1,#0x1
    strh r1,[r0,#0x0]
    strb r2,[r0,#0x7]
    sub r0,r8,#0xC
    mov r1,r11
    bl GetTileSafe
    mov r2,#0x2
    ldrh r1,[r0,#0x0]
    bic r1,r1,#0x3
    orr r1,r1,#0x1
    strh r1,[r0,#0x0]
    strb r2,[r0,#0x7]
    sub r0,r8,#0xC
    mov r1,r9
    bl GetTileSafe
    mov r2,#0x3
    ldrh r1,[r0,#0x0]
    bic r1,r1,#0x3
    orr r1,r1,#0x1
    strh r1,[r0,#0x0]
    strb r2,[r0,#0x7]
    add r11,r11,#0x1
    add r9,r9,#0x1
    cmp r11,r7
    blt generate_four_corner_floor_y_loop
    add r10,r10,#0x1
    add r8,r8,#0x1
    cmp r10,r7
    blt generate_four_corner_floor_x_loop
    add r10,r10,#0xC
    add r5,r4,#0x100
    sub r8,r8,#0xC ; Save End X/Y of Rooms
    strb r10,[r4,#0x4] ; (0,0) 0x4 = 0x004
    strb r11,[r4,#0x6] ; (0,0) 0x6 = 0x006
    strb r10,[r4,#0x22] ; (0,1) 0x1E + 0x4 = 0x022
    strb r9,[r4,#0x24] ; (0,1) 0x1E + 0x6 = 0x024
    strb r8,[r5,#0xC6] ; (1,0) 0x1C2 + 0x4 = 0x1C6
    strb r11,[r4,#0x1C8] ; (1,0) 0x1C2 + 0x6 = 0x1C8
    strb r8,[r4,#0x1E4] ; (1,1) 0x1C2 + 0x1E + 0x4 = 0x1E4
    strb r9,[r5,#0xE6] ; (1,1) 0x1C2 + 0x1E + 0x6 = 0x1E6
    mov r0,#0x0
    mov r1,#0x0
    add r2,sp,#0x80
    ldrb r3,[r6,#0xD]
    bl TryFlagForImperfectionsAndSecondaryStructures
    mov r0,#0x0
    mov r1,#0x1
    add r2,sp,#0x80
    ldrb r3,[r6,#0xD]
    bl TryFlagForImperfectionsAndSecondaryStructures
    mov r0,#0x1
    mov r1,#0x0
    add r2,sp,#0x80
    ldrb r3,[r6,#0xD]
    bl TryFlagForImperfectionsAndSecondaryStructures
    mov r0,#0x1
    mov r1,#0x1
    add r2,sp,#0x80
    ldrb r3,[r6,#0xD]
    bl TryFlagForImperfectionsAndSecondaryStructures
    ; CUSTOM ROOM ASSIGNMENT AND GENERATION
    ; CUSTOM CONNECTION ASSIGNMENTS AND GENERATION
    mov r8,#0x0 ; Connect the rooms along the outermost edge. Use
    str r8,[sp,#0x0] ; the ending values of the previous loop to figure
    mov r0,r10 ; out where to start the hallways.
    mov r1,#0x2
    mov r2,#0x29
    str r2,[sp,#0x4]
    mov r3,#0x2
    bl CreateHallway
    str r8,[sp,#0x0]
    mov r0,r10
    mov r1,#0x1D
    mov r2,#0x29
    str r2,[sp,#0x4]
    mov r3,#0x1D
    bl CreateHallway
    str r9,[sp,#0x0]
    mov r0,#0xE
    mov r1,r11
    mov r2,#0xE
    mov r3,#0x1D
    str r3,[sp,#0x8]
    bl CreateHallway
    str r9,[sp,#0x0]
    mov r0,#0x29
    mov r1,r11
    mov r2,#0x29
    mov r3,#0x1D
    str r3,[sp,#0x8]
    bl CreateHallway
    strb r9,[r4,#0xB] ; (0,0) 0xB = 0x00B
    strb r9,[r4,#0x28] ; (0,1) 0x1E + 0xB = 0x029
    strb r9,[r5,#0xCD] ; (1,0) 0x1C2 + 0xB = 0x1CD
    strb r9,[r5,#0xEB] ; (1,1) 0x1C2 + 0x1E + 0xB = 0x1EB
    ; CUSTOM CONNECTION ASSIGNMENTS AND GENERATION
    ldrb r3,[r6,#0x9]
    add r0,sp,#0x80
    mov r1,#0x2
    mov r2,#0x2
    bl GenerateMazeRoom
    ldr r7,=FLOOR_GENERATION_STATUS
    add r0,sp,#0x80 ; The rooms must have is_connected be true for
    ldrsh r3,[r7,#0xC] ; or else this function wont spawn a Kecleon
    mov r1,#0x2 ; Shop. IDK Why having is_connected needs to be
    mov r2,#0x2 ; true...
    bl GenerateKecleonShop
    add r0,sp,#0x80
    ldrsh r3,[r7,#0x10]
    mov r1,#0x2
    mov r2,#0x2
    bl GenerateMonsterHouse
    ldrb r3,[r6,#0x13]
    add r0,sp,#0x80
    mov r1,#0x2
    mov r2,#0x2
    bl GenerateExtraHallways
    add r0,sp,#0x80
    mov r1,#0x2
    mov r2,#0x2
    bl GenerateRoomImperfections
    add r0,sp,#0x80
    mov r1,#0x2
    mov r2,#0x2
    bl GenerateSecondaryStructures
    add sp,sp,#0xAE0
    add sp,sp,#0x1000
    ldmia sp!,{r3,r4,r5,r6,r7,r8,r9,r10,r11,pc} */
void GenerateFourCornerFloor(struct floor_properties *floor_props) {
    struct dungeon_grid_cell grid[HB_GRID_COLS];
    InitDungeonGrid(grid, 1, 4);
    
    // Generate the rooms.
    int room_size = DungeonRandRange(5, 13);
    if (room_size < 12) {
        room_size |= 0x1;  // Make odd with bias
    }
    CreateRoomInCell(14, 2, 14 + room_size, 2 + room_size, 0, &grid[0], floor_props->room_flags);
    CreateRoomInCell(14, 30 - room_size, 14 + room_size, 30, 1, &grid[1], floor_props->room_flags);
    CreateRoomInCell(42 - room_size, 2, 42, 2 + room_size, 2, &grid[2], floor_props->room_flags);
    CreateRoomInCell(42 - room_size, 30 - room_size, 42, 30, 3, &grid[3], floor_props->room_flags);
    
    // Create the four hallways around the egdes.
    int end_x_left = 14 + room_size;
    int start_x_right = 42 - room_size;
    int end_y_top = 2 + room_size;
    CreateHallway(end_x_left, 2, start_x_right, 2, false, 41, 0); // Top
    CreateHallway(end_x_left, 29, start_x_right, 29, false, 41, 0); // Bottom
    CreateHallway(14, end_y_top, 14, 29, true, 0, 29); // Left
    CreateHallway(41, end_y_top, 41, 29, true, 0, 29); // Right
    
    // Normal dungeon generation.
    GenerateMazeRoom(grid, 1, 4, floor_props->maze_room_chance);
    GenerateKecleonShop(grid, 1, 4, floor_props->kecleon_shop_spawn_chance);
    GenerateMonsterHouse(grid, 1, 4, floor_props->monster_house_spawn_chance);
    GenerateExtraHallways(grid, 1, 4, floor_props->extra_hallways);
    GenerateRoomImperfections(grid, 1, 4);
    GenerateSecondaryStructures(grid, 1, 4);
}

/* The original assembly version of this function has been left in the
// codebase for reference, but it has been rewritten in C.
GenerateHollowPlusFloor:
    stmdb sp!,{r3,r4,r5,r6,r7,r8,lr}
    sub sp,sp,#0xAE0
    sub sp,sp,#0x1000
    mov r6,r0 ; r6 = param_0 (Floor Properties)
    ; CUSTOM GRID POSITIONS
    mov r0,#0xC ; 12 (x)
    mov r1,#0x14 ; 20 (x)
    mov r2,#0x1C ; 28 (x)
    mov r3,#0x24 ; 36 (x)
    mov r4,#0x2C ; 44 (x)
    str r0,[sp,#0x44]
    str r1,[sp,#0x48]
    str r2,[sp,#0x4C]
    str r3,[sp,#0x50]
    str r4,[sp,#0x54]
    mov r0,#0x0 ; 0 (y)
    mov r1,#0x8 ; 8 (y)
    mov r2,#0x10 ; 16 (y)
    mov r3,#0x18 ; 24 (y)
    mov r4,#0x20 ; 32 (y)
    str r0,[sp,#0x8]
    str r1,[sp,#0xC]
    str r2,[sp,#0x10]
    str r3,[sp,#0x14]
    str r4,[sp,#0x18]
    ; CUSTOM GRID POSITIONS
    add r0,sp,#0x80
    mov r1,#0x4
    mov r2,#0x4
    bl InitDungeonGrid
    ; CUSTOM ROOM ASSIGNMENTS
    add r4,sp,#0x80
    mov r5,#0x1
    mov r0,#0x0
    add r7,r4,#0xFE ; Invalidate the 4 corner cells, make
    add r3,r7,#0x400 ; the sides rooms, and make the center
    add r2,r7,#0x200 ; anchors/hallways.
    strb r5,[r4,#0x8] ; (0,0) 0x8 = 0x008
    strb r5,[r3,#0x50] ; (3,0) 0x546 + 0x8 = 0x54E
    strb r5,[r3,#0xAA] ; (3,3) 0x546 + 0x5A + 0x8 = 0x5A8
    strb r5,[r4,#0x62] ; (0,3) 0x5A + 0x8 = 0x62
    strb r0,[r7,#0xEC] ; (1,1) 0x1C2 + 0x1E + 0xA = 0x1EA
    strb r0,[r4,#0x208] ; (1,2) 0x1C2 + 0x3C + 0xA = 0x208
    strb r0,[r2,#0xAE] ; (2,1) 0x384 + 0x1E + 0xA = 0x3AC
    strb r0,[r7,#0x2CC] ; (2,2) 0x384 + 0x3C + 0xA = 0x3CA
    strb r5,[r4,#0x28] ; (0,1) 0x1E + 0xA = 0x028
    strb r5,[r4,#0x1CC] ; (1,0) 0x1C2 + 0xA = 0x1CC
    strb r5,[r7,#0x290] ; (2,0) 0x384 + 0xA = 0x38E
    strb r5,[r7,#0x470] ; (3,1) 0x546 + 0x1E + 0xA = 0x56E
    strb r5,[r4,#0x46] ; (0,2) 0x3C + 0xA = 0x046
    strb r5,[r7,#0x128] ; (1,3) 0x1C2 + 0x5A + 0xA = 0x226
    strb r5,[r3,#0x8E] ; (3,2) 0x546 + 0x3C + 0xA = 0x58C
    strb r5,[r4,#0x3E8] ; (2,3) 0x384 + 0x5A + 0xA = 0x3E8
    ; CUSTOM ROOM ASSIGNMENTS
    add r1,sp,#0x8
    str r1,[sp,#0x0]
    ldrb r3,[r6,#0xD]
    add r0,sp,#0x80
    mov r1,#0x4
    str r3,[sp,#0x4]
    mov r2,#0x4
    add r3,sp,#0x44
    bl CreateRoomsAndAnchors
    ; CUSTOM CONNECTION ASSIGNMENTS
    add r0,r7,#0xF7 ; Connect the center to the sides.
    strb r5,[r7,#0xF5] ; (1,1) 0x1C2 + 0x1E + 0x13 = 0x1F3
    strb r5,[r7,#0xF7] ; (1,1) 0x1C2 + 0x1E + 0x15 = 0x1F5
    strb r5,[r0,#0x1C0] ; (2,1) 0x384 + 0x1E + 0x13 = 0x3B5
    strb r5,[r4,#0x3B8] ; (2,1) 0x384 + 0x1E + 0x16 = 0x3B8
    strb r5,[r7,#0x114] ; (1,2) 0x1C2 + 0x3C + 0x14 = 0x212
    strb r5,[r0,#0x1E] ; (1,2) 0x1C2 + 0x3C + 0x15 = 0x213
    strb r5,[r4,#0x3D4] ; (2,2) 0x384 + 0x3C + 0x14 = 0x3D4
    strb r5,[r7,#0x2D8] ; (2,2) 0x384 + 0x3C + 0x16 = 0x3D6
    ; CUSTOM CONNECTION ASSIGNMENTS
    add r1,sp,#0x8
    str r1,[sp,#0x0]
    mov r0,#1 ; Modified, DISABLE ROOM MERGING
    str r0,[sp,#0x4]
    add r0,sp,#0x80
    mov r1,#0x4
    mov r2,#0x4
    add r3,sp,#0x44
    bl CreateGridCellConnections
    ; CUSTOM ROOM MERGING
    mov r0,#0x1
    mov r1,#0x0
    mov r2,#0x1
    add r3,sp,#0x80
    bl MergeRoomsHorizontally
    mov r0,#0x1
    mov r1,#0x3
    mov r2,#0x1
    add r3,sp,#0x80
    bl MergeRoomsHorizontally
    mov r0,#0x0
    mov r1,#0x1
    mov r2,#0x1
    add r3,sp,#0x80
    bl MergeRoomsVertically
    mov r0,#0x3
    mov r1,#0x1
    mov r2,#0x1
    add r3,sp,#0x80
    bl MergeRoomsVertically
    ; CUSTOM ROOM MERGING
    ; ldrb r3,[r6,#0x9] ; The rooms have at least one even side, thus they are
    ; add r0,sp,#0x80 ; incompatible with mazes...
    ; mov r1,#0x4
    ; mov r2,#0x4
    ; bl GenerateMazeRoom
    ; ldr r7,=FLOOR_GENERATION_STATUS
    ; add r0,sp,#0x80 ; Merged rooms are not allowed to have Kecleon
    ; ldrsh r3,[r7,#0xC] ; shops! Since all the rooms are merged, they
    ; mov r1,#0x4 ; fail this check!
    ; mov r2,#0x4
    ; bl GenerateKecleonShop
    add r0,sp,#0x80
    ldrsh r3,[r7,#0x10]
    mov r1,#0x4
    mov r2,#0x4
    bl GenerateMonsterHouse
    ldrb r3,[r6,#0x13]
    add r0,sp,#0x80
    mov r1,#0x4
    mov r2,#0x4
    bl GenerateExtraHallways
    add r0,sp,#0x80
    mov r1,#0x4
    mov r2,#0x4
    bl GenerateRoomImperfections
    add r0,sp,#0x80
    mov r1,#0x4
    mov r2,#0x4
    bl GenerateSecondaryStructures
    add sp,sp,#0xAE0
    add sp,sp,#0x1000
    ldmia sp!,{r3,r4,r5,r6,r7,r8,pc} */
void GenerateHollowPlusFloor(struct floor_properties *floor_props) {
    // Setup the grid.
    struct dungeon_grid_cell grid[4 * HB_GRID_COLS];
    int start_pos_x[16] = {12, 20, 28, 36, 44};
    int start_pos_y[16] = {0, 8, 16, 24, 32};
    // InitDungeonGrid has a check for the floor size and can ignore the size
    // of the arguments passed for certain sizes... so we must override it for
    // safety reasons to avoid a crash in case the user overwrites the small
    // or medium standard floor layouts with this layout.
    FLOOR_GENERATION_STATUS_PTR->floor_size.val = FLOOR_SIZE_LARGE;
    InitDungeonGrid(grid, 4, 4);

    // Create the rooms.
    grid[0 * HB_GRID_COLS + 0].is_invalid = true;  // (0,0) Invalidate the four corner cells.
    grid[0 * HB_GRID_COLS + 3].is_invalid = true;  // (0,3)
    grid[3 * HB_GRID_COLS + 0].is_invalid = true; // (3,0)
    grid[3 * HB_GRID_COLS + 3].is_invalid = true; // (3,3)
    // Mark side cells as rooms
    grid[0 * HB_GRID_COLS + 1].is_room = true;  // (0,1) Mark the edges (that aren't the corner
    grid[1 * HB_GRID_COLS + 0].is_room = true;  // (1,0) cells) as rooms.
    grid[2 * HB_GRID_COLS + 0].is_room = true;  // (2,0)
    grid[3 * HB_GRID_COLS + 1].is_room = true; // (3,1)
    grid[0 * HB_GRID_COLS + 2].is_room = true;  // (0,2)
    grid[1 * HB_GRID_COLS + 3].is_room = true;  // (1,3)
    grid[3 * HB_GRID_COLS + 2].is_room = true; // (3,2)
    grid[2 * HB_GRID_COLS + 3].is_room = true; // (2,3)
    grid[1 * HB_GRID_COLS + 1].is_room = false; // (1,1) Mark the four center cells as anchors.
    grid[1 * HB_GRID_COLS + 2].is_room = false; // (1,2)
    grid[2 * HB_GRID_COLS + 1].is_room = false; // (2,1)
    grid[2 * HB_GRID_COLS + 2].is_room = false;// (2,2)
    CreateRoomsAndAnchors(grid, 4, 4, start_pos_x, start_pos_y, floor_props->room_flags);

    // Create the hallways.
    grid[1 * HB_GRID_COLS + 1].is_connected_to_left = true;   // (1,1)
    grid[1 * HB_GRID_COLS + 1].is_connected_to_top = true;
    grid[2 * HB_GRID_COLS + 1].is_connected_to_right = true;   // (2,1)
    grid[2 * HB_GRID_COLS + 1].is_connected_to_top = true;
    grid[1 * HB_GRID_COLS + 2].is_connected_to_left = true;    // (1,2)
    grid[1 * HB_GRID_COLS + 2].is_connected_to_bottom = true;
    grid[2 * HB_GRID_COLS + 2].is_connected_to_right = true;   // (2,2)
    grid[2 * HB_GRID_COLS + 2].is_connected_to_bottom = true;
    CreateGridCellConnections(grid, 4, 4, start_pos_x, start_pos_y, true);

    // Now, merge the rooms along the side.
    MergeRoomsHorizontally(1, 0, 1, grid);
    MergeRoomsHorizontally(1, 3, 1, grid);
    MergeRoomsVertically(0, 1, 1, grid);
    MergeRoomsVertically(3, 1, 1, grid);

    // Normal dungeon generation... kinda. Since the rooms have even sides,
    // mazes will fail to generate and kecleon shops can't spawn in merged
    // rooms just skip them.
    // GenerateMazeRoom(grid, 4, 4, floor_props->maze_room_chance);
    // GenerateKecleonShop(grid, 4, 4, floor_props->kecleon_shop_spawn_chance);
    GenerateMonsterHouse(grid, 4, 4, floor_props->monster_house_spawn_chance);
    GenerateExtraHallways(grid, 4, 4, floor_props->extra_hallways);
    GenerateRoomImperfections(grid, 4, 4);
    GenerateSecondaryStructures(grid, 4, 4);
}
#include <pmdsky.h>
#include <cot.h>
#include "hb-eos-c-dun-lib.h"

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
    if (x < x0 || x > x1 || y < y0 || y > y1) {
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
    int block_x_start = x0 + 1;
    int block_y_start = y0 + 1;
    int block_x_end = x1 - 1;
    int block_y_end = y1 - 1;
    for(int x = block_x_start; x <= block_x_end; ++x) {
        abs_floor->tiles[x][y0].x_connect_blocked = true;
        abs_floor->tiles[x][block_y_end].x_connect_blocked = true;
    }
    for(int y = block_y_start; y <= block_y_end; ++y) {
        abs_floor->tiles[x0][y].y_connect_blocked = true;
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
            abs_floor->tiles[x][y].custom_tile_info_1 = false;
            abs_floor->tiles[x][y].custom_tile_info_2 = false;
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

bool TryCarveAbstractTile(const int16_t x, const int16_t y, struct abstract_floor *floor_data, const enum direction_id dir) {
    if ((dir == DIR_LEFT || dir == DIR_RIGHT) && floor_data->tiles[x][y].x_connect_blocked) {
        return false;
    }
    if ((dir == DIR_UP || dir == DIR_DOWN) && floor_data->tiles[x][y].y_connect_blocked) {
        return false;
    }
    enum terrain_type terrain_type = floor_data->tiles[x][y].terrain_type;
    if(TERRAIN_NORMAL != terrain_type) {
        floor_data->tiles[x][y].terrain_type = TERRAIN_NORMAL;
    }
    if (dir == DIR_LEFT || dir == DIR_RIGHT) {
        floor_data->tiles[x][y + 1].x_connect_blocked = true;
        floor_data->tiles[x][y - 1].x_connect_blocked = true;
    } else if (dir == DIR_UP || dir == DIR_DOWN) {
        floor_data->tiles[x + 1][y].y_connect_blocked = true;
        floor_data->tiles[x - 1][y].y_connect_blocked = true;
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
        }
        if(0 < y) {
            abs_floor->tiles[x][y - 1].x_connect_blocked = true;
        }   
    } else if (DIR_UP == dir || DIR_DOWN == dir) {
        if (HB_DUNGEON_MAX_X - 1 > x) {
            abs_floor->tiles[x + 1][y].y_connect_blocked = true;
        }
        if (0 < x) {
            abs_floor->tiles[x - 1][y].y_connect_blocked = true;
        }
    }
    return true;
}

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
    asm("mov   r12,#0x1C"); // DUNGEON_GRID_CELL_BYTES
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
    asm("add r4,r4,#0x1C"); // DUNGEON_GRID_CELL_BYTES
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

void GenerateBinaryTreeFloor(struct floor_properties *floor_props, bool hard_solution) {
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

void GenerateSidewinderFloor(struct floor_properties *floor_props, bool easy_solution) {
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
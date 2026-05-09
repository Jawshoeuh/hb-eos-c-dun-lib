#ifndef _H_HB_EOS_C_DUN_LIB_
#define _H_HB_EOS_C_DUN_LIB_

#define HB_DUNGEON_MAX_X 56
#define HB_DUNGEON_MAX_Y 32
#define HB_MAX_ROOMS 30
#define HB_GRID_ROWS 2
#define HB_GRID_COLS 15
#define HB_X_POS 0
#define HB_Y_POS 1


struct abstract_tile {
    uint8_t terrain_type : 2;
    bool is_room : 1;
    bool room_blocked : 1;
    bool x_connect_blocked : 1;
    bool y_connect_blocked : 1;
    bool custom_tile_info_1 : 1;
    bool custom_tile_info_2 : 1;
};
ASSERT_SIZE(struct abstract_tile, 1);

// This struct is used to store the tile data for an entire floor without
// needing to call GetTileSafe for every tile, which is slow.
struct abstract_floor {
    struct  abstract_tile tiles[HB_DUNGEON_MAX_X][HB_DUNGEON_MAX_Y];
    uint8_t num_rooms;
    uint8_t room_start_pos[HB_MAX_ROOMS][2];
    uint8_t room_end_pos[HB_MAX_ROOMS][2];
    uint8_t padding1;
    uint8_t padding2;
    uint8_t padding3;
};
ASSERT_SIZE(struct abstract_floor, 0x1 + (0x1 * HB_DUNGEON_MAX_X * HB_DUNGEON_MAX_Y) + HB_MAX_ROOMS * 0x4 + 0x3);

bool IsCoordinateValid(const int16_t x, const int16_t y);

bool IsAreaValid(const int16_t x0, const int16_t y0, const int16_t x1, const int16_t y1);

bool DoesAreaOverlap(const int16_t x0a, const int16_t y0a, const int16_t x1a, const int16_t y1a, const int16_t x0b, const int16_t y0b, const int16_t x1b, const int16_t y1b);

bool IsCoordinateInArea (const int16_t x, const int16_t y, const int16_t x0, const int16_t y0, const int16_t x1, const int16_t y1);

bool IsDirectionCardinal(const enum direction_id dir);

enum direction_id MakeDirectionCardinal(enum direction_id dir);

enum direction_id MakeDirectionDiagonal(enum direction_id dir);

enum direction_id RandomCardinalDirection();

enum direction_id RandomDiagonalDirection();

int CalcActualRoomDensity(int room_density);

bool AddRoomToAbstractFloor(int16_t x0, int16_t y0, int16_t x1, int16_t y1, struct abstract_floor *abs_floor);

void InitAbstractFloor(struct abstract_floor *abs_floor);

void ApplyAbstractFloorToFloor(const struct abstract_floor *abs_floor, struct dungeon_grid_cell *grid, const struct floor_properties *floor_props);

bool TryCarveAbstractTile(const int16_t x, const int16_t y, struct abstract_floor *floor_data, const enum direction_id dir);

bool TryCarveAbstractTileSafe(const int16_t x, const int16_t y, struct abstract_floor *abs_floor, const enum direction_id dir);

void CreateRoomInCell(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t room_id, struct dungeon_grid_cell *cell, struct room_flags flags);

void CreateRoom(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t room_id);

void MergeRoomsHorizontally(int16_t x, int16_t y, int16_t dx, struct dungeon_grid_cell *grid);

void CreateRoomInCell(int16_t x0, int16_t y0, int16_t x1, int16_t x2, uint8_t room_id, struct dungeon_grid_cell *cell, struct room_flags flags);

void GenerateRecursiveBacktrackingFloor(struct floor_properties *floor_props);

void GenerateBinaryTreeMaze(const int16_t x0, const int16_t y0, const int16_t x1, const int16_t y1, enum direction_id dir);

void GenerateBinaryTreeFloor(struct floor_properties *floor_props, bool easy_solution);

void GenerateSidewinderMaze(const int16_t x0, const int16_t y0, const int16_t x1, const int16_t y1, enum direction_id dir, const int32_t weight);

void GenerateSidewinderFloor(struct floor_properties *floor_props, bool hard_solution);

#endif
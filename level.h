#ifndef LEVEL_H
#define LEVEL_H

#include <raylib.h>
#include "entities.h" /* For GROUND_Y, etc. */

#define TILE_SIZE 16
#define WORLD_ROWS 38     /* 600/16 ≈ 37.5 */
#define TILES_PER_ROW 125 /* 2000/16 = 125 */
#define STAGE_WIDTH (TILES_PER_ROW * TILE_SIZE)

typedef enum
{
    LEVEL_LAYER_BACKGROUND = 0,
    LEVEL_LAYER_DETAIL,
    LEVEL_LAYER_GROUND,
    LEVEL_LAYER_FOREGROUND,
    LEVEL_LAYER_COUNT
} LevelLayer;

typedef struct
{
    Texture2D tileset;
    int width;
    int height;   /* Number of rows */
    int *tileMap; /* Collision map (row-major) */
    int *layers[LEVEL_LAYER_COUNT];
    Rectangle *colliders; /* Dynamic array of static colliders, NULL for none */
    int numColliders;
} Level;

extern Level currentLevel;
extern Camera2D camera;
extern int currentStage;
extern bool bossSpawned;
extern bool bossDefeated;

int GetStageCount(void);
float LevelStageEndX(void);
float LevelWidth(void);
int LevelTileColumns(void);
int LevelRowCount(void);
int LevelEnemiesRemaining(void);
bool LevelIsCleared(void);

Rectangle GetTileSource(int tileId); /* Get source rect for tile ID */

bool InitLevel(int stage);
void UpdateLevel(float dt, float playerLeadX);
void DrawLevel(void);
void DrawLevelForeground(void);
void UnloadLevel(void); /* Cleanup tileMap, etc. */

#endif /* LEVEL_H */

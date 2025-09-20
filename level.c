#include "assets.h"
#include "entities.h"
#include "level.h"
#include <raylib.h>
#include <stdlib.h> /* malloc, free */
#include <string.h> /* memset */

typedef struct
{
  int width;
  int spawnQuota;
  int concurrentCap;
  int initialWave;
  bool hasBoss;
  int bossHealth;
  float bossTriggerX;
  float bossSpawnX;
  float spawnInterval;
} StageDefinition;

static const StageDefinition STAGE_DEFS[]
    = { { .width = STAGE_WIDTH,
          .spawnQuota = 6,
          .concurrentCap = 3,
          .initialWave = 2,
          .hasBoss = false,
          .bossHealth = 0,
          .bossTriggerX = 0.0f,
          .bossSpawnX = 0.0f,
          .spawnInterval = 3.5f },
        { .width = STAGE_WIDTH,
          .spawnQuota = 8,
          .concurrentCap = 4,
          .initialWave = 3,
          .hasBoss = false,
          .bossHealth = 0,
          .bossTriggerX = 0.0f,
          .bossSpawnX = 0.0f,
          .spawnInterval = 3.0f },
        { .width = STAGE_WIDTH,
          .spawnQuota = 10,
          .concurrentCap = 4,
          .initialWave = 3,
          .hasBoss = true,
          .bossHealth = 160,
          .bossTriggerX = STAGE_WIDTH - 360.0f,
          .bossSpawnX = STAGE_WIDTH - 140.0f,
          .spawnInterval = 2.6f } };

static const int STAGE_COUNT = sizeof (STAGE_DEFS) / sizeof (STAGE_DEFS[0]);
static const StageDefinition *currentStageDef = NULL;
static int tilesPerRow = TILES_PER_ROW;
static bool stageHasBoss = false;
static int tilesetColumns = 8;

Level currentLevel = (Level){ 0 };
Camera2D camera = { 0 };
int currentStage = 0;
float spawnTimer = 0.0f;
bool bossSpawned = false;
bool bossDefeated = false;
int enemiesRemainingToSpawn = 0;
int maxConcurrentEnemies = 3;

static float
ClampFloat (float value, float minValue, float maxValue)
{
  if (value < minValue)
    return minValue;
  if (value > maxValue)
    return maxValue;
  return value;
}

static void
ConsumeEnemySpawnQuota (int count)
{
  if (count <= 0)
    return;
  if (enemiesRemainingToSpawn <= 0)
    return;

  enemiesRemainingToSpawn -= count;
  if (enemiesRemainingToSpawn < 0)
    enemiesRemainingToSpawn = 0;
}

static inline int
TileAtlasColumns (void)
{
  return tilesetColumns > 0 ? tilesetColumns : 1;
}

static inline int
TileIdFromAtlas (int col, int row)
{
  if (col < 0 || row < 0)
    return 0;

  int cols = TileAtlasColumns ();
  return row * cols + col + 1;
}

static void
ClearLayer (int *layer, size_t tileCount)
{
  if (!layer || tileCount == 0)
    return;

  memset (layer, 0, tileCount * sizeof (int));
}

static int
WrapIndex (int value, int modulo)
{
  if (modulo <= 0)
    return 0;

  int result = value % modulo;
  if (result < 0)
    result += modulo;
  return result;
}

static void
FillRepeatedStrip (int *layer, int destRowStart, int rows, int srcRowStart,
                   int srcColStart, int srcCols, int horizontalOffset)
{
  if (!layer || rows <= 0 || srcCols <= 0)
    return;

  for (int rowOffset = 0; rowOffset < rows; ++rowOffset)
    {
      int destRow = destRowStart + rowOffset;
      if (destRow < 0 || destRow >= currentLevel.height)
        continue;

      int srcRow = srcRowStart + rowOffset;
      for (int x = 0; x < tilesPerRow; ++x)
        {
          int patternIndex = WrapIndex (x + horizontalOffset, srcCols);
          int srcCol = srcColStart + patternIndex;
          int tileId = TileIdFromAtlas (srcCol, srcRow);
          layer[destRow * tilesPerRow + x] = tileId;
        }
    }
}

static void
PlaceRegion (int *layer, int destX, int destY, int width, int height,
             int srcColStart, int srcRowStart)
{
  if (!layer || width <= 0 || height <= 0)
    return;

  for (int dy = 0; dy < height; ++dy)
    {
      int row = destY + dy;
      if (row < 0 || row >= currentLevel.height)
        continue;

      int srcRow = srcRowStart + dy;
      for (int dx = 0; dx < width; ++dx)
        {
          int col = destX + dx;
          if (col < 0 || col >= tilesPerRow)
            continue;

          int srcCol = srcColStart + dx;
          int tileId = TileIdFromAtlas (srcCol, srcRow);
          if (tileId <= 0)
            continue;

          layer[row * tilesPerRow + col] = tileId;
        }
    }
}

static void
PlaceWithShift (int *layer, const int *positions, size_t count, int shift,
                int destY, int width, int height, int srcColStart,
                int srcRowStart)
{
  if (!layer || !positions || count == 0)
    return;

  for (size_t i = 0; i < count; ++i)
    {
      int destX = positions[i] + shift;
      if (destX < 0 || destX + width > tilesPerRow)
        continue;

      PlaceRegion (layer, destX, destY, width, height, srcColStart,
                   srcRowStart);
    }
}

int
GetStageCount (void)
{
  return STAGE_COUNT;
}

float
LevelWidth (void)
{
  return (float)currentLevel.width;
}

int
LevelTileColumns (void)
{
  if (!currentLevel.tileMap || tilesPerRow <= 0)
    {
      return 0;
    }
  return tilesPerRow;
}

int
LevelRowCount (void)
{
  if (!currentLevel.tileMap || currentLevel.height <= 0)
    {
      return 0;
    }
  return currentLevel.height;
}

float
LevelStageEndX (void)
{
  const float margin = 120.0f;
  float endX = LevelWidth () - margin;
  if (endX < 0.0f)
    endX = LevelWidth ();
  return endX;
}

static bool
LevelHasPendingBoss (void)
{
  return stageHasBoss && !bossSpawned && !bossDefeated;
}

static bool
LevelHasActiveBoss (void)
{
  return stageHasBoss && bossSpawned && !bossDefeated;
}

int
LevelEnemiesRemaining (void)
{
  int alive = GetAliveEnemiesCount ();
  int pendingBoss = LevelHasPendingBoss () ? 1 : 0;
  return enemiesRemainingToSpawn + alive + pendingBoss;
}

bool
LevelIsCleared (void)
{
  if (stageHasBoss)
    {
      if (!bossDefeated)
        return false;
    }

  return enemiesRemainingToSpawn <= 0 && GetAliveEnemiesCount () == 0
         && !LevelHasActiveBoss ();
}

/* Get source rectangle for a tile ID (assuming simple layout) */
Rectangle
GetTileSource (int tileId)
{
  if (tileId <= 0 || currentLevel.tileset.id <= 0)
    return (Rectangle){ 0, 0, 0, 0 };

  int cols = TileAtlasColumns ();
  if (cols <= 0)
    cols = 1;

  int tx = (tileId - 1) % cols;
  int ty = (tileId - 1) / cols;
  return (Rectangle){ (float)(tx * TILE_SIZE), (float)(ty * TILE_SIZE),
                      TILE_SIZE, TILE_SIZE };
}

static void
GenerateTileMap (int stageIndex)
{
  int groundRow = (int)(GROUND_Y / TILE_SIZE);
  if (groundRow < 0)
    groundRow = 0;
  if (groundRow >= currentLevel.height)
    groundRow = currentLevel.height - 1;

  size_t mapSize = (size_t)tilesPerRow * (size_t)currentLevel.height;

  if (currentLevel.tileMap)
    {
      memset (currentLevel.tileMap, 0, mapSize * sizeof (int));
    }

  for (int layer = 0; layer < LEVEL_LAYER_COUNT; ++layer)
    {
      ClearLayer (currentLevel.layers[layer], mapSize);
    }

  const int walkwayColStart = 3;
  const int walkwayCols = 7;
  const int walkwayRowStart = 14;
  const int walkwayRows = 9;
  int walkwayTopRow = groundRow - walkwayRows + 1;
  if (walkwayTopRow < 0)
    walkwayTopRow = 0;

  int walkwayOffset = stageIndex * 3;
  FillRepeatedStrip (currentLevel.layers[LEVEL_LAYER_GROUND], walkwayTopRow,
                     walkwayRows, walkwayRowStart, walkwayColStart, walkwayCols,
                     walkwayOffset);

  int groundSrcRow = walkwayRowStart + walkwayRows - 1;
  if (currentLevel.tileMap)
    {
      for (int y = groundRow; y < currentLevel.height; ++y)
        {
          for (int x = 0; x < tilesPerRow; ++x)
            {
              int patternIndex = WrapIndex (x + walkwayOffset, walkwayCols);
              int srcCol = walkwayColStart + patternIndex;
              currentLevel.tileMap[y * tilesPerRow + x]
                  = TileIdFromAtlas (srcCol, groundSrcRow);
            }
        }
    }

  const int facadeRows = 6;
  int facadeTopRow = walkwayTopRow - facadeRows;
  if (facadeTopRow < 0)
    facadeTopRow = 0;

  int stageShift = stageIndex * 6;
  if (stageShift > 12)
    stageShift = 12;

  int *backgroundLayer = currentLevel.layers[LEVEL_LAYER_BACKGROUND];
  if (backgroundLayer)
    {
      PlaceRegion (backgroundLayer, 0, facadeTopRow, 14, facadeRows, 0, 0);

      int patternX = 14;
      while (patternX < tilesPerRow - 14)
        {
          PlaceRegion (backgroundLayer, patternX, facadeTopRow, 2, facadeRows,
                       15, 0);
          patternX += 2;
          if (patternX >= tilesPerRow - 14)
            break;

          PlaceRegion (backgroundLayer, patternX, facadeTopRow, 14, facadeRows,
                       18, 0);
          patternX += 14;
        }

      int rightSrcCol = (stageIndex == 1) ? 0 : 18;
      PlaceRegion (backgroundLayer, tilesPerRow - 14, facadeTopRow, 14,
                   facadeRows, rightSrcCol, 0);

      if (stageIndex == 2)
        {
          int centerX = tilesPerRow / 2 - 7;
          PlaceRegion (backgroundLayer, centerX, facadeTopRow, 14, facadeRows,
                       0, 0);
        }
    }

  static const int doorPositions[] = { 20, 52, 74, 90 };
  static const int windowPositions[] = { 26, 58, 82, 106 };
  static const int archPositions[] = { 34 };
  static const int barsPositions[] = { 38, 70 };

  PlaceWithShift (currentLevel.layers[LEVEL_LAYER_DETAIL], doorPositions,
                  sizeof (doorPositions) / sizeof (doorPositions[0]),
                  stageShift, facadeTopRow, 3, facadeRows, 12, 7);
  PlaceWithShift (currentLevel.layers[LEVEL_LAYER_DETAIL], windowPositions,
                  sizeof (windowPositions) / sizeof (windowPositions[0]),
                  stageShift, facadeTopRow, 3, facadeRows, 16, 7);
  PlaceWithShift (currentLevel.layers[LEVEL_LAYER_DETAIL], archPositions,
                  sizeof (archPositions) / sizeof (archPositions[0]),
                  stageShift, facadeTopRow, 2, facadeRows, 20, 7);
  PlaceWithShift (currentLevel.layers[LEVEL_LAYER_DETAIL], barsPositions,
                  sizeof (barsPositions) / sizeof (barsPositions[0]),
                  stageShift, facadeTopRow, 2, facadeRows, 23, 7);

  if (stageIndex >= 1)
    {
      static const int extraWindows[] = { 44 };
      PlaceWithShift (currentLevel.layers[LEVEL_LAYER_DETAIL], extraWindows,
                      sizeof (extraWindows) / sizeof (extraWindows[0]),
                      stageShift / 2, facadeTopRow, 3, facadeRows, 16, 7);
    }

  if (stageIndex >= 2)
    {
      static const int extraDoors[] = { 62, 98 };
      PlaceWithShift (currentLevel.layers[LEVEL_LAYER_DETAIL], extraDoors,
                      sizeof (extraDoors) / sizeof (extraDoors[0]),
                      stageShift / 2, facadeTopRow, 3, facadeRows, 12, 7);
    }

  static const int graffitiPositions[] = { 34, 106 };
  static const int garagePositions[] = { 52, 88 };

  PlaceWithShift (currentLevel.layers[LEVEL_LAYER_GROUND], graffitiPositions,
                  sizeof (graffitiPositions) / sizeof (graffitiPositions[0]),
                  stageShift, walkwayTopRow, 6, 6, 27, 14);
  PlaceWithShift (currentLevel.layers[LEVEL_LAYER_GROUND], garagePositions,
                  sizeof (garagePositions) / sizeof (garagePositions[0]),
                  stageShift, walkwayTopRow, 7, 6, 19, 14);

  if (stageIndex >= 2)
    {
      int bossEntranceX = tilesPerRow - 36;
      PlaceRegion (currentLevel.layers[LEVEL_LAYER_GROUND], bossEntranceX,
                   walkwayTopRow, 7, 6, 19, 14);
      PlaceRegion (currentLevel.layers[LEVEL_LAYER_DETAIL], bossEntranceX - 4,
                   facadeTopRow, 3, facadeRows, 12, 7);
    }
}

static void
SetupColliders (int stageIndex)
{
  int desired = 0;
  switch (stageIndex)
    {
    case 0:
      desired = 2;
      break;
    case 1:
      desired = 3;
      break;
    default:
      desired = 4;
      break;
    }

  currentLevel.colliders = (Rectangle *)malloc (sizeof (Rectangle) * desired);
  if (!currentLevel.colliders)
    {
      currentLevel.numColliders = 0;
      TraceLog (LOG_WARNING, "Failed to allocate colliders for stage %d",
                stageIndex);
      return;
    }

  currentLevel.numColliders = desired;

  currentLevel.colliders[0]
      = (Rectangle){ 420.0f, GROUND_Y - 48.0f, 48.0f, 48.0f };
  if (desired > 1)
    currentLevel.colliders[1]
        = (Rectangle){ 920.0f, GROUND_Y - 36.0f, 80.0f, 36.0f };
  if (desired > 2)
    currentLevel.colliders[2]
        = (Rectangle){ 1350.0f, GROUND_Y - 52.0f, 60.0f, 52.0f };
  if (desired > 3)
    currentLevel.colliders[3]
        = (Rectangle){ 1650.0f, GROUND_Y - 40.0f, 72.0f, 40.0f };
}

static void
SpawnInitialWave (const StageDefinition *def)
{
  if (!def || def->initialWave <= 0)
    return;

  float baseX = 360.0f;
  float spacing = 110.0f;
  for (int i = 0; i < def->initialWave && activeEnemies < MAX_ENEMIES; ++i)
    {
      float spawnX = baseX + spacing * (float)i;
      float maxX = LevelWidth () - PLAYER_WIDTH;
      spawnX = ClampFloat (spawnX, 120.0f, maxX);

      InitEnemy (&enemies[activeEnemies],
                 (Vector2){ spawnX, GROUND_Y - PLAYER_HEIGHT });
      enemies[activeEnemies].facingRight = false;
      activeEnemies++;
    }

  ConsumeEnemySpawnQuota (def->initialWave);
}

bool
InitLevel (int stage)
{
  int stageIndex = stage;
  if (stageIndex < 0)
    stageIndex = 0;
  if (stageIndex >= STAGE_COUNT)
    stageIndex = STAGE_COUNT - 1;

  currentStage = stageIndex;
  currentStageDef = &STAGE_DEFS[stageIndex];
  stageHasBoss = currentStageDef->hasBoss;
  bossSpawned = false;
  bossDefeated = false;
  spawnTimer = 0.0f;

  ClearEnemies ();

  if (currentLevel.tileMap)
    {
      free (currentLevel.tileMap);
      currentLevel.tileMap = NULL;
    }
  for (int layer = 0; layer < LEVEL_LAYER_COUNT; ++layer)
    {
      if (currentLevel.layers[layer])
        {
          free (currentLevel.layers[layer]);
          currentLevel.layers[layer] = NULL;
        }
    }
  if (currentLevel.colliders)
    {
      free (currentLevel.colliders);
      currentLevel.colliders = NULL;
      currentLevel.numColliders = 0;
    }

  currentLevel.tileset = tileset;
  tilesetColumns = currentLevel.tileset.width > 0
                       ? currentLevel.tileset.width / TILE_SIZE
                       : 1;
  if (tilesetColumns <= 0)
    tilesetColumns = 1;
  currentLevel.width = currentStageDef->width;
  currentLevel.height = WORLD_ROWS;
  tilesPerRow = currentLevel.width / TILE_SIZE;
  if (tilesPerRow <= 0)
    {
      tilesPerRow = TILES_PER_ROW;
    }

  size_t tileCount = (size_t)tilesPerRow * (size_t)currentLevel.height;
  if (tileCount == 0)
    {
      TraceLog (LOG_ERROR, "Invalid tile configuration for stage %d",
                stageIndex);
      currentLevel.height = 0;
      return false;
    }

  currentLevel.tileMap = (int *)calloc (tileCount, sizeof (int));
  if (!currentLevel.tileMap)
    {
      TraceLog (LOG_ERROR, "Failed to allocate tile map for stage %d",
                stageIndex);
      currentLevel.height = 0;
      return false;
    }

  for (int layer = 0; layer < LEVEL_LAYER_COUNT; ++layer)
    {
      currentLevel.layers[layer] = (int *)calloc (tileCount, sizeof (int));
      if (!currentLevel.layers[layer])
        {
          TraceLog (LOG_ERROR, "Failed to allocate layer %d for stage %d",
                    layer, stageIndex);
          for (int prev = 0; prev <= layer; ++prev)
            {
              if (currentLevel.layers[prev])
                {
                  free (currentLevel.layers[prev]);
                  currentLevel.layers[prev] = NULL;
                }
            }
          free (currentLevel.tileMap);
          currentLevel.tileMap = NULL;
          currentLevel.height = 0;
          return false;
        }
    }

  GenerateTileMap (stageIndex);
  SetupColliders (stageIndex);

  enemiesRemainingToSpawn = currentStageDef->spawnQuota;
  maxConcurrentEnemies = currentStageDef->concurrentCap;

  SpawnInitialWave (currentStageDef);

  TraceLog (LOG_INFO, "Stage %d initialised (width=%d, quota=%d, boss=%s)",
            stageIndex, currentLevel.width, enemiesRemainingToSpawn,
            stageHasBoss ? "yes" : "no");

  return true;
}

void
UpdateLevel (float dt, float playerLeadX)
{
  if (!currentStageDef)
    return;

  const float stageWidth = LevelWidth ();
  float maxSpawnX = stageWidth - PLAYER_WIDTH;
  float minSpawnAhead = playerLeadX + 120.0f;

  if (enemiesRemainingToSpawn > 0 && activeEnemies < maxConcurrentEnemies
      && activeEnemies < MAX_ENEMIES)
    {
      spawnTimer += dt;
      float interval = currentStageDef->spawnInterval;
      if (interval < 1.6f)
        interval = 1.6f;
      if (spawnTimer >= interval)
        {
          spawnTimer = 0.0f;

          float randomOffset = (float)GetRandomValue (-40, 140);
          float spawnX = minSpawnAhead + randomOffset;
          if (spawnX < playerLeadX + 80.0f)
            spawnX = playerLeadX + 80.0f;
          spawnX = ClampFloat (spawnX, 80.0f, maxSpawnX);

          InitEnemy (&enemies[activeEnemies],
                     (Vector2){ spawnX, GROUND_Y - PLAYER_HEIGHT });
          enemies[activeEnemies].facingRight = false;
          activeEnemies++;
          ConsumeEnemySpawnQuota (1);
        }
    }
  else
    {
      spawnTimer = 0.0f;
    }

  if (stageHasBoss && !bossSpawned && !bossDefeated
      && enemiesRemainingToSpawn <= 0 && activeEnemies == 0)
    {
      float triggerX = currentStageDef->bossTriggerX;
      if (triggerX <= 0.0f)
        triggerX = stageWidth - 400.0f;

      if (playerLeadX >= triggerX)
        {
          if (activeEnemies < MAX_ENEMIES)
            {
              float spawnX = currentStageDef->bossSpawnX;
              if (spawnX <= 0.0f)
                spawnX = stageWidth - 140.0f;
              spawnX = ClampFloat (spawnX, 120.0f, maxSpawnX);

              InitEnemy (&enemies[activeEnemies],
                         (Vector2){ spawnX, GROUND_Y - PLAYER_HEIGHT });
              enemies[activeEnemies].health = currentStageDef->bossHealth;
              enemies[activeEnemies].maxHealth = currentStageDef->bossHealth;
              enemies[activeEnemies].facingRight = false;
              activeEnemies++;
              bossSpawned = true;
              TraceLog (LOG_INFO, "Boss spawned for stage %d (HP=%d)",
                        currentStage, currentStageDef->bossHealth);
            }
        }
    }
}

void
DrawLevel (void)
{
  if (currentLevel.tileset.id <= 0)
    return;

  const int layersToDraw[]
      = { LEVEL_LAYER_BACKGROUND, LEVEL_LAYER_DETAIL, LEVEL_LAYER_GROUND };
  const int layerCount
      = (int)(sizeof (layersToDraw) / sizeof (layersToDraw[0]));

  for (int l = 0; l < layerCount; ++l)
    {
      int *layer = currentLevel.layers[layersToDraw[l]];
      if (!layer)
        continue;

      for (int y = 0; y < currentLevel.height; ++y)
        {
          for (int x = 0; x < tilesPerRow; ++x)
            {
              int tileId = layer[y * tilesPerRow + x];
              if (tileId <= 0)
                continue;

              Rectangle source = GetTileSource (tileId);
              if (source.width <= 0)
                continue;

              Vector2 pos = { (float)(x * TILE_SIZE), (float)(y * TILE_SIZE) };
              DrawTextureRec (currentLevel.tileset, source, pos, WHITE);
            }
        }
    }

  // Draw props at collider positions
  if (currentLevel.colliders && currentLevel.numColliders > 0)
    {
      for (int i = 0; i < currentLevel.numColliders; ++i)
        {
          Rectangle c = currentLevel.colliders[i];
          Vector2 propPos
              = { c.x, c.y - (c.height - PLAYER_HEIGHT) }; // Baseline align
          Texture2D propTex;
          switch (i)
            { // Assign based on index/stage
            case 0:
            case 2:
              propTex = propBush;
              break;
            case 1:
            case 3:
              propTex = propCar;
              break;
            default:
              propTex = propBush;
              break;
            }
          if (propTex.id > 0)
            {
              float scaleX = c.width / (float)propTex.width;
              float scaleY = c.height / (float)propTex.height;
              float scale = (scaleX + scaleY) / 2.0f;
              DrawTextureEx (propTex, propPos, 0.0f, scale, WHITE);
            }
        }
    }

  (void)currentLevel.colliders;
  (void)currentLevel.numColliders;
}

void
DrawLevelForeground (void)
{
  if (currentLevel.tileset.id <= 0)
    return;

  int *layer = currentLevel.layers[LEVEL_LAYER_FOREGROUND];
  if (!layer)
    return;

  for (int y = 0; y < currentLevel.height; ++y)
    {
      for (int x = 0; x < tilesPerRow; ++x)
        {
          int tileId = layer[y * tilesPerRow + x];
          if (tileId <= 0)
            continue;

          Rectangle source = GetTileSource (tileId);
          if (source.width <= 0)
            continue;

          Vector2 pos = { (float)(x * TILE_SIZE), (float)(y * TILE_SIZE) };
          DrawTextureRec (currentLevel.tileset, source, pos, WHITE);
        }
    }
}

void
UnloadLevel (void)
{
  if (currentLevel.tileMap)
    {
      free (currentLevel.tileMap);
      currentLevel.tileMap = NULL;
    }

  for (int layer = 0; layer < LEVEL_LAYER_COUNT; ++layer)
    {
      if (currentLevel.layers[layer])
        {
          free (currentLevel.layers[layer]);
          currentLevel.layers[layer] = NULL;
        }
    }

  if (currentLevel.colliders)
    {
      free (currentLevel.colliders);
      currentLevel.colliders = NULL;
    }

  currentLevel.height = 0;
  currentLevel.width = 0;
  currentLevel.numColliders = 0;
  tilesetColumns = 1;
}

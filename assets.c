#include "assets.h"
#include <raylib.h>
#include <stdio.h> /* For snprintf */

/* Configuration constants for asset loading */
#define ASSET_BASE_PATH "../assets/"
#define MAX_PATH_LENGTH 512

/* Asset loading state tracking */
static bool assetsLoaded = false;

/* Global asset structures */
PlayerAssets playerAssets = { 0 };
EnemyAssets enemyAssets = { 0 };
Texture2D bgFar = { 0 }, bgNear = { 0 }, tileset = { 0 }, shadowTex = { 0 };
Texture2D propBush = { 0 }, propCar = { 0 };
GameSounds gameSounds = { 0 };

/* Animation frame counts - centralized for maintainability */
#define PLAYER_IDLE_FRAMES 4
#define PLAYER_WALK_FRAMES 10
#define PLAYER_JUMP_FRAMES 4
#define PLAYER_JAB_FRAMES 3
#define PLAYER_PUNCH_FRAMES 3
#define PLAYER_KICK_FRAMES 5
#define PLAYER_JUMP_KICK_FRAMES 3
#define PLAYER_DIVE_KICK_FRAMES 5
#define PLAYER_HURT_FRAMES 2

#define ENEMY_IDLE_FRAMES 4
#define ENEMY_WALK_FRAMES 4
#define ENEMY_PUNCH_FRAMES 3
#define ENEMY_HURT_FRAMES 4

/* Asset path definitions - centralized for maintainability */
#define PLAYER_SPRITE_PATH ASSET_BASE_PATH "Sprites/Brawler-Girl/"
#define ENEMY_SPRITE_PATH ASSET_BASE_PATH "Sprites/Enemy-Punk/"
#define STAGE_LAYERS_PATH ASSET_BASE_PATH "Stage Layers/"
#define STAGE_PROPS_PATH STAGE_LAYERS_PATH "props/"
#define SHADOW_PATH ASSET_BASE_PATH "Sprites/shadow.png"
#define BG_FAR_PATH STAGE_LAYERS_PATH "back.png"
#define BG_NEAR_PATH STAGE_LAYERS_PATH "fore.png"
#define TILESET_PATH STAGE_LAYERS_PATH "tileset.png"

/**
 * Safely builds a texture path with bounds checking
 * @param buffer Output buffer for the path
 * @param bufferSize Size of the output buffer
 * @param basePath Base directory path
 * @param filename Filename without extension
 * @param frameNumber Frame number (0-based)
 * @return true if path was built successfully, false on error
 */
static bool
BuildTexturePath (char *buffer, size_t bufferSize, const char *basePath,
                  const char *filename, int frameNumber)
{
  if (!buffer || !basePath || !filename || bufferSize == 0 || frameNumber < 0)
    {
      TraceLog (LOG_ERROR, "Invalid parameters for BuildTexturePath");
      return false;
    }

  int written = snprintf (buffer, bufferSize, "%s%s%d.png", basePath, filename,
                          frameNumber + 1);
  if (written < 0 || (size_t)written >= bufferSize)
    {
      buffer[0] = '\0';
      TraceLog (LOG_ERROR, "Texture path truncated for %s%s", basePath,
                filename);
      return false;
    }

  return true;
}

static void UnloadAnimation (SpriteAnim *anim);

/**
 * Validates animation parameters and loads frames
 * @param anim Animation structure to populate
 * @param basePath Base path for animation frames
 * @param filename Base filename for frames
 * @param numFrames Number of frames to load
 * @return true if all frames loaded successfully, false otherwise
 */
static bool
LoadAnimation (SpriteAnim *anim, const char *basePath, const char *filename,
               int numFrames)
{
  if (!anim || !basePath || !filename)
    {
      TraceLog (LOG_ERROR, "Invalid parameters for LoadAnimation");
      return false;
    }

  /* Validate frame count */
  if (numFrames <= 0 || numFrames > MAX_FRAMES)
    {
      TraceLog (LOG_ERROR, "Invalid frame count: %d (must be 1-%d)", numFrames,
                MAX_FRAMES);
      return false;
    }

  anim->numFrames = numFrames;
  bool allLoaded = true;

  for (int i = 0; i < numFrames; ++i)
    {
      char path[MAX_PATH_LENGTH];

      if (!BuildTexturePath (path, sizeof (path), basePath, filename, i))
        {
          TraceLog (LOG_ERROR, "Failed to build path for frame %d", i);
          allLoaded = false;
          continue;
        }

      anim->frames[i] = LoadTexture (path);
      if (anim->frames[i].id <= 0)
        {
          TraceLog (LOG_ERROR, "Failed to load texture: %s", path);
          allLoaded = false;
        }
    }

  if (!allLoaded)
    {
      UnloadAnimation (anim);
      return false;
    }

  return true;
}

/**
 * Safely unloads animation frames with validation
 * @param anim Animation structure to unload
 */
static void
UnloadAnimation (SpriteAnim *anim)
{
  if (!anim)
    {
      TraceLog (LOG_WARNING, "Attempted to unload NULL animation");
      return;
    }

  for (int i = 0; i < anim->numFrames && i < MAX_FRAMES; ++i)
    {
      if (anim->frames[i].id > 0)
        {
          UnloadTexture (anim->frames[i]);
          anim->frames[i] = (Texture2D){ 0 }; /* Clear the texture */
        }
    }

  anim->numFrames = 0;
}

/**
 * Loads a single texture with error handling
 * @param path Path to the texture file
 * @param description Human-readable description for error messages
 * @return Loaded texture, or empty texture on failure
 */
static Texture2D
LoadTextureWithValidation (const char *path, const char *description)
{
  if (!path || !description)
    {
      TraceLog (LOG_ERROR, "Invalid parameters for LoadTextureWithValidation");
      return (Texture2D){ 0 };
    }

  Texture2D texture = LoadTexture (path);
  if (texture.id <= 0)
    {
      TraceLog (LOG_ERROR, "Failed to load %s texture: %s", description, path);
    }

  return texture;
}

bool
LoadAssets (void)
{
  if (assetsLoaded)
    {
      TraceLog (LOG_WARNING, "Assets already loaded, skipping reload");
      return true;
    }

  bool loadSuccess = true;

  /* Load player animations (Brawler Girl) */
  loadSuccess &= LoadAnimation (&playerAssets.idle, PLAYER_SPRITE_PATH "Idle/",
                                "idle", PLAYER_IDLE_FRAMES);
  loadSuccess &= LoadAnimation (&playerAssets.walk, PLAYER_SPRITE_PATH "Walk/",
                                "walk", PLAYER_WALK_FRAMES);
  loadSuccess &= LoadAnimation (&playerAssets.jump, PLAYER_SPRITE_PATH "Jump/",
                                "jump", PLAYER_JUMP_FRAMES);
  loadSuccess &= LoadAnimation (&playerAssets.jab, PLAYER_SPRITE_PATH "Jab/",
                                "jab", PLAYER_JAB_FRAMES);
  loadSuccess
      &= LoadAnimation (&playerAssets.punch, PLAYER_SPRITE_PATH "Punch/",
                        "punch", PLAYER_PUNCH_FRAMES);
  loadSuccess &= LoadAnimation (&playerAssets.kick, PLAYER_SPRITE_PATH "Kick/",
                                "kick", PLAYER_KICK_FRAMES);
  loadSuccess &= LoadAnimation (&playerAssets.jump_kick,
                                PLAYER_SPRITE_PATH "Jump_kick/", "jump_kick",
                                PLAYER_JUMP_KICK_FRAMES);
  loadSuccess &= LoadAnimation (&playerAssets.dive_kick,
                                PLAYER_SPRITE_PATH "Dive_kick/", "dive_kick",
                                PLAYER_DIVE_KICK_FRAMES);
  loadSuccess &= LoadAnimation (&playerAssets.hurt, PLAYER_SPRITE_PATH "Hurt/",
                                "hurt", PLAYER_HURT_FRAMES);

  /* Load enemy animations (Punk) */
  loadSuccess &= LoadAnimation (&enemyAssets.idle, ENEMY_SPRITE_PATH "Idle/",
                                "idle", ENEMY_IDLE_FRAMES);
  loadSuccess &= LoadAnimation (&enemyAssets.walk, ENEMY_SPRITE_PATH "Walk/",
                                "walk", ENEMY_WALK_FRAMES);
  loadSuccess &= LoadAnimation (&enemyAssets.punch, ENEMY_SPRITE_PATH "Punch/",
                                "punch", ENEMY_PUNCH_FRAMES);
  loadSuccess &= LoadAnimation (&enemyAssets.hurt, ENEMY_SPRITE_PATH "Hurt/",
                                "hurt", ENEMY_HURT_FRAMES);

  /* Load level and effect textures */
  bgFar = LoadTextureWithValidation (BG_FAR_PATH, "background far");
  bgNear = LoadTextureWithValidation (BG_NEAR_PATH, "background near");
  tileset = LoadTextureWithValidation (TILESET_PATH, "tileset");
  shadowTex = LoadTextureWithValidation (SHADOW_PATH, "shadow");
  propBush = LoadTextureWithValidation (STAGE_PROPS_PATH "barrel.png",
                                        "prop barrel");
  propCar = LoadTextureWithValidation (STAGE_PROPS_PATH "car.png", "prop car");

  /* Load sounds */
  gameSounds.punchSound = LoadSound ("../assets/sounds/punch.wav");
  if (gameSounds.punchSound.stream.buffer == NULL)
    {
      TraceLog (LOG_WARNING,
                "Failed to load punch sound - continuing without audio");
    }
  gameSounds.kickSound = LoadSound ("../assets/sounds/punch.wav");
  if (gameSounds.kickSound.stream.buffer == NULL)
    {
      TraceLog (LOG_WARNING,
                "Failed to load kick sound - continuing without audio");
    }
  gameSounds.deathSound = LoadSound ("../assets/sounds/kick.ogg");
  if (gameSounds.deathSound.stream.buffer == NULL)
    {
      TraceLog (LOG_WARNING,
                "Failed to load death sound - continuing without audio");
    }

  /* Check if all critical assets loaded */
  if (bgFar.id <= 0 || bgNear.id <= 0 || tileset.id <= 0 || shadowTex.id <= 0)
    {
      TraceLog (
          LOG_ERROR,
          "Critical assets failed to load - game may not function properly");
      loadSuccess = false;
    }

  assetsLoaded = true;

  if (loadSuccess)
    {
      TraceLog (LOG_INFO, "All assets loaded successfully");
    }
  else
    {
      TraceLog (
          LOG_WARNING,
          "Some assets failed to load - check paths and file availability");
    }

  return loadSuccess;
}

void
UnloadAssets (void)
{
  if (!assetsLoaded)
    {
      TraceLog (LOG_WARNING,
                "Assets not flagged as loaded; attempting cleanup anyway");
    }

  /* Unload player animations */
  UnloadAnimation (&playerAssets.idle);
  UnloadAnimation (&playerAssets.walk);
  UnloadAnimation (&playerAssets.jump);
  UnloadAnimation (&playerAssets.jab);
  UnloadAnimation (&playerAssets.punch);
  UnloadAnimation (&playerAssets.kick);
  UnloadAnimation (&playerAssets.jump_kick);
  UnloadAnimation (&playerAssets.dive_kick);
  UnloadAnimation (&playerAssets.hurt);

  /* Unload enemy animations */
  UnloadAnimation (&enemyAssets.idle);
  UnloadAnimation (&enemyAssets.walk);
  UnloadAnimation (&enemyAssets.punch);
  UnloadAnimation (&enemyAssets.hurt);

  /* Unload level/effect textures */
  if (bgFar.id > 0)
    {
      UnloadTexture (bgFar);
      bgFar = (Texture2D){ 0 };
    }
  if (bgNear.id > 0)
    {
      UnloadTexture (bgNear);
      bgNear = (Texture2D){ 0 };
    }
  if (tileset.id > 0)
    {
      UnloadTexture (tileset);
      tileset = (Texture2D){ 0 };
    }
  if (shadowTex.id > 0)
    {
      UnloadTexture (shadowTex);
      shadowTex = (Texture2D){ 0 };
    }
  if (propBush.id > 0)
    {
      UnloadTexture (propBush);
      propBush = (Texture2D){ 0 };
    }
  if (propCar.id > 0)
    {
      UnloadTexture (propCar);
      propCar = (Texture2D){ 0 };
    }

  /* Unload sounds */
  if (gameSounds.punchSound.stream.buffer != NULL)
    {
      UnloadSound (gameSounds.punchSound);
      gameSounds.punchSound = (Sound){ 0 };
    }
  if (gameSounds.kickSound.stream.buffer != NULL)
    {
      UnloadSound (gameSounds.kickSound);
      gameSounds.kickSound = (Sound){ 0 };
    }
  if (gameSounds.deathSound.stream.buffer != NULL)
    {
      UnloadSound (gameSounds.deathSound);
      gameSounds.deathSound = (Sound){ 0 };
    }

  assetsLoaded = false;
  TraceLog (LOG_INFO, "All assets unloaded successfully");
}

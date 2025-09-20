#include "assets.h"   /* Asset management */
#include "entities.h" /* Entity logic */
#include "level.h"
#include <math.h>   /* fmodf for parallax tiling */
#include <raylib.h> /* Core library */
#include <stdlib.h> /* Standard library */

/* ============================================================================
 * GAME CONSTANTS - UI and display constants
 * ============================================================================
 */

#define TARGET_FPS 60
#define GAME_OVER_FONT_SIZE 40
#define RESTART_FONT_SIZE 20
#define UI_MARGIN 10
#define UI_LINE_HEIGHT 30
#define TEXT_OFFSET_VERTICAL 20
#define CONTROLS_FONT_SIZE 16

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

/**
 * Initializes the game state including player and enemies
 * @param player Player entity to initialize
 * @param enemyCount Number of enemies to spawn
 */
typedef enum
{
  GAME_STATE_MENU = 0,
  GAME_STATE_PLAYING,
  GAME_STATE_PAUSED,
  GAME_STATE_GAME_OVER,
  GAME_STATE_WIN
} GameState;

static void
InitializeGameState (Player *players, int numPlayers)
{
  if (!players || numPlayers < 0 || numPlayers > 2)
    {
      TraceLog (LOG_ERROR, "Invalid parameters for game state initialization");
      return;
    }

  /* Initialize players at starting positions */
  Vector2 startPositions[2] = { { 100.0f, GROUND_Y - PLAYER_HEIGHT },
                                { 150.0f, GROUND_Y - PLAYER_HEIGHT } };

  for (int i = 0; i < 2; ++i)
    {
      if (i < numPlayers)
        {
          InitPlayer (&players[i], startPositions[i]);
        }
      else
        {
          players[i] = (Player){ 0 };
          players[i].health = 0;
          players[i].state = ENTITY_STATE_DEAD;
        }
    }
}

/**
 * Fully resets the game to stage zero with fresh players and enemies
 */
static bool
StartNewGame (Player *players, int numPlayers)
{
  if (!players || numPlayers <= 0)
    {
      TraceLog (LOG_ERROR, "StartNewGame called without valid players array");
      return false;
    }

  currentStage = 0;
  if (!InitLevel (currentStage))
    {
      TraceLog (LOG_ERROR, "Failed to initialise stage %d", currentStage);
      return false;
    }

  InitializeGameState (players, numPlayers);
  camera.target = (Vector2){ players[0].position.x, GROUND_Y * 0.5f };

  return true;
}

/**
 * Draws a parallax background layer scaled to the window and tiled horizontally
 */
static void
DrawParallaxLayer (Texture2D texture, float parallaxFactor, float targetHeight,
                   float bottomY)
{
  if (texture.id <= 0)
    {
      return;
    }

  if (targetHeight <= 0.0f)
    targetHeight = (float)SCREEN_HEIGHT;
  if (bottomY <= 0.0f)
    bottomY = targetHeight;

  float scale = targetHeight / (float)texture.height;
  float scaledWidth = texture.width * scale;
  if (scaledWidth <= 0.0f)
    {
      return;
    }

  float offset = fmodf (-camera.target.x * parallaxFactor, scaledWidth);
  if (offset > 0.0f)
    {
      offset -= scaledWidth;
    }

  Rectangle src = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };
  for (int i = 0; i < 3; ++i)
    {
      Rectangle dest = { offset + i * scaledWidth, bottomY - targetHeight,
                         scaledWidth, targetHeight };
      DrawTexturePro (texture, src, dest, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
    }
}

static float
ClampFloatMain (float value, float minVal, float maxVal)
{
  if (value < minVal)
    return minVal;
  if (value > maxVal)
    return maxVal;
  return value;
}

static float
ComputeLeadPlayerX (const Player *players, int numPlayers)
{
  float leadX = 0.0f;
  bool hasLead = false;
  for (int i = 0; i < numPlayers; ++i)
    {
      if (players[i].health > 0)
        {
          if (!hasLead || players[i].position.x > leadX)
            {
              leadX = players[i].position.x;
              hasLead = true;
            }
        }
    }

  if (!hasLead && numPlayers > 0)
    {
      leadX = players[0].position.x;
    }

  return leadX;
}

static void
ResetPlayersForStage (Player *players, int numPlayers)
{
  Vector2 startPositions[2] = { { 100.0f, GROUND_Y - PLAYER_HEIGHT },
                                { 150.0f, GROUND_Y - PLAYER_HEIGHT } };

  for (int i = 0; i < numPlayers; ++i)
    {
      int preservedHealth = players[i].health;
      bool alive = preservedHealth > 0;

      InitPlayer (&players[i], startPositions[i]);

      if (alive)
        {
          players[i].health = preservedHealth;
        }
      else
        {
          players[i].health = 0;
          players[i].state = ENTITY_STATE_DEAD;
        }
    }
}

static inline float Lerp (float a, float b, float t); // Prototype

static void
UpdateCameraTargetForLead (float leadX, int screenWidth)
{
  float desired = leadX + 80.0f;
  float halfWidth = screenWidth * 0.5f;
  float maxTarget = LevelWidth () - halfWidth;
  if (maxTarget < halfWidth)
    maxTarget = halfWidth;

  camera.target.x = Lerp (camera.target.x,
                          ClampFloatMain (desired, halfWidth, maxTarget), 0.1f);
  camera.target.y = GROUND_Y * 0.5f;
}

static void
DrawMenuScreen (int screenWidth, int screenHeight)
{
  BeginDrawing ();
  ClearBackground (BLACK);

  const char *title = "Dragon Fight";
  int titleSize = 48;
  int titleWidth = MeasureText (title, titleSize);
  DrawText (title, screenWidth / 2 - titleWidth / 2, screenHeight / 3,
            titleSize, WHITE);

  const char *prompt = "Press ENTER to Start";
  int promptSize = 24;
  int promptWidth = MeasureText (prompt, promptSize);
  DrawText (prompt, screenWidth / 2 - promptWidth / 2, screenHeight / 2,
            promptSize, LIGHTGRAY);

  const char *controls = "Player 1: A/D Move, W Jump, J Jab, L Punch, K Kick";
  int controlsSize = 20;
  int controlsWidth = MeasureText (controls, controlsSize);
  DrawText (controls, screenWidth / 2 - controlsWidth / 2,
            (int)(screenHeight * 0.65f), controlsSize, GRAY);

  EndDrawing ();
}

/**
 * Draws the game UI including health bars and controls
 * @param player Player entity for health display
 */
static void
DrawGameUI (const Player *players, int numPlayers, GameState gameState)
{
  if (!players)
    {
      return; /* Null pointer safety */
    }

  if (gameState == GAME_STATE_PAUSED || gameState == GAME_STATE_GAME_OVER
      || gameState == GAME_STATE_WIN)
    {
      DrawRectangle (0, 0, (int)SCREEN_WIDTH, (int)SCREEN_HEIGHT,
                     Fade (BLACK, 0.45f));
    }

  // Health bars and labels
  int barStartY = 10;
  int maxBarWidth = 200;
  int labelY = barStartY + 25;
  int nextUIY = barStartY + 50;

  if (numPlayers > 0 && players[0].health > 0)
    {
      int p1BarWidth = (players[0].health * maxBarWidth) / PLAYER_MAX_HEALTH;
      Color p1Color = (players[0].health > 50)
                          ? GREEN
                          : ((players[0].health > 25) ? YELLOW : RED);
      DrawRectangle (10, barStartY, p1BarWidth, 20, p1Color);
      DrawRectangle (10 + p1BarWidth, barStartY, maxBarWidth - p1BarWidth, 20,
                     Fade (RED, 0.3f));
      DrawText (TextFormat ("P1 HP: %d", players[0].health), 10, labelY, 16,
                BLACK);
    }

  if (numPlayers > 1 && players[1].health > 0)
    {
      int p2BarWidth = (players[1].health * maxBarWidth) / PLAYER_MAX_HEALTH;
      Color p2Color = (players[1].health > 50)
                          ? GREEN
                          : ((players[1].health > 25) ? YELLOW : RED);
      DrawRectangle (220, barStartY, p2BarWidth, 20, p2Color);
      DrawRectangle (220 + p2BarWidth, barStartY, maxBarWidth - p2BarWidth, 20,
                     Fade (RED, 0.3f));
      DrawText (TextFormat ("P2 HP: %d", players[1].health), 220, labelY, 16,
                BLACK);
    }

  // Now the line-based UI starting lower
  int line = 0;
  int uiStartY = nextUIY; // 60

  DrawText (TextFormat ("Stage: %d / %d", currentStage + 1, GetStageCount ()),
            UI_MARGIN, uiStartY + line * UI_LINE_HEIGHT, RESTART_FONT_SIZE,
            WHITE);
  line++;

  DrawText (TextFormat ("Enemies Remaining: %d", LevelEnemiesRemaining ()),
            UI_MARGIN, uiStartY + line * UI_LINE_HEIGHT, RESTART_FONT_SIZE,
            WHITE);
  line++;

  /* Draw controls help text */
  const char *controlsText
      = (numPlayers > 1) ? "P1: A/D Walk, W Jump, J Jab, L Punch, K Kick | P2: "
                           "Arrows Move, Up Jump, Z Jab, X Punch, C Kick"
                         : "Controls: A/D Walk, W Jump, J Jab, L Punch, K Kick";
  DrawText (controlsText, UI_MARGIN, (int)SCREEN_HEIGHT - 60,
            CONTROLS_FONT_SIZE, WHITE);
  DrawFPS (UI_MARGIN, (int)SCREEN_HEIGHT - 30);

  const char *overlayText = NULL;
  Color overlayColor = WHITE;
  int overlaySize = 32;

  switch (gameState)
    {
    case GAME_STATE_PAUSED:
      overlayText = "Paused";
      overlayColor = YELLOW;
      break;
    case GAME_STATE_GAME_OVER:
      overlayText = "Game Over - Press R to Restart";
      overlayColor = RED;
      break;
    case GAME_STATE_WIN:
      overlayText = "You Win! Press R to Restart";
      overlayColor = GREEN;
      break;
    default:
      break;
    }

  if (overlayText)
    {
      int textWidth = MeasureText (overlayText, overlaySize);
      DrawText (overlayText, (int)(SCREEN_WIDTH * 0.5f) - textWidth / 2,
                (int)(SCREEN_HEIGHT * 0.15f), overlaySize, overlayColor);
    }
}

/**
 * Draws all active game entities
 * @param player Player entity to draw
 */
static void
DrawGameEntities (Player *players, int numPlayers)
{
  if (!players)
    {
      return; /* Null pointer safety */
    }

  /* Draw players */
  for (int i = 0; i < numPlayers; ++i)
    {
      if (players[i].health <= 0 && players[i].deathTimer <= 0.0f)
        continue;
      DrawPlayer (&players[i]);
    }

  /* Draw all active enemies */
  for (int i = 0; i < activeEnemies; ++i)
    {
      if (enemies[i].health <= 0 && enemies[i].deathTimer <= 0.0f)
        continue;
      DrawEnemy (&enemies[i]);
    }
}

/* ============================================================================
 * MAIN GAME LOOP
 * ============================================================================
 */

static inline float
Lerp (float a, float b, float t)
{

  return a + (b - a) * t;
}

int
main (void)
{
  const int screenWidth = (int)SCREEN_WIDTH;
  const int screenHeight = (int)SCREEN_HEIGHT;

  InitWindow (screenWidth, screenHeight, "Dragon Fight - Double Dragon Style");

  if (!IsWindowReady ())
    {
      TraceLog (LOG_ERROR, "Failed to create window - check display settings");
      return EXIT_FAILURE;
    }

  SetTargetFPS (TARGET_FPS);
  SetExitKey (KEY_ESCAPE);

  InitAudioDevice (); // Initialize audio device for sound playback

  if (!LoadAssets ())
    {
      TraceLog (LOG_ERROR, "Critical assets failed to load");
      UnloadAssets ();
      CloseAudioDevice (); // Cleanup audio
      CloseWindow ();
      return EXIT_FAILURE;
    }

  camera.zoom = 1.0f;
  camera.rotation = 0.0f;
  camera.offset = (Vector2){ screenWidth / 2.0f, screenHeight / 2.0f };
  camera.target = (Vector2){ 0.0f, GROUND_Y * 0.5f };

  Player players[2] = { 0 };
  int playerCount = 1;
  GameState gameState = GAME_STATE_MENU;
  float p2LastInputTime = 0.0f;

  while (!WindowShouldClose ())
    {
      float dt = GetFrameTime ();

      if (dt > 1.0f / 30.0f)
        dt = 1.0f / 30.0f; // Cap dt to prevent speedup

      const float fixedDt = 1.0f / 60.0f;

      float accumulator = 0.0f;

      accumulator += dt;

      if (gameState == GAME_STATE_MENU)
        {
          if (IsKeyPressed (KEY_ENTER) || IsKeyPressed (KEY_SPACE))
            {
              if (StartNewGame (players, playerCount))
                {
                  UpdateCameraTargetForLead (
                      ComputeLeadPlayerX (players, playerCount), screenWidth);
                  gameState = GAME_STATE_PLAYING;
                }
              else
                {
                  TraceLog (LOG_ERROR, "Unable to start new game");
                }
            }

          DrawMenuScreen (screenWidth, screenHeight);
          continue;
        }

      bool p2Input = IsKeyDown (KEY_LEFT) || IsKeyDown (KEY_RIGHT)
                     || IsKeyPressed (KEY_UP) || IsKeyPressed (KEY_Z)
                     || IsKeyPressed (KEY_X) || IsKeyPressed (KEY_C);
      if (p2Input)
        {
          p2LastInputTime = GetTime ();
        }
      if (playerCount == 1 && p2Input)
        {
          // Activate P2
          Vector2 p2Pos
              = { players[0].position.x + 50.0f, players[0].position.y };
          InitPlayer (&players[1], p2Pos);
          players[1].health = PLAYER_MAX_HEALTH;
          playerCount = 2;
          TraceLog (LOG_INFO, "Player 2 activated");
        }
      if (playerCount == 2)
        {
          float inactivityTime = GetTime () - p2LastInputTime;
          if (inactivityTime > 5.0f)
            {
              // Deactivate P2
              players[1].health = 0;
              players[1].state = ENTITY_STATE_DEAD;
              players[1].deathTimer = 0.0f;
              players[1].velocity = (Vector2){ 0.0f, 0.0f };
              playerCount = 1;
              TraceLog (LOG_INFO, "Player 2 deactivated due to inactivity");
            }
        }

      if ((gameState == GAME_STATE_GAME_OVER || gameState == GAME_STATE_WIN)
          && (IsKeyPressed (KEY_R) || IsKeyPressed (KEY_ENTER)))
        {
          if (StartNewGame (players, playerCount))
            {
              UpdateCameraTargetForLead (
                  ComputeLeadPlayerX (players, playerCount), screenWidth);
              gameState = GAME_STATE_PLAYING;
            }
          else
            {
              TraceLog (LOG_ERROR,
                        "Restart failed - remaining in current state");
            }
        }

      if (IsKeyPressed (KEY_P))
        {
          if (gameState == GAME_STATE_PLAYING)
            {
              gameState = GAME_STATE_PAUSED;
            }
          else if (gameState == GAME_STATE_PAUSED)
            {
              gameState = GAME_STATE_PLAYING;
            }
        }

      bool shouldUpdate = (gameState == GAME_STATE_PLAYING);

      if (shouldUpdate)
        {
          int substeps = 0;
          while (accumulator >= fixedDt && substeps < 3) // Max 3 substeps/frame
            {
              UpdatePlayer (&players[0], fixedDt, false);
              if (playerCount > 1)
                UpdatePlayer (&players[1], fixedDt, true);
              UpdateEnemies (players, playerCount, fixedDt);

              // After substeps, update animations with fixedDt for enemies to
              // prevent skips
              UpdateLevel (fixedDt,
                           ComputeLeadPlayerX (
                               players, playerCount)); // Use leadX from current
              accumulator -= fixedDt;
              substeps++;
            }

          // Non-physics updates with actual dt (UI, etc.)
          float leadX = ComputeLeadPlayerX (players, playerCount);
          if (shouldUpdate)
            {
              UpdateCameraTargetForLead (leadX, screenWidth);
            }

          if (shouldUpdate)
            {
              bool allPlayersDead = true;
              for (int i = 0; i < playerCount; ++i)
                {
                  if (players[i].health > 0 || players[i].deathTimer > 0.0f)
                    {
                      allPlayersDead = false;
                      break;
                    }
                }

              if (allPlayersDead)
                {
                  gameState = GAME_STATE_GAME_OVER;
                }
            }

          if (shouldUpdate && gameState == GAME_STATE_PLAYING)
            {
              if (LevelIsCleared () && leadX >= LevelStageEndX ())
                {
                  int nextStage = currentStage + 1;
                  if (nextStage >= GetStageCount ())
                    {
                      gameState = GAME_STATE_WIN;
                    }
                  else
                    {
                      if (InitLevel (nextStage))
                        {
                          ResetPlayersForStage (players, playerCount);
                          UpdateCameraTargetForLead (
                              ComputeLeadPlayerX (players, playerCount),
                              screenWidth);
                        }
                      else
                        {
                          TraceLog (LOG_ERROR, "Failed to load next stage (%d)",
                                    nextStage);
                          gameState = GAME_STATE_PAUSED;
                        }
                    }
                }
            }
        }

      BeginDrawing ();
      ClearBackground (BLACK);

      float groundScreenY = camera.offset.y + (GROUND_Y - camera.target.y);
      float nearHeight = groundScreenY;

      DrawParallaxLayer (bgFar, 0.2f, (float)screenHeight, (float)screenHeight);
      DrawParallaxLayer (bgNear, 0.5f, nearHeight, groundScreenY);

      BeginMode2D (camera);
      DrawLevel ();
      DrawGameEntities (players, playerCount);
      DrawLevelForeground ();
      EndMode2D ();

      DrawGameUI (players, playerCount, gameState);

      EndDrawing ();
    }

  UnloadAssets ();
  UnloadLevel ();
  CloseAudioDevice (); // Close audio device
  CloseWindow ();

  return EXIT_SUCCESS;
}

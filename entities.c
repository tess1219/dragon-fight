#include "assets.h" /* For gameSounds */
#include "entities.h"
#include "level.h"  /* For world bounds and boss state */
#include <assert.h> /* For assertions */
#include <limits.h> /* For INT_MIN/MAX */
#include <math.h>   /* For fabsf */
#include <stdlib.h> /* For rand, RAND_MAX */
#include <string.h> /* For memset */

/* ============================================================================
 * GLOBAL STATE
 * ============================================================================
 */

Enemy enemies[MAX_ENEMIES] = { 0 };
int activeEnemies = 0;

void
ClearEnemies (void)
{
  memset (enemies, 0, sizeof (enemies));
  activeEnemies = 0;
}

typedef struct
{
  int animIndex;
  int damage;
  float cooldown;
  int hitFrameStart;
  int hitFrameEnd;
} AttackProfile;

static const AttackProfile PLAYER_ATTACK_JAB_PROFILE
    = { ANIM_JAB, ATTACK_DAMAGE, 0.25f, 1, 1 };
static const AttackProfile PLAYER_ATTACK_PUNCH_PROFILE
    = { ANIM_PUNCH, PUNCH_DAMAGE, 0.45f, 1, 2 };
static const AttackProfile PLAYER_ATTACK_KICK_PROFILE
    = { ANIM_KICK, KICK_DAMAGE, 0.6f, 2, 3 };
static const AttackProfile ENEMY_ATTACK_PUNCH_PROFILE
    = { ANIM_PUNCH, ENEMY_DAMAGE, 0.8f, 1, 1 };
static const AttackProfile ENEMY_ATTACK_KICK_PROFILE
    = { ANIM_KICK, KICK_DAMAGE, 1.0f, 2, 3 };

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

/**
 * Safely clamps an integer value to a specified range
 * @param value Value to clamp
 * @param min Minimum allowed value
 * @param max Maximum allowed value
 * @return Clamped value
 */
static inline int
SafeClamp (int value, int min, int max)
{
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

/**
 * Safely clamps a float value to a specified range
 * @param value Value to clamp
 * @param min Minimum allowed value
 * @param max Maximum allowed value
 * @return Clamped value
 */
static inline float
SafeClampFloat (float value, float min, float max)
{
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

static inline float
ClampHorizontalSpeed (float value)
{
  return SafeClampFloat (value, -MAX_ENTITY_SPEED, MAX_ENTITY_SPEED);
}

static Rectangle
ComputeAttackHitbox (const Entity *attacker)
{
  if (!attacker)
    {
      return (Rectangle){ 0, 0, 0, 0 };
    }

  const Rectangle base = attacker->hitbox;
  const float baseWidth = base.width;
  const float reach = baseWidth + ATTACK_EXTEND;

  Rectangle result = base;
  result.width = reach;
  result.height = base.height * 1.15f;
  result.y -= base.height * 0.075f;

  if (attacker->facingRight)
    {
      result.x += baseWidth * 0.35f;
    }
  else
    {
      result.x -= reach - baseWidth * 0.35f;
    }

  return result;
}

static void
ApplyKnockback (Entity *target, float direction)
{
  float desired = direction * KNOCKBACK_FORCE;
  target->velocity.x
      = SafeClampFloat (desired, -MAX_KNOCKBACK_SPEED, MAX_KNOCKBACK_SPEED);
}

/* ============================================================================
 * RENDERING FUNCTIONS
 * ============================================================================
 */

/**
 * Draws a sprite with optional horizontal flipping for facing direction
 * @param tex Texture to draw
 * @param pos Position to draw at
 * @param facingRight Whether sprite should face right
 */
static void
DrawSprite (Texture2D tex, Vector2 pos, bool facingRight)
{
  /* Validate texture before drawing */
  if (tex.id <= 0)
    {
      return; /* Skip invalid texture */
    }

  if (facingRight)
    {
      DrawTexture (tex, (int)pos.x, (int)pos.y, WHITE);
    }
  else
    {
      /* Flip horizontally by negating source width */
      Rectangle source = { 0.0f, 0.0f, -(float)tex.width, (float)tex.height };
      Rectangle dest = { pos.x, pos.y, (float)tex.width, (float)tex.height };
      Vector2 origin = { 0.0f, 0.0f };
      DrawTexturePro (tex, source, dest, origin, 0.0f, WHITE);
    }
}

/**
 * Draws a shadow under an entity with specified tint
 * @param pos Entity position
 * @param shadowY Y position for shadow
 * @param tint Shadow color tint
 */
static void
DrawShadow (Vector2 pos, float shadowY, Color tint)
{
  if (shadowTex.id <= 0)
    {
      return; /* No shadow texture available */
    }

  float offsetX = (PLAYER_WIDTH - SHADOW_WIDTH) / 2.0f;
  DrawTexture (shadowTex, (int)(pos.x + offsetX), (int)shadowY, tint);
}

/* ============================================================================
 * ANIMATION SYSTEM
 * ============================================================================
 */

/**
 * Updates animation state with proper bounds checking
 * @param anim Animation structure to update
 * @param dt Delta time since last update
 */
void
UpdateAnimation (Animation *anim, float dt)
{
  if (!anim)
    {
      return; /* Null pointer safety */
    }

  (void)dt; // Suppress unused param warning (fixed timestep used)

  if (anim->totalFrames <= 0)
    {
      anim->currentFrame = 0;
      return;
    }

  if (anim->totalFrames <= 3)
    {
      anim->timer += dt * 12.0f; // Fast for attacks
    }
  else
    {
      anim->timer += dt * 8.0f; // Normal for other anims
    }

  while (anim->timer >= 1.0f)
    {
      anim->timer -= 1.0f;
      anim->currentFrame++;
      if (anim->currentFrame >= anim->totalFrames)
        {
          anim->currentFrame = anim->totalFrames - 1;
          break;
        }
    }

  anim->currentFrame = SafeClamp (anim->currentFrame, 0, anim->totalFrames - 1);
}

/* ============================================================================
 * PHYSICS SYSTEM
 * ============================================================================
 */

/**
 * Updates entity physics including gravity, position integration, and collision
 * @param e Entity to update
 * @param dt Delta time since last update
 */
static void
UpdatePhysics (Entity *e, float dt)
{
  if (!e)
    {
      return; /* Null pointer safety */
    }

  // Calculate scale for hitbox (bosses)
  float scale = 1.0f;
  if (e->maxHealth > ENEMY_MAX_HEALTH)
    {
      scale = 1.5f;
    }
  float entity_width = PLAYER_WIDTH * scale;
  float entity_height = PLAYER_HEIGHT * scale;

  e->grounded = false;

  const bool hasTileMap
      = currentLevel.tileMap != NULL && currentLevel.height > 0;
  const int levelCols = hasTileMap ? LevelTileColumns () : 0;
  const int levelRows = hasTileMap ? LevelRowCount () : 0;

  // Horizontal movement and collision
  float delta_x = e->velocity.x * dt;
  float new_x = e->position.x + delta_x;
  Rectangle temp_hitbox
      = (Rectangle){ new_x, e->position.y, entity_width, entity_height };

  bool horiz_collided = false;

  if (hasTileMap && levelCols > 0 && levelRows > 0)
    {
      int min_tx = SafeClamp ((int)(fminf (e->position.x, new_x) / TILE_SIZE),
                              0, levelCols - 1);
      int max_tx = SafeClamp (
          (int)(fmaxf (e->position.x + entity_width, new_x + entity_width)
                / TILE_SIZE)
              + 1,
          0, levelCols);
      int min_ty
          = SafeClamp ((int)(e->position.y / TILE_SIZE), 0, levelRows - 1);
      int max_ty = SafeClamp (
          (int)((e->position.y + entity_height) / TILE_SIZE) + 1, 0, levelRows);

      for (int ty = min_ty; ty < max_ty && !horiz_collided; ++ty)
        {
          for (int tx = min_tx; tx < max_tx; ++tx)
            {
              int tile_id = currentLevel.tileMap[ty * levelCols + tx];
              if (tile_id <= 0)
                {
                  continue;
                }

              Rectangle tile_rect
                  = { (float)(tx * TILE_SIZE), (float)(ty * TILE_SIZE),
                      TILE_SIZE, TILE_SIZE };
              if (CheckCollisionRecs (temp_hitbox, tile_rect))
                {
                  if (delta_x > 0.0f)
                    {
                      new_x = (float)(tx * TILE_SIZE) - entity_width;
                    }
                  else if (delta_x < 0.0f)
                    {
                      new_x = (float)((tx + 1) * TILE_SIZE);
                    }
                  horiz_collided = true;
                  break;
                }
            }
        }
    }

  if (currentLevel.colliders && currentLevel.numColliders > 0)
    {
      for (int i = 0; i < currentLevel.numColliders; ++i)
        {
          if (CheckCollisionRecs (temp_hitbox, currentLevel.colliders[i]))
            {
              if (delta_x > 0.0f)
                {
                  new_x = currentLevel.colliders[i].x - entity_width;
                }
              else if (delta_x < 0.0f)
                {
                  new_x = currentLevel.colliders[i].x
                          + currentLevel.colliders[i].width;
                }
              horiz_collided = true;
              break;
            }
        }
    }

  e->position.x = new_x;
  if (horiz_collided)
    {
      e->velocity.x = 0.0f;
    }

  // Vertical movement and collision
  if (!e->grounded)
    {
      e->velocity.y += GRAVITY * dt;
    }
  float delta_y = e->velocity.y * dt;
  float new_y = e->position.y + delta_y;
  temp_hitbox
      = (Rectangle){ e->position.x, new_y, entity_width, entity_height };

  bool vert_collided = false;

  if (hasTileMap && levelCols > 0 && levelRows > 0)
    {
      int min_tx
          = SafeClamp ((int)(e->position.x / TILE_SIZE), 0, levelCols - 1);
      int max_tx = SafeClamp (
          (int)((e->position.x + entity_width) / TILE_SIZE) + 1, 0, levelCols);
      int min_ty = SafeClamp ((int)(fminf (e->position.y, new_y) / TILE_SIZE),
                              0, levelRows - 1);
      int max_ty = SafeClamp (
          (int)(fmaxf (e->position.y + entity_height, new_y + entity_height)
                / TILE_SIZE)
              + 1,
          0, levelRows);

      for (int ty = min_ty; ty < max_ty && !vert_collided; ++ty)
        {
          for (int tx = min_tx; tx < max_tx; ++tx)
            {
              int tile_id = currentLevel.tileMap[ty * levelCols + tx];
              if (tile_id <= 0)
                {
                  continue;
                }

              Rectangle tile_rect
                  = { (float)(tx * TILE_SIZE), (float)(ty * TILE_SIZE),
                      TILE_SIZE, TILE_SIZE };
              if (CheckCollisionRecs (temp_hitbox, tile_rect))
                {
                  if (delta_y >= 0.0f)
                    {
                      new_y = (float)(ty * TILE_SIZE) - entity_height;
                      e->grounded = true;
                    }
                  else
                    {
                      new_y = (float)((ty + 1) * TILE_SIZE);
                    }
                  vert_collided = true;
                  e->velocity.y = 0.0f;
                  break;
                }
            }
        }
    }

  if (currentLevel.colliders && currentLevel.numColliders > 0)
    {
      for (int i = 0; i < currentLevel.numColliders; ++i)
        {
          if (CheckCollisionRecs (temp_hitbox, currentLevel.colliders[i]))
            {
              if (delta_y >= 0.0f)
                {
                  new_y = currentLevel.colliders[i].y - entity_height;
                  e->grounded = true;
                }
              else
                {
                  new_y = currentLevel.colliders[i].y
                          + currentLevel.colliders[i].height;
                }
              vert_collided = true;
              e->velocity.y = 0.0f;
              break;
            }
        }
    }

  e->position.y = new_y;

  float stage_width
      = currentLevel.width > 0 ? (float)currentLevel.width : SCREEN_WIDTH;
  float max_x = stage_width - entity_width;
  if (max_x < 0.0f)
    {
      max_x = 0.0f;
    }
  e->position.x = SafeClampFloat (e->position.x, 0.0f, max_x);
  e->position.y = SafeClampFloat (e->position.y, 0.0f, GROUND_Y);

  float offset_x = (entity_width - PLAYER_WIDTH) / 2.0f;
  float offset_y = (entity_height - PLAYER_HEIGHT) / 2.0f;
  e->hitbox = (Rectangle){ e->position.x + offset_x, e->position.y + offset_y,
                           entity_width, entity_height };
}

/**
 * Applies friction to entity velocity to simulate deceleration
 * @param e Entity to apply friction to
 * @param maxSpeed Maximum speed before friction applies
 */
static void
ApplyFriction (Entity *e, float maxSpeed)
{
  if (!e || !e->grounded)
    {
      return; /* Null pointer safety */
    }

  float clampedLimit = MAX_ENTITY_SPEED;
  if (maxSpeed > 0.0f && maxSpeed < MAX_ENTITY_SPEED)
    {
      clampedLimit = maxSpeed;
    }

  if (fabsf (e->velocity.x) > clampedLimit)
    {
      e->velocity.x
          = SafeClampFloat (e->velocity.x, -clampedLimit, clampedLimit);
    }

  if (fabsf (e->velocity.x) > 0.0f)
    {
      e->velocity.x *= FRICTION;

      if (fabsf (e->velocity.x) < MIN_VELOCITY)
        {
          e->velocity.x = 0.0f;
        }
    }
}

/* ============================================================================
 * ENTITY INITIALIZATION
 * ============================================================================
 */

/**
 * Initializes a player entity with default values
 * @param p Player entity to initialize
 * @param startPos Starting position for the player
 */
void
InitPlayer (Player *p, Vector2 startPos)
{
  if (!p)
    {
      return; /* Null pointer safety */
    }

  /* Clear the entire structure first */
  memset (p, 0, sizeof (Player));

  /* Set initial position and state */
  p->position = startPos;
  p->velocity = (Vector2){ 0.0f, 0.0f };
  p->anim = (Animation){ 0, 0.0f, playerAssets.idle.numFrames };
  p->health = PLAYER_MAX_HEALTH;
  p->maxHealth = PLAYER_MAX_HEALTH;
  p->currentAnimIndex = ANIM_IDLE;
  p->facingRight = true;
  p->hitbox
      = (Rectangle){ startPos.x, startPos.y, PLAYER_WIDTH, PLAYER_HEIGHT };
  p->lastAnimIndex = -1; /* Invalid value to force initial animation setup */
  p->attackDamage = 0;
  p->attackTimer = 0.0f;
  p->attackHasHit = false;
  p->state = ENTITY_STATE_IDLE;
  p->stateTimer = 0.0f;
  p->stunTimer = 0.0f;
  p->hitFrameStart = HIT_FRAMES_START;
  p->hitFrameEnd = HIT_FRAMES_END;
  p->grounded = true;
  p->grabbedEnemyIndex = -1;
}

/**
 * Initializes an enemy entity with default values
 * @param e Enemy entity to initialize
 * @param startPos Starting position for the enemy
 */
void
InitEnemy (Enemy *e, Vector2 startPos)
{
  if (!e)
    {
      return; /* Null pointer safety */
    }

  /* Clear the entire structure first */
  memset (e, 0, sizeof (Enemy));

  /* Set initial position and state */
  e->position = startPos;
  e->velocity = (Vector2){ 0.0f, 0.0f };
  e->anim = (Animation){ 0, 0.0f, enemyAssets.idle.numFrames };
  e->health = ENEMY_MAX_HEALTH;
  e->maxHealth = ENEMY_MAX_HEALTH;
  e->currentAnimIndex = ANIM_IDLE;
  e->facingRight = false; /* Face left by default */
  e->hitbox
      = (Rectangle){ startPos.x, startPos.y, PLAYER_WIDTH, PLAYER_HEIGHT };
  e->lastAnimIndex = -1; /* Invalid value to force initial animation setup */

  /* Initialize AI state */
  e->aiState = AI_STATE_IDLE;
  e->aiTimer = 0.0f;
  e->targetPos = startPos;
  e->wasHurt = false;
  e->attackDamage = 0;
  e->attackTimer = 0.0f;
  e->attackHasHit = false;
  e->state = ENTITY_STATE_IDLE;
  e->stateTimer = 0.0f;
  e->stunTimer = 0.0f;
  e->hitFrameStart = HIT_FRAMES_START;
  e->hitFrameEnd = HIT_FRAMES_END;
  e->grounded = true;
}

/* ============================================================================
 * PLAYER UPDATE SYSTEM
 * ============================================================================
 */

/**
 * Handles player death state logic
 * @param p Player entity
 * @param dt Delta time
 */
static void
UpdatePlayerDeath (Player *p, float dt)
{
  if (p->deathTimer > 0.0f)
    {
      p->deathTimer -= dt;
      p->velocity = (Vector2){ 0.0f, 0.0f };
      p->currentAnimIndex = ANIM_HURT;
      p->state = ENTITY_STATE_DEAD;
      p->stateTimer = 0.0f;
      p->grounded = true;

      if (p->deathTimer <= 0.0f)
        {
          p->health = 0; /* Confirm death */
        }
    }
}

/**
 * Starts a player attack using the provided profile
 * @param p Player entity
 * @param profile Attack profile to apply
 */
static void
StartPlayerAttack (Player *p, const AttackProfile *profile)
{
  if (!p || !profile)
    {
      return;
    }

  p->isAttacking = true;
  p->state = ENTITY_STATE_ATTACK;
  p->stateTimer = 0.0f;
  p->attackTimer = 0.0f;
  p->attackHasHit = false;
  p->attackDamage = profile->damage;
  p->attackCooldown = profile->cooldown;
  p->hitFrameStart = profile->hitFrameStart;
  p->hitFrameEnd = profile->hitFrameEnd;
  p->currentAnimIndex = profile->animIndex;
  p->lastAnimIndex = -1;
  p->idleTimer = 0.0f;
  p->velocity.x = 0.0f;

  // Play attack sound
  if (profile->animIndex == ANIM_KICK)
    {
      PlaySound (gameSounds.kickSound);
    }
  else
    {
      PlaySound (gameSounds.punchSound);
    }
}

static void
StartAirAttack (Player *p, int animIndex, int damage, float cooldown)
{
  if (!p)
    return;
  p->isAttacking = true;
  p->state = ENTITY_STATE_ATTACK;
  p->stateTimer = 0.0f;
  p->attackTimer = 0.0f;
  p->attackHasHit = false;
  p->attackDamage = damage;
  p->attackCooldown = cooldown;
  p->hitFrameStart = 1;
  p->hitFrameEnd = 2;
  p->currentAnimIndex = animIndex;
  p->lastAnimIndex = -1;
  p->idleTimer = 0.0f;
  p->velocity.x *= 0.8f; // slight momentum preservation
}

/**
 * Processes player input and updates movement/attack state
 * @param p Player entity
 * @param dt Delta time
 * @param isPlayer2 Whether this is player 2 (uses different controls)
 */
static void
ProcessPlayerInput (Player *p, float dt, bool isPlayer2)
{
  if (!p)
    {
      return;
    }

  bool canControl = (p->stunTimer <= 0.0f) && (p->state != ENTITY_STATE_ATTACK)
                    && (p->state != ENTITY_STATE_HURT)
                    && (p->state != ENTITY_STATE_DEAD);
  bool moving = false;

  if (canControl)
    {
      float moveDir = 0.0f;

      if (isPlayer2)
        {
          if (IsKeyDown (KEY_LEFT))
            {
              moveDir -= 1.0f;
            }
          if (IsKeyDown (KEY_RIGHT))
            {
              moveDir += 1.0f;
            }

          if (IsKeyPressed (KEY_UP) && p->grounded)
            {
              p->velocity.y = JUMP_VELOCITY;
              p->state = ENTITY_STATE_JUMP;
              p->currentAnimIndex = ANIM_JUMP;
              p->lastAnimIndex = -1;
              p->grounded = false;
            }
        }
      else
        {
          if (IsKeyDown (KEY_A))
            {
              moveDir -= 1.0f;
            }
          if (IsKeyDown (KEY_D))
            {
              moveDir += 1.0f;
            }

          if (IsKeyPressed (KEY_W) && p->grounded)
            {
              p->velocity.y = JUMP_VELOCITY;
              p->state = ENTITY_STATE_JUMP;
              p->currentAnimIndex = ANIM_JUMP;
              p->lastAnimIndex = -1;
              p->grounded = false;
            }
        }

      if (moveDir != 0.0f)
        {
          p->velocity.x = moveDir * PLAYER_SPEED;
          p->facingRight = moveDir > 0.0f;
          moving = true;
          if (p->state != ENTITY_STATE_JUMP)
            {
              p->state = ENTITY_STATE_MOVE;
              p->currentAnimIndex = ANIM_WALK;
            }
        }
      else if (p->state == ENTITY_STATE_MOVE && p->grounded)
        {
          p->velocity.x = 0.0f;
          p->state = ENTITY_STATE_IDLE;
        }

      /* Attack inputs - only if cooldown expired */
      if (p->attackCooldown <= 0.0f)
        {
          bool isAirAttack = !p->grounded;
          int airAnim
              = (p->velocity.y < 0.0f) ? ANIM_JUMP_KICK : ANIM_DIVE_KICK;
          if (isPlayer2)
            {
              if (IsKeyPressed (KEY_Z))
                {
                  if (isAirAttack)
                    {
                      StartAirAttack (p, airAnim, ATTACK_DAMAGE + 5, 0.5f);
                    }
                  else
                    {
                      StartPlayerAttack (p, &PLAYER_ATTACK_JAB_PROFILE);
                    }
                }
              else if (IsKeyPressed (KEY_X))
                {
                  if (isAirAttack)
                    {
                      StartAirAttack (p, airAnim, PUNCH_DAMAGE + 5, 0.5f);
                    }
                  else
                    {
                      StartPlayerAttack (p, &PLAYER_ATTACK_PUNCH_PROFILE);
                    }
                }
              else if (IsKeyPressed (KEY_C))
                {
                  if (isAirAttack)
                    {
                      StartAirAttack (p, airAnim, KICK_DAMAGE + 5, 0.5f);
                    }
                  else
                    {
                      StartPlayerAttack (p, &PLAYER_ATTACK_KICK_PROFILE);
                    }
                }
            }
          else
            {
              if (IsKeyPressed (KEY_J))
                {
                  if (isAirAttack)
                    {
                      StartAirAttack (p, airAnim, ATTACK_DAMAGE + 5, 0.5f);
                    }
                  else
                    {
                      StartPlayerAttack (p, &PLAYER_ATTACK_JAB_PROFILE);
                    }
                }
              else if (IsKeyPressed (KEY_L))
                {
                  if (isAirAttack)
                    {
                      StartAirAttack (p, airAnim, PUNCH_DAMAGE + 5, 0.5f);
                    }
                  else
                    {
                      StartPlayerAttack (p, &PLAYER_ATTACK_PUNCH_PROFILE);
                    }
                }
              else if (IsKeyPressed (KEY_K))
                {
                  if (isAirAttack)
                    {
                      StartAirAttack (p, airAnim, KICK_DAMAGE + 5, 0.5f);
                    }
                  else
                    {
                      StartPlayerAttack (p, &PLAYER_ATTACK_KICK_PROFILE);
                    }
                }
            }
        }

      // Grab mechanics
      if (IsKeyPressed (KEY_G) && p->grabbedEnemyIndex == -1 && canControl
          && p->grounded)
        {
          // Find closest enemy
          int closest = -1;
          float minDist = 60.0f;
          for (int k = 0; k < activeEnemies; k++)
            {
              if (enemies[k].health > 0)
                {
                  float dx = enemies[k].position.x - p->position.x;
                  float dy = fabsf (enemies[k].position.y - p->position.y);
                  float dist = sqrtf (dx * dx + dy * dy);
                  if (dist < minDist)
                    {
                      minDist = dist;
                      closest = k;
                    }
                }
            }
          if (closest != -1)
            {
              p->grabbedEnemyIndex = closest;
              enemies[closest].state = ENTITY_STATE_HURT;
              enemies[closest].velocity = (Vector2){ 0, 0 };
              enemies[closest].stunTimer = 1.0f;
              p->state = ENTITY_STATE_GRAB;
              p->currentAnimIndex = ANIM_PUNCH; // Temporary anim for grab
              p->lastAnimIndex = -1;
            }
        }

      if (IsKeyReleased (KEY_G) && p->grabbedEnemyIndex != -1)
        {
          int idx = p->grabbedEnemyIndex;
          if (idx < activeEnemies && enemies[idx].health > 0)
            {
              float throwVel = p->facingRight ? 400.0f : -400.0f;
              enemies[idx].velocity.x = throwVel;
              enemies[idx].velocity.y = -200.0f;
              enemies[idx].state = ENTITY_STATE_MOVE;
              enemies[idx].stunTimer = 0.5f;
              DamageEntity (&enemies[idx], p, 5, true); // Bonus damage on throw
            }
          p->grabbedEnemyIndex = -1;
          p->state = ENTITY_STATE_IDLE;
        }
    }

  if (!p->isAttacking && p->grounded && !moving && p->stunTimer <= 0.0f
      && p->state != ENTITY_STATE_JUMP && p->state != ENTITY_STATE_ATTACK
      && p->state != ENTITY_STATE_DEAD)
    {
      p->idleTimer += dt;
      if (p->idleTimer > IDLE_DELAY)
        {
          p->currentAnimIndex = ANIM_IDLE;
          p->state = ENTITY_STATE_IDLE;
          p->idleTimer = 0.0f;
          p->lastAnimIndex = -1;
        }
    }
  else if (moving)
    {
      p->idleTimer = 0.0f;
    }

  /* Process attack hit detection */
  if (p->isAttacking && !p->attackHasHit && p->attackDamage > 0
      && p->anim.totalFrames > 0)
    {
      int maxFrame = p->anim.totalFrames - 1;
      int hitStart = SafeClamp (p->hitFrameStart > 0 ? p->hitFrameStart
                                                     : HIT_FRAMES_START,
                                0, maxFrame);
      int hitEnd
          = SafeClamp (p->hitFrameEnd > 0 ? p->hitFrameEnd : HIT_FRAMES_END,
                       hitStart, maxFrame);

      if (p->anim.currentFrame < hitStart || p->anim.currentFrame > hitEnd)
        {
          return;
        }

      Rectangle attackHit = ComputeAttackHitbox (p);

      /* Check collision with all active enemies */
      for (int i = 0; i < activeEnemies; i++)
        {
          if (enemies[i].health > 0
              && CheckCollisionRecs (attackHit, enemies[i].hitbox))
            {
              DamageEntity (&enemies[i], p, p->attackDamage, true);
              p->attackHasHit = true;
              break;
            }
        }
    }
}

/**
 * Updates player animation frame counts based on current animation
 * @param p Player entity
 */
static void
UpdatePlayerAnimationFrames (Player *p)
{
  switch (p->currentAnimIndex)
    {
    case ANIM_IDLE:
      p->anim.totalFrames = playerAssets.idle.numFrames;
      break;
    case ANIM_WALK:
      p->anim.totalFrames = playerAssets.walk.numFrames;
      break;
    case ANIM_JUMP:
      p->anim.totalFrames = playerAssets.jump.numFrames;
      break;
    case ANIM_JAB:
      p->anim.totalFrames = playerAssets.jab.numFrames;
      break;
    case ANIM_PUNCH:
      p->anim.totalFrames = 3; /* Punch has 3 frames */
      break;
    case ANIM_KICK:
      p->anim.totalFrames = playerAssets.kick.numFrames;
      break;
    case ANIM_JUMP_KICK:
      p->anim.totalFrames = playerAssets.jump_kick.numFrames;
      break;
    case ANIM_DIVE_KICK:
      p->anim.totalFrames = playerAssets.dive_kick.numFrames;
      break;
    case ANIM_HURT:
      p->anim.totalFrames = playerAssets.hurt.numFrames;
      break;
    default:
      p->anim.totalFrames = playerAssets.idle.numFrames;
      break;
    }
}

/**
 * Main player update function
 * @param p Player entity to update
 * @param dt Delta time since last update
 * @param isPlayer2 Whether this is player 2 (stub for future co-op)
 */
void
UpdatePlayer (Player *p, float dt, bool isPlayer2)
{
  if (!p)
    {
      return; /* Null pointer safety */
    }

  EntityState previousState = p->state;
  p->stateTimer += dt;

  if (p->stunTimer > 0.0f)
    {
      p->stunTimer -= dt;
      if (p->stunTimer <= 0.0f && p->health > 0
          && p->state == ENTITY_STATE_HURT)
        {
          p->state = p->grounded ? ENTITY_STATE_IDLE : ENTITY_STATE_JUMP;
          p->currentAnimIndex = p->grounded ? ANIM_IDLE : ANIM_JUMP;
          p->lastAnimIndex = -1;
        }
    }

  /* Handle death state - prevents normal gameplay */
  bool isDying = (p->deathTimer > 0.0f);
  if (isDying)
    {
      UpdatePlayerDeath (p, dt);
    }
  else
    {
      /* Normal gameplay updates */
      ProcessPlayerInput (p, dt, isPlayer2);
      ApplyFriction (p, PLAYER_SPEED);
      bool wasGrounded = p->grounded;
      UpdatePhysics (p, dt);
      if (!wasGrounded && p->grounded && p->state != ENTITY_STATE_DEAD
          && !p->isAttacking && p->stunTimer <= 0.0f)
        {
          p->state = ENTITY_STATE_IDLE;
          p->currentAnimIndex = ANIM_IDLE;
          p->lastAnimIndex = -1;
        }
    }

  /* Update cooldowns (always active) */
  if (p->attackCooldown > 0.0f)
    {
      p->attackCooldown -= dt;
      if (p->attackCooldown < 0.0f)
        {
          p->attackCooldown = 0.0f;
        }
    }

  /* Handle animation state changes */
  if (p->currentAnimIndex != p->lastAnimIndex)
    {
      p->lastAnimIndex = p->currentAnimIndex;
      UpdatePlayerAnimationFrames (p);

      /* Reset animation state when switching animations */
      p->anim.currentFrame = 0;
      p->anim.timer = 0.0f;
    }

  /* Update animation playback */
  UpdateAnimation (&p->anim, dt);

  if (p->isAttacking && p->currentAnimIndex >= ANIM_JAB
      && p->currentAnimIndex <= ANIM_DIVE_KICK)
    {
      p->attackTimer += dt;

      bool animationFinished
          = (p->anim.currentFrame >= p->anim.totalFrames - 1);
      bool timeoutExpired = (p->attackTimer >= ATTACK_TIMEOUT);

      if (animationFinished || timeoutExpired)
        {
          p->currentAnimIndex = ANIM_IDLE;
          p->isAttacking = false;
          p->idleTimer = 0.0f; /* Reset idle timer */
          p->attackTimer = 0.0f;
          p->attackDamage = 0;
          p->attackHasHit = false;
          p->state = p->grounded ? ENTITY_STATE_IDLE : ENTITY_STATE_JUMP;
          p->lastAnimIndex = -1;
        }
    }
  else
    {
      p->attackTimer = 0.0f;
      if (!p->isAttacking)
        {
          p->attackDamage = 0;
        }
    }

  /* Trigger death if health depleted */
  if (p->health <= 0 && p->deathTimer == 0.0f)
    {
      p->deathTimer = DEATH_TIME;
    }

  /* Death state physics override */
  if (isDying)
    {
      p->position.y = GROUND_Y - PLAYER_HEIGHT;
      p->velocity = (Vector2){ 0.0f, 0.0f };
      p->hitbox = (Rectangle){ p->position.x, p->position.y, PLAYER_WIDTH,
                               PLAYER_HEIGHT };
    }

  if (p->state != previousState)
    {
      p->stateTimer = 0.0f;
    }

  // Update grabbed enemy position
  if (p->grabbedEnemyIndex != -1)
    {
      int idx = p->grabbedEnemyIndex;
      if (idx >= 0 && idx < activeEnemies && enemies[idx].health > 0)
        {
          float offsetX = p->facingRight ? 40.0f : -40.0f;
          enemies[idx].position.x = p->position.x + offsetX;
          enemies[idx].position.y = p->position.y;
          enemies[idx].velocity = (Vector2){ 0, 0 };
          enemies[idx].grounded = p->grounded;
          enemies[idx].aiState = AI_STATE_IDLE;
          enemies[idx].isAttacking = false;
          p->velocity.x *= 0.5f; // Slow down while grabbing
        }
      else
        {
          p->grabbedEnemyIndex = -1;
          p->state = ENTITY_STATE_IDLE;
        }
    }
}

/* ============================================================================
 * ENEMY AI SYSTEM
 * ============================================================================
 */

/**
 * Updates enemy animation frame counts based on current animation
 * @param e Enemy entity
 */
static void
UpdateEnemyAnimationFrames (Enemy *e)
{
  switch (e->currentAnimIndex)
    {
    case ANIM_IDLE:
      e->anim.totalFrames = enemyAssets.idle.numFrames;
      break;
    case ANIM_WALK:
      e->anim.totalFrames = enemyAssets.walk.numFrames;
      break;
    case ANIM_JUMP:
      /* Reuse walk frames when jumping to avoid blank animations */
      e->anim.totalFrames = enemyAssets.walk.numFrames;
      break;
    case ANIM_PUNCH:
      e->anim.totalFrames = enemyAssets.punch.numFrames;
      break;
    case ANIM_HURT:
      e->anim.totalFrames = enemyAssets.hurt.numFrames;
      break;
    case ANIM_KICK:

      e->anim.totalFrames = enemyAssets.punch.numFrames;

      break;
    default:
      e->anim.totalFrames = enemyAssets.idle.numFrames;
      break;
    }
}

/**
 * Transitions an enemy into the attack state with consistent bookkeeping
 * @param e Enemy entity about to attack
 */
static inline void
StartEnemyAttack (Enemy *e, const AttackProfile *profile)
{
  if (!e || !profile)
    {
      return;
    }

  e->aiState = AI_STATE_ATTACK;
  e->state = ENTITY_STATE_ATTACK;
  e->currentAnimIndex = profile->animIndex;
  e->isAttacking = true;
  e->attackCooldown = profile->cooldown;
  e->attackTimer = 0.0f;
  e->attackHasHit = false;
  e->attackDamage = profile->damage;
  e->hitFrameStart = profile->hitFrameStart;
  e->hitFrameEnd = profile->hitFrameEnd;
  e->lastAnimIndex = -1;

  // Play attack sound
  if (profile->animIndex == ANIM_KICK)
    {
      PlaySound (gameSounds.kickSound);
    }
  else
    {
      PlaySound (gameSounds.punchSound);
    }
}

/**
 * Processes enemy AI decision making with advanced state machine
 * @param e Enemy entity
 * @param player Player entity reference
 * @param dt Delta time for AI timers
 */
static void
ProcessEnemyAI (Enemy *e, Player *players, int numPlayers, float dt)
{
  if (!e || !players || numPlayers <= 0)
    {
      if (e)
        {
          e->aiState = AI_STATE_IDLE;
          e->currentAnimIndex = ANIM_IDLE;
          e->state = ENTITY_STATE_IDLE;
        }
      return;
    }

  // Select closest alive player as target
  Player *targetPlayer = NULL;
  float minDist = ENEMY_SIGHT_DIST + 1.0f;
  for (int j = 0; j < numPlayers; j++)
    {
      if (players[j].health <= 0)
        continue;
      float dist = fabsf (players[j].position.x - e->position.x);
      if (dist < minDist)
        {
          minDist = dist;
          targetPlayer = &players[j];
        }
    }
  if (!targetPlayer)
    {
      // No alive players, idle
      e->aiState = AI_STATE_IDLE;
      e->currentAnimIndex = ANIM_IDLE;
      e->state = ENTITY_STATE_IDLE;
      e->velocity.x = 0.0f;
      return;
    }

  float distToPlayer = minDist;
  float dirToPlayer
      = (targetPlayer->position.x >= e->position.x) ? 1.0f : -1.0f;

  // Boss adjustments
  bool isBoss = (e->health > ENEMY_MAX_HEALTH);
  float sightDist = ENEMY_SIGHT_DIST;
  float attackRange = ENEMY_ATTACK_RANGE;
  float chaseSpeed = ENEMY_CHASE_SPEED;
  float positionSpeed = ENEMY_POSITION_SPEED;
  float retreatSpeed = ENEMY_RETREAT_SPEED;
  if (isBoss)
    {
      sightDist *= 1.2f;
      attackRange *= 1.5f;
      chaseSpeed *= 0.7f;
      positionSpeed *= 0.7f;
      retreatSpeed *= 0.8f;
    }

  e->facingRight = (dirToPlayer > 0.0f);

  if (e->aiTimer > 0.0f)
    {
      e->aiTimer -= dt;
      if (e->aiTimer < 0.0f)
        {
          e->aiTimer = 0.0f;
        }
    }

  /* If we were recently hurt, make sure we honour the retreat */
  if (e->wasHurt && e->aiState != AI_STATE_RETREAT)
    {
      e->aiState = AI_STATE_RETREAT;
      if (e->aiTimer <= 0.0f)
        {
          e->aiTimer = ENEMY_RETREAT_TIME;
        }
      e->currentAnimIndex = ANIM_HURT;
    }

  bool inAttackState = (e->aiState == AI_STATE_ATTACK);
  if (!inAttackState)
    {
      e->isAttacking = false;
    }

  e->velocity.x = 0.0f;

  switch (e->aiState)
    {
    case AI_STATE_IDLE:
      e->currentAnimIndex = ANIM_IDLE;
      e->state = ENTITY_STATE_IDLE;
      if (distToPlayer <= sightDist)
        {
          e->aiState = AI_STATE_CHASE;
          e->currentAnimIndex = ANIM_WALK;
          e->state = ENTITY_STATE_MOVE;
        }
      break;

    case AI_STATE_CHASE:
      e->currentAnimIndex = ANIM_WALK;
      e->state = ENTITY_STATE_MOVE;

      if (distToPlayer > sightDist)
        {
          e->aiState = AI_STATE_IDLE;
          e->currentAnimIndex = ANIM_IDLE;
          e->state = ENTITY_STATE_IDLE;
          break;
        }

      if (distToPlayer <= attackRange)
        {
          if (e->attackCooldown <= 0.0f && !targetPlayer->isAttacking)
            {
              const AttackProfile *chosenProfile
                  = ((float)rand () / RAND_MAX < 0.3f)
                        ? &ENEMY_ATTACK_KICK_PROFILE
                        : &ENEMY_ATTACK_PUNCH_PROFILE;
              StartEnemyAttack (e, chosenProfile);
              return;
            }

          e->aiState = AI_STATE_POSITION;
          e->state = ENTITY_STATE_MOVE;
          break;
        }

      /* Move towards the player */
      float speed = chaseSpeed;
      if (distToPlayer <= attackRange + 50.0f)
        {
          speed = positionSpeed;
        }
      e->velocity.x = dirToPlayer * speed;

      if (targetPlayer->isAttacking && distToPlayer <= attackRange * 1.5f
          && (float)rand () / (float)RAND_MAX < ENEMY_EVADE_CHANCE)
        {
          e->aiState = AI_STATE_EVADE;
          e->aiTimer = 0.5f;
        }
      break;

    case AI_STATE_POSITION:
      e->currentAnimIndex = ANIM_WALK;
      e->state = ENTITY_STATE_MOVE;

      if (distToPlayer < attackRange - 10.0f)
        {
          e->velocity.x = -dirToPlayer * positionSpeed;
        }
      else if (distToPlayer > attackRange + 10.0f)
        {
          e->velocity.x = dirToPlayer * positionSpeed;
        }
      else if (e->attackCooldown <= 0.0f && !targetPlayer->isAttacking)
        {
          const AttackProfile *chosenProfile
              = ((float)rand () / RAND_MAX < 0.3f)
                    ? &ENEMY_ATTACK_KICK_PROFILE
                    : &ENEMY_ATTACK_PUNCH_PROFILE;
          StartEnemyAttack (e, chosenProfile);
          return;
        }

      if (targetPlayer->isAttacking && distToPlayer <= attackRange * 1.2f
          && e->aiTimer <= 0.0f)
        {
          e->aiState = AI_STATE_EVADE;
          e->aiTimer = 0.4f;
          e->state = ENTITY_STATE_MOVE;
        }
      break;

    case AI_STATE_ATTACK:
      e->isAttacking = true;
      e->velocity.x = 0.0f;
      break;

    case AI_STATE_RETREAT:
      e->currentAnimIndex = ANIM_HURT;
      e->state = ENTITY_STATE_HURT;
      e->velocity.x = -dirToPlayer * retreatSpeed;
      e->facingRight = (e->velocity.x > 0.0f);
      if (e->aiTimer <= 0.0f)
        {
          e->wasHurt = false;
          e->aiState = AI_STATE_CHASE;
          e->currentAnimIndex = ANIM_WALK;
          e->state = ENTITY_STATE_MOVE;
        }
      break;

    case AI_STATE_EVADE:
      e->currentAnimIndex = ANIM_WALK;
      e->state = ENTITY_STATE_MOVE;
      e->velocity.x = -dirToPlayer * retreatSpeed;
      e->facingRight = (e->velocity.x > 0.0f);
      if (e->aiTimer <= 0.0f)
        {
          e->aiState = AI_STATE_CHASE;
          e->currentAnimIndex = ANIM_WALK;
          e->state = ENTITY_STATE_MOVE;
        }
      break;
    }
}

/**
 * Processes enemy attack hit detection
 * @param e Enemy entity
 * @param player Player entity reference
 */
static void
ProcessEnemyAttack (Enemy *e, Player *players, int numPlayers)
{
  if (!e || !players || numPlayers <= 0)
    {
      return;
    }

  if (!e->isAttacking || e->attackHasHit || e->attackDamage <= 0)
    {
      return;
    }

  if (e->anim.totalFrames <= 0)
    {
      e->anim.totalFrames
          = enemyAssets.punch.numFrames > 0 ? enemyAssets.punch.numFrames : 1;
    }

  int maxFrame = e->anim.totalFrames - 1;
  int hitStart = SafeClamp (
      e->hitFrameStart > 0 ? e->hitFrameStart : HIT_FRAMES_START, 0, maxFrame);
  int hitEnd = SafeClamp (e->hitFrameEnd > 0 ? e->hitFrameEnd : HIT_FRAMES_END,
                          hitStart, maxFrame);

  if (e->anim.currentFrame < hitStart || e->anim.currentFrame > hitEnd)
    {
      return;
    }

  Rectangle attackHit = ComputeAttackHitbox (e);

  for (int j = 0; j < numPlayers; j++)
    {
      if (players[j].health <= 0)
        continue;
      if (CheckCollisionRecs (attackHit, players[j].hitbox))
        {
          DamageEntity (&players[j], e, e->attackDamage, false);
          e->attackHasHit = true;
          break;
        }
    }
}

/**
 * Removes dead enemies from the active array
 * @param index Index of enemy to remove
 */
static void
RemoveDeadEnemy (int index)
{
  if (index < 0 || index >= activeEnemies)
    {
      return; /* Bounds check */
    }

  bool removedBoss = enemies[index].maxHealth > ENEMY_MAX_HEALTH;

  /* Swap with last active enemy and decrement count */
  if (index < activeEnemies - 1)
    {
      enemies[index] = enemies[activeEnemies - 1];
    }
  activeEnemies--;

  if (removedBoss)
    {
      bossSpawned = false;
      bossDefeated = true;
    }
}

/**
 * Main enemy update function
 * @param player Player entity reference for AI targeting
 * @param dt Delta time since last update
 */
void
UpdateEnemies (Player *players, int numPlayers, float dt)
{
  if (!players)
    {
      return; /* Null pointer safety */
    }

  /* Process each active enemy */
  for (int i = 0; i < activeEnemies; i++)
    {
      Enemy *e = &enemies[i];

      /* Handle death state */
      if (e->deathTimer > 0.0f)
        {
          e->deathTimer -= dt;
          if (e->deathTimer <= 0.0f)
            {
              /* Remove dead enemy */
              RemoveDeadEnemy (i);
              i--; /* Re-check current index after removal */
              continue;
            }
          else
            {
              /* Still dying - show hurt animation */
              e->currentAnimIndex = ANIM_HURT;
              e->velocity = (Vector2){ 0.0f, 0.0f };
              continue;
            }
        }

      /* Skip dead enemies */
      if (e->health <= 0)
        {
          continue;
        }

      /* Update cooldowns */
      if (e->attackCooldown > 0.0f)
        {
          e->attackCooldown -= dt;
          if (e->attackCooldown < 0.0f)
            {
              e->attackCooldown = 0.0f;
            }
        }

      e->stateTimer += dt;

      bool skipAI = false;
      if (e->stunTimer > 0.0f)
        {
          e->stunTimer -= dt;
          if (e->stunTimer <= 0.0f)
            {
              e->stunTimer = 0.0f;
            }
          else
            {
              skipAI = true;
              e->state = ENTITY_STATE_HURT;
              e->currentAnimIndex = ANIM_HURT;
            }
        }

      // Check if grabbed by any player
      bool isGrabbed = false;
      for (int j = 0; j < numPlayers; j++)
        {
          if (players[j].grabbedEnemyIndex == i)
            {
              isGrabbed = true;
              break;
            }
        }
      if (isGrabbed)
        {
          e->currentAnimIndex = ANIM_HURT;
          e->state = ENTITY_STATE_HURT;
          e->velocity = (Vector2){ 0, 0 };
          continue;
        }

      EntityState previousState = e->state;

      if (!skipAI)
        {
          ProcessEnemyAI (e, players, numPlayers, dt);
        }

      if (e->state != previousState)
        {
          e->stateTimer = 0.0f;
        }

      /* Apply friction to movement based on AI state */
      if (e->currentAnimIndex == ANIM_WALK)
        {
          /* Use appropriate speed for friction calculation based on AI state */
          float frictionSpeed = ENEMY_SPEED;
          switch (e->aiState)
            {
            case AI_STATE_CHASE:
              frictionSpeed = ENEMY_CHASE_SPEED;
              break;
            case AI_STATE_RETREAT:
            case AI_STATE_EVADE:
              frictionSpeed = ENEMY_RETREAT_SPEED;
              break;
            case AI_STATE_POSITION:
              frictionSpeed = ENEMY_POSITION_SPEED;
              break;
            }
          ApplyFriction (e, frictionSpeed);
        }
      else if (e->grounded && fabsf (e->velocity.x) > 0.0f)
        {
          ApplyFriction (e, ENEMY_RETREAT_SPEED);
        }

      /* Handle animation state changes */
      if (e->currentAnimIndex != e->lastAnimIndex)
        {
          e->anim.currentFrame = 0;
          e->anim.timer = 0.0f;
          e->lastAnimIndex = e->currentAnimIndex;
          UpdateEnemyAnimationFrames (e);
        }

      /* Physics and movement */
      UpdatePhysics (e, dt);

      /* Animation update */
      UpdateAnimation (&e->anim, dt);

      /* Attack hit detection */
      ProcessEnemyAttack (e, players, numPlayers);

      if (e->isAttacking)
        {

          // Slow down enemy attack animation for visibility (effective ~6 FPS)

          e->anim.timer *= 0.5f; // Halve the timer progress to stretch playback

          // Re-clamp frame to prevent over-advancement

          e->anim.currentFrame
              = SafeClamp (e->anim.currentFrame, 0, e->anim.totalFrames - 1);
        }

      if (e->isAttacking)
        {
          e->attackTimer += dt;
          bool minDurationPassed = (e->attackTimer >= 0.5f);
          bool timeoutExpired = (e->attackTimer >= ATTACK_TIMEOUT);

          if (minDurationPassed || timeoutExpired)
            {
              e->isAttacking = false;
              e->attackTimer = 0.0f;
              e->attackDamage = 0;
              e->attackHasHit = false;

              if (e->aiState == AI_STATE_ATTACK)
                {
                  e->aiState = AI_STATE_CHASE;
                  e->currentAnimIndex = ANIM_WALK;
                  e->state = ENTITY_STATE_MOVE;
                  e->stateTimer = 0.0f;
                }
            }
        }
      else if (e->attackTimer > 0.0f)
        {
          e->attackTimer = 0.0f;
        }
    }
}

int
GetAliveEnemiesCount (void)
{
  int alive = 0;
  for (int i = 0; i < activeEnemies; ++i)
    {
      if (enemies[i].health > 0 && enemies[i].deathTimer <= 0.0f)
        {
          alive++;
        }
    }
  return alive;
}

/* ============================================================================
 * COLLISION AND COMBAT SYSTEM
 * ============================================================================
 */

/**
 * Checks collision between two entities
 * @param a First entity
 * @param b Second entity
 * @return true if entities are colliding, false otherwise
 */
bool
CheckCollision (Entity *a, Entity *b)
{
  if (!a || !b)
    {
      return false; /* Null pointer safety */
    }

  return CheckCollisionRecs (a->hitbox, b->hitbox);
}

/**
 * Applies damage to an entity and handles knockback effects
 * @param target Entity to damage
 * @param attacker Entity doing the damage
 * @param dmg Amount of damage to apply
 * @param isPlayerAttacker Whether the attacker is a player (affects hurt
 * animation)
 */
void
DamageEntity (Entity *target, Entity *attacker, int dmg, bool isPlayerAttacker)
{
  if (!target || !attacker || dmg <= 0)
    {
      return; /* Input validation */
    }

  /* Apply damage with bounds checking */
  target->health
      = SafeClamp (target->health - dmg, 0,
                   target->maxHealth > 0 ? target->maxHealth : INT_MAX);

  /* Reset attack state so entities can't trade hits while staggered */
  target->isAttacking = false;
  target->attackDamage = 0;
  target->attackTimer = 0.0f;
  target->attackHasHit = false;
  float recoveryCooldown = ATTACK_COOLDOWN;
  if (isPlayerAttacker)
    {
      recoveryCooldown
          = fmaxf (ENEMY_ATTACK_PUNCH_PROFILE.cooldown, ATTACK_COOLDOWN);
    }
  target->attackCooldown = fmaxf (target->attackCooldown, recoveryCooldown);
  target->idleTimer = 0.0f;
  target->hitFrameStart = HIT_FRAMES_START;
  target->hitFrameEnd = HIT_FRAMES_END;

  /* Force animation system to pick up the hurt animation immediately */
  target->currentAnimIndex = ANIM_HURT;
  target->lastAnimIndex = -1;
  target->state = ENTITY_STATE_HURT;
  target->stateTimer = 0.0f;

  /* Apply knockback in direction away from attacker */
  float direction = (target->position.x > attacker->position.x ? 1.0f : -1.0f);
  ApplyKnockback (target, direction);

  if (isPlayerAttacker)
    {
      /* Enemy was hurt - force retreat behaviour */
      target->aiState = AI_STATE_RETREAT;
      target->aiTimer = ENEMY_RETREAT_TIME;
      target->wasHurt = true;
      target->stunTimer = ENEMY_STUN_TIME;
    }
  else
    {
      target->stunTimer = PLAYER_STUN_TIME;
    }

  /* Trigger death state if health depleted */
  if (target->health <= 0 && target->deathTimer == 0.0f)
    {
      target->deathTimer = DEATH_TIME;
      target->state = ENTITY_STATE_DEAD;
      target->stunTimer = 0.0f;
      target->velocity.y = 0.0f;
      target->grounded = true;
    }

  if (target->health <= 0 && target->deathTimer == 0.0f)
    {
      PlaySound (gameSounds.deathSound);
    }
}

/* ============================================================================
 * RENDERING SYSTEM
 * ============================================================================
 */

/**
 * Gets the appropriate animation for a player entity
 * @param p Player entity
 * @return Pointer to the current animation asset
 */
static SpriteAnim *
GetPlayerAnimation (const Player *p)
{
  if (!p)
    {
      return &playerAssets.idle; /* Safe fallback */
    }

  switch (p->currentAnimIndex)
    {
    case ANIM_IDLE:
      return &playerAssets.idle;
    case ANIM_WALK:
      return &playerAssets.walk;
    case ANIM_JUMP:
      return &playerAssets.jump;
    case ANIM_JAB:
      return &playerAssets.jab;
    case ANIM_PUNCH:
      return &playerAssets.punch;
    case ANIM_KICK:
      return &playerAssets.kick;
    case ANIM_JUMP_KICK:
      return &playerAssets.jump_kick;
    case ANIM_DIVE_KICK:
      return &playerAssets.dive_kick;
    case ANIM_HURT:
      return &playerAssets.hurt;
    default:
      return &playerAssets.idle;
    }
}

/**
 * Gets the appropriate animation for an enemy entity
 * @param e Enemy entity
 * @return Pointer to the current animation asset
 */
static SpriteAnim *
GetEnemyAnimation (const Enemy *e)
{
  if (!e)
    {
      return &enemyAssets.idle; /* Safe fallback */
    }

  switch (e->currentAnimIndex)
    {
    case ANIM_IDLE:
      return &enemyAssets.idle;
    case ANIM_WALK:
      return &enemyAssets.walk;
    case ANIM_PUNCH:
      return &enemyAssets.punch;
    case ANIM_HURT:
      return &enemyAssets.hurt;
    case ANIM_KICK:
      return &enemyAssets.punch; // Temporary reuse until kick assets added
    default:
      return &enemyAssets.idle;
    }
}

/**
 * Draws a player entity with shadow and death effects
 * @param p Player entity to draw
 */
void
DrawPlayer (const Player *p)
{
  if (!p)
    {
      return; /* Null pointer safety */
    }

  /* Calculate shadow position and tint */
  Color shadowTint = Fade (BLACK, 0.5f);
  float shadowY
      = SafeClampFloat (p->position.y + PLAYER_HEIGHT, 0.0f, GROUND_Y);
  DrawShadow (p->position, shadowY, shadowTint);

  /* Handle death fade effect */
  if (p->deathTimer > 0.0f)
    {
      Color fadeTint = Fade (WHITE, p->deathTimer / DEATH_TIME);
      DrawShadow (p->position, shadowY, fadeTint);
    }

  /* Get current animation and draw */
  SpriteAnim *animTex = GetPlayerAnimation (p);

  if (animTex && animTex->numFrames > 0)
    {
      int frameIdx
          = SafeClamp (p->anim.currentFrame, 0, animTex->numFrames - 1);
      Texture2D frame = animTex->frames[frameIdx];
      DrawSprite (frame, p->position, p->facingRight);
    }
  else
    {
      /* Fallback: draw red rectangle for missing animation */
      DrawRectangle ((int)p->position.x, (int)p->position.y, (int)PLAYER_WIDTH,
                     (int)PLAYER_HEIGHT, RED);
    }
}

/**
 * Draws an enemy entity with shadow and death effects
 * @param e Enemy entity to draw
 */
void
DrawEnemy (const Enemy *e)
{
  if (!e)
    {
      return; /* Null pointer safety */
    }

  // Calculate scale for boss
  float scale = 1.0f;
  if (e->maxHealth > ENEMY_MAX_HEALTH)
    {
      scale = 1.5f;
    }

  /* Calculate shadow position and tint with scale */
  Color shadowTint = Fade (BLACK, 0.5f);
  float shadowY
      = SafeClampFloat (e->position.y + PLAYER_HEIGHT * scale, 0.0f, GROUND_Y);
  float shadow_offset = (PLAYER_WIDTH * scale - SHADOW_WIDTH) / 2.0f;
  DrawTexture (shadowTex, (int)(e->position.x + shadow_offset), (int)shadowY,
               shadowTint);

  /* Handle death fade effect */
  if (e->deathTimer > 0.0f)
    {
      Color fadeTint = Fade (WHITE, e->deathTimer / DEATH_TIME);
      DrawTexture (shadowTex, (int)(e->position.x + shadow_offset),
                   (int)shadowY, fadeTint);
    }

  /* Get current animation and draw with scale and flip */
  SpriteAnim *animTex = GetEnemyAnimation (e);

  if (animTex && animTex->numFrames > 0)
    {
      int frameIdx
          = SafeClamp (e->anim.currentFrame, 0, animTex->numFrames - 1);
      Texture2D frame = animTex->frames[frameIdx];

      if (frame.id > 0)
        {
          Rectangle source
              = { 0.0f, 0.0f, (float)frame.width, (float)frame.height };
          Rectangle dest
              = { e->position.x, e->position.y, (float)frame.width * scale,
                  (float)frame.height * scale };
          Vector2 origin = { 0.0f, 0.0f };
          if (e->facingRight)
            {
              source.width = -source.width; // Flip horizontally
            }
          DrawTexturePro (frame, source, dest, origin, 0.0f, WHITE);
        }
    }
  else
    {
      /* Fallback: draw red rectangle for missing animation */
      DrawRectangle ((int)e->position.x, (int)e->position.y,
                     (int)(PLAYER_WIDTH * scale), (int)(PLAYER_HEIGHT * scale),
                     RED);
    }
}

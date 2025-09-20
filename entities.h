#ifndef ENTITIES_H
#define ENTITIES_H

#include "assets.h"
#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * GAME CONSTANTS - All magic numbers replaced with descriptive names
 * ============================================================================
 */

/* Display and world dimensions */
#define SCREEN_WIDTH 800.0f
#define SCREEN_HEIGHT 600.0f
#define GROUND_Y 448.0f

/* Entity dimensions */
#define PLAYER_WIDTH 47.0f
#define PLAYER_HEIGHT 47.0f
#define SHADOW_WIDTH 20.0f

/* Movement and physics constants */
#define PLAYER_SPEED 200.0f
#define ENEMY_SPEED 100.0f
#define JUMP_VELOCITY -400.0f
#define GRAVITY 980.0f
#define FRICTION 0.9f
#define MIN_VELOCITY 5.0f
#define KNOCKBACK_FORCE 200.0f
#define MAX_ENTITY_SPEED 280.0f
#define MAX_KNOCKBACK_SPEED 220.0f

/* Combat system constants */
#define ATTACK_COOLDOWN 0.5f
#define ATTACK_DAMAGE 15 /* Base/jab damage */
#define PUNCH_DAMAGE 18
#define KICK_DAMAGE 20
#define ENEMY_DAMAGE 10 /* Enemy punch damage */
#define ATTACK_EXTEND 20.0f
#define HIT_FRAMES_START 1
#define HIT_FRAMES_END                                                         \
  2 /* Default damage frames when specific windows aren't set */

/* Timing constants */
#define IDLE_DELAY 0.2f
#define DEATH_TIME 2.0f
#define ATTACK_TIMEOUT 1.5f
#define PLAYER_STUN_TIME 0.35f
#define ENEMY_STUN_TIME 0.45f

/* AI constants */
#define ENEMY_SIGHT_DIST 200.0f    /* How far enemy can see player */
#define ENEMY_ATTACK_RANGE 60.0f   /* Optimal attack distance */
#define ENEMY_RETREAT_DIST 40.0f   /* Distance to retreat when hurt */
#define ENEMY_CHASE_SPEED 120.0f   /* Speed when chasing */
#define ENEMY_RETREAT_SPEED 140.0f /* Speed when retreating */
#define ENEMY_POSITION_SPEED 80.0f /* Speed when positioning for attack */
#define ENEMY_EVADE_CHANCE 0.3f    /* Chance to dodge player attacks */
#define ENEMY_JUMP_CHANCE 0.2f     /* Chance to jump when chasing */
#define ENEMY_RETREAT_TIME 1.5f    /* How long to retreat when hurt */

/* AI state constants */
#define AI_STATE_IDLE 0
#define AI_STATE_CHASE 1
#define AI_STATE_ATTACK 2
#define AI_STATE_RETREAT 3
#define AI_STATE_EVADE 4
#define AI_STATE_POSITION 5

/* Animation indices - centralized for maintainability */
#define ANIM_IDLE 0
#define ANIM_WALK 1
#define ANIM_JUMP 2
#define ANIM_JAB 3
#define ANIM_PUNCH 4
#define ANIM_KICK 5
#define ANIM_JUMP_KICK 6
#define ANIM_DIVE_KICK 7
#define ANIM_HURT 8

/* Health values */
#define PLAYER_MAX_HEALTH 100
#define ENEMY_MAX_HEALTH 50

/* Entity management */
#define MAX_ENEMIES 10

typedef enum
{
  ENTITY_STATE_IDLE = 0,
  ENTITY_STATE_MOVE,
  ENTITY_STATE_JUMP,
  ENTITY_STATE_ATTACK,
  ENTITY_STATE_HURT,
  ENTITY_STATE_DEAD,
  ENTITY_STATE_GRAB // New state for grabbing enemies
} EntityState;

/* ============================================================================
 * TYPE DEFINITIONS
 * ============================================================================
 */

/**
 * Base entity structure shared by all game entities
 * Contains all state information needed for rendering, physics, and AI
 */
typedef struct
{
  Vector2 position;     /* Current world position */
  Vector2 velocity;     /* Movement velocity vector */
  Animation anim;       /* Animation state and timing */
  int health;           /* Current health (0 = dead) */
  int maxHealth;        /* Maximum health for clamps */
  int currentAnimIndex; /* Current animation being played */
  bool facingRight;     /* Facing direction for sprite flipping */
  Rectangle hitbox;     /* Collision detection rectangle */
  bool isAttacking;     /* Whether entity is in attack state */
  float attackCooldown; /* Time until next attack is allowed */
  int attackDamage;     /* Damage amount of the current attack */
  float attackTimer;    /* Time spent in the current attack animation */
  bool attackHasHit;    /* Prevents multiple hits from a single attack */
  int lastAnimIndex;    /* Previous animation index for change detection */
  float deathTimer;     /* Time remaining in death/hurt state */
  float idleTimer;      /* Accumulator for idle animation transition */
  EntityState state;    /* Current high-level entity state */
  float stateTimer;     /* Timer tracking duration in current state */
  float stunTimer;      /* Time remaining before controls/AI resume */
  int hitFrameStart;    /* Start frame for the current attack hit window */
  int hitFrameEnd;      /* End frame for the current attack hit window */
  bool grounded;        /* Whether the entity is standing on the ground */

  /* AI-specific fields */
  int aiState;           /* Current AI state (for enemies) */
  float aiTimer;         /* Timer for AI state transitions */
  Vector2 targetPos;     /* Target position for AI movement */
  bool wasHurt;          /* Whether enemy was recently hurt */
  int grabbedEnemyIndex; /* -1 if not grabbing, index in enemies array for
                            player grab */
} Entity;

/* Type aliases for clarity */
typedef Entity Player;
typedef Entity Enemy;

/* ============================================================================
 * GLOBAL STATE
 * ============================================================================
 */

extern Enemy enemies[MAX_ENEMIES];
extern int activeEnemies;

/* ============================================================================
 * FUNCTION PROTOTYPES
 * ============================================================================
 */

/* Entity initialization */
void InitPlayer (Player *p, Vector2 startPos);
void InitEnemy (Enemy *e, Vector2 startPos);
void ClearEnemies (void);

/* Entity updates */
void UpdatePlayer (Player *p, float dt, bool isPlayer2);
void UpdateEnemies (Player *players, int numPlayers, float dt);
int GetAliveEnemiesCount (void);

/* Physics and collision */
bool CheckCollision (Entity *a, Entity *b);
void DamageEntity (Entity *target, Entity *attacker, int dmg,
                   bool isPlayerAttacker);

/* Rendering */
void DrawPlayer (const Player *p);
void DrawEnemy (const Enemy *e);

/* Internal helper functions (not part of public API) */
void UpdateAnimation (Animation *anim, float dt);

#endif /* ENTITIES_H */

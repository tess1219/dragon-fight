#ifndef ASSETS_H
#define ASSETS_H

#include <raylib.h>
#include <stdbool.h>

#define MAX_FRAMES 10  /* Maximum frames per animation */
#define ANIM_FPS 10.0f /* Animation frames per second */
#define ATTACK_ANIM_FPS                                                        \
  6.0f /* Slower animation speed for attack animations                         \
        */

typedef struct
{
  Texture2D frames[MAX_FRAMES];
  int numFrames;
} SpriteAnim;

typedef struct
{
  SpriteAnim idle, walk, jump, jab, punch, kick, jump_kick, dive_kick, hurt;
} PlayerAssets;

typedef struct
{
  SpriteAnim idle, walk, punch, hurt;
} EnemyAssets;

typedef struct
{
  Sound punchSound;
  Sound kickSound;
  Sound deathSound;
} GameSounds;

extern PlayerAssets playerAssets;
extern EnemyAssets enemyAssets;
extern Texture2D bgFar, bgNear, tileset, shadowTex;
extern Texture2D propBush, propCar;
extern GameSounds gameSounds;

bool LoadAssets (void);   /* Initialize all assets */
void UnloadAssets (void); /* Cleanup all assets */

/* Animation state and update (declared here, implemented in entities.c) */
typedef struct
{
  int currentFrame;
  float timer;
  int totalFrames; /* Set based on current animation */
} Animation;

void UpdateAnimation (Animation *anim, float dt);

#endif /* ASSETS_H */

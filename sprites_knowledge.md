# Sprites Knowledge Base for Dragon Fight Assets

This document provides a structured, LLM-parseable overview of the assets in `assets/Sprites/` (adapted from Streets of Fight files for Dragon Fight). Format: Markdown with YAML-style sections for easy extraction (e.g., by AI for code gen). All under CC0—free for mod/commercial use. Dimensions approx (scale to 47px height for consistency). Animations: Individual PNG frames in subdirectories; frame_count from actual files.

## Players
### Brawler Girl (Female Protagonist)
- **name**: Brawler Girl
- **file_path**: Sprites/Brawler-Girl/ (subdirs: Dive_kick/, Hurt/, Idle/, Jab/, Jump/, Jump_kick/, Kick/, Punch/, Walk/)
- **description**: Red-haired punk female fighter in jacket/skirt. Core player character for beat'em up actions. 9 animations total.
- **dimensions**: 47px height, ~32-47px width per frame (pixel art, 16-color palette).
- **animations**:
  - {name: Idle, frame_count: 4, estimated_fps: 10, purpose: Standing still (loop), notes: Subtle breathing/idle sway for resting state. Files: Sprites/Brawler-Girl/Idle/idle1.png to idle4.png}
  - {name: Walk, frame_count: 10, estimated_fps: 10, purpose: Ground movement left/right (mirror for direction), notes: Use for horizontal traversal in levels. Files: Sprites/Brawler-Girl/Walk/walk1.png to walk10.png}
  - {name: Jump, frame_count: 4, estimated_fps: 15, purpose: Vertical leap (one-shot or loop mid-air), notes: Trigger on space/up; combine with kick for aerial attack. Files: Sprites/Brawler-Girl/Jump/jump1.png to jump4.png}
  - {name: Jab, frame_count: 3, estimated_fps: 12, purpose: Quick basic melee (close-range jab), notes: Arm extension; hitbox on forward frames. Files: Sprites/Brawler-Girl/Jab/jab1.png to jab3.png}
  - {name: Punch, frame_count: 3, estimated_fps: 12, purpose: Stronger melee punch (close-range, 1-2 hits), notes: Longer than jab; combo potential. Files: Sprites/Brawler-Girl/Punch/punch1.png to punch3.png}
  - {name: Kick, frame_count: 5, estimated_fps: 12, purpose: Leg sweep/high kick, notes: Longer range than punches; ground attack. Files: Sprites/Brawler-Girl/Kick/kick1.png to kick5.png}
  - {name: Jump Kick, frame_count: 3, estimated_fps: 15, purpose: Mid-air kick attack, notes: Aerial damage in arc. Files: Sprites/Brawler-Girl/Jump_kick/jump_kick1.png to jump_kick3.png}
  - {name: Dive Kick, frame_count: 5, estimated_fps: 15, purpose: Diving kick (flying attack), notes: Double Dragon-style; from Dive_kick/ dir. Files: Sprites/Brawler-Girl/Dive_kick/dive_kick1.png to dive_kick5.png}
  - {name: Hurt, frame_count: 2, estimated_fps: 10, purpose: Damage reaction (flinch back), notes: Play on hit, reduce health. Files: Sprites/Brawler-Girl/Hurt/hurt1.png to hurt2.png}
- **usage_in_game**: Assign to Player struct (e.g., currentAnimIndex 0=Idle, 1=Walk, 2=Jump, 3=Jab, 4=Punch, 5=Kick, 6=Jump Kick, 7=Dive Kick, 8=Hurt). Draw with shadow under feet. Integrate attacks with collision detection (damage 10-20 on key frames).

## Enemies
### Punk Enemy (Red Mohawk Thug)
- **name**: Punk Enemy
- **file_path**: Sprites/Enemy-Punk/ (subdirs: Hurt/, Idle/, Punch/, Walk/)
- **description**: Basic foe with red mohawk, jacket. 4 animations; spawn in waves for beat'em up hordes.
- **dimensions**: ~40px height, 32px width (similar scale to player).
- **animations**:
  - {name: Idle, frame_count: 4, estimated_fps: 8, purpose: Standing patrol (loop), notes: When far from player; subtle sway. Files: Sprites/Enemy-Punk/Idle/idle1.png to idle4.png}
  - {name: Walk, frame_count: 4, estimated_fps: 10, purpose: Approach player (left/right), notes: AI trigger if dist >100px; mirror for direction. Files: Sprites/Enemy-Punk/Walk/walk1.png to walk4.png}
  - {name: Punch, frame_count: 3, estimated_fps: 12, purpose: Attack on contact, notes: Damage player on overlap (10 HP); cooldown 1s. Files: Sprites/Enemy-Punk/Punch/punch1.png to punch3.png}
  - {name: Hurt, frame_count: 4, estimated_fps: 10, purpose: Flinch on damage or death collapse, notes: If health=0, play full anim then remove. Files: Sprites/Enemy-Punk/Hurt/hurt1.png to hurt4.png}
- **usage_in_game**: Array of Enemies (up to 10). Spawn at screen edge. AI: Idle/Walk if far, Punch if close. Boss: Scale up, more health.

## Effects & UI
### Shadow Overlay
- **name**: Shadow Overlay
- **file_path**: Sprites/shadow.png
- **description**: Simple drop shadow for grounding entities.
- **dimensions**: ~47x20px (elliptical blob).
- **animations**: None (static).
  - {name: Shadow, frame_count: 1, estimated_fps: N/A, purpose: Depth under feet, notes: Semi-transparent; draw at entity position before sprite.}
- **usage_in_game**: Draw under player/enemies for retro effect. UI elements like health bars separate.

## Parsing Notes for LLM
- Extract sections as YAML for code generation.
- Usage: Map animation names to indices (e.g., player.currentAnimIndex = 3 for Jab).
- Mods: Assets are PNG frames; source .ase files may be elsewhere.
- Total: All PNGs in Sprites/ covered: Brawler-Girl (39 frames), Enemy-Punk (15 frames), shadow.png (1).
- Sources: Direct from folder structure and file enumeration.

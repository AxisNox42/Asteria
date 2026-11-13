#pragma once
// Which active skill fired a shot (Bullet::skillTag). ActiveSkills.cpp uses it
// for the follow-up on a hit; EntityDraw.cpp draws each skill's shot its own way.
enum Tag { TAG_NONE, TAG_ARROW, TAG_KNIFE, TAG_FIREBALL, TAG_SHARD, TAG_SNIPE,
           TAG_TURRET, TAG_STAR, TAG_STAR_CHILD };

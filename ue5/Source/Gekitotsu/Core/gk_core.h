// GEKITOTSU combat core. Engine-independent, deterministic, integer-only, fixed 60 Hz.
// The whole match lives in one plain State struct, so rollback is a memcpy and two machines that feed the same
// inputs get bit-identical results. The renderer only reads State and the per-step event list.
#pragma once
#include <cstdint>

namespace gk {

constexpr int F = 100;                 // sub-units per centimetre
constexpr int STAGE = 750 * F;         // half the stage width
constexpr int START_X = 200 * F;
constexpr int MAX_SEP = 860 * F;         // the two fighters are never farther apart than this
constexpr int MAX_HP = 1000, BAR = 1000, MAX_METER = 3 * BAR;
constexpr int ROUND_TIME = 99 * 60, INTRO = 90, KO_TIME = 200;

// Input bits for one player for one frame. Directions are absolute (left/right on screen).
enum : uint16_t { UP = 1, DOWN = 2, LEFT = 4, RIGHT = 8, BL = 16, BM = 32, BH = 64, BS = 128, BP = 256 };
// Two-button commands: L+M throw, M+H homing (Slipstream), H+S super.

enum St : uint8_t { IDLE, PREJUMP, AIR, LAND, DASH, BACKDASH, ATK, HIT, LAUNCH, DOWN_, RISE_, BLOCK, PARRY, PARRIED, GRAB, HELD, TECH, HELPLESS, KO_, WIN_ };

enum Mv : uint8_t {
  M_NONE, S5L, S5M, S5H, S2L, S2M, S2H, JL, JM, JH, J2H,
  FIRE, SURGE, RISE, VANISH, WHIRL, DIVE, HOME, THROW, SUPER1, SUPER2, SUPER3, M_COUNT };

enum : uint16_t { LOW = 1, OVERHEAD = 2, LAUNCHER = 4, SPIKE = 8, WALLB = 16, AIRMOVE = 32, ANTIAIR = 64, SPECIAL = 128, SUPERMV = 256, PROJ = 512, CROUCHING = 1024, NOHIT = 2048 };
enum : uint8_t { C_CHAIN = 1, C_SPECIAL = 2, C_SUPER = 4, C_JUMP = 8, C_HOME = 16 };

struct Box { int16_t x, y, w, h; };    // centimetres, relative to the feet, drawn for a fighter facing right

struct Move {
  const char* name;
  uint8_t su, ac, rc;                  // startup, active, recovery frames. First active frame is su + 1.
  Box hb; int16_t dmg; uint8_t hstun, bstun; int16_t kb;
  int16_t lvx, lvy;                    // launch velocity given to the victim (sub-units per frame)
  uint16_t flags; uint8_t cancel, level;
  int16_t mvx; uint8_t mv0, mv1;       // the attacker's own forward drive between two move frames
  int16_t risevx, risevy; uint8_t risef; // leaves the ground on this move frame
  uint8_t hits, gap;                   // multi-hit: re-arms every gap frames
  uint8_t inv0, inv1;                  // invulnerable between these move frames
  uint8_t landrec;
};
const Move& move(uint8_t id);

struct Player {
  int32_t x, y, vx, vy;
  int16_t hp, meter, sf, mf, stun;
  int8_t face, jd;
  uint8_t st, mv, hitstop, hitId, connected, crouch;
  uint8_t airJumps, homeAir, combo, comboHome, gbUsed, wbUsed, wbArmed, spiked, inv, rounds;
  uint8_t buf[5];                      // frames since L, M, H, S, P were pressed (255 = not buffered)
  uint16_t prev;
  uint8_t super, techWin, pad[2];
};
struct Shot { int32_t x, y, vx; int16_t life; uint8_t on, owner, lvl, pad; };

enum Ev : uint8_t { E_HIT = 1, E_BLOCK, E_PARRY, E_JUMP, E_DJUMP, E_LAND, E_DASH, E_HOME, E_HOMEHIT, E_WALLB, E_GROUNDB, E_SUPER, E_KO, E_ROUND, E_FIGHT, E_TECH, E_THROW, E_THROWTECH, E_VANISH, E_FIRE, E_DOWN, E_WHIFF };
struct Event { uint8_t type, who, a, b; int32_t x, y; };

enum Phase : uint8_t { PH_INTRO, PH_FIGHT, PH_KO, PH_OVER };
struct State {
  uint32_t frame; int16_t time, pt; uint8_t phase, round, freeze, freezeWho; int8_t winner; uint8_t nev, pad[2];
  Player p[2]; Shot shot[2];
  Event ev[16];                        // what happened this step, for sound, effects and camera. Not part of the hash.
};

void init(State& s);
void step(State& s, uint16_t in0, uint16_t in1);
uint64_t hash(const State& s);
Box hurtbox(const Player& p);          // absolute, centimetres: x is the left edge
bool hitbox(const Player& p, Box& out);
const char* stateName(uint8_t st);

}  // namespace gk

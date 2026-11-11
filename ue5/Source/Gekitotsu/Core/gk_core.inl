#include "gk_core.h"
#include <cstring>

namespace gk {

// --------------------------------------------------------------------------------------------- move data
#define N3 C_CHAIN | C_SPECIAL | C_SUPER
#define AIRN C_CHAIN | C_SPECIAL | C_JUMP | C_HOME | C_SUPER
static const Move MOVES[M_COUNT] = {
  //  name              su ac rc  hitbox              dmg hs  bs  kb    lvx   lvy    flags                                  cancel  lvl mvx  m0 m1  rvx  rvy  rf  hits gap inv0 inv1 landrec
  { "",                 0, 0, 0, { 0, 0, 0, 0 },       0,  0,  0,  0,    0,    0,    0,                                     0,      0,  0,   0, 0,  0,   0,   0,  1, 0, 0, 0, 0 },
  { "Flick",            4, 3, 8, { 20, 110, 75, 40 },  30, 13, 10, 500,  0,    0,    0,                                     N3,     1,  0,   0, 0,  0,   0,   0,  1, 0, 0, 0, 0 },
  { "Stumbling Palm",   7, 3, 14, { 20, 90, 100, 50 }, 60, 17, 13, 650,  0,    0,    0,                                     N3,     2,  300, 2, 6,  0,   0,   0,  1, 0, 0, 0, 0 },
  { "Jug Swing",        11, 4, 20, { 20, 80, 120, 70 }, 90, 21, 16, 800, 0,    0,    0,                                     N3,     3,  400, 3, 9,  0,   0,   0,  1, 0, 0, 0, 0 },
  { "Toe Poke",         5, 2, 9, { 20, 0, 80, 40 },    25, 12, 9,  450,  0,    0,    LOW | CROUCHING,                       N3,     1,  0,   0, 0,  0,   0,   0,  1, 0, 0, 0, 0 },
  { "Slide",            7, 3, 15, { 20, 0, 115, 45 },  55, 16, 12, 600,  0,    0,    LOW | CROUCHING,                       N3,     2,  350, 2, 7,  0,   0,   0,  1, 0, 0, 0, 0 },
  { "Last Call",        9, 5, 24, { 10, 60, 95, 170 }, 80, 0,  16, 0,    120,  2100, LAUNCHER | ANTIAIR,                    C_JUMP | C_SPECIAL | C_SUPER | C_HOME, 4, 700, 2, 8, 0, 0, 0, 1, 0, 0, 0, 0 },
  { "Air Flick",        5, 6, 6, { 10, 60, 75, 60 },   30, 14, 10, 400,  0,    0,    AIRMOVE | OVERHEAD,                    AIRN,   1,  0,   0, 0,  0,   0,   0,  1, 0, 0, 0, 4 },
  { "Air Kick",         7, 5, 9, { 10, 40, 100, 70 },  55, 16, 12, 500,  0,    0,    AIRMOVE | OVERHEAD,                    AIRN,   2,  0,   0, 0,  0,   0,   0,  1, 0, 0, 0, 4 },
  { "Air Hook",         9, 5, 12, { 0, 20, 110, 100 }, 80, 18, 14, 600,  220,  1000, AIRMOVE | OVERHEAD,                    AIRN,   3,  0,   0, 0,  0,   0,   0,  1, 0, 0, 0, 5 },
  { "Falling Heel",     11, 5, 14, { -10, -30, 100, 100 }, 90, 20, 14, 600, 150, -2600, AIRMOVE | OVERHEAD | SPIKE,        C_SPECIAL | C_HOME | C_SUPER, 4, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 6 },
  { "Corpse Candle",    14, 1, 31, { 0, 0, 0, 0 },     60, 18, 14, 500,  0,    0,    SPECIAL | PROJ | NOHIT,                C_SUPER, 5, 0,   0, 0,  0,   0,   0,  1, 0, 0, 0, 0 },
  { "Black Current",    14, 4, 22, { 20, 60, 120, 80 }, 100, 0, 18, 900, 1500, 900,  SPECIAL | WALLB | LAUNCHER,            C_SUPER | C_HOME, 5, 1100, 5, 14, 0, 0, 0, 1, 0, 0, 0, 0 },
  { "Spindrift",        5, 10, 14, { 0, 60, 90, 160 }, 110, 0, 18, 0,    200,  1900, SPECIAL | LAUNCHER,                    C_SUPER | C_HOME, 5, 0, 0, 0, 250, 1700, 5, 1, 0, 1, 9, 22 },
  { "Ghost Step",       10, 1, 16, { 0, 0, 0, 0 },     0,  0,  0,  0,    0,    0,    SPECIAL | NOHIT,                       0,      5,  0,   0, 0,  0,   0,   0,  1, 0, 6, 20, 8 },
  { "Waterspout",       8, 18, 10, { -60, 30, 120, 90 }, 35, 16, 10, 300, 250, 700,  SPECIAL | AIRMOVE | OVERHEAD,          C_SUPER | C_HOME, 5, 500, 8, 26, 0, 0, 0, 3, 6, 0, 0, 12 },
  { "Falling Star",     9, 30, 6, { 0, -20, 80, 90 },  90, 20, 12, 500,  300,  -2400, SPECIAL | AIRMOVE | OVERHEAD | SPIKE, C_HOME | C_SUPER, 5, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 16 },
  { "Slipstream",       12, 45, 0, { -10, 20, 90, 120 }, 50, 14, 14, 300, 250, 1900, LAUNCHER | AIRMOVE,                    0,      6,  0,   0, 0,  0,   0,   0,  1, 0, 0, 0, 24 },
  { "Throw",            5, 2, 24, { 0, 0, 95, 150 },   120, 0, 0,  0,    900,  1300, NOHIT,                                 0,      6,  0,   0, 0,  0,   0,   0,  1, 0, 0, 0, 0 },
  { "Moonwake",         6, 6, 40, { 0, 0, 260, 180 },  220, 0, 20, 900,  1600, 1500, SUPERMV | LAUNCHER | WALLB,            0,      7,  2200, 2, 10, 0, 0, 0, 1, 0, 1, 12, 0 },
  { "Storm Surge",      6, 6, 40, { 0, 0, 260, 180 },  330, 0, 20, 900,  1700, 1600, SUPERMV | LAUNCHER | WALLB,            0,      7,  2200, 2, 10, 0, 0, 0, 1, 0, 1, 12, 0 },
  { "King Tide",        6, 6, 40, { 0, 0, 260, 180 },  450, 0, 20, 900,  1800, 1700, SUPERMV | LAUNCHER | WALLB,            0,      7,  2200, 2, 10, 0, 0, 0, 1, 0, 1, 12, 0 },
};
const Move& move(uint8_t id) { return MOVES[id < M_COUNT ? id : 0]; }

static const char* SN[] = { "idle", "prejump", "air", "land", "dash", "backdash", "attack", "hit", "launch", "down", "rise", "block", "parry", "parried", "grab", "held", "tech", "helpless", "ko", "win" };
const char* stateName(uint8_t st) { return st < 20 ? SN[st] : "?"; }

// --------------------------------------------------------------------------------------------- tuning
constexpr int WALK_F = 420, WALK_B = 320, JUMP_VY = 1900, JUMP_VX = 520, DJUMP_VY = 1600, GRAV = 90, FASTFALL = 70;
constexpr int DASH_V = 1000, DASH_T = 14, BDASH_V = 850, BDASH_T = 20, PREJ = 4, HOME_V = 2600;
constexpr int PUSH_W = 28, HURT_W = 30, DOWN_T = 28, RISE_T = 18, PARRY_ACT = 8, PARRY_TOT = 30, BUF = 6;

static inline int sg(int v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }
static inline int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
static inline int absi(int v) { return v < 0 ? -v : v; }
static void ev(State& s, uint8_t t, uint8_t who, uint8_t a, uint8_t b, int x, int y) { if (s.nev < 16) s.ev[s.nev++] = Event{ t, who, a, b, x, y }; }
static inline bool airborne(const Player& p) { return p.y > 0; }
static inline bool buffered(const Player& p, int i, int win = BUF) { return p.buf[i] <= win; }

static void endCombo(Player& v) { v.combo = v.comboHome = v.gbUsed = v.wbUsed = v.wbArmed = v.spiked = 0; }
static void toIdle(Player& p) { p.st = airborne(p) ? AIR : IDLE; p.mv = M_NONE; p.sf = 0; p.mf = 0; p.stun = 0; if (!airborne(p)) { p.vx = 0; p.vy = 0; p.y = 0; } }
static void startMove(Player& p, uint8_t id) {
  const Move& m = move(id); p.st = ATK; p.mv = id; p.mf = 0; p.sf = 0; p.hitId = 255; p.connected = 0; p.crouch = (m.flags & CROUCHING) ? 1 : 0;
  if (!airborne(p)) p.vx = 0;
}
static void land(State& s, Player& p, int i, int rec) { p.y = 0; p.vy = 0; p.vx = 0; p.st = LAND; p.sf = 0; p.stun = rec; p.mv = M_NONE; p.airJumps = 1; p.homeAir = 0; ev(s, E_LAND, i, 0, 0, p.x, 0); }

Box hurtbox(const Player& p) {
  int x = p.x / F, y = p.y / F;
  if (p.st == DOWN_ || p.st == RISE_ || p.st == KO_ || p.st == TECH || p.st == HELD || p.st == WIN_) return Box{ 0, 0, 0, 0 };
  if (p.st == LAUNCH) return Box{ (int16_t)(x - 45), (int16_t)(y + 10), 90, 110 };
  if (airborne(p)) return Box{ (int16_t)(x - HURT_W), (int16_t)(y + 15), 60, 150 };
  return Box{ (int16_t)(x - HURT_W), (int16_t)y, 60, (int16_t)(p.crouch ? 110 : 175) };
}
bool hitbox(const Player& p, Box& o) {
  if (p.st != ATK) return false;
  const Move& m = move(p.mv);
  if ((m.flags & NOHIT) || p.mf <= m.su || p.mf > m.su + m.ac) return false;
  int x = p.x / F, y = p.y / F;
  o = Box{ (int16_t)(p.face > 0 ? x + m.hb.x : x - m.hb.x - m.hb.w), (int16_t)(y + m.hb.y), m.hb.w, m.hb.h };
  return true;
}
static bool overlap(const Box& a, const Box& b) { return a.w > 0 && b.w > 0 && a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h; }

// --------------------------------------------------------------------------------------------- setup
static void resetRound(State& s) {
  for (int i = 0; i < 2; i++) {
    Player& p = s.p[i]; int16_t meter = p.meter; uint8_t rounds = p.rounds;
    std::memset(&p, 0, sizeof(Player));
    p.x = i ? START_X : -START_X; p.face = i ? -1 : 1; p.hp = MAX_HP; p.meter = meter; p.rounds = rounds; p.airJumps = 1; p.hitId = 255;
    for (int k = 0; k < 5; k++) p.buf[k] = 255;
    p.techWin = 255; p.pad[0] = p.pad[1] = 255;       // pad[0], pad[1]: frames since forward / back was tapped
  }
  std::memset(s.shot, 0, sizeof(s.shot)); s.time = ROUND_TIME; s.phase = PH_INTRO; s.pt = 0; s.freeze = 0; s.winner = -1;
}
void init(State& s) { std::memset(&s, 0, sizeof(State)); s.round = 1; resetRound(s); }

uint64_t hash(const State& s) {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](const void* d, size_t n) { const uint8_t* b = (const uint8_t*)d; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; } };
  mix(&s.frame, sizeof s.frame); mix(&s.time, sizeof s.time); mix(&s.pt, sizeof s.pt); mix(&s.phase, 1); mix(&s.round, 1); mix(&s.freeze, 1); mix(&s.winner, 1);
  mix(s.p, sizeof s.p); mix(s.shot, sizeof s.shot); return h;
}

// --------------------------------------------------------------------------------------------- acting
static bool startAttack(State& s, int i, uint16_t in, bool kara) {
  Player& p = s.p[i]; Player& o = s.p[1 - i]; bool air = airborne(p);
  uint16_t fwd = p.face > 0 ? RIGHT : LEFT, back = p.face > 0 ? LEFT : RIGHT;
  auto both = [&](int a, int b) { return p.buf[a] <= 4 && p.buf[b] <= 4; };
  auto eat = [&](int a, int b = -1) { p.buf[a] = 255; if (b >= 0) p.buf[b] = 255; };
  // H + S: the super. It spends the whole gauge; more bars, bigger super.
  if (both(2, 3) && p.meter >= BAR && !air) {
    int lv = p.meter / BAR; eat(2, 3); startMove(p, SUPER1 + lv - 1); p.super = (uint8_t)lv; p.meter = 0; s.freeze = 40; s.freezeWho = i;
    ev(s, E_SUPER, i, lv, (lv == 3 && p.hp * 4 <= MAX_HP) ? 1 : 0, p.x, p.y); return true;
  }
  // M + H: Slipstream, the homing rush, on the ground or in the air. One per trip into the air, two pops per combo.
  if (both(1, 2) && !p.homeAir && o.comboHome < 2 && o.st != DOWN_ && o.st != RISE_ && o.st != KO_) {
    eat(1, 2); startMove(p, HOME); p.homeAir = 1; if (!air) { p.y = 1; p.vy = 500; } else { p.vx = 0; p.vy = 0; }
    ev(s, E_HOME, i, 0, 0, p.x, p.y); return true;
  }
  if (both(0, 1) && !air) { eat(0, 1); startMove(p, THROW); return true; }
  if (kara) return false;
  if (buffered(p, 3)) {
    uint8_t id = air ? ((in & DOWN) ? DIVE : WHIRL) : (in & DOWN) ? RISE : (in & fwd) ? SURGE : (in & back) ? VANISH : FIRE;
    if (id == FIRE && s.shot[i].on) return false;
    eat(3); startMove(p, id);
    if (id == DIVE) { p.vx = 0; p.vy = 0; }
    return true;
  }
  if (buffered(p, 2)) { eat(2); startMove(p, air ? ((in & DOWN) ? J2H : JH) : (in & DOWN) ? S2H : S5H); return true; }
  if (buffered(p, 1)) { eat(1); startMove(p, air ? JM : (in & DOWN) ? S2M : S5M); return true; }
  if (buffered(p, 0)) { eat(0); startMove(p, air ? JL : (in & DOWN) ? S2L : S5L); return true; }
  return false;
}

// What an attack that has connected may be cancelled into.
static bool tryCancel(State& s, int i, uint16_t in) {
  Player& p = s.p[i]; Player& o = s.p[1 - i]; const Move& m = move(p.mv); bool air = airborne(p), hit = p.connected == 1;
  auto both = [&](int a, int b) { return p.buf[a] <= 4 && p.buf[b] <= 4; };
  if ((m.cancel & C_SUPER) && hit && both(2, 3) && p.meter >= BAR && !air) return startAttack(s, i, in, true);
  if ((m.cancel & C_HOME) && hit && both(1, 2) && o.comboHome < 2) { p.homeAir = 0; return startAttack(s, i, in, true); }
  if ((m.cancel & C_JUMP) && hit && (in & UP) && (!air || p.airJumps > 0)) {
    uint16_t fwd = p.face > 0 ? RIGHT : LEFT, back = p.face > 0 ? LEFT : RIGHT;
    if (air) { p.airJumps--; ev(s, E_DJUMP, i, 0, 0, p.x, p.y); } else { p.airJumps = 1; ev(s, E_JUMP, i, 0, 0, p.x, 0); }
    p.st = AIR; p.mv = M_NONE; p.sf = 0; p.y = p.y > 0 ? p.y : 1; p.vy = air ? DJUMP_VY : JUMP_VY; p.vx = (in & fwd) ? p.face * JUMP_VX : (in & back) ? -p.face * JUMP_VX : p.face * 180; p.crouch = 0; return true;
  }
  if ((m.cancel & C_SPECIAL) && buffered(p, 3)) {
    uint16_t fwd = p.face > 0 ? RIGHT : LEFT, back = p.face > 0 ? LEFT : RIGHT;
    uint8_t id = air ? ((in & DOWN) ? DIVE : WHIRL) : (in & DOWN) ? RISE : (in & fwd) ? SURGE : (in & back) ? VANISH : FIRE;
    if (id == FIRE && s.shot[i].on) return false;
    p.buf[3] = 255; startMove(p, id); if (id == DIVE) { p.vx = 0; p.vy = 0; } return true;
  }
  if (m.cancel & C_CHAIN) {
    for (int b = 2; b >= 0; b--) if (buffered(p, b)) {
      uint8_t id = air ? (b == 2 ? ((in & DOWN) ? J2H : JH) : b == 1 ? JM : JL) : (in & DOWN) ? (b == 2 ? S2H : b == 1 ? S2M : S2L) : (b == 2 ? S5H : b == 1 ? S5M : S5L);
      if (move(id).level > m.level) { p.buf[b] = 255; startMove(p, id); return true; }
    }
  }
  return false;
}

static void control(State& s, int i, uint16_t in) {
  Player& p = s.p[i]; Player& o = s.p[1 - i];
  uint16_t fwd = p.face > 0 ? RIGHT : LEFT, back = p.face > 0 ? LEFT : RIGHT;
  bool upNew = (in & UP) && !(p.prev & UP);
  p.sf++;
  switch (p.st) {
    case IDLE: {
      p.crouch = (in & DOWN) ? 1 : 0;
      if (startAttack(s, i, in, false)) break;
      if (buffered(p, 4)) { p.buf[4] = 255; p.st = PARRY; p.sf = 0; p.vx = 0; break; }
      if ((in & fwd) && !(p.prev & fwd) && p.pad[0] <= 10 && p.pad[0] > 0) { p.st = DASH; p.sf = 0; p.vx = p.face * DASH_V; p.crouch = 0; p.pad[0] = 255; ev(s, E_DASH, i, 0, 0, p.x, 0); break; }
      if ((in & back) && !(p.prev & back) && p.pad[1] <= 10 && p.pad[1] > 0) { p.st = BACKDASH; p.sf = 0; p.vx = -p.face * BDASH_V; p.inv = 7; p.crouch = 0; p.pad[1] = 255; ev(s, E_DASH, i, 1, 0, p.x, 0); break; }
      if (in & UP) { p.st = PREJUMP; p.sf = 0; p.jd = (in & fwd) ? 1 : (in & back) ? -1 : 0; p.vx = 0; p.crouch = 0; break; }
      p.vx = p.crouch ? 0 : (in & fwd) ? p.face * WALK_F : (in & back) ? -p.face * WALK_B : 0;
      break; }
    case PREJUMP:
      if (p.sf >= PREJ) { p.st = AIR; p.sf = 0; p.y = 1; p.vy = JUMP_VY; p.vx = p.jd * p.face * JUMP_VX; p.airJumps = 1; ev(s, E_JUMP, i, 0, 0, p.x, 0); }
      break;
    case AIR: {
      if (startAttack(s, i, in, false)) break;
      if (upNew && p.airJumps > 0) { p.airJumps--; p.vy = DJUMP_VY; p.vx = (in & RIGHT) ? JUMP_VX : (in & LEFT) ? -JUMP_VX : 0; ev(s, E_DJUMP, i, 0, 0, p.x, p.y); break; }
      int d = (in & RIGHT) ? 1 : (in & LEFT) ? -1 : 0; p.vx = clampi(p.vx + d * 24, -JUMP_VX, JUMP_VX);
      if ((in & DOWN) && p.vy < 0) p.vy -= FASTFALL;
      break; }
    case LAND: if (p.sf >= p.stun) toIdle(p); break;
    case DASH:
      if (p.sf > 4 && startAttack(s, i, in, false)) break;
      if (p.sf >= DASH_T - 4) p.vx = p.vx * 6 / 10;
      if (p.sf >= DASH_T) toIdle(p);
      break;
    case BACKDASH:
      if (p.sf >= BDASH_T - 8) p.vx = p.vx * 7 / 10;
      if (p.sf >= BDASH_T) toIdle(p);
      break;
    case PARRY: if (p.sf >= PARRY_TOT) toIdle(p); break;
    case BLOCK: case HIT:
      p.vx = p.vx * 86 / 100;
      if (--p.stun <= 0) { endCombo(p); toIdle(p); }
      break;
    case LAUNCH: {
      if (p.stun > 0) p.stun--;
      else if (p.y > 30 * F && (in & (BL | BM | BH | BS)) && !(p.prev & (BL | BM | BH | BS))) {
        p.st = TECH; p.sf = 0; p.inv = 14; p.vy = 900; p.vx = (in & RIGHT) ? 400 : (in & LEFT) ? -400 : 0; endCombo(p); p.airJumps = 1; p.homeAir = 0; ev(s, E_TECH, i, 0, 0, p.x, p.y);
      }
      break; }
    case TECH: if (p.sf >= 14) { p.st = AIR; p.sf = 0; } break;
    case DOWN_: if (p.sf >= DOWN_T) { p.st = RISE_; p.sf = 0; } break;
    case RISE_: if (p.sf >= RISE_T) { endCombo(p); toIdle(p); } break;
    case GRAB: {
      if (p.sf == 20) {
        const Move& m = move(THROW); bool bk = (in & back) != 0; int dir = bk ? -p.face : p.face;
        o.x = clampi(p.x + dir * 60 * F, -STAGE + PUSH_W * F, STAGE - PUSH_W * F); o.y = 60 * F; o.st = LAUNCH; o.sf = 0; o.stun = 30; o.combo = 1; o.comboHome = 0;
        o.vx = dir * (bk ? 1400 : m.lvx); o.vy = bk ? 1100 : m.lvy; o.wbArmed = bk ? 1 : 0; o.hp = (int16_t)(o.hp - m.dmg < 0 ? 0 : o.hp - m.dmg);
        p.meter = (int16_t)clampi(p.meter + 150, 0, MAX_METER); if (bk) p.face = (int8_t)-p.face;
        ev(s, E_THROW, i, bk ? 1 : 0, 0, o.x, o.y);
      }
      if (p.sf >= 34) toIdle(p);
      break; }
    case HELD: {
      // the one being held can break it with L + M in the first few frames
      if (o.st != GRAB) { toIdle(p); break; }
      if (p.techWin > 0 && p.techWin != 255) { p.techWin--; if (p.buf[0] <= 8 && p.buf[1] <= 8) { p.buf[0] = p.buf[1] = 255; toIdle(p); toIdle(o); p.vx = -p.face * 700; o.vx = -o.face * 700; p.st = HIT; p.stun = 12; o.st = HIT; o.stun = 12; p.techWin = 255; ev(s, E_THROWTECH, i, 0, 0, (p.x + o.x) / 2, 100 * F); } }
      break; }
    case HELPLESS: break;
    case ATK: {
      const Move& m = move(p.mv); p.mf++;
      bool air = airborne(p);
      if (p.mf <= 2 && p.mv >= S5L && p.mv <= J2H && startAttack(s, i, in, true)) break;      // two buttons pressed a frame apart
      if (p.connected && p.mf > m.su && tryCancel(s, i, in)) break;
      if (p.mv == HOME) {
        if (p.mf <= m.su) { if (p.vy > 0 && p.y > 40 * F) p.vy = 0; }
        else {
          // aims a little above the feet, so a rush along the ground never touches down; the length is a cheap integer estimate
          int dx = o.x - p.x, dy = (o.y + 40 * F) - p.y, ax = absi(dx), ay = absi(dy), len = ax > ay ? ax + ay / 2 : ay + ax / 2;
          if (len < 1) len = 1;
          p.vx = dx * HOME_V / len; p.vy = dy * HOME_V / len; if (dx) p.face = (int8_t)sg(dx);
          if (p.mf > m.su + m.ac) { p.st = HELPLESS; p.sf = 0; p.vx = p.vx / 3; p.vy = 0; ev(s, E_WHIFF, i, 0, 0, p.x, p.y); }
        }
        break;
      }
      if (p.mv == THROW) {
        if (p.mf > m.su && p.mf <= m.su + m.ac && !p.connected) {
          bool ok = !airborne(o) && o.inv == 0 && absi(o.x - p.x) <= m.hb.w * F && (o.st == IDLE || o.st == DASH || o.st == LAND || o.st == PARRY || o.st == PREJUMP || (o.st == ATK && !(move(o.mv).flags & SUPERMV)));
          if (ok) {
            if (o.st == ATK && o.mv == THROW) { toIdle(p); toIdle(o); p.st = HIT; o.st = HIT; p.stun = o.stun = 12; p.vx = -p.face * 700; o.vx = -o.face * 700; ev(s, E_THROWTECH, i, 0, 0, (p.x + o.x) / 2, 100 * F); break; }
            p.st = GRAB; p.sf = 0; p.vx = 0; p.connected = 1; o.st = HELD; o.sf = 0; o.vx = o.vy = 0; o.y = 0; o.mv = M_NONE; o.techWin = 8; o.crouch = 0; o.x = clampi(p.x + p.face * 55 * F, -STAGE + PUSH_W * F, STAGE - PUSH_W * F); break;
          }
        }
      }
      if (m.mv1 && p.mf >= m.mv0 && p.mf <= m.mv1) p.vx = p.face * m.mvx; else if (!air && !(m.flags & AIRMOVE)) p.vx = p.vx * 7 / 10;
      if (m.risef && p.mf == m.risef) { p.y = 1; p.vx = p.face * m.risevx; p.vy = m.risevy; }
      if (p.mv == DIVE && p.mf == m.su) { p.vx = p.face * 900; p.vy = -1500; }
      if (p.mv == VANISH && p.mf == m.su + 1) {
        int side = o.x >= p.x ? 1 : -1; p.x = clampi(o.x + side * 110 * F, -STAGE + PUSH_W * F, STAGE - PUSH_W * F);
        if (in & UP) { p.y = o.y + 230 * F; p.vy = 0; } p.face = (int8_t)(o.x >= p.x ? 1 : -1); ev(s, E_VANISH, i, 0, 0, p.x, p.y);
      }
      if (p.mv == FIRE && p.mf == m.su + 1 && !s.shot[i].on) { Shot& sh = s.shot[i]; sh.x = p.x + p.face * 80 * F; sh.y = p.y + 110 * F; sh.vx = p.face * 900; sh.life = 110; sh.on = 1; sh.owner = (uint8_t)i; ev(s, E_FIRE, i, 0, 0, s.shot[i].x, s.shot[i].y); }
      if (m.inv1 && p.mf >= m.inv0 && p.mf <= m.inv1) p.inv = p.inv > 1 ? p.inv : 1;
      if (p.mf >= m.su + m.ac + m.rc && p.mv != DIVE) {
        if (!p.connected && p.mf == m.su + m.ac + m.rc && (m.flags & SPECIAL)) p.meter = (int16_t)clampi(p.meter + 20, 0, MAX_METER);
        if (airborne(p) && (m.flags & SPECIAL) && !(m.flags & AIRMOVE) && p.mv != VANISH) { p.st = HELPLESS; p.sf = 0; }
        else toIdle(p);
      }
      break; }
    default: break;
  }
}

// --------------------------------------------------------------------------------------------- physics
static void physics(State& s, int i) {
  Player& p = s.p[i];
  if (p.st == HELD || p.st == GRAB) return;
  bool air = airborne(p);
  if (air) {
    bool hover = p.st == ATK && ((p.mv == HOME) || (p.mv == DIVE && p.mf < move(DIVE).su) || (p.mv == VANISH));
    if (!hover) p.vy -= p.st == LAUNCH ? GRAV + 6 * (p.combo > 10 ? 10 : p.combo) : GRAV;
    if (p.vy < -3200) p.vy = -3200;
  }
  p.x += p.vx; p.y += p.vy;
  int lim = STAGE - PUSH_W * F;
  if (p.x > lim || p.x < -lim) {
    p.x = clampi(p.x, -lim, lim);
    if (p.st == LAUNCH && p.wbArmed && !p.wbUsed) { p.vx = -p.vx * 6 / 10; if (p.vy < 1200) p.vy = 1200; p.wbUsed = 1; p.wbArmed = 0; p.stun = 36; ev(s, E_WALLB, i, 0, 0, p.x, p.y); }
    else if (p.st == LAUNCH || p.st == HELPLESS) p.vx = 0;
  }
  if (air && p.y <= 0) {
    p.y = 0;
    switch (p.st) {
      case LAUNCH:
        if (p.spiked && !p.gbUsed && p.hp > 0) { p.y = 1; p.vy = 1500; p.vx = p.vx / 2; p.gbUsed = 1; p.spiked = 0; p.stun = 30; ev(s, E_GROUNDB, i, 0, 0, p.x, 0); }
        else { p.st = DOWN_; p.sf = 0; p.vx = p.vy = 0; p.stun = 0; ev(s, E_DOWN, i, 0, 0, p.x, 0); }
        break;
      case KO_: p.vx = p.vy = 0; break;
      case ATK: { const Move& m = move(p.mv); int rec = m.landrec ? m.landrec : 4; if (p.mv == HOME) rec = 24; land(s, p, i, rec); break; }
      case HELPLESS: land(s, p, i, 24); break;
      default: land(s, p, i, 3); break;
    }
  }
}

static void pushApart(State& s) {
  Player& a = s.p[0]; Player& b = s.p[1];
  auto solid = [](const Player& p) { return p.st != DOWN_ && p.st != RISE_ && p.st != HELD && p.st != GRAB && p.st != KO_ && !(p.st == ATK && (p.mv == HOME || p.mv == VANISH)) && p.st != TECH; };
  if (!solid(a) || !solid(b)) return;
  int dy = absi(a.y - b.y); if (dy > 140 * F) return;
  int dx = b.x - a.x, need = 2 * PUSH_W * F - absi(dx); if (need <= 0) return;
  int dir = dx > 0 ? 1 : dx < 0 ? -1 : (a.face > 0 ? 1 : -1), lim = STAGE - PUSH_W * F;
  int ha = need / 2, hb2 = need - ha; a.x -= dir * ha; b.x += dir * hb2;
  if (a.x < -lim) { b.x += -lim - a.x; a.x = -lim; } if (a.x > lim) { b.x -= a.x - lim; a.x = lim; }
  if (b.x < -lim) { a.x += -lim - b.x; b.x = -lim; } if (b.x > lim) { a.x -= b.x - lim; b.x = lim; }
  a.x = clampi(a.x, -lim, lim); b.x = clampi(b.x, -lim, lim);
}

// --------------------------------------------------------------------------------------------- hits
struct Pending { bool on; uint8_t mv, id; bool proj; };

static void applyHit(State& s, int ai, uint16_t vin, const Pending& h) {
  Player& a = s.p[ai]; Player& v = s.p[1 - ai]; const Move& m = move(h.mv);
  int ax = h.proj ? s.shot[ai].x : a.x, dir = v.x > ax ? 1 : v.x < ax ? -1 : a.face;
  uint16_t away = dir > 0 ? RIGHT : LEFT; bool vair = airborne(v) || v.st == LAUNCH;
  int hx = (ax + v.x) / 2, hy = v.y + 100 * F;
  if (!h.proj) { a.hitId = h.id; }
  // parry: the attacker hangs in the air a beat, the defender is free at once
  if (v.st == PARRY && v.sf <= PARRY_ACT && !vair) {
    v.st = IDLE; v.sf = 0; v.inv = 5; v.meter = (int16_t)clampi(v.meter + 200, 0, MAX_METER); v.hitstop = 4;
    if (!h.proj) { a.hitstop = 22; a.connected = 2; if (a.mv == HOME) { a.st = HELPLESS; a.sf = 0; a.vx = -dir * 300; a.vy = 1400; a.hitstop = 14; } }
    ev(s, E_PARRY, 1 - ai, 0, 0, hx, hy); return;
  }
  bool canBlock = !vair && (v.st == IDLE || v.st == BLOCK) && (vin & away);
  bool low = (vin & DOWN) != 0;
  if (canBlock && !((m.flags & LOW) && !low) && !((m.flags & OVERHEAD) && low)) {
    v.st = BLOCK; v.sf = 0; v.stun = m.bstun; v.vx = dir * (m.kb ? m.kb : 500); v.crouch = low ? 1 : 0; v.hitstop = 6;
    if (m.flags & (SPECIAL | SUPERMV)) { int chip = m.dmg / 10; v.hp = (int16_t)(v.hp - chip < 1 ? 1 : v.hp - chip); }
    v.meter = (int16_t)clampi(v.meter + m.dmg / 4, 0, MAX_METER);
    if (!h.proj) { a.connected = 2; a.hitstop = 6; if (a.mv == HOME) { a.st = HELPLESS; a.sf = 0; a.vx = -dir * 500; a.vy = 1300; } if (a.mv == DIVE) { a.st = AIR; a.vx = -dir * 300; a.vy = 1000; a.mv = M_NONE; } }
    ev(s, E_BLOCK, 1 - ai, m.level, 0, hx, hy); return;
  }
  // it lands
  int scale = 100 - 9 * v.combo; int floor_ = (m.flags & SUPERMV) ? 50 : 25; if (scale < floor_) scale = floor_;
  int dmg = m.dmg * scale / 100; if ((m.flags & SUPERMV) && a.super == 3 && a.hp * 4 <= MAX_HP) dmg = dmg * 12 / 10;
  if (dmg < 1) dmg = 1;
  v.hp = (int16_t)(v.hp - dmg < 0 ? 0 : v.hp - dmg);
  a.meter = (int16_t)clampi(a.meter + ((m.flags & SUPERMV) ? 0 : m.dmg * 12 / 10), 0, MAX_METER); v.meter = (int16_t)clampi(v.meter + m.dmg * 6 / 10, 0, MAX_METER);
  int stop = (m.flags & SUPERMV) ? 26 : m.level >= 5 ? 12 : 5 + 2 * m.level; if (stop > 26) stop = 26;
  v.hitstop = (uint8_t)stop; if (!h.proj) { a.hitstop = (uint8_t)stop; a.connected = 1; }
  v.mv = M_NONE; v.sf = 0; v.crouch = 0;
  if (h.mv == HOME) v.comboHome++;
  if ((m.flags & SPIKE) && vair) { v.st = LAUNCH; v.vx = dir * m.lvx; v.vy = m.lvy; v.spiked = 1; v.stun = 40; }
  else if (vair || (m.flags & LAUNCHER)) {
    int k = 100 - 4 * v.combo; if (k < 60) k = 60;
    v.st = LAUNCH; if (v.y < 1) v.y = 1; v.vx = dir * (m.lvx ? m.lvx : 180); v.vy = (m.lvy > 0 ? m.lvy : 1100) * k / 100; v.spiked = 0;
    v.stun = (int16_t)((m.flags & LAUNCHER) ? 44 : 34) - 2 * v.combo; if (v.stun < 12) v.stun = 12;
    if ((m.flags & WALLB) && !v.wbUsed) v.wbArmed = 1;
  } else { v.st = HIT; v.stun = m.hstun; v.vx = dir * m.kb; }
  v.combo++;
  if (h.mv == HOME) { a.st = AIR; a.mv = M_NONE; a.sf = 0; a.vx = dir * 150; a.vy = 700; a.airJumps = 1; a.homeAir = 0; ev(s, E_HOMEHIT, ai, v.combo, 0, hx, hy); }
  else if (a.mv == DIVE && !h.proj) { a.st = AIR; a.mv = M_NONE; a.sf = 0; a.vx = -dir * 150; a.vy = 1100; a.airJumps = 1; }
  ev(s, E_HIT, ai, m.level, v.combo, hx, hy);
}

static void hits(State& s, uint16_t in0, uint16_t in1) {
  Pending pd[2] = { { false, 0, 0, false }, { false, 0, 0, false } };
  for (int i = 0; i < 2; i++) {
    Player& a = s.p[i]; Player& v = s.p[1 - i]; Box hb;
    if (!hitbox(a, hb) || v.inv) continue;
    const Move& m = move(a.mv); uint8_t id = m.hits > 1 && m.gap ? (uint8_t)clampi((a.mf - m.su - 1) / m.gap, 0, m.hits - 1) : 0;
    if (a.hitId == id) continue;
    if (v.st == ATK) { const Move& vm = move(v.mv); if ((vm.flags & ANTIAIR) && airborne(a) && v.mf >= 3 && v.mf <= vm.su + vm.ac) continue; }
    if (overlap(hb, hurtbox(v))) pd[i] = Pending{ true, a.mv, id, false };
  }
  for (int i = 0; i < 2; i++) {
    Shot& sh = s.shot[i]; Player& v = s.p[1 - i]; if (!sh.on || pd[i].on || v.inv) continue;
    Box b{ (int16_t)(sh.x / F - 18), (int16_t)(sh.y / F - 18), 36, 36 };
    if (overlap(b, hurtbox(v))) { pd[i] = Pending{ true, FIRE, 0, true }; }
  }
  if (s.shot[0].on && s.shot[1].on && absi(s.shot[0].x - s.shot[1].x) < 40 * F && absi(s.shot[0].y - s.shot[1].y) < 40 * F) { s.shot[0].on = s.shot[1].on = 0; }
  for (int i = 0; i < 2; i++) if (pd[i].on) { if (pd[i].proj) { applyHit(s, i, i ? in0 : in1, pd[i]); s.shot[i].on = 0; } else applyHit(s, i, i ? in0 : in1, pd[i]); }
}

// --------------------------------------------------------------------------------------------- step
void step(State& s, uint16_t in0, uint16_t in1) {
  s.nev = 0; s.frame++;
  uint16_t in[2] = { in0, in1 };
  if (s.phase == PH_OVER) return;
  if (s.phase == PH_INTRO) { if (s.pt == 0) ev(s, E_ROUND, 0, s.round, 0, 0, 0); if (++s.pt >= INTRO) { s.phase = PH_FIGHT; s.pt = 0; ev(s, E_FIGHT, 0, 0, 0, 0, 0); } in[0] = in[1] = 0; }
  if (s.phase == PH_KO) { in[0] = in[1] = 0; }
  // remember presses for a few frames so a button pressed slightly early still comes out
  for (int i = 0; i < 2; i++) {
    Player& p = s.p[i]; static const uint16_t B[5] = { BL, BM, BH, BS, BP };
    bool held = p.hitstop > 0 || s.freeze > 0;   // a press made during hitstop or a super flash waits for it to end
    for (int k = 0; k < 5; k++) { if ((in[i] & B[k]) && !(p.prev & B[k])) p.buf[k] = 0; else if (p.buf[k] < 255 && !held) p.buf[k]++; }
    uint16_t fwd = p.face > 0 ? RIGHT : LEFT, back = p.face > 0 ? LEFT : RIGHT;
    for (int k = 0; k < 2; k++) { uint16_t d = k ? back : fwd; if (p.pad[k] < 255) p.pad[k]++; if ((in[i] & d) && !(p.prev & d)) { if (!(p.st == IDLE && p.pad[k] <= 10)) p.pad[k] = 0; } }
  }
  if (s.freeze > 0) { s.freeze--; for (int i = 0; i < 2; i++) s.p[i].prev = in[i]; return; }
  bool act[2];
  for (int i = 0; i < 2; i++) { Player& p = s.p[i]; act[i] = p.hitstop == 0; if (p.hitstop) p.hitstop--; else if (p.inv) p.inv--; }
  for (int i = 0; i < 2; i++) if (act[i]) control(s, i, in[i]);
  for (int i = 0; i < 2; i++) s.p[i].prev = in[i];
  for (int i = 0; i < 2; i++) if (act[i]) physics(s, i);
  pushApart(s);
  for (int i = 0; i < 2; i++) { Player& p = s.p[i]; const Player& o = s.p[1 - i]; if ((p.st == IDLE || p.st == PREJUMP || p.st == AIR || p.st == LAND || p.st == BLOCK || p.st == PARRY) && o.x != p.x) p.face = (int8_t)(o.x > p.x ? 1 : -1); }
  for (int i = 0; i < 2; i++) { Shot& sh = s.shot[i]; if (!sh.on) continue; sh.x += sh.vx; if (--sh.life <= 0 || absi(sh.x) > STAGE + 100 * F) sh.on = 0; }
  if (s.phase != PH_KO) hits(s, in[0], in[1]);
  // rounds
  if (s.phase == PH_FIGHT) {
    if (s.time > 0) s.time--;
    bool d0 = s.p[0].hp <= 0, d1 = s.p[1].hp <= 0, to = s.time <= 0;
    if (d0 || d1 || to) {
      int w = d0 && d1 ? -1 : d1 ? 0 : d0 ? 1 : s.p[0].hp > s.p[1].hp ? 0 : s.p[1].hp > s.p[0].hp ? 1 : -1;
      s.phase = PH_KO; s.pt = 0; s.winner = (int8_t)w;
      for (int i = 0; i < 2; i++) if (s.p[i].hp <= 0) { Player& p = s.p[i]; p.st = KO_; p.sf = 0; if (p.y < 1) p.y = 1; if (p.vy < 900) p.vy = 900; p.hitstop = 30; s.p[1 - i].hitstop = 30; }
      ev(s, E_KO, (uint8_t)(w < 0 ? 2 : w), to ? 1 : 0, 0, 0, 0);
    }
  } else if (s.phase == PH_KO) {
    for (int i = 0; i < 2; i++) { Player& p = s.p[i]; if (p.st == KO_ && p.y <= 0) { p.vx = 0; p.vy = 0; } if (s.pt == 50 && s.winner == i && !airborne(p) && p.st != KO_) { p.st = WIN_; p.sf = 0; p.vx = 0; } }
    if (++s.pt >= KO_TIME) {
      if (s.winner >= 0) s.p[s.winner].rounds++;
      if (s.p[0].rounds >= 2 || s.p[1].rounds >= 2) { s.phase = PH_OVER; s.winner = (int8_t)(s.p[0].rounds >= 2 ? 0 : 1); }
      else { s.round++; resetRound(s); }
    }
  }
}

}  // namespace gk

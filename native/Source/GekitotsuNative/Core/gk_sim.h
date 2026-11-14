// GEKITOTSU combat simulation. Native C++, engine-independent, deterministic, integer-only, fixed 60 Hz.
// It is a function-for-function translation of the browser game's rules (sim.js): the same constants, the same
// order of operations, the same arithmetic. The whole match is one plain State, so a rollback is a copy, and
// two machines fed the same inputs stay identical. Nothing here knows about rendering, animation or frame rate.
#pragma once
#include <cstdint>

namespace gk {

constexpr int F = 100;                       // sub-units per unit; the fighter stands about 89 units tall
constexpr int STAGE = 375 * F, SEP = 345 * F, PW = 17 * F, JG = 58, GRAV = 62;
constexpr int BUF = 5, NBUF = 40, DMAX = 6000, BAR = 1000, NB = 8, MASK = 4095;
constexpr int INTRO = 120, KOTIME = 290, KD = 26, GETUP = 22;

// One player's input for one frame. Directions are absolute (left and right on screen).
enum : int32_t { IN_U = 1, IN_D = 2, IN_L = 4, IN_R = 8, IN_LT = 16, IN_MD = 32, IN_HV = 64, IN_SP = 128, IN_AS = 256, IN_DI = 512, IN_PA = 1024, IN_HM = 2048 };

enum St : int8_t { IDLE, PREJUMP, JUMP, LAND, DASHF, DASHB, RUN, DRUSH, ATK, BLOCK, HIT, AHIT, KDOWN, GETUP_, THROWN, THROWING, WIN, PARRY, PREC, TECH, ST_COUNT };
enum Sub : int8_t { STAND, CROUCH, WALKF, WALKB };
enum Phase : int8_t { PH_INTRO, PH_FIGHT, PH_KO, PH_OVER };
enum Ko : int8_t { KO_NONE, KO_KO, KO_PERFECT, KO_TIME, KO_DOUBLE, KO_DRAW };
enum Auto : int8_t { AC_NONE, AC_L, AC_LC, AC_M, AC_H };
enum : int32_t { C_CH = 1, C_SP = 2, C_SU = 4 };

struct Hit {
	int s = 0, e = 0, d = 0, stun = 0, bstun = 0, wb = 0, gb = 0, hsp = 0, lvl = 0, kb = 0, grp = 0, lk = 0, reb = 0, free_ = 0, np = 0, req = 0, xup = 0, fin = 0, dd = 0;
	char g = 'm';
	int8_t has_hb = 0, has_launch = 0, has_air = 0, has_carry = 0, has_pop = 0, has_dd = 0;
	int hb[4] = { 0, 0, 0, 0 }, launch[2] = { 0, 0 }, air[2] = { 0, 0 }, carry[2] = { 0, 0 }, pop[2] = { 0, 0 };
};
struct Move {
	int su = 0, ac = 0, rc = 0, d = 0, hsp = 0, kb = 0, lvl = 0, can = 0, t = 0, lastAct = 0, crouch = 0, cx = 0, wr = 0, stand = 0, hardKd = 0, landRec = 0, sp = 0, od = 0, hover = 0, thr = 0, arm = 0, di = 0, ag = 0, thru = 0,
		sup = 0, sa = 0, frz = 0, minScale = 0, stopOnHit = 0, partAt = 0, ca = 0, ss = 0, isAir = 0, sg = 0;
	char g = 'm';
	int8_t has_sg = 0, has_mv = 0, has_pcLaunch = 0, has_rise = 0, has_home = 0, has_pinv = 0, has_inv = 0, has_fc = 0, has_ainv = 0, has_lowInv = 0, has_wEnd = 0, has_tc = 0, has_chain = 0, has_fu = 0, has_proj = 0;
	int mv[3] = { 0, 0, 0 }, pcLaunch[2] = { 0, 0 }, rise[3] = { 0, 0, 0 }, home[3] = { 0, 0, 0 }, pinv[2] = { 0, 0 }, inv[2] = { 0, 0 }, fc[2] = { 0, 0 }, ainv[2] = { 0, 0 }, lowInv[2] = { 0, 0 }, wEnd[2] = { 0, 0 };
	int16_t tc[3] = { -1, -1, -1 }, chain5 = -1, chain2 = -1, fuTo[6] = { -1, -1, -1, -1, -1, -1 };   // tc: L M H.  fuTo: S4 S2 S5 L M H
	int fuw[2] = { 0, 0 };
	int nhits = 0; Hit hits[9];
	int pf = 0, pvx = 0, py = 0, pw = 0, ph = 0, phits = 0; Hit proj;
};
const Move& move(int id);
const char* moveKey(int id);
const char* moveName(int id);
int moveCount();
int moveByKey(const char* key);

struct Fighter {
	int32_t ch, x, y, vx, vy, pv, face, hp, sa, dr, bo, drT, sf, subf, mf, hitMask, contact, hitOK, hs, stun, prev, d66, d44, combo, comboDmg, jug;
	int32_t wbUsed, gbUsed, wbPend, gbPend, jd, rec, cb, hitHi, tinv, wins, back, red, redT, air2, noTech;
	int32_t ca, rq, part, jg, cxSeq, fum, mseq, cs, lastSeq, acq, rcn, sshift, drs, pok, roll, hard, arm, armHit, drb, mod, aci, crum, pp, plock, half, hm, hmc, fr, hmE, dj, wj, aa;
	int32_t tk, tf, pmT, pmN, airBeamUsed;
	int32_t age[8], pd[8];
	int16_t buf[NBUF];
	int16_t mv, fuq;                       // move ids, -1 for none
	int8_t st, sub, ac;
	bool runFromDash;
};
struct Shot { int32_t o, ch, k, x, y, dir, left, cd, pause, life, mod; };

enum EvType : int8_t { E_HIT, E_BLOCK, E_PARRY, E_PPARRY, E_ARMOR, E_DICLASH, E_CLASH, E_SPLAT, E_KD, E_GBOUNCE, E_WBOUNCE, E_JUMP, E_WALLJUMP, E_LAND, E_DASH, E_RUN, E_RUSH, E_PARRYON, E_HOME, E_RISE, E_FIRE,
	E_WHIFF, E_GRAB, E_TECH, E_PEND, E_SUPER, E_EX, E_DI, E_SPECIAL, E_BURNOUT, E_RECOVER, E_ROUND, E_FIGHT, E_KO, E_WINPOSE, E_OVER };
// bits of a hit event's flags
enum : int32_t { H_THROW = 1, H_KO = 2, H_SPECIAL = 4, H_IMPACT = 8, H_CRUMPLE = 16, H_FREE = 32, H_FINISH = 64, H_LOW = 128, H_OVERHEAD = 256 };
// who: the fighter concerned, or -1. a: level, round number, KO kind. hit: b damage, c combo count, d combo damage, e counter kind (1 counter, 2 punish), f flags. super: a level, b critical, c move.
struct Event { int32_t type, who, x, y, a, b, c, d, e, f; };

constexpr int MAX_SHOTS = 8, MAX_EVENTS = 48;
struct Config { int32_t training, rounds; bool experimentalMovement; };
struct State {
	int32_t frame, slow, pt, kx, round, time, freeze, frzWho, winner, rw, nshots, nev;
	int8_t phase, koType, pad[2];
	Config cfg;
	Fighter p[2];
	Shot shot[MAX_SHOTS];
	Event ev[MAX_EVENTS];                  // what happened this step, for sound, effects and camera
};

void newMatch(State& s, bool training, int rounds);
void step(State& s, int in0, int in1);
void debugStart(State& s, int who, int moveId);

struct Box { int x0, y0, x1, y1; };
int hurtboxes(const Fighter& p, Box out[12]);      // returns how many
int hitboxes(const Fighter& p, Box out[9]);
bool crouching(const Fighter& p);
bool invul(const Fighter& p);

}  // namespace gk

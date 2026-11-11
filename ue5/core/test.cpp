#include "gk_core.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
using namespace gk;
static int pass = 0, fail = 0;
#define CHECK(c, name) do { if (c) pass++; else { fail++; printf("FAIL: %s\n", name); } } while (0)
struct Seen { bool e[32]; int last[32]; };
static Seen run(State& s, uint16_t a, uint16_t b, int n, Seen sn = Seen{}) { for (int i = 0; i < n; i++) { step(s, a, b); for (int k = 0; k < s.nev; k++) { sn.e[s.ev[k].type] = true; sn.last[s.ev[k].type] = s.ev[k].a; } } return sn; }
static bool go(State& s, uint16_t in, uint8_t want, int max = 40) { for (int f = 0; f < max; f++) { step(s, (f & 1) ? (in & (UP | DOWN | LEFT | RIGHT)) : in, 0); if (s.p[0].st == ATK && s.p[0].mv == want) return true; } return false; }
static State fresh(int dist = 100) { State s; init(s); run(s, 0, 0, INTRO); s.p[0].x = -dist * F / 2; s.p[1].x = dist * F / 2; return s; }
static uint32_t rs = 12345; static uint32_t rnd() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static uint16_t rin() { uint32_t r = rnd(); uint16_t d = 0; switch (r % 9) { case 1: d = UP; break; case 2: d = DOWN; break; case 3: d = LEFT; break; case 4: d = RIGHT; break; case 5: d = UP | RIGHT; break; case 6: d = DOWN | LEFT; break; case 7: d = LEFT; break; default: break; }
  uint16_t b = 0; uint32_t q = (r >> 8) % 24; if (q == 0) b = BL; else if (q == 1) b = BM; else if (q == 2) b = BH; else if (q == 3) b = BS; else if (q == 4) b = BP; else if (q == 5) b = BM | BH; else if (q == 6) b = BL | BM; else if (q == 7) b = BH | BS; return d | b; }

int main() {
  { // normals, chain, launcher, jump cancel, air chain
    State s = fresh(90); Seen e = run(s, BL, 0, 1); e = run(s, 0, 0, 5, e);
    CHECK(e.e[E_HIT] && s.p[1].st == HIT, "5L hits at close range");
    s = fresh(90); bool ok = true; ok &= go(s, BL, S5L); ok &= go(s, BM, S5M); ok &= go(s, BH, S5H); ok &= go(s, BH | DOWN, S2H); e = run(s, DOWN, 0, 14);
    CHECK(ok && s.p[1].st == LAUNCH && s.p[1].combo == 4, "L > M > H > 2H launches as a 4-hit chain");
    e = run(s, UP, 0, 12, Seen{}); CHECK(e.e[E_JUMP] && s.p[0].st == AIR, "launcher is jump-cancellable on hit");
    ok = true; run(s, 0, 0, 3); ok &= go(s, BL, JL); ok &= go(s, BM, JM); ok &= go(s, BH, JH); run(s, 0, 0, 12);
    CHECK(ok && s.p[1].combo >= 7, "air chain continues the juggle");
    int hp = s.p[1].hp; CHECK(hp < MAX_HP - 250 && hp > 0, "combo damage is scaled, not lethal");
  }
  { // blocking rules
    State s = fresh(90); Seen e = run(s, BM | DOWN, RIGHT, 12); CHECK(!e.e[E_BLOCK] && e.e[E_HIT], "a low hits a standing guard");
    s = fresh(90); e = run(s, BM | DOWN, RIGHT | DOWN, 12); CHECK(e.e[E_BLOCK] && !e.e[E_HIT], "a low is blocked crouching");
    s = fresh(90); e = run(s, BM, RIGHT, 12); CHECK(e.e[E_BLOCK], "a mid is blocked standing");
    s = fresh(60); run(s, UP, 0, 6); run(s, 0, 0, 26); e = run(s, BH, RIGHT | DOWN, 1); e = run(s, 0, RIGHT | DOWN, 16, e); CHECK(e.e[E_HIT], "a jump-in hits a crouching guard");
  }
  { // homing
    State s = fresh(900); Seen e = run(s, BM | BH, 0, 1); e = run(s, 0, 0, 70, e);
    CHECK(e.e[E_HOME] && e.e[E_HOMEHIT] && s.p[1].st == LAUNCH && (s.p[0].st == AIR || s.p[0].st == LAND || s.p[0].st == IDLE), "Slipstream crosses the stage, launches, and leaves the attacker free");
    s = fresh(900); e = run(s, BM | BH, RIGHT, 1); e = run(s, 0, RIGHT, 50, e); CHECK(e.e[E_BLOCK] && s.p[0].st == HELPLESS, "blocked Slipstream leaves the attacker helpless");
    e = run(s, 0, 0, 60, Seen{}); CHECK(e.e[E_LAND], "and he falls and lands");
    s = fresh(900); run(s, UP, 0, 20); e = run(s, BM | BH, 0, 1); e = run(s, 0, 0, 70, e); CHECK(e.e[E_HOMEHIT], "Slipstream works from the air");
    s = fresh(500); run(s, BM | BH, 0, 1); e = Seen{}; for (int f = 0; f < 60; f++) { bool near_ = s.p[1].x - s.p[0].x < 330 * F && s.p[0].st == ATK; e = run(s, 0, (near_ && s.p[1].st == IDLE) ? (BH | DOWN) : (s.p[1].st == ATK ? DOWN : 0), 1, e); }
    CHECK(s.p[0].st == LAUNCH || s.p[0].st == DOWN_ || s.p[0].st == RISE_, "the launcher beats Slipstream as an anti-air");
    s = fresh(900); int n = 0, maxh = 0; bool inCombo = false;
    for (int f = 0; f < 260; f++) { step(s, (f & 1) ? 0 : (BM | BH), 0); for (int k = 0; k < s.nev; k++) if (s.ev[k].type == E_HOMEHIT) { n++; inCombo = true; } if (inCombo && s.p[1].combo == 0) break; if (s.p[1].comboHome > maxh) maxh = s.p[1].comboHome; }
    CHECK(n == 2 && maxh == 2, "exactly two Slipstream pops are possible in one combo");

  }
  { // parry
    State s = fresh(90); run(s, BM, 0, 4); Seen e = run(s, 0, BP, 1); e = run(s, 0, 0, 8, e); CHECK(e.e[E_PARRY] && s.p[1].st == IDLE && s.p[0].hitstop > 6, "a timed parry frees the defender and freezes the attacker");
    s = fresh(90); e = run(s, 0, BP, 1); run(s, 0, 0, 12); e = run(s, BL, 0, 1); e = run(s, 0, 0, 6, e); CHECK(e.e[E_HIT], "a parry that misses its timing is punishable");
  }
  { // throws
    State s = fresh(70); Seen e = run(s, BL | BM, 0, 1); e = run(s, 0, 0, 40, e); CHECK(e.e[E_THROW] && s.p[1].hp == MAX_HP - 120, "throw connects and does its damage");
    s = fresh(70); e = run(s, BL | BM, 0, 1); e = run(s, 0, 0, 7, e); e = run(s, 0, BL | BM, 1, e); e = run(s, 0, 0, 20, e); CHECK(e.e[E_THROWTECH] && s.p[1].hp == MAX_HP, "throw can be broken");
    s = fresh(70); e = run(s, BL | BM, RIGHT, 1); e = run(s, 0, RIGHT, 40, e); CHECK(e.e[E_THROW], "throw beats a guard");
    s = fresh(70); e = run(s, BL | BM | LEFT, 0, 1); e = run(s, LEFT, 0, 60, e); CHECK(e.e[E_THROW] && e.last[E_THROW] == 1 && s.p[1].x < s.p[0].x, "back throw switches sides");
  }
  { // specials
    State s = fresh(600); Seen e = run(s, BS, 0, 1); e = run(s, 0, 0, 110, e); CHECK(e.e[E_FIRE] && e.e[E_HIT], "the projectile travels and hits");
    s = fresh(600); e = run(s, BS, RIGHT, 1); e = run(s, 0, RIGHT, 110, e); CHECK(e.e[E_BLOCK] && s.p[1].hp == MAX_HP - 6, "blocked projectile chips");
    s = fresh(300); e = run(s, BS | LEFT, 0, 1); e = run(s, 0, 0, 30, e); CHECK(e.e[E_VANISH] && s.p[0].x > s.p[1].x, "Ghost Step reappears behind the opponent");
    s = fresh(90); s.p[0].x = 500 * F; s.p[1].x = 620 * F; e = run(s, BS | RIGHT, 0, 1); e = run(s, 0, 0, 90, e); CHECK(e.e[E_HIT] && e.e[E_WALLB], "Black Current wall-bounces near the corner");
    s = fresh(80); e = run(s, BS | DOWN, 0, 1); e = run(s, 0, 0, 10, e); CHECK(e.e[E_HIT] && s.p[1].st == LAUNCH, "Spindrift launches");
    s = fresh(80); run(s, BS | DOWN, 0, 1); run(s, 0, BL, 1); e = run(s, 0, 0, 10); CHECK(s.p[0].hp == MAX_HP, "Spindrift is invulnerable on the way up");
    s = fresh(80); s.p[1].st = LAUNCH; s.p[1].y = 160 * F; s.p[1].stun = 60; s.p[0].y = 170 * F; s.p[0].st = AIR; e = run(s, BH | DOWN, 0, 1); e = run(s, 0, 0, 60, e); CHECK(e.e[E_GROUNDB], "an air spike bounces them off the floor");
    s = fresh(80); run(s, BH | DOWN, 0, 1); for (int f = 0; f < 60 && s.p[1].y < 60 * F; f++) step(s, 0, 0); s.p[1].stun = 0; e = run(s, 0, 0, 2); e = run(s, 0, BL, 1, e); e = run(s, 0, 0, 3, e); CHECK(e.e[E_TECH], "the juggled fighter can flip out once the stun ends");
  }
  { // super and rounds
    State s = fresh(120); s.p[0].meter = 3 * BAR; Seen e = run(s, BH | BS, 0, 1); e = run(s, 0, 0, 80, e);
    CHECK(e.e[E_SUPER] && e.last[E_SUPER] == 3 && s.p[0].meter < BAR && s.p[1].hp == MAX_HP - 450, "three bars: King Tide, full damage, gauge spent");
    s = fresh(120); s.p[0].meter = 500; e = run(s, BH | BS, 0, 1); e = run(s, 0, 0, 30, e); CHECK(!e.e[E_SUPER], "no super without a bar");
    s = fresh(90); int guard = 0; while (s.phase != PH_OVER && guard++ < 4000) { if (s.phase == PH_FIGHT) s.p[1].hp = s.p[1].hp > 20 ? 20 : s.p[1].hp; step(s, (s.frame % 12 == 0) ? BL : 0, 0); if (s.phase == PH_FIGHT && s.p[1].x - s.p[0].x > 90 * F) s.p[1].x = s.p[0].x + 90 * F; }
    CHECK(s.phase == PH_OVER && s.winner == 0 && s.p[0].rounds == 2, "two rounds won ends the match");
  }
  { // determinism, rollback, invariants, no stuck states
    State a, b; init(a); init(b); bool same = true, inv = true, roll = true; rs = 777; static uint16_t log0[240000], log1[240000];
    for (int f = 0; f < 240000; f++) {
      uint16_t i0 = (f / 37) % 5 == 4 ? 0 : rin(), i1 = (f / 53) % 4 == 3 ? 0 : rin(); log0[f] = i0; log1[f] = i1; step(a, i0, i1);
      for (int i = 0; i < 2; i++) { const Player& p = a.p[i]; if (p.x > STAGE || p.x < -STAGE || p.y < 0 || p.hp < 0 || p.hp > MAX_HP || p.meter < 0 || p.meter > MAX_METER || p.st > WIN_ || p.mv >= M_COUNT) inv = false; }
      if (a.phase == PH_OVER) { init(a); }
      if (f % 997 == 500) { State snap = a; uint64_t h0; State t = a; for (int k = 0; k < 8; k++) step(t, rin(), rin()); (void)t; State r = snap; uint32_t keep = rs; for (int k = 0; k < 8; k++) step(r, 5, 9); h0 = hash(r); r = snap; for (int k = 0; k < 8; k++) step(r, 5, 9); if (hash(r) != h0) roll = false; rs = keep; }
    }
    for (int f = 0; f < 240000; f++) { step(b, log0[f], log1[f]); if (b.phase == PH_OVER) init(b); }
    same = hash(a) == hash(b);
    CHECK(same, "same inputs give the same state (240,000 frames)"); CHECK(inv, "positions, health and gauge stay in range"); CHECK(roll, "restore and re-simulate matches");
    bool free_ = true; rs = 4242;
    for (int t = 0; t < 400; t++) { State s; init(s); run(s, 0, 0, INTRO); for (int f = 0; f < 240; f++) step(s, rin(), rin()); for (int f = 0; f < 420; f++) step(s, 0, 0);
      if (s.phase == PH_FIGHT) for (int i = 0; i < 2; i++) if (s.p[i].st != IDLE) { free_ = false; printf("  stuck in %s (move %s)\n", stateName(s.p[i].st), move(s.p[i].mv).name); } }
    CHECK(free_, "nobody is ever left stuck: hands off, both return to neutral");
  }
  printf("checks passed %d failed %d\n", pass, fail); return fail ? 1 : 0;
}

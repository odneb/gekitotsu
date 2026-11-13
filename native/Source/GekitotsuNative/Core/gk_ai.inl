#include "gk_ai.h"
#include <cmath>

namespace gk {

namespace {
const int FW = 8192, BK = 16384, HM = IN_HM, D = IN_D, U = IN_U, L = IN_LT, M = IN_MD, H = IN_HV, SP = IN_SP, AS = IN_AS, DI = IN_DI, PA = IN_PA;
struct Part { int m, n; };

double hsh(int64_t v)
{
	uint32_t n = uint32_t(v); n = (n ^ 0x9e3779b9u) * 0x85ebca6bu; n ^= n >> 13; n *= 0xc2b2ae35u;
	return double(n ^ (n >> 16)) / 4294967296.0;
}
}

void aiMake(Ai& ai, int level) { ai.qn = 0; ai.qh = 0; ai.level = level; ai.hold = 0; ai.cool = 0; }

int aiThink(Ai& ai, const State& s, int me)
{
	const Fighter& p = s.p[me]; const Fighter& o = s.p[1 - me]; const int lv = ai.level;
	auto out = [&p](int m) { int r = m & MASK; if (m & FW) { r |= p.face > 0 ? IN_R : IN_L; } if (m & BK) { r |= p.face > 0 ? IN_L : IN_R; } return r; };
	auto qlen = [&ai]() { return ai.qn - ai.qh; };
	auto qclear = [&ai]() { ai.qn = 0; ai.qh = 0; };
	auto qshift = [&ai]() { return ai.q[ai.qh++]; };
	if (s.phase != PH_FIGHT) { qclear(); return 0; }
	if (qlen() > 0) { if (p.st == HIT || p.st == AHIT || p.st == KDOWN || p.st == THROWN || p.st == BLOCK) { qclear(); } else { return out(qshift()); } }
	const double dist = double(o.x > p.x ? o.x - p.x : p.x - o.x) / F / 0.7;
	auto r = [&](int n) { return int(hsh(int64_t(s.frame) * 31 + p.x + int64_t(o.x) * 7 + int64_t(o.hp) * 13 + int64_t(n) * 977) * 100.0); };
	const Move* om = o.st == ATK ? &move(o.mv) : nullptr; const bool grounded = p.y <= 0 && p.st == IDLE;
	auto add = [&ai](const Part* parts, int count) { for (int k = 0; k < count; k++) { for (int j = 0; j < (parts[k].n ? parts[k].n : 1); j++) { if (ai.qn < 256) { ai.q[ai.qn++] = parts[k].m; } } } };
	auto addAuto = [&](int b) { for (int k = 0; k < 9; k++) { const Part two[2] = { { AS | b, 2 }, { AS, 8 } }; add(two, 2); } };
	auto begin = [&]() { qclear(); };
	auto go = [&]() { return out(qshift()); };
#define PUSH(...) do { const Part parts_[] = { __VA_ARGS__ }; begin(); add(parts_, int(sizeof(parts_) / sizeof(parts_[0]))); return go(); } while (0)
	static const Part drc[] = { { D | M, 2 }, { 0, 3 }, { FW, 2 }, { 0, 2 }, { FW, 2 }, { 0, 18 }, { AS | M, 2 }, { AS, 9 }, { AS | M, 2 }, { AS, 12 }, { FW | SP, 2 }, { 0, 20 } };
	// holding a grab: squeeze a few times, then pick a direction (up and down lead into air follow-ups)
	if (p.st == THROWING)
	{
		if (p.tk >= 3 && p.tf >= 18 && lv >= 2) { PUSH({ 0, 3 }, { U | FW, 3 }, { FW, 4 }); }
		if (p.tk || p.sf < 10) { return 0; }
		if (p.pmN < (1 + lv < 5 ? 1 + lv : 5)) { return (s.frame & 3) == 0 ? out(L) : 0; }
		const int k = r(30); return out(k < 40 ? U : k < 62 ? D : k < 82 ? FW : BK);
	}
	const bool drive = !p.bo && p.dr >= 2000, lowHit = om && om->nhits > 0 && om->hits[0].g == 'l', hiHit = om && om->nhits > 0 && om->hits[0].g == 'h' && !om->isAir;
	bool myProj = false, incoming = false;
	for (int k = 0; k < s.nshots; k++) { const Shot& q = s.shot[k]; if (q.o == me) { myProj = true; } else if ((q.x > p.x ? q.x - p.x : p.x - q.x) < 160 * F && int64_t(q.x - p.x) * q.dir < 0) { incoming = true; } }
	if (p.st == BLOCK) { if (lv >= 3 && drive && p.dr >= 3000 && r(9) < 6) { return out(DI); } return out(BK | (lowHit ? D : 0)); }
	if (p.st == KDOWN || p.st == AHIT) { return out(r(10) < 40 ? BK : 0); }
	if (grounded && om && om->di && o.mf < 22 && dist < 170)
	{
		static const int a[4] = { 0, 15, 55, 90 }, b[4] = { 0, 30, 75, 96 };
		const int k = r(11); if (!p.bo && k < a[lv]) { return out(DI); } if (!p.bo && k < b[lv]) { PUSH({ PA, 30 }); } return out(BK);
	}
	// time until a launched opponent lands, and how long a Slipstream needs to reach them
	const double tLand = o.y > 0 ? (o.vy + std::sqrt(std::fmax(0.0, double(o.vy) * o.vy + 2.0 * 66 * o.y))) / 66 : 0.0;
	const double hx = double(o.x - p.x), hy = double(o.y - p.y), tHome = 15 + std::sqrt(hx * hx + hy * hy) / 840;
	const bool chase = o.st == AHIT && o.hmc < 2 && !p.hm && tLand > tHome + 3;
	{
		static const int a[4] = { 0, 0, 22, 45 };
		if (grounded && om && om->has_proj && o.mf < 16 && dist > 130 && drive && lv >= 2 && r(33) < a[lv]) { PUSH({ AS | HM, 2 }, { AS, 3 }); }   // through the fireball
	}
	const bool threat = (om && dist < 150 && o.mf <= om->lastAct + 1) || incoming;
	if (grounded && threat)
	{
		static const int a[4] = { 0, 20, 50, 85 }, b[4] = { 0, 35, 65, 92 };
		if (om && om->thr) { if (r(1) < lv * 25) { return out(L | M); } }
		if (o.y > 0 && dist < 95 && r(2) < a[lv]) { PUSH({ FW | SP, 2 }, { 0, 6 }); }
		if (incoming && !om && drive && lv >= 2 && r(12) < 30) { PUSH({ PA, 16 }); }
		if (r(3) < b[lv]) { return out(BK | (lowHit && !hiHit && o.y <= 0 ? D : 0)); }
	}
	if (!grounded)
	{
		// in the air: flick them upward, spike them, hit behind, follow with the second jump, kick off walls
		if (p.y > 0 && (p.st == JUMP || (p.st == ATK && p.contact)))
		{
			const double dy = double(o.y - p.y) / F, ax = double(o.x > p.x ? o.x - p.x : p.x - o.x) / F; const bool behind = int64_t(o.x - p.x) * p.face < 0, up = o.y > 0 || o.st == AHIT;
			static const int a[4] = { 0, 18, 45, 70 };
			if (p.st == ATK && !p.dj && o.st == AHIT && dy > 30 && r(20) < 30 + lv * 20) { return out(U | (behind ? BK : FW)); }
			if (up && !behind && ax < 66 && dy > 14 && dy < 120 && r(21) < 40 + lv * 20) { return out(FW | SP); }
			if (up && ax < 56 && dy < -6 && dy > -100 && r(22) < 35 + lv * 20) { return out(lv >= 2 && r(23) < 50 ? D | SP : D | H); }
			if (up && lv >= 3 && !behind && ax < 60 && dy > 10 && p.vy > 0 && r(24) < 12) { return out(FW | SP); }
			if (behind && ax < 76 && std::fabs(dy) < 60 && r(25) < 45 + lv * 15) { return out(lv >= 3 && ax > 30 && r(26) < 35 ? HM : M); }
			if (p.st == JUMP && !p.airBeamUsed && !behind && ax > 100 && ax < 280 && std::fabs(dy) < 35 && r(34) < 5 + lv * 3) { return out(SP); }
			if (p.st == JUMP && chase && (ax > 64 || std::fabs(dy) > 70) && r(32) < a[lv]) { return out(HM); }
			if (p.st == JUMP && lv >= 2 && !p.wj && p.y > 30 * F && (p.x < 0 ? -p.x : p.x) > STAGE - 20 * F && r(27) < 8) { return out(U); }
			if (p.st == JUMP && lv >= 2 && !p.dj && incoming && p.vy < 0 && r(28) < 25) { return out(U); }
			if (o.st == AHIT && p.st == JUMP) { if (p.vy < 100 && dist < 90 && r(4) < 60 + lv * 10) { return out(H); } return out(behind ? BK : FW); }
		}
		if (p.st == JUMP && p.vy < 100 && dist < 90 && r(4) < 60 + lv * 10) { return out(H); }
		if (p.st == DRUSH && p.sf >= 10 && dist < 80) { PUSH({ AS | M, 2 }, { AS, 9 }, { AS | M, 2 }, { AS, 12 }, { FW | SP, 2 }, { 0, 12 }); }
		return out(ai.hold);
	}
	{
		static const int a[4] = { 0, 20, 50, 85 }, b[4] = { 0, 24, 52, 80 }, c[4] = { 0, 35, 70, 95 };
		if (o.y > 20 * F && o.vy < 300 && dist < 105 && o.st != AHIT && r(5) < a[lv]) { if (r(6) < 70) { PUSH({ FW | SP, 2 }, { 0, 6 }); } else { PUSH({ D | H, 2 }, { 0, 6 }); } }
		if (chase && o.y > 24 * F && r(31) < b[lv]) { PUSH({ HM, 2 }, { 0, 3 }); }   // fly in after a launched opponent
		if (o.st == LAND && o.rec >= 20 && o.rec - o.sf > 10 && dist >= 76 && dist < 170) { return out(FW); }   // walk up to someone stuck in a long landing
		if (o.st == AHIT && o.y > 40 * F && o.vy > -700 && dist < 150 && r(29) < c[lv]) { PUSH({ U | FW, 3 }, { FW, 2 }); }   // go up after a launched opponent
	}
	const bool open = (om && !om->isAir && o.mf > om->lastAct + 1 && om->t - o.mf > 9) || (o.st == LAND && o.rec > 6) || o.st == PREC || (o.st == HIT && o.crum && o.stun > 30);
	{
		static const int a[4] = { 0, 40, 78, 98 };
		if (open && dist < 78 && r(7) < a[lv]) { begin(); addAuto(p.sa >= 2000 || lv < 2 ? M : H); return go(); }
	}
	if (ai.cool > 0) { ai.cool--; return out(ai.hold); }
	{ static const int a[4] = { 0, 16, 9, 4 }; ai.cool = a[lv]; }
	const int k = r(8);
	if (o.st == KDOWN || o.st == GETUP_)
	{
		if (dist > 64) { ai.hold = FW; return out(FW); }
		ai.hold = 0;
		if (o.st == GETUP_ && o.sf > 12) { if (k < 30) { return out(L | M); } if (k < 70) { begin(); addAuto(L); return go(); } return out(BK | D); }
		return 0;
	}
	if (dist > 240)
	{
		static const int a[4] = { 0, 6, 4, 2 };
		if (k < 14 && !myProj) { PUSH({ SP, 2 }, { 0, 24 }); }
		if (k < 58) { ai.hold = FW; return out(FW); }
		if (drive && lv >= 2 && k < 72) { PUSH({ FW, 1 }, { 0, 1 }, { FW | PA, 2 }, { PA, 4 }, { 0, 4 }); }
		if (k < 80) { PUSH({ FW, 1 }, { 0, 1 }, { FW, 2 }, { 0, 14 }); }
		if (k < 90) { PUSH({ U | FW, 3 }, { 0, 26 }, { H, 2 }, { 0, 10 }); }
		if (k < 90 + a[lv]) { PUSH({ HM, 2 }, { 0, 30 }); }   // the gamble: fly in raw
		ai.hold = 0; return 0;
	}
	if (dist > 80)
	{
		if (dist < 108 && k < 26)
		{
			if (lv >= 2 && p.dr >= 4000 && !p.bo && r(13) < 55) { begin(); add(drc, 12); return go(); }
			PUSH({ D | M, 2 }, { D, 4 }, { SP, 2 }, { 0, 18 });
		}
		if (dist < 116 && k < 38) { PUSH({ BK | H, 2 }, { 0, 14 }); }
		if (dist < 112 && k < 46) { PUSH({ FW | M, 2 }, { 0, 14 }); }
		if (dist < 104 && k < 54) { PUSH({ D | FW | H, 2 }, { D, 14 }); }
		if (!p.bo && dist < 130 && k < 60 && lv >= 2) { return out(DI); }
		if (k < 68 && dist > 110 && dist < 190) { const Part pre[] = { { U | FW, 3 }, { 0, 27 }, { H, 2 }, { 0, 12 } }; begin(); add(pre, 4); addAuto(M); return go(); }
		if (k < 72 && dist < 190 && lv >= 2)
		{
			static const int xs[3] = { M, H, L }; const int x = xs[r(15) % 3];
			const Part pre[] = { { M | H, 2 }, { 0, 13 }, { x, 2 }, { 0, x == L ? 16 : 24 } }; begin(); add(pre, 4);
			if (x == L) { const Part post[] = { { L | M, 2 }, { 0, 10 } }; add(post, 2); }
			return go();
		}
		if (k < 74 && dist > 150 && !myProj) { PUSH({ SP, 2 }, { 0, 22 }); }
		if (k < 80 && dist < 150) { static const int fs[3] = { BK | SP, D | SP, SP }; const int f2 = fs[r(14) % 3]; PUSH({ D | SP, 2 }, { 0, 22 }, { f2, 3 }, { 0, 26 }); }
		if (k < 92) { ai.hold = FW; return out(FW); }
		ai.hold = BK; return out(BK);
	}
	ai.hold = 0;
	if (k < 22) { begin(); addAuto(L); return go(); }
	if (k < 42) { begin(); addAuto(M); return go(); }
	if (k < 52) { begin(); addAuto(H); return go(); }
	if (k < 64) { return out(L | M); }
	if (k < 72) { static const int fs[3] = { BK | SP, D | SP, SP }; const int f2 = fs[r(16) % 3]; PUSH({ D | SP, 2 }, { 0, 22 }, { f2, 3 }, { 0, 26 }); }
	if (k < 80 && lv >= 2 && p.dr >= 4000 && !p.bo) { begin(); add(drc, 12); return go(); }
	if (k < 88) { PUSH({ BK, 1 }, { 0, 1 }, { BK, 2 }, { 0, 16 }); }
	ai.hold = BK | D; return out(BK | D);
#undef PUSH
}

}  // namespace gk

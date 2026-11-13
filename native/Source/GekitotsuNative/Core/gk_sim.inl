// GEKITOTSU combat simulation: see gk_sim.h. Each function below is the C++ of the function of the same name in the
// browser game's sim.js, kept in the same order with the same arithmetic, so the two can be compared frame by frame.
#include "gk_sim.h"
#include <cstring>
#include <cmath>

namespace gk {

#include "gk_moves.inl"

static const int SCALE[10] = { 100, 100, 80, 70, 60, 50, 40, 30, 20, 10 };
static Move g_moves[M_COUNT]; static bool g_built = false;
static inline const Move& MV(int id) { return g_moves[id]; }
// All move rules come from the generated table, including beam supers.
static void ready() { if (!g_built) { buildMoves(g_moves); g_built = true; } }

const Move& move(int id) { ready(); return g_moves[id < 0 || id >= M_COUNT ? 0 : id]; }
const char* moveKey(int id) { return id >= 0 && id < M_COUNT ? MOVE_KEY[id] : ""; }
const char* moveName(int id) { return id >= 0 && id < M_COUNT ? MOVE_NAME[id] : ""; }
int moveCount() { return M_COUNT; }
int moveByKey(const char* key) { for (int i = 0; i < M_COUNT; i++) { if (!std::strcmp(MOVE_KEY[i], key)) { return i; } } return -1; }

static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }
static inline int iabs(int a) { return a < 0 ? -a : a; }

static void push(State& s, int type, int who = -1, int x = 0, int y = 0, int a = 0, int b = 0, int c = 0, int d = 0, int e = 0, int f = 0)
{
	if (s.nev >= MAX_EVENTS) { return; }
	Event& ev = s.ev[s.nev++]; ev.type = type; ev.who = who; ev.x = x; ev.y = y; ev.a = a; ev.b = b; ev.c = c; ev.d = d; ev.e = e; ev.f = f;
}

// ------------------------------------------------------------ state
static void newFighter(Fighter& p, int ch, int i)
{
	std::memset(&p, 0, sizeof(p));
	p.ch = ch; p.x = (i ? 80 : -80) * F; p.face = i ? -1 : 1; p.hp = CH_HP; p.dr = DMAX; p.st = IDLE; p.sub = STAND;
	p.mv = -1; p.fuq = -1; for (int k = 0; k < 8; k++) { p.age[k] = 99; }
	p.d66 = 99; p.d44 = 99; p.hitHi = 1; p.red = CH_HP; p.rq = 99; p.cxSeq = -1; p.lastSeq = -1; p.ac = AC_NONE;
}

void newMatch(State& s, bool training, int rounds)
{
	ready(); std::memset(&s, 0, sizeof(s));
	s.phase = PH_INTRO; s.round = 1; s.time = 99 * 60; s.winner = -1; s.rw = -1; s.koType = KO_NONE; s.cfg.training = training ? 1 : 0; s.cfg.rounds = rounds > 0 ? rounds : 2;
	newFighter(s.p[0], 0, 0); newFighter(s.p[1], 1, 1);
	if (s.cfg.training) { s.phase = PH_FIGHT; s.p[0].sa = 3000; s.p[1].sa = 3000; }
}

static void resetRound(State& s)
{
	const int keep[2][2] = { { s.p[0].wins, s.p[0].sa }, { s.p[1].wins, s.p[1].sa } };
	newFighter(s.p[0], 0, 0); newFighter(s.p[1], 1, 1);
	for (int i = 0; i < 2; i++) { s.p[i].wins = keep[i][0]; s.p[i].sa = keep[i][1]; }
	s.nshots = 0; s.time = 99 * 60; s.phase = PH_INTRO; s.pt = 0; s.freeze = 0; s.slow = 0; s.koType = KO_NONE;
}

// ------------------------------------------------------------ input helpers
static inline int hdir(int inp, int face) { return (((inp & 8) ? 1 : 0) - ((inp & 4) ? 1 : 0)) * face; }
static inline int vdir(int inp) { return ((inp & 1) ? 1 : 0) - ((inp & 2) ? 1 : 0); }
static inline int npad(int inp, int face) { return 5 + hdir(inp, face) + 3 * vdir(inp); }
static bool motion(const Fighter& p, const int* seq, int len, int win)
{
	int k = len - 1; const int n = NBUF;
	for (int i = n - 1; i >= n - win && i >= 0; i--) { if (npad(p.buf[i], p.face) == seq[k]) { k--; if (k < 0) { return true; } } }
	return false;
}
enum { MO_236 = 236, MO_214 = 214, MO_623 = 623, MO_SUP = 2626 };
static bool motionOK(const Fighter& p, int m)
{
	static const int s236[3] = { 2, 3, 6 }, s214[3] = { 2, 1, 4 }, s623[3] = { 6, 2, 3 }, sup[4] = { 2, 6, 2, 6 };
	if (m == MO_236) { return motion(p, s236, 3, 15); }
	if (m == MO_214) { return motion(p, s214, 3, 15); }
	if (m == MO_623)
	{
		if (!motion(p, s623, 3, 20)) { return false; }
		for (int i = NBUF - 1; i >= NBUF - 12; i--) { const int d = npad(p.buf[i], p.face); if (d == 3 || d == 2 || d == 1) { return true; } if (d == 6) { return false; } }
		return false;
	}
	if (m == MO_SUP) { return motion(p, sup, 4, 34); }
	return false;
}
static bool dashIn(const Fighter& p, int dir)
{
	const int16_t* b = p.buf; const int n = NBUF;
	if (hdir(b[n - 1], p.face) != dir || vdir(b[n - 1]) != 0) { return false; }
	if (hdir(b[n - 2], p.face) != 0) { return false; }
	for (int i = n - 3; i >= n - 11; i--) { const int h = hdir(b[i], p.face); if (h == dir) { return true; } if (h != 0) { return false; } }
	return false;
}
static inline bool parryHeld(int inp) { return (inp & 1024) != 0; }

// ------------------------------------------------------------ drive gauge
static void burn(State& s, Fighter& p, int i) { if (p.dr <= 0 && !p.bo) { p.dr = 0; p.bo = 1; push(s, E_BURNOUT, i); } }
static bool spend(State& s, Fighter& p, int i, int cost, int pause)
{
	if (p.bo || p.dr <= 0) { return false; }
	p.dr -= cost; p.drT = imax(p.drT, pause ? pause : 120); if (p.dr <= 0) { p.dr = 0; burn(s, p, i); }
	return true;
}

// ------------------------------------------------------------ fighter logic
static inline const Move* cur(const Fighter& p) { return p.st == ATK ? &MV(p.mv) : nullptr; }
static inline void clearAges(Fighter& p) { for (int k = 0; k < 8; k++) { p.age[k] = 99; } }
static void toIdle(Fighter& p) { p.st = IDLE; p.sf = 0; p.mv = -1; p.vx = 0; p.sub = STAND; p.air2 = 0; p.arm = 0; }

static bool start(State& s, Fighter& p, int key, int i, int optMod = 0, int optAuto = 0)
{
	if (key == M_SA3 && p.hp * 4 <= CH_HP) { key = M_CA; }
	const Move& m = MV(key); const bool fromDr = p.st == DRUSH; const Move* pm = p.st == ATK ? &MV(p.mv) : nullptr; const Fighter& o = s.p[1 - i];
	if (m.sup || (pm && pm->thru)) { if (o.x > p.x) { p.face = 1; } else if (o.x < p.x) { p.face = -1; } }
	p.st = ATK; p.mv = int16_t(key); p.mseq++; p.mf = 0; p.sf = 0; p.hitMask = 0; p.contact = 0; p.hitOK = 0; p.arm = m.arm; p.armHit = 0;
	p.drb = fromDr && !m.sp && !m.sup && !m.thr ? 1 : 0; if (fromDr) { p.pv = p.face * 480; }
	p.mod = optMod ? 1 : 0; p.fuq = -1; p.part = 0; p.ca = m.ca ? 1 : 0; p.fr = 0; p.hmE = 0; if (m.has_home) { p.hm = 1; } if (key==M_ABEAM) { p.airBeamUsed=1; } if (!optAuto) { p.ac = AC_NONE; p.aci = 0; }
	if (!m.isAir) { p.vx = 0; if (p.y > 0) { p.y = 0; p.vy = 0; p.air2 = 0; p.jg = 0; } }
	if (m.sa) { p.sa -= m.sa * 1000; } else if (m.sp) { p.sa = imin(3000, p.sa + 10); }
	if (m.frz) { s.freeze = m.frz; s.frzWho = i; push(s, E_SUPER, i, 0, 0, m.sa, p.ca, key); }
	else if (m.od) { push(s, E_EX, i); }
	else if (m.di) { push(s, E_DI, i); }
	if (m.sp || m.sup) { push(s, E_SPECIAL, i, 0, 0, 0, 0, key); }
	clearAges(p);
	return true;
}

static int pick(char b, int h, int v, bool as)
{
	if (b == 'L') { return v < 0 ? M_2LK : as ? M_5LK : M_5LP; }
	if (b == 'M') { return v < 0 ? M_2MK : as ? M_5MP : M_5MK; }
	return v < 0 ? (h > 0 ? M_2HK : M_2HP) : as ? M_5HP : M_5HK;
}

static void enterRun(State& s, Fighter& p, int i, bool keepAuto);

static bool tryAttack(State& s, Fighter& p, Fighter& o, int inp, int i)
{
	(void)o;
	const int* A = p.age; const bool air = p.y > 0;
	const bool lt = A[0] <= BUF, md = A[1] <= BUF, hv = A[2] <= BUF, spb = A[3] <= BUF, dib = A[5] <= BUF, hmb = A[7] <= BUF;
	const Move* m = cur(p);
	if (!(lt || md || hv || spb || dib || hmb) && !(m && m->has_tc && p.contact)) { return false; }
	bool canSp = true, canSu = true, chain = false, kara = false, canN = true, ok = false;
	if (m)
	{
		ok = p.contact && p.mf <= m->lastAct + 10;
		kara = !m->sp && !m->sup && !m->thr && !m->di && p.mf <= 2 && !p.contact;
		canSp = (ok && (m->can & C_SP)) || kara || (ok && m->isAir && !m->sp && p.hitOK);   // any air normal that hits cancels into an air special
		canSu = (ok && (m->can & C_SU)) || kara || (m->has_fc && p.mf >= m->fc[0] && p.mf <= m->fc[1] && (m->can & C_SU));
		chain = ok && (m->can & C_CH) && m->has_chain; canN = false;
		if (m->sup || m->has_home) { return false; }
	}
	const int bi = spb ? 3 : hv ? 2 : md ? 1 : 0, pin = (p.pd[bi] & 15) | (inp & 256) | (p.pd[bi] & 256);
	const int h = hdir(pin, p.face), v = vdir(pin); const bool as = (pin & 256) != 0;
	bool myProj = false; for (int q = 0; q < s.nshots; q++) { if (s.shot[q].o == i) { myProj = true; } }
	if (m && ok && m->has_tc && p.rq > 12)
	{
		int k = -1;
		if (A[1] <= 12 && m->tc[1] >= 0) { k = m->tc[1]; } else if (A[0] <= 12 && m->tc[0] >= 0) { k = m->tc[0]; } else if (A[2] <= 12 && m->tc[2] >= 0) { k = m->tc[2]; }
		if (k >= 0) { return start(s, p, k, i); }
	}
	if (canSu && !air)
	{
		int lv = 0, mod = 0;
		if (motionOK(p, MO_SUP) && (lt || md || hv)) { lv = hv ? 3 : md ? 2 : 1; }
		else if (spb && (inp & 64) && A[2] <= BUF + 2) { lv = v < 0 ? 3 : h < 0 ? 2 : 1; mod = 1; }
		else if (hv && (inp & 128) && A[3] <= BUF + 2) { lv = v < 0 ? 3 : h < 0 ? 2 : 1; mod = 1; }
		if (lv && p.sa >= lv * 1000) { return start(s, p, lv == 1 ? M_SA1 : lv == 2 ? M_SA2 : M_SA3, i, mod); }
		if (lv && mod) { return false; }
	}
	if (dib && !air && (canN || kara) && !p.bo && p.dr > 0) { spend(s, p, i, BAR, 120); return start(s, p, M_DI, i); }
	// the homing button: Slipstream from anywhere, whatever direction is held. Assist makes it the Overdrive.
	if (hmb && !p.hm && canSp && !(m && m->sp && !kara))
	{
		const bool od = (inp & 256) && !p.bo && p.dr > 0; if (od) { spend(s, p, i, 2 * BAR, 120); }
		return start(s, p, air ? (od ? M_AHOME_OD : M_AHOME) : (od ? M_HOME_OD : M_HOME), i, 1);
	}
	if (canSp && !(m && m->sp && !kara))
	{
		const int nAtk = ((inp & 16) ? 1 : 0) + ((inp & 32) ? 1 : 0) + ((inp & 64) ? 1 : 0); const bool canOD = !p.bo && p.dr > 0;
		if (lt || md || hv)
		{
			for (const MoDef& sp : MO_LIST)
			{
				if ((sp.air != 0) != air || (sp.fwd && p.jd <= 0) || !motionOK(p, sp.motion)) { continue; }
				if (nAtk >= 2 && canOD) { spend(s, p, i, 2 * BAR, 120); return start(s, p, sp.OD, i); }
				return start(s, p, hv ? sp.H : md ? sp.M : sp.L, i);
			}
		}
		if (spb)
		{
			const int d = v < 0 ? 2 : (air && v > 0) ? 8 : h > 0 ? 6 : h < 0 ? 4 : 5;
			for (const SpDef& sp : SP_LIST)
			{
				if ((sp.air != 0) != air || (!air && sp.dir != d) || (air && sp.adir && sp.adir != d) || (sp.fwd && p.jd <= 0) || (MV(sp.k).has_home && p.hm)) { continue; }
				if (as && canOD && sp.od != sp.k && !(MV(sp.od).has_proj && myProj)) { spend(s, p, i, 2 * BAR, 120); return start(s, p, sp.od, i, 1); }
				if ((MV(sp.k).has_proj && myProj) || (sp.k==M_ABEAM && p.airBeamUsed)) { continue; }
				return start(s, p, sp.k, i, 1);
			}
		}
	}
	if (!(lt || md || hv)) { return false; }
	if (air)
	{
		if (m && !(m->isAir && p.contact && p.aa < 3)) { return false; }
		if (m) { p.aa = p.aa + 1; }
		const int k = as ? (hv ? M_jHP : md ? M_jMK : M_jLK) : (hv ? M_jHK : md ? M_jMP : M_jLP);
		return start(s, p, k, i);
	}
	if ((canN || kara) && (inp & 16) && (inp & 32) && A[0] <= BUF + 2 && A[1] <= BUF + 2) { p.back = h < 0 ? 1 : 0; return start(s, p, M_throw, i); }
	const char b = hv ? 'H' : md ? 'M' : 'L'; const int nb = hv ? 2 : md ? 1 : 0, hN = hdir(p.pd[nb], p.face), vN = vdir(p.pd[nb]), k = pick(b, hN, vN, as);
	if (canN) { if (as) { p.ac = b == 'L' ? AC_L : b == 'M' ? AC_M : AC_H; p.aci = 0; return start(s, p, k, i, 0, 1); } return start(s, p, k, i); }
	if (chain && b == 'L') { const int k2 = vN < 0 ? m->chain2 : m->chain5; if (k2 >= 0) { return start(s, p, k2, i); } }
	return false;
}

// Assist combo: one buffered press advances to the next step as soon as that step is legal.
static bool autoStep(State& s, Fighter& p, Fighter& o, int i)
{
	(void)o;
	const AutoList* list = p.ac != AC_NONE ? &AUTO[p.ac - 1] : nullptr;
	const AutoStep* stp = (list && p.aci + 1 >= 0 && p.aci + 1 < list->n) ? &list->s[p.aci + 1] : nullptr;
	const Move* m = cur(p); const bool inRun = p.st == RUN;
	if (!stp || (!m && !inRun)) { p.acq = 0; return false; }
	for (int n = 0; n < stp->n; n++)
	{
		const int k = stp->k[n];
		if ((k == A_RUN || k == M_KASAI) && !p.hitOK) { if (!m || p.mf > m->lastAct + 12) { p.acq = 0; } return false; }
		if (k == A_RUN)
		{
			if (m && p.contact && p.mf <= m->lastAct + 12) { p.aci++; p.acq = 0; enterRun(s, p, i, true); return true; }
			if (m && p.mf > m->lastAct + 12) { p.acq = 0; }
			return false;
		}
		const Move& mm = MV(k);
		if (mm.sa && p.sa < mm.sa * 1000) { continue; }
		if (mm.od && (p.bo || p.dr <= 0)) { continue; }
		bool legal;
		if (inRun) { legal = p.sf >= 11; }
		else if (m->has_fu)
		{
			bool has = false; for (int q = 0; q < 6; q++) { if (m->fuTo[q] == k) { has = true; } }
			if (!has) { continue; }
			if (p.mf > m->fuw[1]) { p.acq = 0; return false; }
			legal = p.mf >= m->fuw[0];
		}
		else
		{
			if (!((mm.sup && (m->can & C_SU)) || (mm.sp && !m->sp && !mm.sup) || (m->has_tc && (m->tc[1] == k || m->tc[0] == k || m->tc[2] == k)) || (m->has_chain && !mm.sp && !mm.sup) || (k == M_SRK_H && p.mv == M_QDL))) { continue; }
			if (p.mf > m->lastAct + 12 && !(m->thru && p.y > 0)) { p.acq = 0; return false; }
			legal = p.contact && (!(mm.sup || m->sp) || m->nhits < 2 || p.mf > m->hits[m->nhits - 1].s) && !(m->thru && p.y > 14 * F);
		}
		if (!legal) { return false; }
		if (mm.od) { spend(s, p, i, 2 * BAR, 120); }
		p.aci++; p.acq = 0; start(s, p, k, i, 0, 1); return true;
	}
	p.acq = 0; return false;
}

static void autoPress(Fighter& p, int inp)
{
	if (!(inp & 256)) { return; }
	if (p.ac == AC_NONE) { if (p.st == ATK && p.contact && p.age[0] <= BUF && (p.mv == M_5LP || p.mv == M_2LK)) { p.ac = AC_LC; p.aci = -1; p.age[0] = 99; p.acq = 1; } return; }
	const int b = p.ac == AC_L || p.ac == AC_LC ? 0 : p.ac == AC_M ? 1 : 2;
	if (p.age[b] <= BUF) { p.age[b] = 99; p.acq = 1; }
}

static bool fuKey(const Fighter& p, const Move& m, int& k, int& mod)
{
	const int h = hdir(p.pd[3], p.face), v = vdir(p.pd[3]);
	if (p.age[3] <= BUF) { k = m.fuTo[v < 0 ? 1 : h < 0 ? 0 : 2]; mod = 1; return true; }
	for (int b = 0; b < 3; b++) { if (p.age[b] <= BUF && hdir(p.pd[b], p.face) > 0) { k = m.fuTo[3 + b]; mod = 0; return true; } }
	return false;
}
static inline bool runHeld(const Fighter& p) { return p.rq <= BUF; }
static bool enterParry(State& s, Fighter& p, int i)
{
	if (!spend(s, p, i, BAR / 2, 240)) { return false; }
	p.st = PARRY; p.sf = 0; p.mv = -1; p.vx = 0; p.plock = 0; p.pok = 0; p.arm = 0; push(s, E_PARRYON, i); return true;
}
static void enterRush(State& s, Fighter& p, Fighter& o, int i, bool fromCancel)
{
	p.st = DRUSH; p.sf = 0; p.rcn = fromCancel ? 1 : 0; p.mv = -1; p.vx = 0; p.d66 = 99; p.arm = 0; p.drT = fromCancel ? 120 : 240; if (fromCancel && o.combo > 0) { o.drs = 1; }
	s.freeze = fromCancel ? 9 : 10; s.frzWho = i; push(s, E_RUSH, i);
}
static void enterRun(State& s, Fighter& p, int i, bool keepAuto)
{
	p.st = RUN; p.rq = 99; p.sf = 0; p.mv = -1; p.arm = 0; clearAges(p); if (!keepAuto) { p.ac = AC_NONE; p.aci = 0; p.acq = 0; } push(s, E_RUN, i);
}

static void neutral(State& s, Fighter& p, Fighter& o, int inp, int i)
{
	p.st = IDLE; p.mv = -1;
	if (o.x > p.x) { p.face = 1; } else if (o.x < p.x) { p.face = -1; }
	if (runHeld(p)) { enterRun(s, p, i, false); return; }
	if (tryAttack(s, p, o, inp, i)) { return; }
	const int h = hdir(inp, p.face), v = vdir(inp);
	if (parryHeld(inp) && enterParry(s, p, i)) { return; }
	if (p.d66 <= BUF) { p.d66 = 99; p.st = DASHF; p.sf = 0; push(s, E_DASH, i); return; }
	if (p.d44 <= BUF) { p.d44 = 99; p.st = DASHB; p.sf = 0; push(s, E_DASH, i); return; }
	int sub;
	if (v > 0) { p.st = PREJUMP; p.sf = 0; p.jd = h; p.vx = 0; return; }
	if (v < 0) { sub = CROUCH; p.vx = 0; }
	else if (h > 0) { sub = WALKF; p.vx = p.face * CH_WALKF; }
	else if (h < 0) { sub = WALKB; p.vx = -p.face * CH_WALKB; }
	else { sub = STAND; p.vx = 0; }
	if (sub != p.sub) { p.sub = int8_t(sub); p.subf = 0; } else { p.subf++; }
}

static void atkFrame(State& s, Fighter& p, Fighter& o, const Move& m, int i)
{
	const int f = p.mf;
	if (!m.isAir || m.sp)
	{
		int vx = 0; bool on = false;
		if (m.has_mv && !(m.stopOnHit && p.contact)) { if (f >= m.mv[0] && f <= m.mv[1]) { vx = m.mv[2]; on = true; } }
		if (p.y <= 0 && !p.air2) { p.vx = p.face * vx; } else if (on) { p.vx = p.face * vx; }
	}
	if (m.hover && f < m.hover) { p.vx = 0; p.vy = GRAV; }
	if (m.has_home && f >= m.home[0] && !p.contact && !p.hmE)
	{
		if (f == m.home[0]) { if (p.y <= 0) { p.y = 1; } p.air2 = 1; p.jg = 0; push(s, E_HOME, i, p.x, p.y, m.od ? 1 : 0); }
		if (f <= m.home[1])
		{
			const int64_t dx = o.x - p.x, dy = (o.y > 0 ? o.y : 14 * F) - p.y; const int64_t len = int64_t(std::sqrt(double(dx * dx + dy * dy))), sp = m.home[2];
			if (dx > 20 * F) { p.face = 1; } else if (dx < -20 * F) { p.face = -1; }
			if (len > sp) { p.vx = int(dx * sp / len); p.vy = int(dy * sp / len) + GRAV; }
			else { p.hmE = 1; p.vx = p.vx / 3; }   // arrived and nothing to hit (they were invulnerable): the flight is over
		}
		else { p.hmE = 1; p.vx = p.vx / 3; }
	}
	if (m.has_rise && f == m.rise[0] && !(m.has_wEnd && !p.hitOK)) { p.jg = 0; p.vx = p.face * m.rise[1]; p.vy = m.rise[2]; if (p.y <= 0) { p.y = 1; } p.air2 = 1; push(s, E_RISE, i); }
	if (m.has_proj && f == m.pf)
	{
		if (s.nshots < MAX_SHOTS) { Shot& q = s.shot[s.nshots++]; q.o = i; q.ch = p.ch; q.k = p.mv; q.x = p.x + p.face * 30 * F; q.y = p.y + m.py * F; q.dir = p.face; q.left = m.phits; q.cd = 0; q.pause = 0; q.life = 0; q.mod = p.mod; }
		push(s, E_FIRE, i, 0, 0, 0);
	}
	if (m.has_wEnd && f == m.wEnd[0] && !p.hitOK) { if (m.wEnd[1] < 0) { p.st = LAND; p.sf = 0; p.rec = -m.wEnd[1]; p.mv = -1; p.vx = 0; return; } p.mf = m.wEnd[1]; }
	if (m.arm && f > m.lastAct) { p.arm = 0; }
	if (m.partAt && f == m.partAt && !p.hitOK) { p.part = 1; }
	for (int n = 0; n < m.nhits; n++) { const Hit& h = m.hits[n]; if (f == h.s && !(h.req && !p.hitOK)) { push(s, E_WHIFF, i, 0, 0, h.lvl); } }
	if (m.thr && f == 4) { push(s, E_WHIFF, i, 0, 0, 0); }
}

static void burnOther(State& s, Fighter& o, int oi) { burn(s, o, oi); }

static void update(State& s, Fighter& p, Fighter& o, int inp, int i)
{
	p.sf++; if (p.tinv > 0) { p.tinv--; } if (p.pp > 0) { p.pp--; }
	switch (p.st)
	{
		case IDLE: neutral(s, p, o, inp, i); break;
		case PREJUMP:
			if (p.sf >= 4) { p.st = JUMP; p.sf = 0; p.dj = 0; p.wj = 0; p.aa = 0; p.jg = 1; p.vy = 1125; p.vx = p.jd * p.face * (p.jd < 0 ? CH_JXB : CH_JX); p.y = 1; push(s, E_JUMP, i); }
			break;
		case JUMP:
		{
			// steer in the air; Up again for one weak second jump; Up against the wall to kick off it
			const int pv = p.buf[NBUF - 2]; const bool upNew = (inp & 1) && !(pv & 1); const int hx = ((inp & 8) ? 1 : 0) - ((inp & 4) ? 1 : 0), lim = STAGE - PW;
			if ((inp & 2) && p.vy < 0 && p.vy > -1700) { p.vy -= 55; }   // hold down on the way down to fall faster
			if (hx) { p.vx += hx * 22; const int mx = CH_JX * 11 / 10; if (p.vx > mx) { p.vx = mx; } if (p.vx < -mx) { p.vx = -mx; } }
			if (upNew && p.sf > 5 && !(inp & 240))
			{
				const int wall = p.x >= lim - 2 * F ? 1 : p.x <= -lim + 2 * F ? -1 : 0;
				if (wall && !p.wj && p.y > 10 * F) { p.wj = 1; p.vx = -wall * 330; p.vy = 1000; p.jg = 1; p.sf = 0; p.jd = -wall * p.face; push(s, E_WALLJUMP, i, p.x + wall * PW, p.y); }
				else if (!p.dj) { p.dj = 1; p.vy = 800; p.vx = hx * (CH_JX * 8 / 10); p.jg = 1; p.sf = 0; if (hx) { p.jd = hx * p.face; } push(s, E_JUMP, i, 0, 0, 1); }
			}
			tryAttack(s, p, o, inp, i); break;
		}
		case LAND: p.vx = 0; if (p.sf >= p.rec) { toIdle(p); neutral(s, p, o, inp, i); } break;
		case DASHF: p.vx = p.face * (p.sf <= 12 ? 490 : 85); if (p.sf >= 19) { toIdle(p); neutral(s, p, o, inp, i); } break;
		case DASHB: p.vx = -p.face * (p.sf <= 14 ? 290 : 51); if (p.sf >= 23) { toIdle(p); neutral(s, p, o, inp, i); } break;
		case DRUSH:
		{
			// builds speed out of the freeze, holds it, then bleeds off
			const int f = p.sf; p.vx = p.face * (f <= 9 ? 250 + f * 35 : f <= 24 ? 557 : imax(0, 557 - ((f - 24) * 53 / 2)));
			if (f >= (p.rcn ? 10 : 9) && tryAttack(s, p, o, inp, i)) { atkFrame(s, p, o, MV(p.mv), i); break; }
			if (f >= (p.rcn ? 25 : 24) && (hdir(inp, p.face) < 0 || vdir(inp) != 0 || parryHeld(inp) || runHeld(p))) { p.pv = p.vx / 2; toIdle(p); neutral(s, p, o, inp, i); break; }
			if (f >= (p.rcn ? 46 : 45)) { p.pv = p.vx / 2; toIdle(p); neutral(s, p, o, inp, i); }
			break;
		}
		case RUN:
		{
			const int f = p.sf; p.vx = p.face * (CH_RUNV * (f <= 8 ? 30 + f * 9 : f > 35 ? imax(20, 100 - (f - 35) * 8) : 100) / 100);
			autoPress(p, inp); if (p.acq && autoStep(s, p, o, i)) { atkFrame(s, p, o, MV(p.mv), i); break; }
			int k = -1; bool stop = false;
			if (f >= 9 && p.age[2] <= BUF) { k = RUN_H; }
			else if (f >= 11)
			{
				if (p.age[3] <= BUF)
				{
					const int h = hdir(p.pd[3], p.face), v = vdir(p.pd[3]); k = v < 0 ? RUN_S2 : h < 0 ? RUN_S4 : h > 0 ? RUN_S6 : RUN_S5;
					start(s, p, k, i, 1); atkFrame(s, p, o, MV(k), i); break;
				}
				else if (p.age[7] <= BUF && !p.hm)
				{
					const bool od = (inp & 256) && !p.bo && p.dr > 0; if (od) { spend(s, p, i, 2 * BAR, 120); } k = od ? M_HOME_OD : M_HOME;
					start(s, p, k, i, 1); atkFrame(s, p, o, MV(k), i); break;
				}
				else if (p.age[0] <= BUF || p.age[1] <= BUF)
				{
					if (motionOK(p, MO_623)) { k = RUN_623; } else if (motionOK(p, MO_214)) { k = RUN_214; } else if (p.age[1] <= BUF) { k = RUN_M; } else { stop = true; }
				}
			}
			if (stop) { p.st = LAND; p.sf = 0; p.rec = 16; p.vx = 0; p.pv = p.face * 180; clearAges(p); break; }
			if (k >= 0) { start(s, p, k, i); atkFrame(s, p, o, MV(k), i); break; }
			if (f >= 45) { toIdle(p); neutral(s, p, o, inp, i); }
			break;
		}
		case PARRY:
			p.vx = 0; p.dr -= 9; if (p.drT < 20) { p.drT = 20; }
			if (p.dr <= 0) { p.dr = 0; burn(s, p, i); p.st = PREC; p.sf = 0; break; }
			if (p.plock > 0) { p.plock--; break; }
			if (p.sf >= 4 && p.d66 <= BUF && p.dr > 0) { spend(s, p, i, BAR / 2, 240); enterRush(s, p, o, i, false); break; }
			if (!parryHeld(inp) && p.sf >= 12) { if (p.pok) { toIdle(p); neutral(s, p, o, inp, i); } else { p.st = PREC; p.sf = 0; } }
			break;
		case PREC: if (p.sf >= 33) { toIdle(p); neutral(s, p, o, inp, i); } break;
		case ATK:
		{
			if (p.fr)
			{
				if (p.fr == 1) { p.vy = 900; p.vx = p.face * 30; p.dj = 0; push(s, E_JUMP, i, 0, 0, 1); }
				p.fr = 0; p.st = JUMP; p.mv = -1; p.sf = 10; p.jg = 1; p.jd = 1; p.air2 = 0; p.aa = 0; tryAttack(s, p, o, inp, i); break;
			}
			p.mf++; const Move& m = MV(p.mv);
			if (m.isAir && p.y > 0)
			{
				const int pv = p.buf[NBUF - 2], hx = ((inp & 8) ? 1 : 0) - ((inp & 4) ? 1 : 0);
				if (!m.sp && hx) { p.vx += hx * 16; const int mx = CH_JX * 11 / 10; if (p.vx > mx) { p.vx = mx; } if (p.vx < -mx) { p.vx = -mx; } }
				if ((inp & 1) && !(pv & 1) && p.contact && !p.dj && !m.has_home) { p.dj = 1; p.vy = 800; p.vx = hx * (CH_JX * 8 / 10); p.jg = 1; p.st = JUMP; p.mv = -1; p.sf = 0; push(s, E_JUMP, i, 0, 0, 1); break; }
			}
			if (m.t < 900 && p.mf >= m.t + (m.wr && !p.contact ? m.wr : 0)) { toIdle(p); neutral(s, p, o, inp, i); break; }
			autoPress(p, inp); if (p.acq && autoStep(s, p, o, i)) { if (p.st == ATK) { atkFrame(s, p, o, MV(p.mv), i); } break; }
			if (m.has_fu && p.mf <= m.fuw[1])
			{
				if (p.fuq < 0 && p.mf >= m.su - 1) { int k = -1, md = 0; if (fuKey(p, m, k, md) && k >= 0) { p.fuq = int16_t(k); p.fum = md; clearAges(p); } }
				if (p.fuq >= 0 && p.mf >= m.fuw[0]) { const int k = p.fuq; start(s, p, k, i, p.fum, 1); atkFrame(s, p, o, MV(k), i); break; }
			}
			const bool live = p.contact && p.mf <= m.lastAct + 10, kara = !m.sp && !m.sup && !m.thr && !m.di && p.mf <= 2 && !p.contact, ground = p.y <= 0;
			if (ground && ((live && (m.can & C_SP) && !m.sp) || kara) && runHeld(p)) { enterRun(s, p, i, false); break; }
			if (live && (m.can & C_SP) && !m.sp && ground && !p.bo && p.dr > 0 && (p.d66 <= BUF || p.age[6] <= BUF)) { spend(s, p, i, 3 * BAR, 120); clearAges(p); enterRush(s, p, o, i, true); break; }
			if (kara && ground && parryHeld(inp) && enterParry(s, p, i)) { break; }
			if (!tryAttack(s, p, o, inp, i)) { atkFrame(s, p, o, m, i); } else { atkFrame(s, p, o, MV(p.mv), i); }
			break;
		}
		case BLOCK:
			if (p.age[5] <= BUF && hdir(p.pd[5], p.face) > 0 && !p.bo && p.dr > 0 && p.y <= 0) { spend(s, p, i, 2 * BAR, 120); p.combo = 0; start(s, p, M_DREV, i); push(s, E_EX, i); atkFrame(s, p, o, MV(M_DREV), i); break; }
			p.stun--; if (p.stun <= 0) { p.combo = 0; p.tinv = 2; toIdle(p); neutral(s, p, o, inp, i); } break;
		case HIT:
			p.stun--;
			if (p.stun <= 0)
			{
				if (p.crum) { p.crum = 0; p.st = KDOWN; p.sf = 0; p.vx = 0; push(s, E_KD, i, p.x, 0); }
				else { p.combo = 0; p.half = 0; p.tinv = 2; toIdle(p); neutral(s, p, o, inp, i); }
			}
			break;
		case KDOWN: p.vx = p.roll && p.sf > 8 ? -p.face * 420 : 0; if (p.sf >= KD && p.hp > 0) { p.st = GETUP_; p.sf = 0; p.combo = 0; p.half = 0; } break;
		case GETUP_: p.vx = p.roll && p.sf < 10 ? -p.face * 300 : 0; if (p.sf >= GETUP) { p.tinv = 1; toIdle(p); neutral(s, p, o, inp, i); } break;
		case TECH: if (p.sf >= 24) { toIdle(p); neutral(s, p, o, inp, i); } break;
		case THROWING:
		{
            if(p.tk==5) {
                // Confirmed super: both participants share this clock. No input can restart the latch.
                ++p.tf;
                if(o.st==THROWN){o.x=p.x+p.face*24*F;o.y=p.y;o.vx=o.vy=o.pv=0;}
                if(p.tf==60 && o.st==THROWN){
                    const int damage=p.pmT;
                    o.hp=imax(s.cfg.training?1:0,o.hp-damage);o.combo++;o.comboDmg+=damage;o.redT=50;
                    o.st=AHIT;o.sf=0;o.y=imax(1,p.y+20*F);o.vx=p.face*900;o.vy=900;o.hard=1;o.jug=3;
                    o.hs=p.hs=18;o.wbPend=1;o.gbPend=0;
                    push(s,E_HIT,i,o.x,o.y+45*F,2,damage,o.combo,o.comboDmg,0,H_SPECIAL|H_FINISH|(o.hp<=0?H_KO:0));
                }
                if(p.tf>=76){p.tk=0;p.pmT=0;p.y=imax(1,p.y);p.vx=-p.face*180;p.vy=420;p.st=JUMP;p.sf=0;p.mv=-1;}
                break;
            }
			const int k = o.noTech ? 17 : 10;
			if (!p.tk)
			{
				if (o.st == THROWN) { o.x = p.x + p.face * 30 * F; o.y = 0; o.vx = 0; o.vy = 0; }
				if (p.sf < 9 && o.st == THROWN && (o.prev & 16) && (o.prev & 32) && o.age[0] <= 10 && o.age[1] <= 10 && !o.noTech)
				{
					p.st = TECH; o.st = TECH; p.sf = 0; o.sf = 0; o.y = 0; p.pv = -p.face * 620; o.pv = p.face * 620; push(s, E_TECH, -1, (p.x + o.x) >> 1, 60 * F); break;
				}
				// no hold and no pummel: the grab is the throw. Forward unless a direction is given in the first frames, and the first one given sticks
				// (back throws him behind, down slams him so he bounces up over the thrower, up launches him)
				{
					const int fw = inp & (p.face > 0 ? 8 : 4), bw = inp & (p.face > 0 ? 4 : 8);
					if (!p.pmN) { p.pmN = bw ? 2 : (inp & 2) ? 4 : (inp & 1) ? 3 : fw ? 1 : 0; }
					if (p.sf >= 9) { p.tk = p.pmN ? p.pmN : 1; p.tf = 0; p.back = p.tk == 2 ? 1 : 0; }
				}
			}
			else
			{
				p.tf++; const int side = p.tk == 2 && p.tf >= 5 ? -1 : 1;
				if (o.st == THROWN) { o.x = p.x + p.face * side * 30 * F; o.y = p.tk == 4 ? 0 : imin(p.tf, 10) * 3 * F; o.vx = 0; o.vy = 0; }
				if (p.tf == 24 && o.st == THROWN)
				{
					static const int V[5][2] = { { 0, 0 }, { 430, 720 }, { 430, 720 }, { 40, 1480 }, { -70, 1260 } };
					const int d = 120 * k / 10;
					o.hp = imax(s.cfg.training ? 1 : 0, o.hp - d); o.st = AHIT; o.sf = 0; o.y = p.tk == 4 || p.tk==2 ? 1 : 60 * F; o.x=p.x+p.face*side*45*F; o.vx = p.face * side * V[p.tk][0]; o.vy = V[p.tk][1]; o.combo = 1; o.comboDmg = d;
					o.jug = p.tk >= 3 ? 0 : 3; o.hard = p.tk >= 3 ? 0 : 1; o.hs = 8; p.hs = 8; o.redT = 50; o.gbUsed = 0; o.wbUsed = 0;
					p.sa = imin(3000, p.sa + 200); o.sa = imin(3000, o.sa + 40); if (!p.bo) { p.dr = imin(DMAX, p.dr + 200); } if (o.noTech && !o.bo) { o.dr -= 1000; burnOther(s, o, 1 - i); }
					push(s, E_HIT, i, o.x, 50 * F, 2, d, 1, d, o.noTech ? 2 : 0, H_THROW); if (p.tk == 4) { push(s, E_GBOUNCE, 1 - i, o.x, 0); }
				}
				if (p.tf >= 44) { p.tk = 0; toIdle(p); }
			}
			break;
		}
		case AHIT: { const int hx = ((inp & 8) ? 1 : 0) - ((inp & 4) ? 1 : 0); if (hx && p.y > 0) { p.vx += hx * 9; if (p.vx > 560) { p.vx = 560; } if (p.vx < -560) { p.vx = -560; } } break; }
		default: break;
	}
}

static void land(State& s, Fighter& p, int i)
{
	p.dj = 0; p.wj = 0; p.aa = 0; p.hm = 0; p.airBeamUsed = 0; p.fr = 0;
	p.y = 0; p.vy = 0; p.air2 = 0; p.jg = 0;
	if (p.st == AHIT)
	{
		if (p.gbPend && !p.gbUsed && p.hp > 0) { p.gbUsed = 1; p.gbPend = 0; p.y = 1; p.vy = 880; p.vx = p.vx / 3; p.hs = 5; push(s, E_GBOUNCE, i, p.x, 0); return; }
		p.st = KDOWN; p.sf = 0; p.pv = p.vx / 2; p.vx = 0; p.roll = (!p.hard && p.hp > 0 && hdir(p.prev, p.face) < 0) ? 1 : 0; push(s, E_KD, i, p.x, 0);
	}
	else if (p.st == ATK) { const Move& m = MV(p.mv); p.st = LAND; p.sf = 0; p.rec = m.landRec ? m.landRec : 3; p.mv = -1; p.vx = 0; p.arm = 0; push(s, E_LAND, i); }
	else if (p.st == JUMP) { p.st = LAND; p.sf = 0; p.rec = 2; p.vx = 0; push(s, E_LAND, i); }
}

static void physics(State& s, Fighter& p, Fighter& o, int i)
{
	if (p.st == THROWN || (p.st == THROWING && p.tk==5)) { return; }
	const int lim = STAGE - PW;
	if (p.y > 0)
	{
		const int ag = p.st == ATK ? MV(p.mv).ag : 0;
		p.vy -= (ag ? ag : p.jg && p.st != AHIT ? JG : GRAV) + (p.st == AHIT ? p.jug * 3 : 0);
		p.x += p.vx; p.y += p.vy;
		if (p.y <= 0) { land(s, p, i); }
	}
	else if (p.st == AHIT) { land(s, p, i); }
	else { p.x += p.vx + p.pv; p.pv = p.pv * 86 / 100; if (p.pv > -12 && p.pv < 12) { p.pv = 0; } }
	if (p.x > lim || p.x < -lim)
	{
		const int sg = p.x > 0 ? 1 : -1; p.x = sg * lim;
		if (p.st == AHIT && p.wbPend && !p.wbUsed && p.vx * sg >= 260 && p.hp > 0) { p.wbUsed = 1; p.wbPend = 0; p.vx = -sg * 260; p.vy = 820; p.hs = 9; push(s, E_WBOUNCE, i, p.x + sg * PW, p.y + 45 * F); }
		else if (p.y <= 0 && p.pv * sg > 0 && (p.st == HIT || p.st == BLOCK) && o.y <= 0) { o.pv = -p.pv; p.pv = 0; }
	}
}

static bool thruBox(const Fighter& p, const Fighter& q)
{
	if (p.st != ATK) { return false; }
	const Move& m = MV(p.mv);
	return (m.thru && q.st == AHIT) || (m.has_home && p.y > 0 && (q.st == KDOWN || q.st == GETUP_));
}
static void pushboxes(State& s)
{
	Fighter& a = s.p[0]; Fighter& b = s.p[1]; const int lim = STAGE - PW;
	const bool skip = a.st == THROWN || b.st == THROWN || a.st == THROWING || b.st == THROWING || thruBox(a, b) || thruBox(b, a);
	if (!skip && a.y < b.y + 70 * F && b.y < a.y + 70 * F)
	{
		const int dx = b.x - a.x, ov = 2 * PW - iabs(dx);
		if (ov > 0)
		{
			const int dir = dx > 0 ? 1 : dx < 0 ? -1 : (a.face > 0 ? 1 : -1);
			const int h = ov >> 1; a.x -= dir * h; b.x += dir * (ov - h);
			if (a.x < -lim) { b.x += -lim - a.x; a.x = -lim; } if (a.x > lim) { b.x -= a.x - lim; a.x = lim; }
			if (b.x < -lim) { a.x += -lim - b.x; b.x = -lim; } if (b.x > lim) { a.x -= b.x - lim; b.x = lim; }
		}
	}
	for (int i = 0; i < 2; i++) { Fighter& p = s.p[i]; if (p.x > lim) { p.x = lim; } if (p.x < -lim) { p.x = -lim; } }
	const int d = b.x - a.x; if (iabs(d) > SEP) { const int ex = iabs(d) - SEP, sg = d > 0 ? 1 : -1, h = ex >> 1; a.x += sg * h; b.x -= sg * (ex - h); }
}

// ------------------------------------------------------------ boxes
bool crouching(const Fighter& p)
{
	if (p.st == IDLE) { return p.sub == CROUCH; }
	if (p.st == ATK) { return MV(p.mv).crouch != 0; }
	if (p.st == BLOCK || p.st == HIT) { return p.cb != 0; }
	return false;
}
bool invul(const Fighter& p)
{
	if (p.st == KDOWN && p.hp <= 0) { return false; }   // a knocked-out body can still be hit
	if (p.st == KDOWN || p.st == GETUP_ || p.st == THROWN || p.st == THROWING || p.st == WIN) { return true; }
	if (p.st == ATK) { const Move& m = MV(p.mv); if (m.has_inv && p.mf >= m.inv[0] && p.mf <= m.inv[1]) { return true; } }
	return false;
}
static Box box(const Fighter& p, const int* hb)
{
	const int x0 = p.x + p.face * hb[0] * F, x1 = p.x + p.face * (hb[0] + hb[2]) * F;
	return Box{ imin(x0, x1), p.y + hb[1] * F, imax(x0, x1), p.y + (hb[1] + hb[3]) * F };
}
int hurtboxes(const Fighter& p, Box out[12])
{
	ready(); if (invul(p)) { return 0; }
	int n = 0;
	if (p.st == KDOWN) { out[n++] = Box{ p.x - 38 * F, 0, p.x + 38 * F, 40 * F }; }
	else if (p.y > 0) { out[n++] = Box{ p.x - 16 * F, p.y + 4 * F, p.x + 16 * F, p.y + 72 * F }; }
	else if (crouching(p)) { out[n++] = Box{ p.x - 20 * F, 0, p.x + 20 * F, 64 * F }; }
	else { out[n++] = Box{ p.x - 19 * F, 0, p.x + 19 * F, 89 * F }; }
	if (p.st == ATK) { const Move& m = MV(p.mv); if (!m.sp && !m.sup) { for (int k = 0; k < m.nhits; k++) { const Hit& h = m.hits[k]; if (p.mf >= h.s && p.mf <= h.e + 3 && n < 12) { out[n++] = box(p, h.hb); } } } }
	return n;
}
static int liveHits(const Fighter& p, const Hit* hs[9], Box bs[9])
{
	if (p.st != ATK) { return 0; }
	const Move& m = MV(p.mv); int n = 0;
	for (int k = 0; k < m.nhits; k++)
	{
		const Hit& h = m.hits[k];
		if (p.mf >= h.s && p.mf <= h.e && !(p.hitMask & (1 << h.grp)) && !(h.req && !(h.req == 2 ? p.contact : p.hitOK))) { hs[n] = &h; bs[n] = box(p, h.hb); n++; }
	}
	return n;
}
int hitboxes(const Fighter& p, Box out[9]) { ready(); const Hit* hs[9]; return liveHits(p, hs, out); }
static Box pbox(const Shot& q) { const Move& m = MV(q.k); return Box{ q.x - m.pw * F / 2, q.y - m.ph * F / 2, q.x + m.pw * F / 2, q.y + m.ph * F / 2 }; }
static inline bool ovl(const Box& a, const Box& b) { return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1; }

// ------------------------------------------------------------ hits
enum How { HOW_HIT, HOW_BLOCK, HOW_PARRY, HOW_ARMOR };
static bool canBlock(const Fighter& d, int inp, int fromX, char g)
{
	if (d.y > 0) { return false; }
	if (!(d.st == BLOCK || d.st == IDLE || d.st == LAND)) { return false; }
	const int side = d.x > fromX ? 1 : d.x < fromX ? -1 : -d.face;
	if (!(inp & (side > 0 ? 8 : 4)) || (inp & (side > 0 ? 4 : 8))) { return false; }
	const bool cr = (inp & 2) != 0;
	if (g == 'l' && !cr) { return false; } if (g == 'h' && cr) { return false; }
	return true;
}
// how the defender answers a touch
static How answer(const Fighter& d, const Hit& h, int fromX, const Move& m, bool isProj)
{
	if (d.st == PARRY && d.y <= 0) { return HOW_PARRY; }
	if (d.st == ATK && d.arm > 0 && !m.sup) { return HOW_ARMOR; }
	return canBlock(d, d.prev, fromX, isProj ? 'm' : h.g) ? HOW_BLOCK : HOW_HIT;
}
static bool immune(const Fighter& d, const Fighter& a, const Hit& h, bool isProj)
{
	if (d.st != ATK) { return false; } const Move& m = MV(d.mv);
	if (!isProj && m.has_ainv && a.y > 0 && d.mf >= m.ainv[0] && d.mf <= m.ainv[1]) { return true; }
	if (!isProj && m.has_lowInv && h.g == 'l' && d.mf >= m.lowInv[0] && d.mf <= m.lowInv[1]) { return true; }
	return false;
}
static int kindOf(const Fighter& d)
{
	if (d.st == PREC || d.st == PARRY) { return 2; }
	if (d.st == RUN || d.st == DRUSH || d.st == DASHF || d.st == DASHB) { return 1; }
	if (d.st != ATK) { return 0; }
	return d.mf > MV(d.mv).lastAct ? 2 : 1;
}

static void applyHit(State& s, int ai, const Hit& h, int fromX, int cx, int cy, How how, bool isProj, bool last, int kind, const Move& m, int mod)
{
	(void)last;
	Fighter& a = s.p[ai]; Fighter& d = s.p[1 - ai]; const int di = 1 - ai, dinp = d.prev;
	const int side = d.x > fromX ? 1 : d.x < fromX ? -1 : -d.face;
	const bool special = m.sp || m.sup; const int drb = !isProj && a.drb ? 4 : 0;
	int hsp = h.hsp; if (!isProj) { a.drb = 0; }
	if (h.free_ && !isProj && how != HOW_HIT) { a.vx = -a.face * 110; a.vy = 520; }
	if (how == HOW_PARRY)
	{
		const bool bk = (dinp & (side > 0 ? 8 : 4)) != 0, cr = (dinp & 2) != 0;
		const bool perfect = d.sf <= 2 && d.plock == 0 && (isProj || (bk && !(h.g == 'l' && !cr) && !(h.g == 'h' && cr && !m.isAir)));
		d.dr = imin(DMAX, d.dr + (isProj ? 150 : 350)); d.drT = 20; d.pok = 1; d.plock = imax(1, h.bstun - 3); d.hs = 8; if (!isProj) { a.hs = 8; a.contact = 1; }
		if (perfect) { s.freeze = isProj ? 8 : 20; s.frzWho = di; toIdle(d); d.hs = 0; d.tinv = 4; a.pp = 70; if (!isProj) { a.hs = 14; } push(s, E_PPARRY, di, cx, cy); }
		else { push(s, E_PARRY, di, cx, cy); }
		return;
	}
	if (how == HOW_ARMOR)
	{
		const int dm = imax(1, h.d / 2); d.hp = imax(1, d.hp - dm); d.arm--; d.armHit = 1; d.redT = 50; d.hs = 9; if (!isProj) { a.hs = 9; a.contact = 1; a.hitOK = 1; }
		push(s, E_ARMOR, di, cx, cy); if (m.di && MV(d.mv).di) { s.slow = 54; push(s, E_DICLASH, -1, cx, cy); } return;
	}
	if (how == HOW_BLOCK)
	{
		const int chip = (special && d.bo) ? imax(1, h.d / 4) : 0;
		d.hp = imax(s.cfg.training ? 1 : 0, d.hp - chip);
		if (!d.bo) { d.dr -= h.has_dd ? h.dd : h.d * 4; d.drT = imax(d.drT, 90); burn(s, d, di); }
		d.st = BLOCK; d.sf = 0; d.stun = h.bstun + drb + (d.bo ? 4 : 0); d.cb = (dinp & 2) ? 1 : 0; d.pv = (h.lk || h.np) ? 0 : side * imax(h.kb, 260); d.vx = 0; d.redT = 40;
		hsp = imax(4, hsp - 2); d.hs = hsp; if (!isProj) { a.hs = hsp; a.contact = 1; }
		a.sa = imin(3000, a.sa + h.d * 3 / 10); d.sa = imin(3000, d.sa + h.d / 5);
		push(s, E_BLOCK, ai, cx, cy, h.lvl);
		if (m.di && iabs(d.x) >= STAGE - PW - 50 * F)
		{
			d.st = HIT; d.crum = 2; d.stun = d.bo ? 120 : 72; d.pv = 0; d.combo = 1; d.comboDmg = 0; d.jug = 0; d.wbUsed = 0; d.gbUsed = 0; d.cb = 0;
			push(s, E_SPLAT, ai, d.x + side * PW, 50 * F, d.bo ? 1 : 0);
		}
		if (d.hp <= 0) { d.st = AHIT; d.y = 1; d.vx = side * 500; d.vy = 800; }
		return;
	}
	const bool inCombo = d.combo > 0 && (d.st == HIT || d.st == AHIT);
	if (inCombo) { d.combo++; } else { d.combo = 1; d.comboDmg = 0; d.jug = 0; d.wbUsed = 0; d.gbUsed = 0; d.hmc = 0; d.half = d.pp > 0 ? 1 : 0; }
	const int seq = isProj ? -2 - ai : a.mseq;
	if (!inCombo) { d.sshift = m.ss; d.drs = 0; d.cs = 1; d.lastSeq = seq; } else if (seq != d.lastSeq) { d.cs++; d.lastSeq = seq; }
	if (drb) { d.drs = 1; }
	if (m.cx == 1 && seq != d.cxSeq) { d.cxSeq = seq; d.sshift++; }
	int pct = SCALE[imin(d.cs - 1 + (d.cs > 1 ? d.sshift : 0), 9)];
	if (d.drs && d.cs > 1) { pct = pct * 85 / 100; }
	if (m.cx == 2 && seq != d.cxSeq) { d.cxSeq = seq; d.sshift++; }
	if (m.minScale && pct < m.minScale) { pct = m.minScale; }
	int dmg = h.d * pct / 100; if (kind) { dmg = dmg * 120 / 100; } if (mod) { dmg = dmg * 80 / 100; } if (a.part && m.sup && !isProj) { dmg = dmg * 6 / 10; } if (d.half) { dmg = dmg / 2; } if (a.bo) { dmg = dmg * 90 / 100; } if (dmg < 1) { dmg = 1; }
	d.hp = imax(s.cfg.training ? 1 : 0, d.hp - dmg); d.comboDmg += dmg; d.redT = 50;
	const bool ko = d.hp <= 0, wasAir = d.y > 0 || d.st == AHIT;
	const int* launch = h.has_launch ? h.launch : nullptr;
	const bool crumple = m.di && (kind || a.armHit) && !wasAir && !ko;
	if (kind == 2 && m.has_pcLaunch && !wasAir) { launch = m.pcLaunch; }
	if (crumple) { launch = nullptr; s.slow = imax(s.slow, 36); }
	if (m.di && !d.bo) { d.dr -= (kind == 2 || a.armHit) ? 1500 : BAR; burn(s, d, di); }
	if (kind) { hsp += 2; } if (ko) { hsp = 22; }
	const bool wasCr = crouching(d);
	const int wrong = (!wasAir && !isProj && (h.g == 'l' || (h.g == 'h' && !m.isAir)) && (d.st == IDLE || d.st == BLOCK)) ? (h.g == 'l' ? H_LOW : H_OVERHEAD) : 0;
	d.crum = 0; d.arm = 0;
	if (wasAir || launch || ko || h.has_carry)
	{
		static const int defAir[2] = { 300, 620 }, defLaunch[2] = { 520, 820 }, defPop[2] = { 24, 430 };
		const int* vec = wasAir ? (h.has_air ? h.air : defAir) : (launch ? launch : defLaunch);
		d.st = AHIT; d.sf = 0; d.mv = -1;
		d.vx = side * vec[0] + (isProj ? 0 : a.vx / 4); d.vy = vec[1]; if (d.y <= 0) { d.y = 1; if (d.vy < 0) { d.vy = 300; } }
		if (ko && d.jug > 2) { d.jug = 2; }
		if (h.lk) { const int* pp = h.has_pop ? h.pop : defPop; d.vx = 0; d.vy = pp[1]; if (d.y < pp[0] * F) { d.y = pp[0] * F; } } else { d.jug++; }
		if (h.xup && wasAir) { a.vx = a.face * h.xup; }
		if (h.reb && a.y > 0) { a.vy = h.reb; a.vx = a.face * 90; a.fr = 2; }   // bounce off them and get your air options back
		if (h.has_carry) { d.x = a.x + a.face * h.carry[0] * F; d.y = a.y + h.carry[1] * F; d.vx = a.vx; d.vy = a.vy; }
		if (h.free_ && !isProj)
		{
			// the first two in a combo pop them and hand the attacker a fresh jump; after that it is just a knock-away
			if (d.hmc < 2 && !ko)
			{
				d.hmc = d.hmc + 1; a.fr = 1; a.vx = 0; a.vy = GRAV; d.x = a.x + a.face * 24 * F;
				{ const int lm = STAGE - PW; if (d.x > lm) { a.x -= d.x - lm; d.x = lm; } else if (d.x < -lm) { a.x += -lm - d.x; d.x = -lm; } }
				if (d.y < a.y + 8 * F) { d.y = a.y + 8 * F; } d.vx = a.face * 30; d.vy = 960; d.gbPend = 0;
			}
			else { a.vx = a.vx / 4; d.vx = side * 540; d.vy = 560; }
		}
		d.pv = 0; d.hard = (m.sup || m.hardKd) ? 1 : 0;
		d.wbPend = (h.wb && !d.wbUsed) ? 1 : 0; d.gbPend = (h.gb && !d.gbUsed) ? 1 : 0;
	}
	else
	{
		d.st = HIT; d.sf = 0; d.mv = -1; d.stun = crumple ? 84 : h.stun + (kind == 2 ? 4 : kind == 1 ? 2 : 0) + drb; d.pv = crumple ? side * 200 : side * h.kb; d.vx = 0;
		d.cb = wasCr && !crumple && !m.stand ? 1 : 0; d.hitHi = (cy - d.y) > 44 * F ? 1 : 0; if (crumple) { d.crum = 1; }
	}
	d.hs = hsp; if (!isProj) { a.hs = hsp; a.contact = 1; a.hitOK = 1; }
	const int parts = imax(1, m.nhits ? m.nhits : (m.has_proj ? m.phits : 1));
	a.sa = imin(3000, a.sa + (m.sup ? 0 : m.has_sg ? m.sg / parts : dmg * 7 / 10)); d.sa = imin(3000, d.sa + dmg * 3 / 10);
	if (!a.bo) { a.dr = imin(DMAX, a.dr + dmg * 3 / 2); }
	push(s, E_HIT, ai, cx, cy, h.lvl, dmg, d.combo, d.comboDmg, kind,
		(ko ? H_KO : 0) | (special ? H_SPECIAL : 0) | (m.di ? H_IMPACT : 0) | (crumple ? H_CRUMPLE : 0) | wrong | (a.fr == 1 ? H_FREE : 0) | (h.fin ? H_FINISH : 0));
    if(!isProj && a.mv==M_SA2 && !ko) {
        const int height=imax(a.y,d.y);
        int blast=270*pct/100;
        if(kind)blast=blast*120/100;if(mod)blast=blast*80/100;if(a.part)blast=blast*6/10;if(d.half)blast/=2;if(a.bo)blast=blast*90/100;
        a.st=THROWING;a.tk=5;a.tf=0;a.sf=0;a.pmT=imax(1,blast);a.vx=a.vy=a.pv=0;a.y=height;
        d.st=THROWN;d.sf=0;d.mv=-1;d.vx=d.vy=d.pv=0;d.y=height;d.face=-a.face;d.noTech=1;
        // Leave room for the pair at either wall; simulation and render share the anchor.
        a.x=imax(-STAGE+PW+36*F,imin(STAGE-PW-36*F,a.x));d.x=a.x+a.face*24*F;
        push(s,E_GRAB,ai,d.x,d.y+60*F,5);
    }
}

static void resolveHits(State& s)
{
	struct Touch { int ai; const Hit* h; int cx, cy; How how; int kind; const Move* m; int mod; };
	Touch list[2]; int n = 0;
	for (int i = 0; i < 2; i++)
	{
		Fighter& a = s.p[i]; Fighter& d = s.p[1 - i];
		const Hit* hs[9]; Box bs[9]; const int nh = liveHits(a, hs, bs); if (!nh) { continue; }
		Box hu[12]; const int nu = hurtboxes(d, hu); const Move& m = MV(a.mv); bool done = false;
		for (int k = 0; k < nh && !done; k++)
		{
			if (immune(d, a, *hs[k], false)) { continue; }
			for (int u = 0; u < nu; u++)
			{
				if (!ovl(bs[k], hu[u])) { continue; }
				const int cx = (imax(bs[k].x0, hu[u].x0) + imin(bs[k].x1, hu[u].x1)) >> 1, cy = (imax(bs[k].y0, hu[u].y0) + imin(bs[k].y1, hu[u].y1)) >> 1;
				list[n++] = Touch{ i, hs[k], cx, cy, answer(d, *hs[k], a.x, m, false), kindOf(d), &m, a.mod }; done = true; break;
			}
		}
	}
	for (int k = 0; k < n; k++) { const Touch& r = list[k]; Fighter& a = s.p[r.ai]; a.hitMask |= (1 << r.h->grp); applyHit(s, r.ai, *r.h, a.x, r.cx, r.cy, r.how, false, false, r.kind, *r.m, r.mod); }
}

static void resolveThrows(State& s)
{
	bool g[2] = { false, false };
	for (int i = 0; i < 2; i++)
	{
		const Fighter& a = s.p[i]; const Fighter& d = s.p[1 - i];
		if (a.st != ATK || a.hs > 0) { continue; } const Move& m = MV(a.mv); if (!m.thr || a.mf < 4 || a.mf > 6) { continue; }
		if (d.y > 0 || d.tinv > 0) { continue; }
		const bool open = d.st == IDLE || d.st == ATK || d.st == DASHF || d.st == LAND || d.st == PARRY || d.st == PREC || d.st == DRUSH || d.st == RUN;
		if (!open && !(d.st == DASHB && d.sf >= 15)) { continue; }
		if (d.st == ATK && MV(d.mv).has_inv && invul(d)) { continue; }
		if (iabs(d.x - a.x) > 58 * F) { continue; }   // the ribbons reach further than arms would
		g[i] = true;
	}
	if (g[0] && g[1])
	{
		for (int i = 0; i < 2; i++) { Fighter& p = s.p[i]; p.st = TECH; p.sf = 0; p.mv = -1; p.pv = -p.face * 620; }
		push(s, E_TECH, -1, (s.p[0].x + s.p[1].x) >> 1, 60 * F); return;
	}
	for (int i = 0; i < 2; i++)
	{
		if (!g[i]) { continue; }
		Fighter& a = s.p[i]; Fighter& d = s.p[1 - i];
		d.noTech = ((d.st == ATK && !MV(d.mv).thr) || d.st == PARRY || d.st == PREC || d.st == DRUSH || d.st == RUN) ? 1 : 0;
		a.st = THROWING; a.sf = 0; a.tk = 0; a.tf = 0; a.pmN = 0; a.pmT = 0; a.mv = -1; a.vx = 0;
		d.st = THROWN; d.sf = 0; d.mv = -1; d.vx = 0; d.vy = 0; d.pv = 0; d.face = -a.face; d.combo = 0; d.arm = 0; d.crum = 0;
		push(s, E_GRAB, i, d.x, 60 * F);
	}
}

static void projectiles(State& s)
{
	for (int k = 0; k < s.nshots; k++)
	{
		Shot& q = s.shot[k]; const Move& m = MV(q.k); q.life++;
		if (q.pause > 0) { q.pause--; continue; }
		if (q.cd > 0) { q.cd--; q.x += q.dir * (m.pvx / 6); } else { q.x += q.dir * m.pvx; }
		if (iabs(q.x) > STAGE + 60 * F || q.life > 300) { q.left = 0; }
	}
	for (int i = 0; i < s.nshots; i++)
	{
		for (int j = i + 1; j < s.nshots; j++)
		{
			Shot& a = s.shot[i]; Shot& b = s.shot[j]; if (a.o == b.o || a.left <= 0 || b.left <= 0) { continue; }
			if (ovl(pbox(a), pbox(b))) { a.left--; b.left--; a.cd = 6; b.cd = 6; push(s, E_CLASH, -1, (a.x + b.x) >> 1, a.y); }
		}
	}
	for (int k = 0; k < s.nshots; k++)
	{
		Shot& q = s.shot[k];
		if (q.left <= 0 || q.cd > 0 || q.pause > 0) { continue; }
		Fighter& d = s.p[1 - q.o]; const Move& m = MV(q.k); const Box b = pbox(q); const Move* dm = d.st == ATK ? &MV(d.mv) : nullptr;
		if (dm && dm->has_pinv && d.mf >= dm->pinv[0] && d.mf <= dm->pinv[1]) { continue; }
		Box hu[12]; const int nu = hurtboxes(d, hu);
		for (int u = 0; u < nu; u++)
		{
			if (!ovl(b, hu[u])) { continue; }
			q.left--; const bool last = q.left <= 0; const int fx = q.x - q.dir * 50 * F;
			applyHit(s, q.o, m.proj, fx, (imax(b.x0, hu[u].x0) + imin(b.x1, hu[u].x1)) >> 1, q.y, answer(d, m.proj, fx, m, true), true, last, kindOf(d), m, q.mod);
			q.cd = 5; q.pause = d.hs; break;
		}
	}
	for (int i = s.nshots - 1; i >= 0; i--)
	{
		if (s.shot[i].left > 0) { continue; }
		push(s, E_PEND, -1, s.shot[i].x, s.shot[i].y);
		for (int k = i; k + 1 < s.nshots; k++) { s.shot[k] = s.shot[k + 1]; }
		s.nshots--;
	}
}

// ------------------------------------------------------------ step
void step(State& s, int i0, int i1)
{
	ready();
	s.nev = 0; s.frame++;
	int raw[2] = { i0 & MASK, i1 & MASK };
	const bool live = s.phase == PH_FIGHT;
	for (int i = 0; i < 2; i++)
	{
		Fighter& p = s.p[i]; int inp = (live || (s.phase == PH_KO && s.rw == i && s.koType != KO_TIME)) ? raw[i] : 0;
		if (s.phase == PH_KO && p.st == WIN && (inp & 0xFF0)) { p.st = IDLE; p.sub = STAND; p.sf = 0; }
		if ((inp & 12) == 12) { inp &= ~12; } if ((inp & 3) == 3) { inp &= ~3; }
		raw[i] = inp;
		const bool frozen = s.freeze > 0 || p.hs > 0;
		std::memmove(p.buf, p.buf + 1, sizeof(int16_t) * (NBUF - 1)); p.buf[NBUF - 1] = int16_t(inp);
		const int pr = inp & ~p.prev;
		for (int b = 0; b < NB; b++) { if (pr & (16 << b)) { p.age[b] = 0; p.pd[b] = inp & 271; } else if (!frozen && p.age[b] < 99) { p.age[b]++; } }
		if ((inp & 32) && (inp & 64) && p.age[1] <= 3 && p.age[2] <= 3 && (pr & 96)) { p.rq = 0; } else if (!frozen && p.rq < 99) { p.rq++; }
		if (dashIn(p, 1)) { p.d66 = 0; } else if (!frozen && p.d66 < 99) { p.d66++; }
		if (dashIn(p, -1)) { p.d44 = 0; } else if (!frozen && p.d44 < 99) { p.d44++; }
		p.prev = inp;
	}
	if (s.phase == PH_INTRO) { s.pt++; if (s.pt == 1) { push(s, E_ROUND, -1, 0, 0, s.round); } if (s.pt >= INTRO) { s.phase = PH_FIGHT; s.pt = 0; push(s, E_FIGHT); } }
	if (s.freeze > 0) { s.freeze--; return; }
	if (s.phase == PH_KO) { s.pt++; if (s.pt < 56 && s.pt % 4 != 0) { return; } }
	else if (s.slow > 0) { s.slow--; if (s.slow % 3 != 0) { return; } }
	const bool fr[2] = { s.p[0].hs > 0, s.p[1].hs > 0 };
	for (int i = 0; i < 2; i++) { Fighter& p = s.p[i]; if (fr[i]) { p.hs--; } else { update(s, p, s.p[1 - i], raw[i], i); } }
	for (int i = 0; i < 2; i++) { if (!fr[i]) { physics(s, s.p[i], s.p[1 - i], i); } }
	pushboxes(s);
	resolveThrows(s);
	projectiles(s);
	resolveHits(s);
	for (int i = 0; i < 2; i++)
	{
		Fighter& p = s.p[i];
		if (p.redT > 0) { p.redT--; } else if (p.red > p.hp) { p.red = imax(p.hp, p.red - 8); } if (p.red < p.hp) { p.red = p.hp; }
		const bool rest = p.st == IDLE || p.st == DASHF || p.st == DASHB || p.st == JUMP || p.st == LAND || p.st == PREJUMP || p.st == ATK || p.st == RUN;
		if (p.drT > 0) { p.drT--; }
		else if (rest && s.phase == PH_FIGHT)
		{
			p.dr = imin(DMAX, p.dr + (p.bo ? 5 : p.y > 0 ? 3 : p.st == IDLE && p.sub == WALKF ? 9 : 6));
			if (p.bo && p.dr >= DMAX) { p.bo = 0; push(s, E_RECOVER, i); }
		}
	}
	if (s.phase == PH_FIGHT)
	{
		if (s.cfg.training)
		{
			for (int i = 0; i < 2; i++) { Fighter& p = s.p[i]; if (p.combo == 0 && p.st == IDLE && p.redT == 0) { p.hp = CH_HP; p.sa = 3000; if (p.drT == 0 && !p.bo) { p.dr = DMAX; } } }
		}
		else
		{
			s.time--;
			const bool d0 = s.p[0].hp <= 0, d1 = s.p[1].hp <= 0;
			if (d0 || d1 || s.time <= 0)
			{
				s.phase = PH_KO; s.pt = 0; s.kx = 0; int w = -1;
				if (d0 || d1) { s.koType = KO_KO; w = d0 && d1 ? -1 : d0 ? 1 : 0; }
				else { s.koType = KO_TIME; const int r0 = s.p[0].hp * CH_HP, r1 = s.p[1].hp * CH_HP; w = r0 > r1 ? 0 : r1 > r0 ? 1 : -1; }
				s.rw = w;
				if (w >= 0) { s.p[w].wins++; if (s.koType == KO_KO && s.p[w].hp >= CH_HP) { s.koType = KO_PERFECT; } } else { s.koType = d0 ? KO_DOUBLE : KO_DRAW; }
				push(s, E_KO, w, 0, 0, s.koType);
			}
		}
	}
	else if (s.phase == PH_KO)
	{
		// every hit on the body buys a little more time, up to ten extra seconds
		bool anyHit = false; for (int k = 0; k < s.nev; k++) { if (s.ev[k].type == E_HIT) { anyHit = true; } }
		if (s.rw >= 0 && anyHit && s.kx < 600 && s.pt > KOTIME - 100) { const int add = imin(600 - s.kx, s.pt - (KOTIME - 100)); s.kx += add; s.pt -= add; }
		if (s.pt == 150 && s.rw >= 0 && s.kx == 0) { Fighter& w = s.p[s.rw]; if (w.st == IDLE && w.y <= 0) { w.st = WIN; w.sf = 0; w.vx = 0; } push(s, E_WINPOSE, s.rw); }
		if (s.pt >= KOTIME)
		{
			const int need = s.cfg.rounds;
			if (s.p[0].wins >= need || s.p[1].wins >= need || s.round >= need * 2 + 1)
			{
				s.phase = PH_OVER; s.winner = s.p[0].wins > s.p[1].wins ? 0 : s.p[1].wins > s.p[0].wins ? 1 : -1; push(s, E_OVER, s.winner);
			}
			else { s.round++; resetRound(s); }
		}
	}
}

void debugStart(State& s, int who, int moveId) { ready(); Fighter& p = s.p[who & 1]; p.y = 0; start(s, p, moveId, who & 1); }

}  // namespace gk

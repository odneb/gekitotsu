// Replays an oracle file (tools/oracle.js) through the C++ rules and reports the first frame that differs.
#include "gk_sim.h"
#include "gk_ai.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
using namespace gk;
static void dump(const State& s, std::vector<int>& o, std::vector<std::string>* names)
{
	auto put = [&](const std::string& n, int v) { o.push_back(v); if (names) { names->push_back(n); } };
	put("frame", s.frame); put("slow", s.slow); put("phase", s.phase); put("pt", s.pt); put("kx", s.kx); put("round", s.round); put("time", s.time); put("freeze", s.freeze); put("frzWho", s.frzWho); put("winner", s.winner); put("rw", s.rw); put("koType", s.koType);
	static const char* PF = "x y vx vy pv face hp sa dr bo drT sf subf mf hitMask contact hitOK hs stun prev d66 d44 combo comboDmg jug wbUsed gbUsed wbPend gbPend jd rec cb hitHi tinv wins back red redT air2 noTech ca rq part jg cxSeq fum mseq cs lastSeq acq rcn sshift drs pok roll hard arm armHit drb mod aci crum pp plock half hm hmc fr hmE dj wj aa tk tf pmT pmN";
	for (int i = 0; i < 2; i++)
	{
		const Fighter& p = s.p[i]; const std::string P = "p" + std::to_string(i) + ".";
		const int v[] = { p.x, p.y, p.vx, p.vy, p.pv, p.face, p.hp, p.sa, p.dr, p.bo, p.drT, p.sf, p.subf, p.mf, p.hitMask, p.contact, p.hitOK, p.hs, p.stun, p.prev, p.d66, p.d44, p.combo, p.comboDmg, p.jug, p.wbUsed, p.gbUsed, p.wbPend, p.gbPend, p.jd, p.rec, p.cb, p.hitHi, p.tinv, p.wins, p.back, p.red, p.redT, p.air2, p.noTech,
			p.ca, p.rq, p.part, p.jg, p.cxSeq, p.fum, p.mseq, p.cs, p.lastSeq, p.acq, p.rcn, p.sshift, p.drs, p.pok, p.roll, p.hard, p.arm, p.armHit, p.drb, p.mod, p.aci, p.crum, p.pp, p.plock, p.half, p.hm, p.hmc, p.fr, p.hmE, p.dj, p.wj, p.aa, p.tk, p.tf, p.pmT, p.pmN };
		const char* q = PF; for (int k = 0; k < int(sizeof(v) / sizeof(v[0])); k++) { const char* e = std::strchr(q, ' '); std::string n = e ? std::string(q, e) : std::string(q); q = e ? e + 1 : q; put(P + n, v[k]); }
		put(P + "st", p.st); put(P + "sub", p.sub); put(P + "mv", p.mv); put(P + "fuq", p.fuq); put(P + "ac", p.ac);
		for (int k = 0; k < 8; k++) { put(P + "age" + std::to_string(k), p.age[k]); } for (int k = 0; k < 8; k++) { put(P + "pd" + std::to_string(k), p.pd[k]); }
	}
	put("nshots", s.nshots);
	for (int i = 0; i < s.nshots; i++) { const Shot& q = s.shot[i]; const std::string P = "shot" + std::to_string(i) + "."; put(P + "o", q.o); put(P + "x", q.x); put(P + "y", q.y); put(P + "dir", q.dir); put(P + "left", q.left); put(P + "cd", q.cd); put(P + "pause", q.pause); put(P + "life", q.life); put(P + "mod", q.mod); put(P + "k", q.k); }
	put("nev", s.nev);
	for (int i = 0; i < s.nev; i++) { const Event& e = s.ev[i]; const std::string P = "ev" + std::to_string(i) + "."; put(P + "type", e.type); put(P + "who", e.who); put(P + "x", e.x); put(P + "y", e.y); put(P + "a", e.a); put(P + "b", e.b); put(P + "c", e.c); put(P + "d", e.d); put(P + "e", e.e); put(P + "f", e.f); }
}
int main(int argc, char** argv)
{
	if (argc < 2) { return 2; }
	FILE* f = std::fopen(argv[1], "r"); if (!f) { std::printf("cannot open %s\n", argv[1]); return 2; }
	const int detail = argc > 2 ? std::atoi(argv[2]) : -1; const int aiMode = argc > 3 ? std::atoi(argv[3]) : 0;   // 1: the CPU plays both sides, 2: only the second
	static Ai ai[2]; aiMake(ai[0], 3); aiMake(ai[1], 2);
	int training = 0, frames = 0; if (std::fscanf(f, "%d %d", &training, &frames) != 2) { return 2; }
	static State s; newMatch(s, training != 0, 2);
	for (int n = 1; n <= frames; n++)
	{
		int i0, i1, reset, fw, fm, fhp, fsa; unsigned want; if (std::fscanf(f, "%d %d %d %x %d %d %d %d", &i0, &i1, &reset, &want, &fw, &fm, &fhp, &fsa) != 8) { std::printf("short file at %d\n", n); return 2; }
		if (reset) { newMatch(s, training != 0, 2); aiMake(ai[0], 3); aiMake(ai[1], 2); }
		if (aiMode == 1) { const int a0 = aiThink(ai[0], s, 0); if (a0 != i0) { std::printf("CPU DIFFERS (side 1) at frame %d: %d, browser %d\n", n, a0, i0); return 1; } }
		if (aiMode >= 1) { const int a1 = aiThink(ai[1], s, 1); if (a1 != i1) { std::printf("CPU DIFFERS (side 2) at frame %d: %d, browser %d\n", n, a1, i1); return 1; } }
		if (fw >= 0) { s.p[fw].hp = fhp; s.p[fw].sa = fsa; debugStart(s, fw, fm); }
		step(s, i0, i1);
		std::vector<int> d; std::vector<std::string> names; dump(s, d, &names);
		unsigned h = 2166136261u; for (int v : d) { h ^= unsigned(v); h *= 16777619u; }
		if (n == detail) { for (size_t k = 0; k < d.size(); k++) { std::printf("%s=%d\n", names[k].c_str(), d[k]); } return 0; }
		if (h != want && detail < 0) { std::printf("DIFFERS at frame %d (of %d)\n", n, frames); return 1; }
	}
	if (detail < 0) { std::printf("IDENTICAL %d frames\n", frames); }
	return 0;
}

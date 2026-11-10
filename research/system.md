# SF6 Universal System Data (for 60 fps reimplementation)

Compiled 2026-11-10. All frame counts are at 60 fps. "UNCONFIRMED" = no reachable source gave a number; do not treat as data.

**Source access notes.** wiki.supercombo.gg is blocked: one URL is not on the fetch allowlist and the others return an Anubis bot wall. Its mirror (srk.shib.live) was deliberately not used. The official per-character frame page at streetfighter.com/6 loaded for Sagat; Ken's was blocked by the allowlist.

Main sources used:
- [OFF-SAGAT] Official Sagat frame data: https://www.streetfighter.com/6/en-us/character/sagat/frame (current patch at fetch time)
- [UFD-KEN] https://ultimateframedata.com/sf6/ken ; [UFD-MARISA] https://ultimateframedata.com/sf6/marisa
- [MOCHI] JP spec summary, patch 2026.05.28: https://note.com/mochimochi_sf/n/n08a56f1b4f06 (most current source)
- [MAYUGE] JP system spec, Apr 2024 with later edits (cites patches 2024.02.27 and 2024.05.22): https://www.mayuge-purys.com/entry/2024/04/04/【スト6】バトルシステム全般の細かい仕様について
- [SAVA] JP frame memo: https://note.com/savava/n/n320aa87c6f65
- [ROZE] JP frame memo: https://sizu.me/rozeroze/posts/63v7dvf94vwa
- [ROKU-DI] DI article: https://note.com/rokujokazuma/n/nfeb621a89504 ; [ROKU-Y3] https://note.com/rokujokazuma/n/n462e2a21cb40
- [KAGE] https://sf6combo.kagewebsite.com/page/drive-system
- [NIKO] Drive management guide: https://note.com/nikotarosun/n/n039a875e5b52
- [KAMI] https://kamigame.jp/streetfighter6/page/267963550562041974.html
- [NINDO] https://note.com/brave_borage507/n/nfb34d25573b8 (updated for the 2024/05/22 patch)
- [LIBI] Scaling: https://note.com/libitina_dgh/n/n6dd052c3f434 ; [HAMEKO] https://note.com/hameko/n/n933d2911d655
- [EH-S2] Season 2 patch, May 2024: https://www.eventhubs.com/news/2024/may/21/sf6-season-2-patch-notes/
- [EH-S3] Season 3 patch, Jun 2025: https://www.eventhubs.com/news/2025/jun/03/street-fighter-s3-patch-notes/
- [EH-ALEX] Mar 2026 patch: https://www.eventhubs.com/news/2026/mar/16/street-fighter-alex-patch-notes/
- [STEAM-G] https://steamcommunity.com/sharedfiles/filedetails/?id=2989488745
- [FGG] https://fightinggameguide.com/sf6.html
- [KANI] Back recovery: https://note.com/kani_chahan/n/nca3ba6295054

---

## 0. Units

| Item | Value | Source |
|---|---|---|
| Drive gauge | 6 bars x 10,000 = 60,000 units | [KAGE]; official columns use this scale (e.g. DI block −5000 = 0.5 bar) [OFF-SAGAT] |
| Health | 10,000 standard. Zangief 11,000; Honda, Marisa, Alex 10,500; Akuma 9,000 | [MOCHI], [MAYUGE] |
| Super gauge | Official "SA gauge increase" is in raw units (st.LP +300, st.HP +1000, PC throw +4000). 3 levels are standard. Likely 10,000 per level (30,000 max), but **UNCONFIRMED**: no source states the bar size outright | [OFF-SAGAT] |
| Position units | "765 units is the distance from center stage to a corner", so the stage spans about 1530 units wall to wall | Tweet by @TsuguSpabobin, quoted in the search-result title (the page itself refuses fetch): https://x.com/TsuguSpabobin/status/1667261108674088973 |
| UFD walk/dash numbers | Same unit system (Ken forward dash = 132.321). Walk speed is presumably units/frame (**UNCONFIRMED** label) | [UFD-KEN] |
| Supercombo "range" numbers (1.0 = ?) | **UNCONFIRMED**: supercombo is unreachable | — |

## 1. Drive Gauge

| Item | Value | Source |
|---|---|---|
| Size / start | 6 bars, full at the start of every round (does not carry over between rounds) | [KAGE], [FGG] |
| Passive regen, idle | 0.006 bar/frame (60 u/f), about 2.8 s per bar | [NIKO] |
| Passive regen, walking forward | 0.009 bar/frame (90 u/f), about 1.9 s per bar | [NIKO] |
| Passive regen, jumping or being hit | 0.003 bar/frame, about 5.6 s per bar | [NIKO] |
| Conflicting figure | Idle or back-walk about 4.1 s/bar (25 s for 6 bars); forward walk about 3.6 s/bar (22 s for 6) | [KAMI] |
| Relative rates | Forward walk >50% faster than idle; back walk about 5% faster; jump about 50% slower | https://www.eventhubs.com/news/2023/may/15/sf6-drive-gauge-visual-guide/ |
| Forward-walk regen start | Regen applies from frame 11 of forward walk (S3; [MAYUGE] said 2F+ before S3). The forward-walk rate was also increased in Year 3 | [EH-S3], https://note.com/tadaotogames/n/n7aa2a90d5ba4 |
| Exact per-frame rates, current patch | **UNCONFIRMED** (the two sources above disagree) | — |

**Regen-pause timers** ([MAYUGE], frames):

| Event | Pause |
|---|---|
| After guarding | 90 |
| Successful parry | 20 (was 60 before 2024-05-22, confirmed in [EH-S2]) |
| Whiffed parry | 240 |
| Parry Drive Rush | 240 |
| OD move | 120 |
| Cancel Drive Rush | 120 |
| Drive Impact | 120 |
| Drive Reversal | 120 |

[NIKO] gives similar figures in seconds: SA hit or block 1.5 s, OD/DI/DR 2 s after the animation, CDR 2 s after the combo, failed parry or rush 4 s, successful parry 1 s.

Other pause rules:
- Recovery stops during an SA3 or lock-SA2 cinematic. It also stops during a Cancel Drive Rush combo that hits, but not one that is blocked [NINDO].
- Your own gauge keeps regenerating while your SA3 animation plays [KAMI], [MOCHI].

**Gains and losses** (1 bar = 10,000):

| Event | Amount | Source |
|---|---|---|
| Hit with normals/specials (attacker) | Per move. Sagat: st.LP +250, st.MP +2000, st.HP +3000, fireballs +1000, throw +2000 | [OFF-SAGAT] |
| Blocked normals/specials (defender loses) | Per move. Sagat: st.LP −500, st.MP −3000, st.HP −6000, L fireball −2500, M/H −4000 | [OFF-SAGAT] |
| Blocked lights / mediums / heavies (generic) | Lights 0.05 bar, mediums 0.3, heavies 0.4–0.6 (a second table: 0.15 / 0.25 / 0.4–0.5) | [KAMI] |
| Punish-counter drive loss (defender) | Per move. Sagat: st.LP −2000, st.MP −4000, st.HP −8000, throw −10,000. Heavies are usually about 0.8 bar | [OFF-SAGAT], [NINDO] |
| Hit by Drive Impact | Block −0.5, hit −1.0, counter hit −1.1 or −1.2 (sources differ), punish counter or armor-crumple −1.5 (official PC column: −15,000) | [NINDO], [NIKO], [KAMI], [OFF-SAGAT] |
| Hit by SA | SA1 −0.5, SA2 −1.0, SA3 −1.5, CA −2.0 | [NINDO], [MOCHI] |
| Parry gain | Projectile +0.5 bar, normal/special/DRev +1.0, SA or DI +2.0 | [KAGE], [NIKO] |
| Perfect parry | Attacker's gain and defender's loss both ×50% since S2. [MOCHI] says JP gives +1 bar | [EH-S2], [MOCHI] |
| Throw tech (escaper) | +1 bar since S3 (June 2025) | [MOCHI], [EH-S3] |

## 2. Drive Impact (universal; all characters share frame data, reach differs)

| Item | Value | Source |
|---|---|---|
| Cost | 1 bar | many |
| Startup / active / recovery | 26 / 2 (frames 26–27) / 35. Total 62 | [OFF-SAGAT], [ROKU-DI] |
| On block | −3 | [OFF-SAGAT] |
| On hit (midscreen, normal hit) | Knockdown (official "D"). +23 per [ROZE]; one older table says "35" (likely the hit column misread) | [ROZE], https://h2blo.com/game/st6-frame/ |
| Damage | 800. Armor absorbs take 50% as recoverable white damage (400) | [OFF-SAGAT], [SAVA] |
| Armor | 2 hits, frames 1–27. The 3rd hit breaks it. Throws and SAs (armor-break) beat it, as do multi-hit moves of 3+ hits and Drive Reversal (armor-break) | [OFF-SAGAT], [STEAM-G], [MAYUGE] |
| Scaling | Starter 20% (official "Starter 20%"): next hit 80%. Wall splat from a blocked DI applies an 80% multiplier. Burnout stun combos start at 80% | [OFF-SAGAT], [HAMEKO], [MAYUGE] |
| SA gauge | 0 on normal hit; +3000 on punish counter or after armor absorb | [OFF-SAGAT], https://dood.gg/en/street-fighter-6/gouki_akuma/Drive%20Impact-%20Oni%20Goroshi/ |
| Buffer | 10F (was 5F before S2) | [EH-S2] |
| Counter hit | Struck on frame 26 = counter hit | [MAYUGE] |
| Punish counter, or hit after armoring a hit | Crumple at any screen position. Airborne victims get an extended juggle | dood Akuma DI page, [KAGE] |
| Crumple follow-up window | Ground hit up to 21F after DI recovery ends; air hit from 22F onward. Exact crumple duration **UNCONFIRMED** | [ROKU-DI] |
| Block or hit near wall | Wall splat ("CRASH"): opponent is stuck on the wall and the first hit puts them in juggle state. Wall-distance threshold and advantage **UNCONFIRMED** | [MOCHI], https://hotondo.work/archives/sfvi-drive-impact.html |
| Burnout victim and wall | Stun of fixed length (mashing does not shorten it). [ROKU-DI]: full stun 209F; a standing hit connected at 135F and whiffed at 137F. Victim's gauge refills at next wake-up (hotondo) | [ROKU-DI], [MAYUGE] |
| Burnout victim blocking midscreen | +4 extra blockstun (−3 becomes +1) | [MAYUGE] |
| During a lock/blockstring | DI knocks back with no splat or stun | [MOCHI] |
| DI vs DI | Later DI wins with a punish counter (crumple). Simultaneous DIs cancel: no damage, 0 advantage, both pushed apart. Slow-motion durations **UNCONFIRMED** | [MAYUGE], https://www.eventhubs.com/news/2022/jul/25/drive-impact-big-weakness/ |
| Counter-DI cutoff | From DI frame 22 onward you can no longer DI-counter with a light or throw (ROZE). Reaction math: about 12F reaction + 26F startup | [ROZE], [ROKU-DI] |
| Cancels into DI | Any "C"-cancelable normal (special-cancelable) can cancel into DI or Drive Rush | [OFF-SAGAT] legend |
| Parried DI | DI user is −3, no splat | hotondo |
| Midscreen block pushback | Large; numeric value **UNCONFIRMED** | — |
| Hitstop/freeze on startup, hit, absorb | **UNCONFIRMED** | — |

## 3. Drive Parry

| Item | Value | Source |
|---|---|---|
| Input | MP+MK | [FGG] |
| Startup / active | 1 / minimum 12 frames (official "[2] 1-12"). It was 8 before S2 (2024-05) | [OFF-SAGAT], [EH-S2] |
| Recovery after release | 33 (was 29 before S2). Only blocking is allowed during recovery; being hit there is a punish counter | [OFF-SAGAT], [EH-S2] |
| Cost | 0.5 bar on activation, then 50 units/frame while held (200F = 1 bar) | [KAGE] |
| Hold drain | The Mar 2026 Dhalsim bug note says drain applies "after 30 frames" of holding | [EH-ALEX] (exact universal rule **UNCONFIRMED**) |
| Gain per parried hit | +0.5 projectile, +1 strike, +2 SA or DI | [KAGE] |
| Perfect Parry window | First 2 frames of parry (press at most 2F before the hit). Since S3 it needs the correct guard direction: back for high/mid, down-back for low | [KAGE], [MOCHI], [EH-S3] |
| PP vs strike | Screen freeze, then the parrier recovers in about 1–2F, with 5–6F of full invincibility. Sources: "1F rec + 6F inv" [KAGE]; "2F rec + 5F inv" [MAYUGE], [SAVA] | |
| PP vs projectile | 11F fixed recovery, then up to 13F holdable before the normal recovery | [KAGE] |
| PP punish | 50% multiplicative scaling. SA gauge ×80% for both sides (S2) | [LIBI], [EH-S2] |
| PP advantage | Opponent's remaining recovery minus about 1–2F. Example: a blocked light gives +8 or more | [ROZE], [MAYUGE], [MOCHI] |
| PP freeze length | **UNCONFIRMED** | — |
| Throwing a parrying opponent | Punish counter: 170% damage (1200 → 2040), hard knockdown, −1 bar drive. SA gain for thrower 4000 (×2). Thrown player gets 1400 | [MOCHI], [OFF-SAGAT] |
| PP into throw | Punish counter at 50% scaling = 1020 | [MOCHI] |
| Air parry | Does not exist (no source lists an air version; all describe it as a ground stance). Not explicitly stated | — |
| Projectile pushback while parrying | Halved | [KAGE] |

## 4. Drive Rush

| Item | Value | Source |
|---|---|---|
| Parry Drive Rush cost | 1 bar total. Raw = 66 + parry, parry → 66, or 6 → parry → 6. Second forward within 8F; parry within 3F of the step (2F per older note) | [MOCHI], [MAYUGE], [SAVA] |
| Parry DR timing (official) | 45 total frames. Screen freeze until frame 10; can cancel into an attack on frame 9 | [OFF-SAGAT] |
| Parry DR (community breakdown) | Rush cancel from parry frame 4. Raw rush frames: 1–3 parry, 4 = 10F blackout + rush start, 5–10 running only, 11–22 attack cancel, 23–45 running only. From an existing parry: attack from rush frame 8, guard or jump from 20, stops at 43 | [ROZE] |
| Fastest raw-rush attack | 3 (parry) + 8 + 4F light = hits on frame 15 (not counting the 10F blackout) | [SAVA], [MOCHI] |
| Cancel Drive Rush | 3 bars. 46 total frames; freeze until frame 9; attack from frame 10. Cancelable from any "C" normal on hit or block; light normals can cancel even on whiff | [OFF-SAGAT], [MAYUGE], [MOCHI] |
| Blackout input | Inputs buffer during blackout (not walk). Since Mar 2026, Up during the DR freeze produces a jump | [MAYUGE], [EH-ALEX] |
| +4 rule | The first normal or command normal out of the rush gets +4 hitstun and blockstun. This stacks with CH +2, PC +4, burnout +4 (rush + PC = +8) | [STEAM-G], [ROZE], [MAYUGE] |
| Rush overheads | Since Mar 2026: −3 on block, so +1 with DR | [EH-ALEX] |
| DR scaling | ×0.85 multiplier from the next hit after any DR or CDR used mid-combo. Applied once only; not applied to combos that start from a raw rush | [LIBI], [HAMEKO], [OFF-SAGAT] ("Multiplier 15%") |
| Momentum into the attack | Rush attacks carry forward momentum and have reduced pushback (qualitative) [FGG]. Velocity curve **UNCONFIRMED** | |
| Distance / speed profile | **UNCONFIRMED**. Per-frame data exists in a tweet (https://x.com/ssbmaltimor/status/1670233588048994305), but it is not fetchable | |
| Blocking during rush | No guard until rush frame 20 (from parry) or 23 (raw) | [ROZE] |
| Burnout | Drive Rush is unusable. Minimum needed for a parry rush is a sliver (0.1 bar) | [MOCHI] |

## 5. Drive Reversal

| Item | Value | Source |
|---|---|---|
| Input / cost | Forward + MP+MK during blockstun (or after a parry), 2 bars | [KAGE], [MAYUGE] |
| Startup / active / recovery | Blocking version 20 / 20–22 / 26. Wake-up version (added S2) 18 / 18–20 / 26 | [OFF-SAGAT] |
| On block | −6 (was −8 before 2024-05). Burnout defender blocking it: +2 | [EH-S2], [MOCHI] |
| On hit | Knockdown, +23. 5F of recovery is added on hit | [MAYUGE], [OFF-SAGAT] |
| Damage | 500, all recoverable (white). Breaks armor | [OFF-SAGAT], [MAYUGE] |
| Invulnerability | Fully invincible through frame 22 (blocking) or 20 (wake-up) | [OFF-SAGAT] |
| Buffer | 7F | [SAVA] |

## 6. Overdrive / Burnout

**Overdrive**
- Costs 2 bars; input is two attack buttons.
- In burnout, ODs are unavailable and only SAs give invincibility.
- With less than 2 bars, OD activation is impossible. One guide claims OD "burns you out" instead; **UNCONFIRMED**.
- Sources: [MOCHI], [KAGE].

**Burnout** is entered at 0 drive.
- **Blockstun:** +4 on every blocked hit. Hitstun is unchanged.
- **Chip:** blocked specials, SAs and DI deal chip equal to 25% of the move's damage (CA chip = 1250). Chip can KO.
- **Drive damage while blocking SAs:** halved.
- **Disabled:** parry, DI, Drive Rush, Drive Reversal, OD.
- **DI on a cornered burnout target:** stun (see §2).
- **Duration:** 20 s with no gauge-gaining actions, 24 s per [NINDO]. Gaining actions shorten it, so typical burnouts last about 10 s [STEAM-G]. Blocking speeds recovery [MOCHI].
- **No regen pause:** burnout has no recovery cooldown, so regen continues even after a punish counter [NINDO].
- **Exit:** when the gauge fills, the player returns with all 6 bars.
- **Damage dealt/taken and walk speed in burnout:** no reliable source gives a change. Dashfight claims lower damage dealt and higher damage taken; that matches none of the spec sources, so treat it as wrong / **UNCONFIRMED**.
- **Exact burnout regen rate:** **UNCONFIRMED**.
- Sources: [MAYUGE], [KAGE], [STEAM-G], [NINDO], [MOCHI], [SAVA].

## 7. Super Art Gauge

- **Size:** 3 levels (bar units **UNCONFIRMED**, likely 10,000 each). Carries over between rounds [FGG]. About 2 bars are gained per round [MOCHI].
- **Gain multipliers** ([MAYUGE], [SAVA]):

  | Situation | Attacker | Defender |
  |---|---|---|
  | Hit | 100% of move value | 70% |
  | Block | 50% | 25% |

  Unaffected by CH or scaling. Punish-counter throws give double (4000).
- **Gain per move:** see official SA column, e.g. Sagat st.LP 300, st.MP 700, st.HP 1000, special 600 [OFF-SAGAT].
- **Costs:** SA1 = 1 bar, SA2 = 2, SA3 = 3. CA = SA3 used at ≤25% health (health bar turns yellow) [MOCHI].
- **Minimum damage floor:** SA1 30%, SA2 40%, SA3 and CA 50% [LIBI], [HAMEKO], [MOCHI]. Example: Guile SA3 4000 → minimum 2000; CA 4500 → minimum 2250.
- **Super cancel:** a special canceled into SA3/CA adds 10% scaling ("Immediate 10%"). OD special into SA2 adds no extra [HAMEKO], [OFF-SAGAT].
- **Freeze durations for SA1/2/3:** **UNCONFIRMED**.
- **Timer:** pauses during SA3/CA cinematics and DR blackouts, but not during SA1/SA2, except lock SA2s [MOCHI].

## 8. Damage Scaling

| Item | Value | Source |
|---|---|---|
| Per-hit base | 100, 100, 80, 70, 60, 50, 40, 30, 20, 10% (floor 10%; total scaling is capped at 90 points). [MAYUGE]/[SAVA] say the minimum is 8% | [HAMEKO], [LIBI] |
| Formula | (100 − total scaling points) × PP(0.5) × DR(0.85) × attack-power × Modern(0.8), rounded down | [LIBI] |
| Starter scaling | Lights: 20% since S2 (was 10%), so 100, 80, 70, 60…. Cancelable cr.MK and DI starters: 20%. Shoryuken/Headbutt starters: about 30% (next hit 70%). Throws self-scale 20% (official "Immediate 20%") | [EH-S2], [SAVA], [HAMEKO], [OFF-SAGAT] |
| Additional combo scaling | Some moves add more (official "Combo 20%") | [OFF-SAGAT] |
| Perfect parry | ×0.5 (50, 50, 40, 35…) | [HAMEKO] |
| Drive Rush | ×0.85 once per combo (100, 85, 68, 59, 51…) | [SAVA], [HAMEKO] |
| Wall splat from blocked DI | ×0.8 | [HAMEKO], [MAYUGE] |
| Counter hit | +20% damage, +2 hitstun/advantage (SA/CA get no bonus damage) | [MOCHI], [MAYUGE] |
| Punish counter | +20% damage, +4. Drains drive per move. Normal throw PC = 170% (2040) and hard knockdown. Command-throw PC = 115% | [MAYUGE], [MOCHI] |
| Rush counter | CH +6, PC +8 | [MAYUGE] |
| Max health | 10,000 | [MOCHI] |
| Throw damage | 1200 forward or back (most characters) | [MAYUGE], [UFD-KEN] |
| Modern assisted specials | ×0.8 | [LIBI] |

## 9. Hitstop / Stun Conventions

- **Hitstop:** light 9F, medium 11F, heavy 13F [MAYUGE], [SAVA], marked "approximate" by [MAYUGE]. Cancelable mediums (e.g. cr.MK) use 9F, giving a 12F confirm window.
- **Specials and blockstop:** **UNCONFIRMED**.
- **Hitstop on CH/PC:** unchanged, except moves with special hitstun [MAYUGE].
- **Additive stun:** CH +2, PC +4, DR +4, burnout block +4. These stack [STEAM-G].
- **Pushback values:** **UNCONFIRMED**.

## 10. Movement (Ken where specified)

| Item | Value | Source |
|---|---|---|
| Pre-jump | 4F (hittable as grounded on frames 1–4; airborne from frame 5; throw-invincible from frame 1) | [UFD-KEN], [ROZE] |
| Jump total | 45F standard (Ken). Zangief 46, Lily/AKI 47, Chun 49, Dhalsim 75 | [MOCHI] |
| Jump breakdown (45F) | 1–4 ground. 5 airborne with no actions. 6–42 air attacks allowed. Empty jump: 43–45 landing (guard/tech/parry only), act on 46. With an attack: 44–46 no actions, act on 47 | [ROZE] |
| Landing recovery | 3F | [MAYUGE] |
| Ken walk | Forward 4.7, back 3.2 (units/frame presumed) | [UFD-KEN] |
| Ken forward dash | 19F, 132.321 units | [UFD-KEN] |
| Ken back dash | 23F, 92.3 units. Throw-invincible frames 1–15 (some sources: from frame 1) | [UFD-KEN], [SAVA] |
| Ken jump apex / distance | **UNCONFIRMED** | — |
| Throw | Startup 5, active 5–7 (3F), recovery 23, whiff total 30. Damage 1200 | [OFF-SAGAT], [MAYUGE], [UFD-KEN] |
| Throw advantage, Ken | Forward +20, back +6 (UFD). [ROKU-Y3] cites a separate case at +20 | [UFD-KEN] |
| Tech window | 9F after throw startup [MAYUGE], [STEAM-G], [ROKU-Y3]; [SAVA] says 10F including the connect frame. Input LP+LK | |
| Throw invulnerability | 1F after wake-up; 2F after hitstun or blockstun ends | [MAYUGE], [FGG] |
| Throw range | Same for all characters except Zangief and Marisa; numeric value **UNCONFIRMED** | [MAYUGE] |
| Strike vs throw on the same frame | Strike wins | [MOCHI] |

## 11. Knockdown / Wake-up

**Wake-up options**
- **Normal rise:** no input.
- **Back rise (back recovery):** press any 2 buttons at once (within a 1F gap) at knockdown. It has the same total duration as a normal rise; you just end up further back [KANI], [FGG], https://takukakugamer.com/sf6_getup/.

**Hard knockdown** (no back rise allowed)
- Punish-counter throws.
- Many punish-counter cr.HP/cr.HK sweeps.
- Command throws.
- SA3 and CA.
- Some CH/PC knockdowns.
- Sources: [KANI], takukakugamer.

**Knockdown advantage examples**
- DI hit: +23 [ROZE].
- Drive Reversal hit: +23 [MAYUGE].
- Ken forward throw: +20 [UFD-KEN].

**Not found**
- Universal knockdown durations and the back-rise distance: **UNCONFIRMED**.

**Wake-up buffer**
- The reversal buffer after a knockdown is 7F (was 4F before 2024-02) [MAYUGE].
- Mar 2026 patch: "+3 frames buffer on wake-up" [EH-ALEX].

## 12. Input System

| Item | Value | Source |
|---|---|---|
| Base input buffer | 4F. Forward dash 7F (2023-09). Reversal after knockdown 7F, plus 3 more in Mar 2026 | [MAYUGE], [SAVA], [EH-ALEX] |
| Motion windows | 236/214 on the ground: 9F (was 11 before S2). 63214/41236: 9F (was 12). In the air: 11F. 623: 8F from the last diagonal to the button. SA motions: 10F | [EH-S2], [SAVA] |
| Simultaneous press | 1F gap allowed. Stronger button wins; kick wins ties at equal strength | [SAVA], [KANI], https://h2blo.com/game/st6-frame/ |
| Jump input leniency | 2F | h2blo |
| Special cancel window | Generally: during hitstop plus the cancel window. Exact window **UNCONFIRMED** | — |
| Light chains | Rapid-cancel lights (official "Can be rapid canceled"). Chain window **UNCONFIRMED** | [OFF-SAGAT] |
| Kara cancel, negative edge | **UNCONFIRMED** | — |

## 13. Round Flow

| Item | Value | Source |
|---|---|---|
| Timer | 99. Pauses during DR blackouts and SA3/CA cinematics | [MOCHI] |
| Frames per timer count | **UNCONFIRMED** | — |
| Stage half-width | 765 units from center to corner | Tsugu tweet (search snippet) |
| Starting distance, camera max separation, KO slow-mo, intro lock | **UNCONFIRMED** | — |

## 14. Presentation Cues

All of the following is **UNCONFIRMED**. It is observational description from general knowledge, not sourced numbers:
- "COUNTER" and "PUNISH COUNTER" text banners appear on the hit side.
- Drive Impact has a large ink/paint-splash effect in drive colours, with a sustained armor-absorb flash.
- Crumple is a slow collapse; wall splat shows the body pinned to a cracked wall.
- Perfect Parry gives a brief slowdown/greyscale freeze. Simultaneous DIs make a clash flash, then push both players apart.

Durations: none found.

---

## Top 20 Numbers That Matter Most

1. Drive gauge: 6 bars × 10,000 = 60,000; full every round.
2. Costs: Parry 0.5 (+50/frame held), DI 1, Parry DR 1 total, CDR 3, OD 2, Drive Reversal 2.
3. DI: 26 startup, 2 active, 35 recovery (62 total), −3 on block, 2-hit armor frames 1–27, 800 damage.
4. DI drive damage: block −0.5, hit −1, punish counter or armor-crumple −1.5.
5. Burnout: +4 blockstun, 25% chip from specials/SA/DI, about 20–24 s to recover with no actions.
6. Drive Parry: 1F startup, minimum 12 active, 33 recovery; perfect parry = first 2F.
7. Perfect parry punish scaling ×0.5; parry gains +0.5 (projectile), +1 (strike), +2 (SA/DI).
8. Drive Rush +4 hit/block on the first normal; ×0.85 combo multiplier once per combo.
9. Parry DR: 45F total, 10F blackout, attack from about frame 8–9. CDR: 46F, 9F blackout, attack from frame 10.
10. Drive Reversal: 20F startup (18F on wake-up), −6 on block, 500 white damage, invincible through active.
11. Counter hit +20% / +2F; punish counter +20% / +4F; punish-counter throw 170% (2040) + hard knockdown.
12. Combo scaling 100, 100, 80, 70, 60… down to a 10% floor; lights, cr.MK and DI starters take 20% (100, 80, 70…).
13. SA minimum damage 30% / 40% / 50% (CA 50%); CA available at ≤25% health; special into SA3 adds +10% scaling.
14. Throw: 5F startup, 3 active, 30F whiff total, 1200 damage, ~9F tech window; throw invulnerability 1F after wake-up, 2F after stun.
15. Jump: 4F pre-jump, 45F total, 3F landing.
16. Ken: forward dash 19F / 132.3 units; back dash 23F / 92.3 units; walk 4.7 forward / 3.2 back.
17. Hitstop: light 9, medium 11, heavy 13 (cancelable cr.MK 9).
18. Health 10,000; stage is ±765 units from center.
19. Buffers: 4F base; 7F for dash and reversal on wake-up; motion inputs 9F (ground 236/214).
20. Drive regen: about 60 units/frame idle, about 90 forward walk, about 30 airborne (contested). Regen pauses: block 90, parry 20, DI/OD/DR/CDR 120, whiffed parry or parry DR 240.

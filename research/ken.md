# SF6 Ken: move data and Modern control mapping

Compiled 2026-11-10. Sources:
- **[OFF]** Capcom official frame data: streetfighter.com/6/character/ken/frame and /movelist. This is the primary source and is assumed to be the current patch.
- **[UFD]** ultimateframedata.com/sf6/ken: stats, knockdown (KD) advantage numbers and patch history.
- **[DOC]** "Ken's REAL Combo Trials" Google Doc. Its damage numbers predate the latest patch; the author says the routes still work.
- **[JP]** Japanese Modern Ken guides: volx.jp command list (Modern inputs), kamigame.jp (Modern evaluation and combos), goziline.com, sukoreru.com, note.com posts by hairanndo, teoteskatol and SUGAKNEE.
- **[MISC]** dood.jp, dashfight, esports.gg, dexerto, eventhubs, infil.net and the official Fighting Ground page.

SuperCombo wiki could not be used: the fetch tool refused it (allowlist, then an Anubis bot block), and it was not worked around. **Juggle start/increase/limit values, hitbox ranges, projectile speeds and durability, jump arc frames and throw range all come from SuperCombo-type data, so they are UNCONFIRMED.**

Notation: numpad (5 = neutral, 2 = down, 6 = forward). Classic buttons are LP MP HP LK MK HK. Modern buttons are L M H SP (special) and A (Assist).

**Attack height.** The official site uses Capcom's terms. "High" (上段) means the move can be blocked standing or crouching; I write it as **mid**. "Mid" (中段) means it must be blocked standing; I write it as **OH** (overhead). "Low" stays **low**.

**Drive Gauge (DG) column.** Format is hit gain / block damage / Punish Counter (PC) damage. The full gauge is 60000 (6 bars × 10000).

**Total frames** = (startup − 1) + active + recovery, computed from the official numbers. **D** means the move knocks down; KD+ numbers are from [UFD]/[DOC].

## F. Stats and universal moves
| Item | Value |
|---|---|
| Health | 10,000 [UFD] |
| Walk speed | forward 4.7, back 3.2 (units per frame) [UFD] |
| Forward dash | 19 frames total, 132.321 distance [OFF/UFD] |
| Back dash | 23 frames total, 92.300 distance; throw-invulnerable frames 1-15 [OFF/UFD] |
| Prejump | 4 frames [UFD]. Airtime, landing and jump arc are UNCONFIRMED. Jump normals have 3 frames of landing recovery [OFF]. |
| Drive Impact: **Flame Strike** (HP+HK; Modern: DI button) | Startup 26, active 26-27, recovery 35 (total 62). Block −3. 800 damage. Starter scaling 20%. Super armor (2 hits) frames 1-27. Wall splat if pushback hits the wall. On Punish Counter, or on a hit after armoring, it crumples (generic SF6 system behaviour). Drive damage on block 5000, on PC 15000. Costs 1 bar. |
| Drive Reversal: **Lightning Crack** (6+HP+HK; Modern: 6+DI) | From block: startup 20, active 20-22, recovery 26, fully invulnerable 1-22. From wakeup: startup 18, active 18-20, recovery 26, invulnerable 1-20. Block −6, 500 recoverable damage, D on hit, +5 recovery on hit. Costs 2 bars [dashfight]. |
| Drive Parry (MP+MK; Modern: DP button) | Active 1-12 (extends while held), recovery 33. Can cancel into Drive Rush from frame 4. Always in a punish-counter state. Perfect Parry = hit during the first 2 frames [dashfight]; 50% starter scaling [OFF]. |
| Parry Drive Rush | 45 frames total. Screen freeze to frame 10. Attack possible from frame 9; non-parry actions from frame 24. 15% multiplier scaling. Costs 1 bar. |
| Cancel Drive Rush (66 from a cancelable normal) | 46 frames total. Screen freeze to frame 9. Attack from frame 10; actions from frame 25. 15% scaling. Costs 3 bars. The first normal out of Drive Rush gets +4 on hit and block [dashfight]. |
| OD moves | Cost 2 bars. |
| Burnout | +4 blockstun on the burnt-out player; specials chip; a Drive Impact in the corner stuns [dashfight/infil]. |

## E. Throws [OFF, UFD]
| Throw | Classic | Modern | Startup | Active | Rec | Dmg | KD adv | Notes |
|---|---|---|---|---|---|---|---|---|
| Knee Strikes (forward) | 5 or 6 + LP+LK | 5 or 6 + L+M (or Throw button) | 5 | 5-7 | 23 | 1200 | +20 [UFD, dood] | Opponent stays in front. PC: 2040 damage, +4000 SA, hard KD. DG +2000, PC −10000. SA gain 2000. |
| Hell Wheel (back) | 4+LP+LK | 4+L+M (or 4+Throw) | 5 | 5-7 | 23 | 1200 | +6 [UFD] vs +19 [dood]: **conflict, UNCONFIRMED** | Rolling throw; the opponent ends up on the side Ken was facing away from (sides switch). Same PC properties. |

Throw range is UNCONFIRMED.

## A. Normals [OFF; KD from UFD]
| Move | Classic | Modern | St | Active | Rec | Tot | Hit | Blk | Dmg | DG | SA | Cancel | Ht | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| st.LP | 5LP | 5L | 4 | 4-6 | 7 | 13 | +4 | −1 | 300 | 250/500/2000 | 300 | special/SA, rapid cancel (chain) | mid | 20% starter scaling |
| st.LK | 5LK | A+5L | 5 | 5-6 | 12 | 18 | 0 | −2 | 300 | 250/500/2000 | 300 | none | mid | 20% starter |
| st.MP | 5MP | A+5M | 5 | 5-8 | 14 | 22 | +4 | −2 | 600 | 1500/3000/4000 | 500 | special/SA, TC to HP | mid | June 2025 patch: hitbox reduced, foot hurtbox larger |
| st.MK | 5MK | 5M | 8 | 8-10 | 20 | 30 | +3 | −5 | 600 | 2000/4000/6000 | 700 | SA only; TC to MK | mid | |
| st.HP | 5HP | A+5H | 10 | 10-14 | 17 | 31 | +3 | −2 | 800 | 2000/5000/8000 | 1000 | special/SA | mid | Long-range shimmy punisher |
| st.HK | 5HK | 5H | 12 | 12-13 | 23 | 36 | +1 | −5 | 800 | 3000/6000/8000 | 1000 | none | mid | 30% starter. +2 recovery on whiff or armor. PC = spinning KD (comboable). |
| cr.LP | 2LP | only as a chain after 2L (not standalone) [JP] | 4 | 4-5 | 9 | 14 | +5 | −1 | 300 | 250/500/2000 | 300 | special/SA, rapid cancel | mid | 20% starter |
| cr.LK | 2LK | 2L | 5 | 5-7 | 10 | 17 | +1 | −3 | 200 | 250/500/2000 | 300 | official cancel cell blank; [OFF] notes rapid cancel | low | 20% starter. Chains into cr.LP / st.LP. |
| cr.MP | 2MP | **not available** [JP] | 6 | 6-8 | 14 | 22 | +5 | 0 | 600 (UFD shows 700, pre-patch) | 1500/3000/4000 | 600 | special/SA | mid | +2 recovery on whiff (June 2025). Key link/frame-trap normal. |
| cr.MK | 2MK | 2M | 7 | 7-9 | 19 | 28 | −2 | −6 | 500 | 1000/2000/4000 | 600 | special/SA/Drive Rush | low | 20% starter. Main poke. |
| cr.HP | 2HP | 2H | 8 | 8-11 | 24 | 35 | +3 | −7 (UFD −10: conflict) | 800 | 2000/6000/8000 | 1000 | special/SA, only until 2nd active frame | mid | Forces standing. Anti-air. |
| cr.HK | 2HK | 3H | 8 | 8-10 | 24 | 34 | D (KD +40) | −10 | 900 | 3000/4000/10000 | 1000 | none | low | +1 recovery on block. PC = hard KD with extended down time. |
| j.LP | j.LP | **not available** [JP] | 5 | 5-11 | 3 land | – | – | – | 300 | 500/1500/2000 | 300 | – | OH | |
| j.LK | j.LK | j.L | 6 | 6-11 | 3 land | – | – | – | 300 | 500/1500/2000 | 300 | – | OH | Cross-up |
| j.MP | j.MP | **not available** [JP] | 6 | 6-9 | 3 land | – | – | – | 700 | 1000/2500/4000 | 500 | special (air tatsu) | OH | Forces a juggle state on air hit (air-to-air into 214KK/623KK) |
| j.MK | j.MK | j.M | 7 | 7-12 | 3 land | – | – | – | 500 | 1000/2500/4000 | 500 | – | OH | Cross-up |
| j.HP | j.HP | j.H (diagonal) | 9 | 9-14 | 3 land | – | – | – | 800 | 2000/4000/5000 | 1000 | – | OH | Slam KD on counter/PC |
| j.HK (diagonal) | j.HK | **not available** [JP] | 10 | 10-16 | 3 land | – | – | – | 800 | 2000/4000/5000 | 1000 | – | OH | |
| nj.HK | 8 HK | 8 H (neutral jump) [JP: "vertical jump H 900"] | 6 | 6-11 | 3 land | – | – | – | 900 | 2000/4000/5000 | 1000 | – | OH | Used after a DI crumple |

The neutral vs diagonal jump split is officially listed only for HK. Neutral-jump versions of the other buttons are UNCONFIRMED (they likely share data).

On-block numbers for jump-ins depend on height. UFD lists early-hit values: j.HP +8 block, j.MK +6, nj.HK +9 (these are illustrative, not fixed).

## B. Command normals, target combos and Quick Dash [OFF; UFD]
| Move | Classic | Modern | St | Active | Rec | Tot | Hit | Blk | Dmg | DG | SA | Cancel | Ht | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Quick Dash (run, 風迅脚) | LK+MK/any KK | M+H | – | – | – | 45 | – | – | – | – | – | Follow-ups below | – | Forward Step Kick available from frame 10; other branches from frame 12. Can be cancelled into from specials-cancelable normals (e.g. 5MP>HP xx KK). Costs no Drive. |
| Emergency Stop | QD > LK | QD > L | – | – | – | 16 | – | – | – | – | – | – | – | Stops the run; used to link (e.g. QD~LK > 2MP). UFD lists "+16". |
| Thunder Kick | QD > MK | QD > M | 18 | 18-20 | 20 | 40 | +3 | −3 | 1000 | 1000/2500/4000 | 500 | SA3/CA only | **OH** | Run overhead (UFD shows startup 11 counted from QD frame) |
| Forward Step Kick | QD > HK | QD > H | 8 | 8-11 | 24 | 35 | +1 | −4 | 800 | 3000/5000/7000 | 1000 | any SA | mid | 20% starter. +4 recovery on whiff or armor. Hurtbox extended (Dec 2024). |
| Chin Buster (TC) | 5MP > HP | A+M > A+M | 2nd hit: 11 | 11-13 | 27 | 40 | D (KD +28) | −14 | 400 | 400/1000/4000 | 300 | special/SA, QD (KK) | mid | 20% combo scaling. First hit = st.MP. |
| Triple Flash Kicks (TC) | 5MK > MK > HK | M > M > M | hit 2: 11 / hit 3: 13 | 11-12 / 13-15 | 27 / 28 | 39 / 43 | D (KD +28) / D (KD +32) | −12 / −11 | 400 / 700 | 1000/2500/4000; 1500/3000/4000 | 400 / 500 | none | mid | 3rd hit is always in a PC state and whiffs on crouchers [DOC]. Full string = 1410 damage. |

Ken has **no** 6HK, 4HK, 6MK or 4MP command normal in SF6. "Forward Step Kick" and "Thunder Kick" exist only as Quick Dash branches. Also listed in the movelist: Senka/Gorai/Kazekama (Jinrai follow-ups, below).

## C. Specials [OFF; KD and per-hit damage from UFD]
| Move | Classic | Modern | St | Active | Rec | Tot | Hit | Blk | Dmg (per hit) | DG | SA | Cancel | Invuln / airborne / notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| L Hadoken | 236LP | n/a | 16 | – | – | 49 | −1 | −7 | 600 | 1000/2500/2000 | 600 | SA3 | Slowest projectile [dood] |
| M Hadoken | 236MP | n/a | 14 | – | – | 49 | −3 | −9 | 600 | same | 600 | SA3 | |
| H Hadoken | 236HP | **5SP** (480 dmg) | 12 | – | – | 49 | −5 | −11 | 600 | same | 600 | SA3 | Fastest. Speed/durability UNCONFIRMED (normal: 1 hit). |
| OD Hadoken | 236PP | A+SP | 12 | – | – | 40 | +2 | −2 | 400,400 (800) | 0/2500/5000 | 600 | SA2 | 2 hits. 20% starter scaling (May 2024). Beats single-hit fireballs (implied by 2 hits). |
| L Shoryuken | 623LP | manual 623L only | 5 | 5-14 | 21 + 12 land | 47 | D (KD +33) | −23 | 1100 (800 from 3rd active frame) | 2000/4000/5000 | 800 | SA3 | Anti-air invulnerable 1-14. Airborne 7-35. 20% starter. |
| M Shoryuken | 623MP | **6SP** (2 hits, 1040) | 6 | 6-15 | 28 + 12 | 55 | D (KD +33) | −28 | 700, 600 (1300) | 4000/4000/5000 | 1000 | SA3 | Anti-air invulnerable 1-8. Airborne 8-43. |
| H Shoryuken | 623HP | manual 623H | 7 | 7-16 | 35 + 15 | 66 | D (KD +25) | −36 | 800, 300, 300 (1400) | 4000/4200/5000 | 1200 | SA3 | Anti-air invulnerable 1-9, projectile invulnerable 5-11. Airborne 9-51. |
| OD Shoryuken | 623PP | A+6SP | 6 | 6-39 | 35 + 15 | 89 | D (KD +22, Dec 2024) | −40 | 200×5, 600 (1600) | 0/5000/0 | 1500 | none | **Fully invulnerable 1-10.** Airborne 9-24 and 33-74. |
| L Tatsumaki | 214LK | manual 214L | 4 | 4-14 | 32 | 46 | D (KD +46; +38 if only the 2nd hit lands) | −14 | 300, 400 (700) | 2000/4000/5000 | 800 | none | Airborne 5-28. Combo extender (2HP xx 214LK > 623MP). |
| M Tatsumaki | 214MK | manual 214M | 14 | 14-30 | 31 | 61 | D (KD +44) | −12 | 450, 450 (900) | 3000/5000/5000 | 1000 | none | Airborne 8-41 |
| H Tatsumaki | 214HK | manual 214H | 16 | 16-47 | 31 | 78 | D (KD +36) | −12 | 300, 600, 400 (1000 listed by OFF) | 4000/6000/5000 | 1200 | none | Airborne 10-61. Corner carry. |
| OD Tatsumaki | 214KK | 214 + L+M (manual) [JP] | 9 | 9-56 | 22 + 12 | 90 | D (KD +28) | −61 | 300, 200, 200, 200, 800 (1700) | 0/4000/5000 | 2000 | none | Airborne 10-77. Pulls opponent in; juggle ender. |
| Air Tatsumaki (forward jump only) | j.214K | j.SP (forward jump) | 11 | 11-27 | 14 land | – | D (limited juggle) | – | 900 | 2000/3000/5000 | 600 | – | 30% starter |
| OD Air Tatsumaki | j.214KK | A+j.SP | 11 | 11-28 | 14 land (+7 on whiff) | – | D | – | 900 (3 hits; 2nd and 4th attacks only hit behind, 700) | 0/3000/5000 | 1000 | – | 30% starter |
| L Dragonlash | 623LK | **not available** | 18 | 18-23 | 21 | 44 | +2 | −4 | 1000 (950 frames 21-23) | 3000/3000/5000 | 1200 | none | OH-type flip kick (official height "High", so blockable low; actual overhead status UNCONFIRMED). Airborne 9-28. Doesn't hit grounded opponents on frames 18-20. 30% starter. |
| M Dragonlash | 623MK | **not available** | 23 | 23-27 | 25 | 52 | +3 | −8 | 1100 | 3000/3000/5000 | 1200 | none | Airborne 14-32. +4 vs crouching [SUGAKNEE]. |
| H Dragonlash | 623HK | **4SP** (960) | 28 | 28-32 | 21 | 53 | +3 | **+1** | 1200 | 2000/3000/5000 | 800 | none | Airborne 19-37. Plus on block. |
| OD Dragonlash | 623KK | A+4SP | 9 | 9-24 | 24 | 48 | +1 | −9 | 1200 (UFD: 2 hits; 700 listed) | 0/3000/5000 | 1200 | none | 10% starter, 5% immediate on 2nd hit. Airborne 10-27. Juggle tool (DI > j.HK > 623KK 623KK SA1). |
| L Jinrai Kick | 236LK | manual 236L | 12 | 12-14 | 28 | 42 | +1 | −11 | 500 | 2000/2000/5000 | 550 | SA3 | Follow-up windows: Kazekama/Gorai 32-35, Senka 30-35 |
| M Jinrai Kick | 236MK | **2SP** (480) or 236M | 16 | 16-18 | 24 | 42 | +2 | −7 | 600 | 2000/2000/5000 | 550 | SA3 | Same windows as L. Combos only from 2MK or HP [dood]. |
| H Jinrai Kick | 236HK | manual 236H | 25 | 25-27 | 19 | 46 | D (KD +42; launches in corner juggles) | −2 | 700 | 3000/4000/5000 | 550 | SA3 | Windows: Kazekama/Gorai 36-41, Senka 34-41 |
| OD Jinrai Kick | 236KK | A+2SP | 13 | 13-15 | 25 | 40 | −4 | −7 | 600 | 0/3000/5000 | 400 | SA2 | OD follow-up windows: Kaze/Gorai 21-27, Senka 19-27 |
| Kazekama Shin Kick | Jinrai > 6LK | Jinrai > 4SP, or 6L | 6 | 6-9 | 19 | 28 | +3 | −5 | 600 (UFD 500) | 2000/2000/5000 | 400 | SA3 | **Low.** KD on counter/PC. Always in a PC state. 20% starter / 15% combo. In corner juggles it re-launches (Jinrai loop). |
| OD Kazekama | OD Jinrai > 6LK | > 4SP | 6 | 6-8 | 20 | 28 | +3 | −5 | 500 | 0/3000/5000 | 400 | SA2 | Kasai from frame 11 |
| Gorai Axe Kick | Jinrai > 6MK | Jinrai > 2SP, or 6M | 18 | 18-20 | 20 | 40 | +3 | −3 | 1000 | 1000/2500/4000 | 500 | SA3 | **Overhead.** Ground bounce on air hit (May 2024). Always PC state. |
| OD Gorai Axe Kick | OD Jinrai > 6MK | > 2SP | 17 | 17-19 | 24 | 43 | −3 | −7 | 1000 | 0/2500/4000 | 500 | SA2 | Kasai from frame 24. 20% starter. |
| Senka Snap Kick | Jinrai > 6HK | Jinrai > 5SP/6SP, or 6H | 10 | 10-12 | 25 | 37 | D (KD +33/33/38 from L/M/H Jinrai) | −3 | 800 | 2000/7000/7000 | 600 | SA3 | **Whiffs on crouchers.** Big drive damage. Always PC state. |
| OD Senka Snap Kick | OD Jinrai > 6HK | > 5SP/6SP | 10 | 10-15 | 18 | 33 | D | −4 | 800 | 0/7000/7000 | 600 | SA2 | Kasai from frame 22 |
| Kasai Thrust Kick | OD follow-up > 6K | > SP | 15 / 11 / 15 (after Kaze / Gorai / Senka) | 3f | 29 / 29 / 37 | 46/42/54 | D (KD +31/+34/+42) | −12/−12/−20 | 500 | 0/5000/5000 | 600 | SA2 | 40% starter (June 2025), 20% combo |
| [QD] Shoryuken | QD > 623P | QD > 6SP (1360) | 8 | 8-17 | 33 + 15 | 65 | D (KD +25) | −35 | 500, 450, 250, 250, 250 (1700) | 3500/4000/5000 | 1400 | SA3 | Anti-air invulnerable 1-18, projectile invulnerable 3-11, airborne 11-51 |
| [QD] Tatsumaki | QD > 214K | QD > 2SP [volx, goziline, SUGAKNEE] (hairanndo says 4SP: conflict) | 13 | 13-58 | 20 | 78 | **+3** (not KD) | −9 | 200×4, 400 (1200) | 3600/6000/5000 | 1300 | none | 15% starter. Hits 2/4/6/8/10 only hit behind. Airborne 10-61. Corner carry; +5 after two dashes [SUGAKNEE]. |
| [QD] Dragonlash | QD > 623K | QD > 4SP | 9 | 9-23 | 24 | 47 | +2 | −8 | 1200 (2 hits) | 4000/3000/5000 | 1200 | none | Airborne 10-28. Crosses over in juggles (side switch). |

**How the run (QD) buffs specials:**
- **Shoryuken:** 1700 damage vs 1400, 18 frames of anti-air invulnerability, projectile invulnerability.
- **Tatsumaki:** becomes +3 on hit (no knockdown) and −9 on block, versus a −12 knockdown.
- **Dragonlash:** startup 9 vs 18-28, no meter needed (it behaves like the OD version).

## D. Super Arts [OFF; KD from UFD/DOC]
| SA | Classic | Modern | St | Active | Rec | Tot | Hit | Blk | Dmg | Min dmg | Invuln | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| SA1 Dragonlash Flame | 214214K | 5 or 6 + SP+H (1600 at 80%) | 7 | 7-9 | 41 | 50 | D (KD +9, Dec 2024; side switch) | −24 | 2000 | 30% | strike + throw 1-9 | DG on block 2500 |
| SA2 Shippu Jinrai-kyaku | 236236K | 4 + SP+H (2160) | 6 | 6-61 | 28 | 89 | D (KD +36) | **−5** (June 2025) | 2800 (UFD/dashfight 2700: conflict) | 40% | none listed | Goes into the full attack (cinematic) after the 6th hit. DG on block 5000. |
| SA3 Shinryu Reppa | 236236P | 2 + SP+H (3200) | 7 | 7-45 | 55 | 100 | D (KD +15) | −40 (June 2025) | 4000 | 50% | **fully invulnerable 1-18** | 10% immediate scaling (only during Thunder Kick or when cancelled from a special). If the first hit is blocked or whiffs: no cinematic, 2100 damage, 9000 DG. |
| CA Shinryu Reppa (≤25% HP) | same | same | 7 | 7-45 | 55 | 100 | D (KD +21) | −40 | 4500 | 50% | 1-18 | Partial version: 2300 damage. DG block 16000 / PC 20000. |

Cinematic length in frames is UNCONFIRMED. Modern manual-motion mapping (214214+H / 236236+H) and which SA it gives is UNCONFIRMED.

## G. MODERN controls
**Default PS layout** (esports.gg and dexerto agree on the face buttons; videogamer disagrees; the official site confirms the shoulder buttons):

| PS button | Modern function |
|---|---|
| Square | Light |
| Cross | Medium |
| Circle | Heavy |
| Triangle | Special (SP) |
| R2 | Assist |
| L1 | Drive Impact |
| R1 | Drive Parry |
| L2 | Throw |

Shortcuts:
- Throw = L+M [volx]
- Quick Dash = M+H [volx, SUGAKNEE]
- Drive Reversal = 6+DI
- Drive Rush = 66 during parry, or after a cancelable hit
- Two-button DI/DP shortcuts (H+SP, M+H) are UNCONFIRMED. M+H is QD for Ken.

**Normals:**

| Modern input | Gives |
|---|---|
| 5L | st.LP |
| A+5L | st.LK |
| 2L | cr.LK; repeat for cr.LP chain |
| 5M | st.MK |
| M,M,M | Triple Flash Kicks |
| A+5M | st.MP |
| A+M,A+M | Chin Buster |
| 2M | cr.MK |
| 5H | st.HK |
| A+5H | st.HP |
| 2H | cr.HP |
| 3H | cr.HK sweep |
| j.L | j.LK |
| j.M | j.MK |
| j.H | j.HP |
| nj.H | nj.HK |

- **Missing in Modern:** standalone cr.LP, cr.MP, j.LP, j.MP, diagonal j.HK, L/M Hadoken, L/M Dragonlash, and Tatsumaki on an SP button.
- 4/6 + button normals for Modern Ken are UNCONFIRMED (the official list has none for Ken).
- Assist + 2/3 + button is UNCONFIRMED.

These mappings were assembled from [JP] guides (hairanndo, sukoreru, goziline, kamigame, SUGAKNEE) and are consistent across them. Mark them **reported**, not officially tabulated.

**SP specials (80% damage):**

| Modern input | Gives |
|---|---|
| 5SP | H Hadoken |
| 6SP | M Shoryuken |
| 4SP | H Dragonlash |
| 2SP | M Jinrai |
| j.SP (forward jump) | Air Tatsu |

Manual motions use L/M/H for strength at full damage:
- 623+button = Shoryuken. Heavy DP is manual only.
- 236+button = Jinrai (not Hadoken).
- 214+button = Tatsu. OD Tatsu = 214+L+M [goziline].

**OD:** A+SP plus a direction, e.g. A+6SP = OD DP (goziline, hairanndo, dexerto). One Steam guide claims DP+SP; treat that as wrong or minority.

**Jinrai follow-ups:**
- 4SP = Kazekama (low)
- 2SP = Gorai (overhead)
- 5SP or 6SP = Senka
- Manual 6L / 6M / 6H also work. Kamigame and goziline routes use 6L and 6H.
- Kasai = SP during an OD follow-up.

**Quick Dash (M+H) branches:**
- L = Emergency Stop, M = Thunder Kick, H = Forward Step Kick
- 6SP = run DP
- 2SP = run Tatsu
- 4SP = run Dragonlash (hairanndo conflicts on 2SP/4SP)
- Run specials with manual motions also work (kamigame: 623+button during QD).

**Super Arts:** 5/6+SP+H = SA1, 4+SP+H = SA2, 2+SP+H = SA3 [volx, hairanndo]. Damage is 80% when done with SP.

**Damage rule:** specials and supers done with SP (one-button) do 80%; manual motions do 100% (eventhubs, infil). Assisted combos use whatever input they generate.

**Assist combos** (volx: A+L ×3, A+M ×5, A+H ×5; contents from kamigame, sukoreru and teoteskatol):

| Assist combo | Sequence | Damage |
|---|---|---|
| A+L ×3 | st.LK > L Jinrai > Kazekama Shin Kick (low) | 1050 |
| A+M ×5 | st.MP > HP (Chin Buster) > Quick Dash > [QD] Dragonlash (crosses over) > SA2 (needs 2 bars) | 3310 with SA2. Midscreen 4-hit without SA ≈ 1770 from Drive Rush [hairanndo] (UNCONFIRMED raw). |
| A+H ×5 | st.HP > OD Jinrai > OD Kazekama > Kasai Thrust Kick > SA3 (needs 3 bars) | 4150 (4000 in burnout, when it uses M Jinrai > Kazekama instead of OD) |

- Exact enders with insufficient SA meter are UNCONFIRMED.
- On block, the A+M string reportedly stops after Chin Buster / QD [sukoreru], UNCONFIRMED.

## H. Combos (regression tests)
Classic routes are from [DOC] (pre-latest-patch damage; 2MP routes may be slightly lower now). "KD" is the advantage after the ender.

| # | Route | Dmg | Cost | KD | Notes / tight links |
|---|---|---|---|---|---|
| 1 | 2LP 5LP 5LP xx 623HP | 1790 | – | +25 | Light confirm; chains |
| 2 | 2LK 2LP xx 214LK > 623MP | 1940 | – | +33 | Link the M DP as soon as Ken lands from the tatsu |
| 3 | 5MK MK HK (Triple Flash) | 1410 | – | +32 | 3rd hit whiffs on crouchers |
| 4 | (PC) 5HP > 2MK xx 623HP | 2580 | – | +25 | Shimmy punish. 5HP PC +? → 2MK link (UNCONFIRMED gap) |
| 5 | j.HP > 2HP xx 214LK > 623MP | 3070 | – | +33 | Jump-in BnB (2MP version 2970) |
| 6 | 5MP HP xx KK ~ 623K > 623HP | 2540 | – | +26 | Run DLK side-switch; wait for the switch before DP |
| 7 | 5MP HP xx KK ~ 623K > 236236P | 3970 (CA 4245) | SA3 | +15/+21 | |
| 8 | 5MP HP xx KK ~ 214K | 1640+ | – | +43 | Run tatsu carry (1640 with 2LP starter) |
| 9 | 2MK DRC 2HP xx KK ~ LK > 5MP HP xx KK ~ 623P | 2180 | 3 bars | +22 | cr.MK Drive Rush + run-stop link (QD~LK then 5MP is a tight link, exact gap UNCONFIRMED) |
| 10 | 2MK DRC 5MP 2HP xx 236HK dl.6LK > 236LK 6HK xx SA3 | 4260 (CA 4510) | 3 bars + SA3 | +16 | Corner. 6LK delayed 17-18 frames. |
| 11 | (PC) 5HK DR 5HP xx KK ~ 623P | 2297 | 1 bar (parry DR) | +22 | st.HK PC spins. Delay the DP 1 frame for an SA3 follow-up. |
| 12 | j.HP 2HP xx 236HK dl.6LK > 236LK > 6LK > 623HP | 3430 | – | +27 | Corner Jinrai loop. 6LK delay 17-18 frames (≈2f window). |
| 13 | DI (blocked, wall splat) 5HP xx 236HK > 6HK > 623MP | 2440 (3060 if DI hit) | 1 bar | +33 | Corner wall-splat route |
| 14 | DI wall splat 5HP xx 236KK dl.6HK > 236KK dl.6HK > 236KK dl.6MK > 6K xx SA3 | 5032 (CA 5282) | 1 + 6 bars, SA3 | +16 | First delay 17+ frames |
| 15 | (PC) DI > nj/j.HK > dl.KK ~ 623K > 623HP | 3220 | 1 bar | +26 | DI crumple; 2-8 frame delay before QD |
| 16 | (PC) DI > j.HK > 623KK > 623KK > SA1 | 3720 | 1+4 bars, SA1 | +34 | Anywhere. First OD DLK must hit once. |
| 17 | (PC) 236PP DR 5HP xx KK ~ 623P | 2724-3120 | 3 bars | +22 | Fireball punish full screen |
| 18 | (PC) KK ~ 214K > 2MP xx 214LK > 623MP | 3405 | – | +33 | Run-tatsu reversal punish (+3 on hit allows the 2MP link) |
| 19 | 5HP DRC 5HP 2MP DRC 5HP 5MP HP xx KK ~ 623K SA3 | 5146 (CA 5396) | 6 bars + SA3 | +16 | Max-resource punish |
| 20 | Anti-air: 2HP or 623LP; air-to-air j.MP xx 214KK > 623KK > SA3 | 5990 | OD + SA3 | +16 | j.MP forces a juggle state |
| M1 | Modern: 2L 2L 5L > 623H (manual) | 1490 | – | – | kamigame. SP DP gives M, which whiffs. |
| M2 | Modern: 2M DRC A+M > A+M > A+M (Chin Buster > QD) > 2SP (run tatsu) | 1738 | 3 bars | – | kamigame |
| M3 | Modern: 2M DRC A+M > A+M,A+M > 2SP(M Jinrai) > 6SP(Senka) > 623H | 1997-2108 | 3 bars | – | Corner-ish |
| M4 | Modern: A+H (corner) > 236H > 6L > 236L > 6L > 623H | 3220 | – | – | Modern Jinrai loop, late 6L |
| M5 | Modern: 2M DRC 2H DRC A+H > A+M,A+M > M+H > 623(button) > 2SP(Jinrai) > H > SA3 | 4337 | 6 bars + SA3 | – | Kill route |
| M6 | Modern: (PC) 5H(st.HK) parry-DR A+H > M+H > QD 623 | 2297 | 1 bar | – | |

## I. How Ken plays neutral (for the AI)
Ken is a rushdown shoto who wins by converting small hits into big corner carry.

**Mid-range pokes:**
- cr.MK (7f, low): always buffered into Drive Rush when he has 3+ bars. On hit he goes into DR 2HP/5HP combos; on block DR gives +4 pressure.
- st.MP (5f) and st.HK, the long +1 on hit spinning PC: whiff punishes against walk-forward.
- st.HP: long reach; used to punish throw attempts (shimmy) for 5HP PC > 2MK xx 623HP.

**Anti-air:** cr.HP (forces stand) or L Shoryuken, invulnerable 1-14.

**Hadoken:** used sparingly to build drive gauge; OD Hadoken beats fireballs.

**Jinrai Kick:** pressure off cr.MK/HP. H Jinrai is −2 and leaves a delayed three-way guess:
- Kazekama (low, +3)
- Gorai (overhead, +3)
- Senka (big drive damage, whiffs crouchers)

Ken delays the follow-up to bait buttons. All follow-ups are punish-counterable, so opponents mash or DI them.

**Run (KK / M+H) mixups** after knockdowns and plus frames:
- QD ~ Thunder Kick (overhead +3 / −3)
- QD ~ Forward Step Kick (mid, −4, SA-cancel)
- QD ~ Emergency Stop into throw or 2LK
- QD ~ Shoryuken (anti-air/projectile-invulnerable reversal-bait)
- QD ~ Tatsu (+3 hit / −9 block)

**Oki:** run tatsu ender + two forward dashes = +5 meaty setups [SUGAKNEE]. H Dragonlash (+1 on block, overhead-looking) is a gap closer.

**Habits to imitate:** shimmy (walk back from throw range, then 5HP); delayed Jinrai follow-ups; DR 5HP pressure; DI against predictable OD/drive rush; jump-in cross-ups with j.LK/j.MK.

## UNCONFIRMED summary
- Juggle start / increase / limit values for all moves.
- Hitbox ranges in pixels, projectile speeds and durability, and throw range.
- Jump airtime and arc (only prejump 4 is known).
- Cinematic lengths of the SAs.
- Exact Modern 4/6-direction normals.
- Modern manual SA motion mapping.
- Assist-combo enders when meter is short.
- Back-throw KD advantage (+6 vs +19).
- cr.HP on-block value (−7 OFF vs −10 UFD).
- SA2 damage (2800 OFF vs 2700 dashfight).
- Whether the Dragonlash Kick must be blocked standing (OFF lists it as "High", i.e. not an overhead).
- Exact frame gaps of the run-stop links.

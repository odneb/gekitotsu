# SF6 Ken spatial data research (partial; most spatial items UNCONFIRMED)

Method note: srk.shib.live (SRK wiki mirror, which holds the Ken/Movement/Game Data pages) refused WebFetch (Anubis "Access Denied"). Per instructions I did not work around it. wiki.supercombo.gg and one Google-doc export were not on the fetch allowlist. No fetchable source with per-move reach/hitbox extents was found.

## Sources actually read
- Capcom official frame data: https://www.streetfighter.com/6/en-us/character/ken/frame (frame data only, no distances)
- Ultimate Frame Data: https://ultimateframedata.com/sf6/ken (movement numbers, hitboxes only as images)
- esports.net wiki: https://www.esports.net/wiki/guides/ken-street-fighter-guide/ (movement table, same numbers as UFD, unit unstated)
- dood.jp: https://dood.jp/en/_print/sf6/ken (frame data only)
- Mobalytics: https://mobalytics.gg/blog/street-fighter-6/how-to-beat-ken/
- The Loadout: https://www.theloadout.com/street-fighter-6/ken-guide
- Hotspawn Drive guide: https://www.hotspawn.com/street-fighter/guide/street-fighter-6-drive-system
- Hitbox colour legend (frames site): https://frames-cloudflare.pages.dev/more
- Destructoid Drive Impact guide: https://destructoid.com/?p=382156
- Dashfight Modern controls: https://dashfight.com/news/classic-vs-modern-controls-in-street-fighter-6-and-beyond-3123
- StrategyWiki Ken (move list names only): https://strategywiki.org/wiki/Street_Fighter_6/Ken

## 1. Reach / range per move, units, reference lengths
- UNCONFIRMED: numeric reach for every normal, command normal and special. UFD shows hitbox images only; no site fetched lists "attack range".
- UNCONFIRMED: unit system, pushbox width, standing height, round-start distance, stage width, throw range (the "0.8" guess is not confirmed).
- Only qualitative: Mobalytics calls st.HK "fast startup, short range, long recovery" and lists st.MK, cr.MK, st.HP and mis-spaced Dragonlash as whiff-punish targets. Dragonlash is "a mid-range pulling attack that swaps positions with the opponent" (The Loadout).
- Hitbox viewer colour legend (hurtbox green, hitbox red, throw pink, proximity guard grey, purple armor): https://frames-cloudflare.pages.dev/more

## 2. Movement (secondary sources; unit NOT stated, treat as relative only)
UFD and esports.net agree except one typo (132.321 vs 132.231):
- Fwd walk speed 4.7; back walk speed 3.2 (UFD)
- Fwd dash: 19 frames, distance 132.231 (UFD wrote 132.321); back dash: 23 frames, distance 92.300
- Back dash throw-invincible frames 1-15 (Capcom)
- Prejump 4 frames (UFD). esports.net: neutral jump "4+39+3", fwd/back jump "4+38+3" (prejump+air+landing; so about 46/45 total frames; page garbled, moderate confidence)
- esports.net: fwd jump distance 190, back jump distance 152 (unit unstated)
- Quick Dash total 45 frames; Emergency Stop 16 frames; Forward Step Kick cancellable from QD frame 10, other branches from frame 12 (Capcom)
- Parry Drive Rush 45 total frames (screen freeze until f10, can cancel to attack f9); Cancel Drive Rush 46 total (freeze until f9, attack from f10) (Capcom)
- UNCONFIRMED: jump apex height, Quick Dash speed/distance, Drive Rush distance (from parry vs cancel), step distance of Forward Step Kick / Dragonlash / Jinrai.

## 3. Poke ranking by reach
- UNCONFIRMED (no numbers). Frame data for context (Capcom: startup / active / total-recovery as listed):
  st.LP 4/4-6/7; st.LK 5/5-6/12; st.MP 5/5-8/14; st.MK 8/8-10/20; st.HP 10/10-14/17; st.HK 12/12-13/23 (+2 on armor hit or miss)
  cr.LP 4/4-5/9; cr.LK 5/5-7/10 (low); cr.MP 6/6-8/14; cr.MK 7/7-9/19 (low); cr.HP 8/8-11/24; cr.HK 8/8-10/24 (low, knockdown)
  (Capcom column labels were garbled in extraction; "recovery" may be recovery frames.)
- Forward movement per move: UNCONFIRMED.

## 4. Hurtbox notes
- Confirmed by Capcom: Shoryuken, Tatsumaki Senpu-kyaku and Dragonlash Kick have "considered airborne" frame ranges (exact ranges not captured). OD Shoryuken fully invincible frames 1-10; DP startups L5/M6/H7. Hitbox-legend page notes Ken's HP Shoryuken is air-invulnerable and projectile-invulnerable.
- Mobalytics: all anti-airs beat Dragonlash; a jab can knock it out of the air. Light Hadoken can stop Drive Rush and Dragonlash.
- UNCONFIRMED: which normals extend the hurtbox before the hitbox, low-profile moves, hitbox shape of Shoryuken and cr.HP anti-air.

## 5. Pushback
- UNCONFIRMED for all normals (no source found). Jab-string out-of-range counts and corner pushback behaviour UNCONFIRMED.
- Block advantage (Capcom/UFD context): jabs -1, st.MK -5, st.HP -2, st.HK -5.

## 6. Animations (only what sources say)
- Shoryuken: no fire on regular versions (including HP); fire on OD versions and run-cancel Shoryuken (The Loadout).
- Quick Dash: input KK; LK cancels run, MK = Thunder Kick (overhead), HK = Forward Step Kick (no longer knocks down, cancellable); OD follow-up = Kasai Thrust Kick.
- Jinrai Kick: follow-ups Kazekama Shin Kick (low), Gorai Axe Kick (overhead, ground-bounce on air hit), Senka Snap Kick.
- Target combos: Chin Buster 2nd (5MP,HP), Triple Flash Kicks (5MK,MK,HK).
- Throws: Knee Strikes and Hell Wheel (5f startup, 1200 dmg).
- Super Arts: SA1 Dragonlash Flame (7f, invincible 1-9, kick chain), SA2 Shippu Jinrai-kyaku (6f, multi-hit windows to frame 61), SA3 Shinryu Reppa (7f, invincible 1-18; CA at <25% vitality).
- Drive Impact = "Flame Strike" (26f startup, 2-hit armor, wall splat; crumple if it hits mid-move); Drive Reversal = "Lightning Crack".
- Ken look: classic-style hair, large coat, sweatpants, big boots (The Loadout).
- UNCONFIRMED: limb-by-limb pose descriptions for every move, st.HP vs st.HK animation, cr.MK extension, crumple, wall-splat, win pose, idle stance, parry stance. Modern controls: Assist+Heavy auto combo is not Ken-specific in any source; which neutral-Heavy move Ken gets is UNCONFIRMED. (Heavy picks punch/kick by character/situation per Dashfight.)

## 7. Presentation timing
- Perfect Parry: slow motion/screen freeze, guaranteed combo after (Hotspawn); perfect-parry window 2 frames; Drive Parry active 8 frames tapped (Hotspawn); Capcom: strike perfect parry 1f with 5 frames full invincibility after; projectile perfect parry 10 frames with no screen freeze.
- UNCONFIRMED: KO slow-motion length/speed, Drive Impact clash slow-mo, paint splash colours, screen shake, SA1/SA2/SA3 cinematic lengths, round-intro timing, start positions.

## Recommended next step
Capture hitbox extents directly: open SF6 Training Mode hitbox display or the Frame Assist Tool/SRK pages in a real browser, or use a data-mined source (not found here).

# writes script.txt: every slot at four moments (with the clip the game uses, and with the hand pose)
import json, sys
mv = json.load(open('/home/claude/gkn/moves.json'))
states = ['idle','walkf','walkb','crouch','prejump','jump','land','dashf','dashb','run','drush','tech','win','blockS','blockC','parry','prec','hitL','hitM','hitH','hitLow','hitCr','crumple','splat','ahit','kd','getup','thrown','grab','bite','throw1','throw2','throw3','throw4']
def frames(m):
    su, ac, t = m['su'] or 5, m['ac'] or 3, m['t']
    if m.get('has_proj'): su = m['pf'] or su
    tot = t if t < 900 else su + ac + 30
    return [max(1, su // 2), su, su + ac + (tot - su - ac) // 3, tot - 2]
out = []
which = sys.argv[1] if len(sys.argv) > 1 else 'clip'
only = sys.argv[2].split(',') if len(sys.argv) > 2 else None
for s in states:
    if only and s not in only: continue
    for k, f in enumerate([0, 6, 12, 20]):
        out.append('lab|%s|%d%s' % (s, f, '|none' if which == 'pose' else '')); out.append('wait|1'); out.append('shot|%s_%s_%d' % (which, s, k))
for m in mv:
    if only and m['key'] not in only: continue
    for k, f in enumerate(frames(m)):
        out.append('lab|mv_%s|%d%s' % (m['key'], f, '|none' if which == 'pose' else '')); out.append('wait|1'); out.append('shot|%s_mv_%s_%d' % (which, m['key'], k))
out.append('quit')
open('/home/claude/gkn/script.txt', 'w').write('\n'.join(out) + '\n'); print(len(out))

# Build, cook and stage the Mac game (run from the editor's Python watcher).
import subprocess, threading, os, time, unreal
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    if a.get_actor_label().startswith('GkT_'): a.destroy_actor()
P = PROJECT.rstrip('/')
UAT = '/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/RunUAT.sh'
LOG = os.path.join(P, 'Saved', 'package.log')
def run():
    t = time.time()
    with open(LOG, 'w') as f:
        r = subprocess.call([UAT, 'BuildCookRun', '-project=' + os.path.join(P, 'GekitotsuNative.uproject'), '-noP4', '-platform=Mac',
                             '-clientconfig=Development', '-build', '-cook', '-stage', '-pak', '-unattended', '-utf8output', '-ignorecookerrors'], stdout=f, stderr=subprocess.STDOUT)
    open(os.path.join(P, 'Saved', 'package.done'), 'w').write('%d %.0fs %s\n' % (r, time.time() - t, time.ctime()))
threading.Thread(target=run, daemon=True).start()
log('build started')

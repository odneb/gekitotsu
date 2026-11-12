# Open the built game once with the arguments in Saved/run_args.txt (run from the editor's Python watcher).
import subprocess, os, time
P = PROJECT.rstrip('/')
subprocess.run("pkill -f MacOS/GekitotsuNative", shell=True); time.sleep(3.5)
try: extra = open(os.path.join(P, 'Saved', 'run_args.txt')).read().split()
except Exception: extra = []
out = os.path.join(P, 'Saved/game.out')
subprocess.Popen(['open', '-n', os.path.join(P, 'Saved/StagedBuilds/Mac/GekitotsuNative.app'), '--stdout', out, '--stderr', out, '--args'] + extra + ['-gkstatus=' + os.path.join(P, 'Saved/status.txt')])
log('game opened with %s' % extra)

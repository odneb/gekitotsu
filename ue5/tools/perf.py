# Measures the frame time of a match with nothing else going on.
import unreal, time, traceback
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
def match():
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    a = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.GkMatch) if w else []
    return a[0] if len(a) else None
def cmd(c):
    try: m = match(); log('  >', '%.2f' % time.time(), c, '=>', (m.debug(c) if m else 'NO MATCH')[:110])
    except Exception: log(traceback.format_exc())
later(2, lambda: LES.editor_request_begin_play())
later(5, lambda: [cmd('cpu 0 1'), cmd('set idle 0')])
later(9, lambda: cmd('state')); later(13, lambda: cmd('state')); later(13.1, lambda: cmd('lumen 1')); later(17, lambda: cmd('state')); later(17.1, lambda: cmd('lumen 0')); later(20, lambda: cmd('state'))
later(20.5, lambda: LES.editor_request_end_play())

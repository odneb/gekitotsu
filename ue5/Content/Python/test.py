# Drives the game for testing: starts it in the editor, sends it commands, takes screenshots, stops it.
import unreal, traceback
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

def match():
    try:
        w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        a = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.GkMatch) if w else []
        return a[0] if len(a) else None
    except Exception: log(traceback.format_exc()); return None

def cmd(c):
    m = match(); r = m.debug(c) if m else 'NO MATCH'; log('  >', c, '=>', r); return r

def shot(name, w=1600, h=900):
    unreal.AutomationLibrary.take_high_res_screenshot(w, h, 't_%s.png' % name); log('  shot', name)

def play(steps, total=None):
    """steps: (seconds since the previous step, 'command; command' or ('shot', name))"""
    try: LES.editor_request_begin_play(); log('play started')
    except Exception: log(traceback.format_exc()); return
    t = 3.0
    for d, a in steps:
        t += d
        if isinstance(a, tuple): later(t, lambda a=a: shot(a[1]))
        else: later(t, lambda a=a: [cmd(c.strip()) for c in a.split(';')])
    def stop():
        try: LES.editor_request_end_play(); log('play stopped')
        except Exception: log(traceback.format_exc())
    later(t + (total or 2.5), stop)

steps = [(1.0, 'cpu 0 1; cpu 1 1; set idle 0'), (0.5, ('shot', 'intro'))]
for k in range(5): steps += [(2.2, ('shot', 'fight%d' % k)), (0.2, 'state')]
steps += [(0.5, 'hp 0 240; meter 1 3000; set stormrate 3'), (2.5, ('shot', 'storm0')), (2.0, ('shot', 'storm1')), (0.2, 'state')]
steps += [(0.6, 'cpu 0 0; cpu 1 0; unset stormrate; reset'), (2.2, 'meter 0 3000; state'), (0.1, 'in 0 192 3'), (0.3, ('shot', 'super0')), (0.6, ('shot', 'super1')), (0.7, ('shot', 'super2')), (0.2, 'state')]
play(steps)

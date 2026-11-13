"""Import the user's YOUFULCA announcer and purchased TSUKAERU Male 6 takes."""
import unreal, os
from pathlib import Path
ann=Path(os.environ.get('GK_ANNOUNCER_WAV',str(Path.home()/'Downloads/Narration_You-Fulca/wav')))
robot=Path(os.environ.get('GK_MALE6_WAV',str(Path(__file__).resolve().parents[3]/'voice-pack/tsukaeru_battle_male/audio/wav/male6')))
lines={'Ann_Round1':'ROUND01','Ann_Round2':'ROUND02','Ann_Round3':'ROUND03','Ann_Final':'Shiai_final','Ann_Fight':'ROUND_Fight','Ann_KO':'ROUND_KO','Ann_Win':'ROUND_YouWin','Ann_Perfect':'ROUND_Perfect','Ann_Time':'Hanyou_TIMEUP','Ann_Combo':'Hanyou_WIN_02'}
selected={'R6_Intro':'battle/ikuze_Lets_go','R6_Beam':'battle/kurae_Take_this','R6_Home':'battle/nigasanai_I_wont_let_you_get_away','R6_AntiAir':'skill/maiagare_rise','R6_Super1':'skill/hazero_Explode','R6_Super2':'battle/moratta_gotcha','R6_Super3':'skill/todoroke_Blast','R6_Critical':'skill/rengoku_Inferno','R6_Win':'system_etc/mataasobo_Lets_play_again','R6_Parry':'battle/sonoteido_Is_that_all_youve_got','R6_Oops':'battle/otto_oops','R6_Defeat':'battle/damage5'}
for k in range(1,5): selected['R6_Attack'+str(k)]='battle/attack'+(str(k) if k>1 else '')
for k in range(1,4): selected['R6_Hurt'+str(k)]='battle/damage'+(str(k) if k>1 else '')
tasks=[]
for folder,source,group in [('Announcer',ann, {k:'Narration-'+v for k,v in lines.items()}),('Robot1',robot,selected)]:
 for key,name in group.items():
  filename=source/(name+'.wav');assert filename.exists(),str(filename)
  t=unreal.AssetImportTask();t.filename=str(filename);t.destination_path='/Game/GkVoice/'+folder;t.destination_name=key;t.automated=True;t.replace_existing=True;t.save=True;tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
 assert t.imported_object_paths,t.destination_name
 unreal.log('FILM VOICE '+t.destination_name+' '+str(t.imported_object_paths))

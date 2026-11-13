import unreal,math
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
L.load_level('/Game/DreamscapeSeries/DreamscapeDreadmarsh/Maps/Dreadmarsh_DemoMap_Resize')
mesh=unreal.load_asset('/Game/DreamscapeSeries/DreamscapeDreadmarsh/Meshes/Foliage/Trees/OakTree/SM_DreadMarsh_OakTree_01')
b=mesh.get_bounds();unreal.log('FILM OAK BOUNDS '+str(b))
label='GkFilm_CanopyLeft'
actor=next((a for a in A.get_all_level_actors() if a.get_actor_label()==label),None)
x,y,z=1800,-1700,-5150;rad=math.radians(73)
pos=unreal.Vector(-23164+x*math.cos(rad)-y*math.sin(rad),-21088+x*math.sin(rad)+y*math.cos(rad),z)
if not actor:actor=A.spawn_actor_from_class(unreal.StaticMeshActor,pos,unreal.Rotator(yaw=100));actor.set_actor_label(label)
actor.set_actor_rotation(unreal.Rotator(yaw=100),True)
c=actor.static_mesh_component;c.set_static_mesh(mesh);c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
scale=2400/max(1,b.box_extent.z*2);actor.set_actor_scale3d(unreal.Vector(scale,scale,scale));actor.set_actor_location(pos,False,True)
unreal.log('FILM CANOPY '+str(pos)+' scale '+str(scale))
# This arena never enters the demo's ocean underwater post-process volume.
for a in A.get_all_level_actors():
 if a.get_class().get_name() in ('BP_UnderwaterHandler_C','RecastNavMesh','NavMeshBoundsVolume') or 'WaterBodyOcean' in a.get_class().get_name():
  unreal.log('FILM REMOVE UNUSED DEMO ACTOR '+a.get_name());A.destroy_actor(a)
for a in A.get_all_level_actors():
 for light in a.get_components_by_class(unreal.PointLightComponent):
  a.modify();light.modify();light.set_editor_property("cast_shadows",False)
  unreal.log('FILM FILL SHADOWS DISABLED '+a.get_name())
for a in A.get_all_level_actors():
 if isinstance(a,unreal.Landscape):
  a.modify();a.set_editor_property('use_dynamic_material_instance',True)
  m=a.get_editor_property('landscape_material')
  unreal.log('FILM LAND MATERIAL '+str(m)+' SCALARS '+str(unreal.MaterialEditingLibrary.get_scalar_parameter_names(m)))
# The actual landscape owns the painted floor; no overlay cards or raised slabs.
film_land=unreal.load_asset('/Game/GkFx/MI_GkFilmLandscape')
assert film_land
for actor in A.get_all_level_actors():
 if actor.get_actor_label().startswith('GkFilm_PathStone_'): A.destroy_actor(actor)
 elif isinstance(actor,unreal.Landscape):
  actor.modify();actor.set_editor_property('landscape_material',film_land)
  actor.set_editor_property('use_dynamic_material_instance',True)
assert L.save_current_level()
L.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()

import unreal

unreal.EditorLoadingAndSavingUtils.load_map('/Game/Castle/Maps/Castle')
actors = unreal.EditorLevelLibrary.get_all_level_actors()
unreal.log('CVAD_CASTLE_ACTORS count={}'.format(len(actors)))
for actor in actors:
    name = actor.get_class().get_name()
    if name in ('PlayerStart', 'NavMeshBoundsVolume', 'WorldSettings', 'BlockingVolume') or 'Landscape' in name:
        unreal.log('CVAD_CASTLE_KEY {} {} loc={}'.format(name, actor.get_actor_label(), actor.get_actor_location()))

mins = unreal.Vector(1e9, 1e9, 1e9)
maxs = unreal.Vector(-1e9, -1e9, -1e9)
count = 0
for actor in actors:
    if actor.get_class().get_name() != 'StaticMeshActor':
        continue
    origin, extent = actor.get_actor_bounds(False)
    if extent.x > 100000 or extent.y > 100000:
        continue
    mins.x=min(mins.x,origin.x-extent.x); mins.y=min(mins.y,origin.y-extent.y); mins.z=min(mins.z,origin.z-extent.z)
    maxs.x=max(maxs.x,origin.x+extent.x); maxs.y=max(maxs.y,origin.y+extent.y); maxs.z=max(maxs.z,origin.z+extent.z)
    count += 1
unreal.log('CVAD_CASTLE_BOUNDS meshes={} min={} max={}'.format(count, mins, maxs))
near=[]
for actor in actors:
    if actor.get_class().get_name()!='StaticMeshActor': continue
    loc=actor.get_actor_location()
    if abs(loc.x)<15000 and abs(loc.y)<15000:
        origin,extent=actor.get_actor_bounds(False)
        near.append((extent.x*extent.y,actor.get_actor_label(),loc,extent))
for _,label,loc,extent in sorted(near,reverse=True)[:40]:
    unreal.log('CVAD_CASTLE_NEAR {} loc={} extent={}'.format(label,loc,extent))
for actor in actors:
    label=actor.get_actor_label().lower()
    if any(key in label for key in ('castle','floor','courtyard','fountain')):
        unreal.log('CVAD_CASTLE_SITE {} {} loc={} extent={}'.format(actor.get_class().get_name(),actor.get_actor_label(),actor.get_actor_location(),actor.get_actor_bounds(False)[1]))

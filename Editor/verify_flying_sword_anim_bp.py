import unreal

bp = unreal.load_asset('/Game/CVAD/Animations/ABP_LanFang_FlyingSword')
blend = unreal.load_asset('/Game/CVAD/Animations/BS_LanFang_FlyingSword')
assert bp and bp.generated_class(), 'Flying sword AnimBP or generated class missing'
assert blend, 'Flying sword BlendSpace missing'
names = [sample.get_editor_property('animation').get_name()
         for sample in blend.get_editor_property('sample_data')]
unreal.log('CVAD_FLYING_BP_CLASS={}'.format(bp.generated_class().get_path_name()))
unreal.log('CVAD_FLYING_BLEND_SAMPLES={}'.format(','.join(names)))
assert names == ['Anim_FS_Standing', 'Anim_FS_Walk', 'Anim_FS_Run']

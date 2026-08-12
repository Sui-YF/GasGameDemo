import unreal

blend = unreal.load_asset('/Game/LanFang/Animations/In-Place/MoveBasic/Female_2D')
unreal.log('CVAD_BLEND_CLASS=' + str(blend.get_class().get_name()))
unreal.log('CVAD_BLEND_METHODS=' + ','.join(x for x in dir(blend) if 'sample' in x.lower() or 'axis' in x.lower()))
for prop in ('sample_data', 'blend_parameters', 'target_weight_interpolation_speed_per_sec'):
    try:
        unreal.log('CVAD_BLEND_PROP {}={}'.format(prop, blend.get_editor_property(prop)))
    except Exception as exc:
        unreal.log_warning('CVAD_BLEND_PROP_FAIL {}={}'.format(prop, exc))
for index, sample in enumerate(blend.get_editor_property('sample_data')):
    unreal.log('CVAD_SAMPLE_DIR_{}={}'.format(index, ','.join(x for x in dir(sample) if not x.startswith('_'))))
    for prop in ('animation', 'sample_value', 'rate_scale'):
        try:
            unreal.log('CVAD_SAMPLE_{}_{}={}'.format(index, prop, sample.get_editor_property(prop)))
        except Exception as exc:
            unreal.log_warning('CVAD_SAMPLE_FAIL_{}_{}={}'.format(index, prop, exc))

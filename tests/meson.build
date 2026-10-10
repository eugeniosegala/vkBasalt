config_reload_test = executable(
    'config_reload_test',
    'config_reload_test.cpp',
    '../src/config.cpp',
    '../src/logger.cpp',
    include_directories: include_directories('../src'),
)

test('config reload', config_reload_test)

live_effects_test = executable(
    'live_effects_test',
    'live_effects_test.cpp',
    include_directories: include_directories('../src'),
)

test('live effect selection', live_effects_test)

toggle_key_poll_test = executable(
    'toggle_key_poll_test',
    'toggle_key_poll_test.cpp',
    include_directories: include_directories('../src'),
)

test('toggle key polling interval', toggle_key_poll_test)

reshade_module_test = executable(
    'reshade_module_test',
    'reshade_module_test.cpp',
    '../src/stb_image.c',
    '../src/config.cpp',
    '../src/logger.cpp',
    include_directories: include_directories('../src'),
    dependencies: reshade_dep,
)
test('live custom shader compilation', reshade_module_test)

descriptor_set_test = executable(
    'descriptor_set_test',
    'descriptor_set_test.cpp',
    '../src/descriptor_set.cpp',
    '../src/logger.cpp',
    include_directories: include_directories('../src'),
)
test('sampler-free shader descriptors', descriptor_set_test)

dds_test = executable('dds_test', 'dds_test.cpp', '../src/stb_image.c', include_directories: include_directories('../src'))
test('bounded DDS textures', dds_test)

vulkan_construction_test = executable(
    'vulkan_construction_test', 'vulkan_construction_test.cpp',
    objects: vkBasalt_library.extract_all_objects(recursive: false),
    link_with: keyboard_input_x11_lib,
    dependencies: [x11_dep, reshade_dep],
    include_directories: include_directories('../src'),
)
test('transactional Vulkan construction', vulkan_construction_test)

layer_interface_test = executable(
    'layer_interface_test', 'layer_interface_test.cpp',
    objects: vkBasalt_library.extract_all_objects(recursive: false),
    link_with: keyboard_input_x11_lib,
    dependencies: [x11_dep, reshade_dep],
    include_directories: [include_directories('../src'), vkBasalt_include_path],
)
test('Vulkan layer version and forwarding', layer_interface_test,
     args: [join_paths(meson.project_build_root(), 'packaging/vkBasalt.x86_64.json'),
            join_paths(meson.project_build_root(), 'packaging/vkBasalt.x86.json')])

hdr_policy_test = executable('hdr_policy_test', 'hdr_policy_test.cpp',
    include_directories: [include_directories('../src'), vkBasalt_include_path])
test('explicit HDR colour and graph lifetime', hdr_policy_test)

# Analytic dense sweep, float32 evaluation, and generated-source freshness.
test('HDR sharpening transfer accuracy', find_program('python3'),
    args: [files('../scripts/generate-hdr-sharpen-curve.py'), '--check'])

depth_lifetime_test = executable('depth_lifetime_test', 'depth_lifetime_test.cpp',
    objects: vkBasalt_library.extract_all_objects(recursive: false),
    link_with: keyboard_input_x11_lib, dependencies: [x11_dep, reshade_dep],
    include_directories: [include_directories('../src'), vkBasalt_include_path])
foreach scenario : ['colour', 'pending', 'unbound', 'out-of-order', 'failures', 'drain-failure', 'churn', 'fence-reuse', 'fence-rollback']
    test('depth lifetime ' + scenario, depth_lifetime_test, args: [scenario])
endforeach

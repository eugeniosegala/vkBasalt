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

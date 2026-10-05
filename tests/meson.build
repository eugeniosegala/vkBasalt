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

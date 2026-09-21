config_reload_test = executable(
    'config_reload_test',
    'config_reload_test.cpp',
    '../src/config.cpp',
    '../src/logger.cpp',
    include_directories: include_directories('../src'),
)

test('config reload', config_reload_test)

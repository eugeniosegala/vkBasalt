project('vkBasalt', ['c', 'cpp'], default_options: ['c_std=c11', 'cpp_std=c++2a'])

cpp = meson.get_compiler('cpp')

# Reviewed API support, separate from the header pin used by the packager.
vkbasalt_api_version = '1.4.365'
header_revision = import('fs').read('vulkan-headers-revision.txt').strip()
header_version = header_revision.substring(1)
header_version_args = header_version.replace('.', ', ')
vulkan_headers = get_option('vulkan_headers')
if vulkan_headers != ''
    add_project_arguments('-I' + vulkan_headers, language: 'cpp')
endif
header_args = vulkan_headers == '' ? [] : ['-I' + vulkan_headers]
if not cpp.compiles('''
    #include <vulkan/vulkan.h>
    static_assert(VK_HEADER_VERSION_COMPLETE >= VK_MAKE_API_VERSION(0, @0@));
    int main() { return 0; }
'''.format(header_version_args), args: header_args, name: 'Vulkan Headers ' + header_version + ' or newer')
    error('Use Vulkan Headers ' + header_version + ' or newer; see README.md or scripts/package-mako-release.sh')
endif
version_config = configuration_data()
version_config.set('VKBASALT_API_VERSION', 'VK_MAKE_API_VERSION(0, ' + vkbasalt_api_version.replace('.', ', ') + ')')
configure_file(output: 'vulkan_version.hpp', configuration: version_config)

vkBasalt_include_path = include_directories('.')

lib_dir = get_option('libdir')
ld_lib_dir_vkbasalt = ''

if get_option('append_libdir_vkbasalt')
    lib_dir = join_paths(lib_dir, 'vkbasalt')
    ld_lib_dir_vkbasalt = get_option('prefix') + '/$LIB/vkbasalt/'
endif

subdir('packaging')

if get_option('with_so')
    subdir('src')
    subdir('tests')
endif

if get_option('with_json')
    subdir('config')
endif

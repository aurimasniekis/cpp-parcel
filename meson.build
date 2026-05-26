project('parcel', 'cpp',
    version: '0.2.0',
    license: 'MIT',
    default_options: [
        'cpp_std=c++23',
        'warning_level=3',
        'werror=false',
    ],
    meson_version: '>=1.3.0',
)

json_dep = dependency('nlohmann_json', version: '>=3.12.0',
    fallback: ['nlohmann_json', 'nlohmann_json_dep'])

parcel_inc = include_directories('include')

parcel_dep = declare_dependency(
    include_directories: parcel_inc,
    dependencies: [json_dep],
    version: meson.project_version(),
)

meson.override_dependency('parcel', parcel_dep)

install_subdir('include/parcel', install_dir: get_option('includedir'))

import('pkgconfig').generate(
    name: 'parcel',
    description: 'Wrappable, wire-transferable C++23 value system with JSON serialization',
    version: meson.project_version(),
    requires: ['nlohmann_json'],
)

if get_option('tests')
    subdir('tests')
endif
if get_option('examples')
    subdir('examples')
endif

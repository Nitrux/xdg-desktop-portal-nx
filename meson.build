project(
    'xdg-desktop-portal-nx',
    'cpp',
    version: '1.0.0',
    meson_version: '>=1.0.0',
    default_options: ['cpp_std=c++23', 'buildtype=release'],
)

qt6 = import('qt6')
qt = dependency('qt6', version: '>=6.6', modules: ['Core', 'DBus', 'Gui', 'Qml', 'Quick'])
kf6_core = dependency('KF6CoreAddons', version: '>=6.13')
cpp_compiler = meson.get_compiler('cpp')
kf6_i18n = declare_dependency(
    include_directories: include_directories(get_option('prefix') / 'include' / 'KF6' / 'KI18n'),
    dependencies: [cpp_compiler.find_library('KF6I18n'), cpp_compiler.find_library('KF6I18nQml')],
)
mauikit = dependency('MauiKit4', version: '>=4.0.5')
mauikit_filebrowsing = dependency('MauiKitFileBrowsing4', version: '>=4.0.5')

cpp_args = ['-DQT_NO_CAST_FROM_ASCII', '-DQT_NO_CAST_TO_ASCII', '-DQT_NO_URL_CAST_FROM_STRING']
if host_machine.cpu_family() == 'x86_64'
    cpp_args += ['-march=x86-64-v3', '-mtune=generic']
endif
if get_option('buildtype') != 'debug'
    cpp_args += ['-DQT_NO_DEBUG_OUTPUT', '-DQT_NO_INFO_OUTPUT']
endif

portal_sources = [
    'src/main.cpp',
    'src/portalrequest.cpp',
    'src/filechooser.cpp',
    'src/appchooser.cpp',
    'src/openuri.cpp',
    'src/settings.cpp',
]
portal_moc = qt6.preprocess(
    qresources: 'src/qml.qrc',
    moc_headers: [
        'src/portalrequest.hpp',
        'src/filechooser.hpp',
        'src/appchooser.hpp',
        'src/openuri.hpp',
        'src/settings.hpp',
    ],
    moc_sources: ['src/portalrequest.cpp'],
)

executable(
    'xdg-desktop-portal-nx',
    portal_sources + portal_moc,
    dependencies: [qt, kf6_core, kf6_i18n, mauikit, mauikit_filebrowsing],
    cpp_args: cpp_args,
    install: true,
    install_dir: get_option('libexecdir'),
)

test_sources = qt6.preprocess(qresources: 'tests/test.qrc', moc_sources: ['tests/main.cpp'])
executable(
    'xdg-desktop-portal-nx-test',
    ['tests/main.cpp'] + test_sources,
    dependencies: [qt, kf6_core, kf6_i18n, mauikit],
)

install_data('data/nx.portal', install_dir: get_option('datadir') / 'xdg-desktop-portal' / 'portals')
install_data('data/org.freedesktop.impl.portal.desktop.nx.service', install_dir: get_option('datadir') / 'dbus-1' / 'services')
install_data('data/portals.conf', install_dir: get_option('sysconfdir') / 'xdg-desktop-portal')

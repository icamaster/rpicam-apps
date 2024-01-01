rpicam_app_src += files([
    'null_preview.cpp',
    'preview.cpp',
])

preview_headers = files([
    'preview.hpp',
])

enable_drm = get_option('enable_drm')
drm_deps = dependency('libdrm', required : false)

if enable_drm and drm_deps.found()
    rpicam_app_dep += drm_deps
    rpicam_app_src += files('drm_preview.cpp')
    cpp_arguments += '-DLIBDRM_PRESENT=1'
else
    enable_drm = false
endif

enable_egl = get_option('enable_egl')
x11_deps = dependency('x11', required : false)
sdl_dep = dependency('sdl2', required : true)
epoxy_deps = dependency('epoxy', required : true)

imgui_prj = subproject('imgui')
imgui_dep = dependency('imgui', required : true)

if enable_egl and x11_deps.found() and sdl_dep.found() and epoxy_deps.found()
    rpicam_app_dep += [x11_deps, sdl_dep, epoxy_deps, imgui_dep]
    rpicam_app_src += files('egl_preview.cpp')
    cpp_arguments += '-DLIBEGL_PRESENT=1'
else
    enable_egl = false
endif

enable_qt = get_option('enable_qt')
qt_dep = dependency('qt5', modules : ['Core', 'Widgets'], required : false)

if enable_qt and qt_dep.found()
    rpicam_app_dep += qt_dep
    rpicam_app_src += files('qt_preview.cpp')
    cpp_arguments += '-DQT_PRESENT=1'
else
    enable_qt = false
endif

install_headers(preview_headers, subdir: meson.project_name() / 'preview')

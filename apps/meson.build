rpicam_hello = executable('rpicam-hello', files('rpicam_hello.cpp'),
                          include_directories : include_directories('..'),
                          dependencies: libcamera_dep,
                          link_with : rpicam_app,
                          install : true)

# Install symlinks to the old app names for legacy purposes.
install_symlink('libcamera-hello',
                install_dir: get_option('bindir'),
                pointing_to: 'rpicam-hello')

if enable_tflite
    rpicam_detect = executable('rpicam-detect', files('rpicam_detect.cpp'),
                               include_directories : include_directories('..'),
                               dependencies: [libcamera_dep, boost_dep],
                               link_with : rpicam_app,
                               install : true)

    install_symlink('libcamera-detect',
                    install_dir: get_option('bindir'),
                    pointing_to: 'rpicam-detect')
endif

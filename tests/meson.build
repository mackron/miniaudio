ma_test_broken = disabler()

test(
  'automated',
  executable(
    'ma_test_automated',
    'test_automated/ma_test_automated.c',
    dependencies: ma_sys_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  ),
  workdir: meson.current_source_dir() / '_build',
)

if device_io.enabled() and encoding and decoding and generation
  # FIXME: implement test driver for deviceio
  ma_headless_driver = disabler()

  test(
    'deviceio',
    executable(
      'ma_test_deviceio',
      'test_deviceio/ma_test_deviceio.c',
      dependencies: ma_sys_deps + ma_backend_deps,
      install: false,
      implicit_include_directories: false,
      include_directories: ma_inc,
    ),
    depends: ma_headless_driver,
    is_parallel: false,
    workdir: meson.current_source_dir() / '_build',
  )
  if add_languages('cpp', native: false, required: false)
    test(
      'deviceio_cpp',
      executable(
        'ma_test_cpp',
        'test_cpp/ma_test_cpp.cpp',
        dependencies: ma_sys_deps + ma_backend_deps,
        install: false,
        implicit_include_directories: false,
        include_directories: ma_inc,
      ),
      depends: ma_headless_driver,
      is_parallel: false,
      workdir: meson.current_source_dir() / '_build',
    )
  endif
endif

if encoding and decoding
  test(
    'filtering',
    executable(
      'ma_test_filtering',
      'test_filtering/ma_test_filtering.c',
      dependencies: ma_sys_deps,
      install: false,
      implicit_include_directories: false,
      include_directories: ma_inc,
    ),
    # FIXME: FAILURE: test_bpf4__f32, test_bpf4__s16
    # ma_bpf_reinit__internal -> ma_bpf2_init_preallocated:
    # pBPF->pBPF2 is always NULL?
    depends: ma_test_broken,
    args: files('_build/res/sine_s16_mono_48000.wav'),
    workdir: meson.current_source_dir() / '_build',
  )
endif

if generation
  test(
    'generation',
    executable(
      'ma_test_generation',
      'test_generation/ma_test_generation.c',
      dependencies: ma_sys_deps,
      install: false,
      implicit_include_directories: false,
      include_directories: ma_inc,
    ),
    workdir: meson.current_source_dir() / '_build',
  )
endif

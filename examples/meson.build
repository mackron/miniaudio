ma_example_broken = disabler()

if 'sdl' in backends and device_io.enabled()
  executable(
    'custom_backend',
    'custom_backend.c',
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
endif

if decoding and device_io.enabled() and (
  opus_dep.found()
  and opusfile_dep.found()
  and vorbis_dep.found()
  and vorbisfile_dep.found()
)
  executable(
    'custom_decoder',
    'custom_decoder.c',
    dependencies: ma_sys_deps
    + ma_backend_deps
    + [opus_dep, opusfile_dep, vorbis_dep, vorbisfile_dep],
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )

  if engine and resource_manager.enabled()
    executable(
      'engine_custom_decoder',
      'engine_custom_decoder.c',
      dependencies: ma_sys_deps
      + ma_backend_deps
      + [opus_dep, opusfile_dep, vorbis_dep, vorbisfile_dep],
      install: false,
      implicit_include_directories: false,
      include_directories: ma_inc,
    )
  endif
endif

if decoding and device_io.enabled()
  executable(
    'data_source_chaining',
    'data_source_chaining.c',
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
endif

if node_graph.enabled() and device_io.enabled()
  executable(
    'duplex_effect',
    'duplex_effect.c',
    # FIXME: duplex_effect.c:119:14:
    # error: too few arguments to function ‘ma_audio_buffer_ref_init’; expected 6, have 5
    ma_example_broken,
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
endif

if engine and device_io.enabled() and resource_manager.enabled()
  executable(
    'engine_hello_world',
    'engine_hello_world.c',
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
  executable(
    'engine_advanced',
    'engine_advanced.c',
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
  if real_sdl2_dep.found()
    executable(
      'engine_sdl',
      'engine_sdl.c',
      dependencies: ma_sys_deps + ma_backend_deps + real_sdl2_dep,
      install: false,
      implicit_include_directories: false,
      include_directories: ma_inc,
    )
  endif
endif

if engine and node_graph.enabled()
  executable(
    'engine_effects',
    'engine_effects.c',
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
endif

if engine
  phonon_dep = dependency('phonon', required: false)
  if phonon_dep.found()
    executable(
      'engine_steamaudio',
      'engine_steamaudio.c',
      dependencies: ma_sys_deps + ma_backend_deps + phonon_dep,
      install: false,
      implicit_include_directories: false,
      include_directories: ma_inc,
    )
  endif
endif

if device_io.enabled() and engine
  executable(
    'hilo_interop',
    'hilo_interop.c',
    # FIXME: not working: hilo_interop.c:122:14:
    # error: too few arguments to function ‘ma_sound_init_from_data_source’; expected 6, have 5
    ma_example_broken,
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
endif

if node_graph.enabled() and decoding and device_io.enabled()
  executable(
    'node_graph',
    'node_graph.c',
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
endif

if resource_manager.enabled() and device_io.enabled()
  executable(
    'resource_manager',
    'resource_manager.c',
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
  executable(
    'resource_manager_advanced',
    'resource_manager_advanced.c',
    # FIXME: resource_manager_advanced.c:159:9:
    # error: unknown type name ‘ma_resource_manager_job’
    ma_example_broken,
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
endif

if device_io.enabled() and encoding
  executable(
    'simple_capture',
    'simple_capture.c',
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
endif
if device_io.enabled()
  executable(
    'simple_duplex',
    'simple_duplex.c',
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
  executable(
    'simple_enumeration',
    'simple_enumeration.c',
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
endif

if device_io.enabled() and (
  'wasapi' in backends
  or (backends == ['_miniaudio_default'] and host_system == 'windows')
)
  executable(
    'simple_loopback',
    'simple_loopback.c',
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
endif

if device_io.enabled() and decoding
  executable(
    'simple_looping',
    'simple_looping.c',
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
  executable(
    'simple_mixing',
    'simple_mixing.c',
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
  executable(
    'simple_playback',
    'simple_playback.c',
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
endif

if device_io.enabled() and generation
  executable(
    'simple_playback_sine',
    'simple_playback_sine.c',
    # FIXME: simple_playback_sine.c:60:48:
    # error: ‘struct <anonymous>’ has no member named ‘name’
    ma_example_broken,
    dependencies: ma_sys_deps + ma_backend_deps,
    install: false,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
endif

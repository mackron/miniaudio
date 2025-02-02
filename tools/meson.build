if decoding and encoding
  audioconverter_exe = executable(
    'audioconverter',
    'audioconverter/audioconverter.c',
    dependencies: ma_sys_deps,
    install: true,
    implicit_include_directories: false,
    include_directories: ma_inc,
  )
  if not meson.is_cross_build()
    meson.override_find_program('audioconverter', audioconverter_exe)
  endif
endif

project(
  'miniaudio',
  'c',
  version: '0.12-dev',
  license: 'Unlicense OR MIT-0',
  license_files: ['LICENSE'],
  default_options: [
    'warning_level=3',
    'c_std=gnu11,c11,gnu99,c99,gnu89,c89',
    'default_library=static',
  ],
  meson_version: '>=1.7.0',
)

cc = meson.get_compiler('c')
host_system = host_machine.system()

ma_c_args = []

backends = get_option('backends')
runtime_linking = get_option('runtime_linking').disable_auto_if(
  # on darwin, disable runtime linking if not requested explictly,
  # to avoid requesting extra entitlements.
  host_system == 'darwin',
).enable_auto_if(
  # enable runtime linking if backend is not specified.
  '_meson_default' in backends,
)
use_sdl = get_option('sdl').enable_if('sdl' in backends)
if runtime_linking.disabled()
  real_sdl2_dep = dependency('sdl2', required: use_sdl)
  sdl_dep = real_sdl2_dep
else
  real_sdl2_dep = disabler()
  # miniaudio have stub types for sdl.
  sdl_dep = declare_dependency()
endif

# features
decoding = get_option('decoding')
encoding = get_option('encoding')
device_io = get_option('device_io').enable_if(backends.length() > 0)
resource_manager = get_option('resource_manager').require(decoding)
engine = get_option('engine')
node_graph = get_option('node_graph').enable_if(engine)
threading = get_option('threading').enable_if(device_io.enabled())
generation = get_option('generation')

ma_global_args = []
if not decoding
  ma_global_args += '-DMA_NO_DECODING'
endif
if not encoding
  ma_global_args += '-DMA_NO_ENCODING'
endif
if device_io.disabled()
  ma_global_args += '-DMA_NO_DEVICE_IO'
endif
if resource_manager.disabled()
  ma_global_args += '-DMA_NO_RESOURCE_MANAGER'
endif
if not engine
  ma_global_args += '-DMA_NO_ENGINE'
endif
if node_graph.disabled()
  ma_global_args += '-DMA_NO_NODE_GRAPH'
endif
if threading.disabled()
  ma_global_args += '-DMA_NO_THREADING'
endif
if not generation
  ma_global_args += '-DMA_NO_GENERATION'
endif

add_project_arguments(ma_global_args, language: ['c', 'cpp'])

# backend not specified?
if '_meson_default' in backends
  if runtime_linking.disabled()
    # opinionated.
    # this does not reflect the default enabled / suppoted backends.
    default_backends = {
      'darwin': ['coreaudio'],
      'linux': ['pulseaudio'],
      'openbsd': ['sndio', 'audio4'],
      'netbsd': ['audio4'],
      'freebsd': ['oss'],
      'dragonfly': ['oss'],
      'windows': ['wasapi', 'dsound', 'winmm'],
      'android': ['aaudio', 'opensl'],
      'emscripten': ['webaudio'],
    }
    if host_system in default_backends
      backends = default_backends[host_system]
      if host_system == 'emscripten'
        # background thread is not properly supported.
        # disable null backend explictly.
        backends += ['custom']
      else
        backends += ['custom', 'null']
      endif
      if sdl_dep.found()
        backends += ['sdl']
      endif
    else
      error(
        'backend unknown at configuration time, but runtime linking is disabled.',
      )
    endif
  else
    # enable all supported backends.
    backends = ['_miniaudio_default']
    runtime_linking = runtime_linking.enable_if(true)
  endif
endif

assert('_meson_default' not in backends, 'default backends should be resolved')

# TODO: use unbundled dr_libs
# https://github.com/mackron/miniaudio/issues/934
if not get_option('dr_wav')
  ma_c_args += '-DMA_NO_WAV'
endif
if not get_option('dr_flac')
  ma_c_args += '-DMA_NO_FLAC'
endif
if not get_option('dr_mp3')
  ma_c_args += '-DMA_NO_MP3'
endif

examples = get_option('examples').disable_auto_if(meson.is_subproject())
tools = get_option('tools').disable_auto_if(meson.is_subproject())
tests = get_option('tests').disable_auto_if(meson.is_subproject())

ma_inc = include_directories('.')

# required system dependencies.
ma_sys_deps = []

if host_system in [
  'linux',
  'openbsd',
  'freebsd',
  'netbsd',
  'dragonfly',
  'android',
  'darwin',
  'emscripten',
]
  if runtime_linking.enabled()
    ma_sys_deps += dependency('dl')
  endif
  ma_sys_deps += dependency('threads', required: threading)
  ma_sys_deps += dependency('atomic', required: threading)
  ma_sys_deps += cc.find_library('m', required: false)
endif

if backends != ['_miniaudio_default']
  ma_c_args += '-DMA_ENABLE_ONLY_SPECIFIC_BACKENDS'
  foreach backend : backends
    ma_c_args += '-DMA_ENABLE_' + backend.to_upper()
  endforeach
endif

ma_backend_deps = []
if runtime_linking.disabled()
  foreach runtime_backend : [
    'wasapi',
    'dsound',
    'winmm',

    'aaudio',
  ]
    if runtime_backend in backends
      # NOTE: https://github.com/mackron/miniaudio/issues/935
      # not planned at the time.
      error('runtime linking is required for backend', runtime_backend)
    endif
  endforeach

  ma_c_args += '-DMA_NO_RUNTIME_LINKING'

  if 'coreaudio' in backends
    ma_backend_deps += dependency('appleframeworks', modules: ['foundation', 'coreaudio'])
    audiotoolbox_dep = dependency('appleframeworks', modules: 'audiotoolbox', required: false)

    if audiotoolbox_dep.found()
      ma_backend_deps += audiotoolbox_dep
    else
      audiounit_dep = dependency('appleframeworks', modules: 'audiounit', required: false)
      if audiounit_dep.found()
        ma_backend_deps += audiounit_dep
      else
        error('unable to find a suitable audio framework to link with')
      endif
    endif
  endif

  if 'sndio' in backends
    ma_backend_deps += dependency('sndio')
  endif
  if 'audio4' in backends
    cc.has_header('sys/audioio.h', required: true)
  endif
  if 'oss' in backends
    cc.has_header('sys/soundcard.h', required: true)
  endif

  if 'pulseaudio' in backends
    if cc.get_argument_syntax() == 'gcc'
      # FIXME: argument qualifier types mismatch:
      # error: initialization of ‘ma_pa_threaded_mainloop_get_retval_proc’
      # {aka ‘int (*)(pa_threaded_mainloop *)’} from incompatible pointer type
      # ‘int (*)(const pa_threaded_mainloop *)’ [-Wincompatible-pointer-types]
      ma_c_args += cc.first_supported_argument('-Wno-error=incompatible-pointer-types')
    endif
    ma_backend_deps += dependency('libpulse')
  endif
  if 'alsa' in backends
    ma_backend_deps += dependency('alsa')
  endif
  if 'jack' in backends
    ma_backend_deps += dependency('jack')
  endif

  if 'opensl' in backends
    ma_backend_deps += dependency('OpenSLES')
  endif

  # TODO: should we support non-wasm webaudio builds?
  if 'webaudio' in backends and host_system == 'emscripten'
    ma_backend_deps += declare_dependency(
      compile_args: [
        '-sWASM_WORKERS=1',
        '-DMA_ENABLE_AUDIO_WORKLETS',
      ],
      link_args: [
        '-sAUDIO_WORKLET=1',
        '-sWASM_WORKERS=1',
        '-sASYNCIFY',
      ],
    )
  endif

  if 'sdl' in backends
    ma_backend_deps += sdl_dep
  endif
endif

# miniaudio + extra modules.
ma_all_libs = []

# FIXME: workaround impl-only defines when building in different TUs.
# need upstream changes to move the defines into header.
ma_impl_workaround = true

subdir('extras')

# NOTE: miniaudio_split will be _the_ only version of miniaudio starting from 0.12:
# https://github.com/mackron/miniaudio/issues/934#issuecomment-2628895596
# this can be removed afterwards.
miniaudio_impl_src = configure_file(
  input: 'miniaudio.h',
  output: 'miniaudio_impl.c',
  copy: true,
)
ma_impl_lib = static_library(
  'miniaudio_impl',
  miniaudio_impl_src,
  ma_extra_srcs,
  extra_files: ['miniaudio.h'],
  c_args: ['-DMINIAUDIO_IMPLEMENTATION'] + ma_c_args,
  dependencies: ma_sys_deps + ma_backend_deps,
  install: false,
  implicit_include_directories: false,
  include_directories: ma_inc,
  pic: true,
  override_options: ma_impl_workaround ? [
    'unity=on',
    'unity_size=32', # should be sufficient for now.
  ] : [],
)
ma_all_libs += ma_impl_lib

ma_lib = library(
  'miniaudio',
  link_whole: ma_all_libs,
  install: true,
)

ma_dep = declare_dependency(
  include_directories: ma_inc,
  dependencies: ma_sys_deps + ma_backend_deps,
  link_with: ma_lib,
  compile_args: ma_global_args,
)

pkg = import('pkgconfig')
pkg.generate(
  ma_lib,
  description: 'Audio playback and capture library written in C, in a single source file.',
  url: 'https://miniaud.io',
)

meson.override_dependency('miniaudio', ma_dep)

if examples.allowed()
  subdir('examples')
endif
if tools.allowed()
  subdir('tools')
endif
if tests.allowed()
  subdir('tests')
endif

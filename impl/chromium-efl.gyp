{
  'variables': {
    'data_dir%': '/usr/share/chromium-efl/',
    'exe_dir%': '/usr/lib/chromium-efl/',
    'edje_compiler%': 'edje_cc',
  },

  'targets': [{
    'target_name': 'chromium-efl',
    'type': 'shared_library',
    'includes': [
      # NOTE: gyp includes need to be relative
      '../src/skia/skia_common.gypi',
    ],
    'include_dirs': [
      '.',
      '<(chrome_src_dir)',
      '<(chrome_src_dir)/third_party/WebKit', # [M34] without this, build errors occur due to #include path changes in M34. for example, see WebFrame.h.
      '<(chrome_src_dir)/third_party/skia/include/core',
      # XXX: This should be fixed, no ewk api headers should be required by the chromium-efl port
      '../ewk_api_headers',
      '../ewk_api_headers/public',

      '<(PRODUCT_DIR)/gen',
      '<(SHARED_INTERMEDIATE_DIR)',
      '<(SHARED_INTERMEDIATE_DIR)/webkit/',
    ],
    'dependencies': [
      '<(chrome_src_dir)/base/allocator/allocator.gyp:allocator',
      '<(chrome_src_dir)/content/content.gyp:content',
      '<(chrome_src_dir)/content/content.gyp:content_app_browser',
      '<(chrome_src_dir)/content/content_shell_and_tests.gyp:content_shell_resources',
      '<(chrome_src_dir)/content/content_shell_and_tests.gyp:content_shell_pak',
      '<(chrome_src_dir)/components/components.gyp:visitedlink_browser',
      '<(chrome_src_dir)/components/components.gyp:visitedlink_renderer',
      '<(chrome_src_dir)/third_party/icu/icu.gyp:icuuc',
    ],
    'defines': [
      'CHROMIUMCORE_IMPLEMENTATION=1',
      'DATA_DIR="<(data_dir)"',
      'EXE_DIR="<(exe_dir)"',
    ],
    'sources': [
      'browser/device_sensors/data_fetcher_impl_tizen.cc',
      'browser/device_sensors/data_fetcher_impl_tizen.h',
      'browser/device_sensors/data_fetcher_shared_memory_tizen.cc',
      'browser/download_manager_delegate_efl.cc',
      'browser/download_manager_delegate_efl.h',
      # [M37] Geolocation related code changed. Figure out how to fix it.
      #'browser/geolocation/geolocation_permission_context_efl.cc',
      #'browser/geolocation/geolocation_permission_context_efl.h',
      'paths_efl.cc',
      'paths_efl.h',
      'web_contents_delegate_efl.cc',
      'web_contents_delegate_efl.h',
    ],
    'cflags!': [
      # Symbol visibility controled by chromium-efl.filter
      '-fvisibility=hidden',
    ],
    'link_settings': {
      'ldflags': [
        '-Wl,--no-undefined',
        '-Wl,--version-script,<(efl_impl_dir)/chromium-efl.filter',
        '-rdynamic',
      ],
      'conditions': [
        ['_toolset=="target"', {
          'libraries': [ '<!($(echo ${CXX_target:-g++}) -print-libgcc-file-name)', ]
        }],
      ],
    },
    'rules': [{
        'rule_name': 'edje_resources',
        'message': 'Compiling edje files <(RULE_INPUT_NAME)',
        'extension': 'edc',
        'outputs': [
          '<(PRODUCT_DIR)/resources/<(RULE_INPUT_ROOT).edj',
        ],
        'action': [
          '<(edje_compiler)',
          '-id', 'resource/images',
          '<(RULE_INPUT_PATH)',
          '<(PRODUCT_DIR)/resources/<(RULE_INPUT_ROOT).edj',
        ],
    }], #rules
  },
  {
    'target_name': 'efl_webprocess',
    'defines': [
      'DATA_DIR="<(data_dir)"',
      'EXE_DIR="<(exe_dir)"',
    ],
    'type': 'executable',
    'include_dirs': [
      '.',
      '../ewk_api_headers',
    ],
    'dependencies': [
      'chromium-efl',
    ],
  },
  ],
}

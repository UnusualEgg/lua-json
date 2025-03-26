project('lua-json', 'c', version: '0.1', default_options: ['warning_level=3',
                      'c_args=-ggdb -O0'])

luaib = dependency('lua-5.3')
json = subproject('json-c')
jsondep = json.get_variable('json_lib')
incdir = include_directories(['inc'])
exe = shared_library(
  'lua-json',
  'lua_json.c',
  install: true,
  dependencies: [luaib, jsondep],
  include_directories: incdir,
)

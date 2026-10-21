# 源文件自动枚举：`lfw/` 下新增或删除 .cpp 不必再回来改这份清单。
# CONFIGURE_DEPENDS ⇒ 构建时重新 glob（有新文件会触发自动 re-configure）；
# 相对模式 "*.cpp" 拿到的是相对本目录的路径，object 目录结构与手写清单时一致。
file(GLOB_RECURSE LFW_CORE_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_library(lfw_core STATIC ${LFW_CORE_SOURCES})

target_include_directories(lfw_core PUBLIC
  $<BUILD_INTERFACE:${LFW_NATIVE_ROOT}>
)

if(MSVC)
  target_compile_options(lfw_core PRIVATE /GR-)
  target_compile_definitions(lfw_core PRIVATE _HAS_EXCEPTIONS=0)
else()
  target_compile_options(lfw_core PRIVATE -fno-exceptions -fno-rtti)
endif()

target_link_libraries(lfw_core PUBLIC)

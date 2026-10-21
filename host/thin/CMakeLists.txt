# 录制型薄宿主（`lfw_thin`）：照 `lfw_run` 的形状，只把世界渲染器换成录制的那份。
#
# 与 `lfw_run` / `lfw_wasm` 共用同一批源文件（`host/common/lfw_boot.cpp`、`host/fs_zip.cpp`、
# `host/headless_host.cpp`）—— 装配与推帧**不重写**，只多一个 `IWorldRenderer` 的实现。
#
# 只进 MSVC（桌面）构建：WASM 预设下整段跳过（与 `host/wasm/CMakeLists.txt` 同一道守卫的
# 反向写法），免得 emscripten 那边多出一个带 `main` 的可执行目标。
if(EMSCRIPTEN)
  return()
endif()

# 源文件自动枚举：本目录的 .cpp + `host/CMakeLists.txt` 里 glob 出来的共享清单
#（`lfw_run` 的 `main` 已在那边排除，这里不会重复）。
file(GLOB LFW_THIN_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_executable(lfw_thin
  ${LFW_THIN_SOURCES}
  ${LFW_HOST_SHARED_SOURCES}
)

target_include_directories(lfw_thin PRIVATE "${LFW_NATIVE_ROOT}")
target_link_libraries(lfw_thin PRIVATE lfw_core)

if(MSVC)
  target_compile_options(lfw_thin PRIVATE /W4 /permissive- /utf-8)
  # 与 `lfw_run` 同款：RTTI / 异常关掉（`Value` 的 `std::variant` 两边布局必须一致）。
  target_compile_options(lfw_thin PRIVATE /GR-)
  target_compile_definitions(lfw_thin PRIVATE _HAS_EXCEPTIONS=0)
else()
  target_compile_options(lfw_thin PRIVATE -Wall -Wextra)
endif()

set_target_properties(lfw_thin PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

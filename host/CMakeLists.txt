# headless 宿主 + 可运行入口（`lfw_run`）：把 `ILfwHost` 接到 `LFW` 上，读真实数据包推帧。
#
# 源文件自动枚举：本目录与 `common/` 下的 .cpp 一次 glob 出来（`CONFIGURE_DEPENDS` ⇒ 构建时
# 重新 glob），新增共享源不必再改任何花名册。子目录里各自有宿主自己的 `main`，不会被抓进来。
# 这份共享清单同时供 `host/thin` 复用（它只多一个渲染器实现）。
file(GLOB LFW_HOST_SHARED_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/common/*.cpp"
)
list(REMOVE_ITEM LFW_HOST_SHARED_SOURCES "${CMAKE_CURRENT_LIST_DIR}/lfw_run.cpp")

add_executable(lfw_run
  ${LFW_HOST_SHARED_SOURCES}
  lfw_run.cpp
)

target_include_directories(lfw_run PRIVATE "${LFW_NATIVE_ROOT}")
target_link_libraries(lfw_run PRIVATE lfw_core)

if(MSVC)
  target_compile_options(lfw_run PRIVATE /W4 /permissive- /utf-8)
  # 与 `lfw_core` 同款：RTTI / 异常关掉。混着编会让两边各自实例化同一批头文件模板
  #（`Value` 的 `std::variant` 就在里面），两边对 `std::` 内部布局的理解必须一致。
  target_compile_options(lfw_run PRIVATE /GR-)
  target_compile_definitions(lfw_run PRIVATE _HAS_EXCEPTIONS=0)
else()
  target_compile_options(lfw_run PRIVATE -Wall -Wextra)
endif()

set_target_properties(lfw_run PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 录制型薄宿主（`lfw_thin`）：同一批源文件，只换世界渲染器 —— 见 `host/thin/CMakeLists.txt`。
add_subdirectory(thin)

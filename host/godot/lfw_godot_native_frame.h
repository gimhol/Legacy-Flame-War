#pragma once

#include <string>

namespace lfw::host {

bool install_native_frame(void* hwnd);
bool native_frame_installed();

std::string native_frame_evidence(void* hwnd);

/** 直接改窗口标题（绕开 Godot 给标题追加的 " (DEBUG)" 后缀） */
bool set_window_title(void* hwnd, const std::string& title);

/** 用 `.ico` 文件设置窗口/任务栏图标（Godot 的 Image 不认 ICO，走 Win32） */
bool set_window_icon(void* hwnd, const std::string& ico_path);

void native_frame_decoration(void* hwnd, int* out_dx, int* out_dy);

void native_frame_insets(void* hwnd, int* out_left, int* out_top, int* out_right,
                         int* out_bottom);

}
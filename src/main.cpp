#ifdef WIN32
	// Use Win32 window backend instead of SDL (allows transparent background)
	#define WINDOW_WIN32
	#include <windows.h>
	#include <dwmapi.h>
#endif
#include <gl/GL.h>
#include "SDL/SDL.h"
#include "SDL/SDL_video.h"
#include "SDL/SDL_syswm.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#define forloop(i,end) for(unsigned int i=0; i<(end); i++)
typedef unsigned int uint;

struct Texture
{
	GLuint id;
};

struct Color
{
	float r, g, b;
};

struct ButtonInputAction
{
	uint buttonIndex;
};

// D-pads are sometimes mapped to 8-way HAT inputs
struct HatInputAction
{
	uint pov;
};

struct KeyboardInputAction
{
	uint keyIndex;
};

struct InputAction
{
	enum Type { Type_button, Type_hat, Type_keyboard };

	union {
		ButtonInputAction button;
		HatInputAction hat;
		KeyboardInputAction key;
	};

	Type type;
};

struct InputMapping
{
	InputAction input;
	Texture image;
};

struct Config
{
	Color backgroundColor;
	bool alwaysOnTop;
	bool transparentBackground;
	uint imageWidth;
	uint imageHeight;
	uint maxDisplayedInputs;
	std::vector<InputMapping> inputMaps;
};

struct InputDisplay
{
	Texture image;
	uint frameNumber;
};
struct InputDisplayList
{
	std::vector<InputDisplay> inputs;
};

struct Input
{
	struct Button {
		bool pressed = 0; // True for one update when the button is first pressed.
		bool down = 0;    // True while the button is held down.
	};
	struct Joystick {
		uint buttonCount;
		Button *buttons;
		int hat;
		int previousHat;
		SDL_Joystick* sdlJoy;
	};

	static const uint supportedKeyCount = 0xFF;
	Button keyboard[supportedKeyCount];
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

void updateButton(Input::Button* inout_button, uint isDown)
{
	if (isDown) {
		inout_button->pressed = !inout_button->down;
		inout_button->down = true;
	}
	else {
		inout_button->pressed = false;
		inout_button->down = false;
	}
}

void updateInput(Input* input)
{
	// Handle joysticks beening plugged in or taken out
	int joystickCount = SDL_NumJoysticks();
	if (joystickCount != input->joystickCount)
	{
		// Free joysticks
		forloop(i, input->joystickCount)
		{
			delete[] input->joysticks[i].buttons;
			SDL_JoystickClose(input->joysticks[i].sdlJoy);
		}
		delete[] input->joysticks;
		// Allocate joysticks
		input->joystickCount = joystickCount;
		input->joysticks = new Input::Joystick[joystickCount];
		forloop(i, input->joystickCount)
		{
			input->joysticks[i].sdlJoy = SDL_JoystickOpen(i);
			if (input->joysticks[i].sdlJoy)
			{
				input->joysticks[i].buttonCount = SDL_JoystickNumButtons(input->joysticks[i].sdlJoy);
				input->joysticks[i].buttons = new Input::Button[input->joysticks[i].buttonCount];
			}
		}
	}

	// Update joystick
	SDL_JoystickUpdate();
	forloop(joystickIndex, input->joystickCount)
	{
		Input::Joystick* joystick = &input->joysticks[joystickIndex];
		// Buttons
		forloop(buttonIndex, joystick->buttonCount)
		{
			updateButton(&joystick->buttons[buttonIndex], SDL_JoystickGetButton(joystick->sdlJoy, buttonIndex));
		}
		// Hat
		joystick->previousHat = joystick->hat;
		joystick->hat = SDL_JoystickGetHat(joystick->sdlJoy, 0);
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
		updateButton(&input->keyboard[i], isKeyDown);
	}
}

// Checks if two inputs are equal
bool compareInputActions(InputAction a, InputAction b)
{
	if (a.type != b.type) return false;
	switch (a.type)
	{
	case InputAction::Type_button:
		return a.button.buttonIndex == b.button.buttonIndex;
	case InputAction::Type_hat:
		return a.hat.pov == b.hat.pov;
	case InputAction::Type_keyboard:
		return a.key.keyIndex == b.key.keyIndex;
	default:
		SDL_assert(!"Unimplemented input action type");
		return false;
	}
}

// Make a list of all inputs that changed this frame
std::vector<InputAction> getActiveInputsList(Input input)
{
	std::vector<InputAction> list;

	forloop(joystickIndex, input.joystickCount)
	{
		forloop(buttonIndex, input.joysticks[joystickIndex].buttonCount)
		{
			if (input.joysticks[joystickIndex].buttons[buttonIndex].pressed)
			{
				InputAction action;
				action.type = InputAction::Type_button;
				action.button.buttonIndex = buttonIndex;
				list.push_back(action);
			}
		}

		if (input.joysticks[joystickIndex].hat != input.joysticks[joystickIndex].previousHat)
		{
			InputAction action;
			action.type = InputAction::Type_hat;
			action.hat.pov = input.joysticks[joystickIndex].hat;
			list.push_back(action);
		}
	}

	forloop(keyIndex, input.supportedKeyCount)
	{
		if (input.keyboard[keyIndex].pressed)
		{
			if (keyIndex != SDL_SCANCODE_RETURN && keyIndex != SDL_SCANCODE_BACKSPACE)
			{
				InputAction action;
				action.type = InputAction::Type_keyboard;
				action.key.keyIndex = keyIndex;
				list.push_back(action);
			}
		}
	}

	return list;
}

void createTextureFromImage(Texture* out, const char* filePath)
{
	int width, height, channels;
	unsigned char* imageData = stbi_load(filePath, &width, &height, &channels, 0);
	if (imageData) {
		GLuint textureID;
		glGenTextures(1, &textureID);
		GLenum pixelFormat = (channels == 4)? GL_RGBA : GL_RGBA;
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, pixelFormat, width, height, 0, pixelFormat, GL_UNSIGNED_BYTE, imageData);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		stbi_image_free(imageData);

		out->id = textureID;
	}
}

void parseBool(std::istream& input, bool* out)
{
	std::string lineBuffer;
	std::getline(input, lineBuffer);
	std::stringstream line(lineBuffer);
	std::string value;
	line >> value;
	if (value == "true") {
		*out = true;
	}
	else {
		*out = false;
	}
}

void parseUInt(std::istream& input, uint* out)
{
	std::string lineBuffer;
	std::getline(input, lineBuffer);
	std::stringstream line(lineBuffer);
	line >> *out;
}

void parseColor(std::istream& input, Color* out)
{
	std::string lineBuffer;
	std::getline(input, lineBuffer);
	std::stringstream line(lineBuffer);
	line >> out->r >> out->g >> out->b;
}

bool parseInputMapping(std::istream& input, InputMapping* out)
{
	std::string lineBuffer;
	std::getline(input, lineBuffer);
	if (lineBuffer.empty()) {
		return false;
	}
	std::stringstream line(lineBuffer);
	char type;
	line >> type;
	// Parse button
	if (type == 'b') {
		out->input.type = InputAction::Type_button;
		line >> out->input.button.buttonIndex;
		std::string file;
		line >> file;
		createTextureFromImage(&out->image, file.c_str());
		return true;
	}
	else if (type == 'h') {
		out->input.type = InputAction::Type_hat;
		std::string direction;
		line >> direction;
		if (direction == "left") out->input.hat.pov = SDL_HAT_LEFT;
		else if (direction == "right") out->input.hat.pov = SDL_HAT_RIGHT;
		else if (direction == "up") out->input.hat.pov = SDL_HAT_UP;
		else if (direction == "down") out->input.hat.pov = SDL_HAT_DOWN;
		else if (direction == "upleft") out->input.hat.pov = SDL_HAT_LEFTUP;
		else if (direction == "downleft") out->input.hat.pov = SDL_HAT_LEFTDOWN;
		else if (direction == "upright") out->input.hat.pov = SDL_HAT_RIGHTUP;
		else if (direction == "downright") out->input.hat.pov = SDL_HAT_RIGHTDOWN;
		else return false;
		std::string file;
		line >> file;
		createTextureFromImage(&out->image, file.c_str());
		return true;
	}
	return false;
}

void parseConfigFile(Config* out, const char* filePath)
{
	std::ifstream inputFile(filePath);
	parseBool(inputFile, &out->alwaysOnTop);
	parseBool(inputFile, &out->transparentBackground);
	parseColor(inputFile, &out->backgroundColor);
	parseUInt(inputFile, &out->imageWidth);
	parseUInt(inputFile, &out->imageHeight);
	parseUInt(inputFile, &out->maxDisplayedInputs);
	InputMapping mapping={0};
	while (parseInputMapping(inputFile, &mapping))
	{
		out->inputMaps.push_back(mapping);
	}
}

void renderImage(Texture texture, float x, float y, float width, float height)
{
	glBindTexture(GL_TEXTURE_2D, texture.id);
	// Draw triangle with immediate mode interface
	glBegin(GL_TRIANGLE_STRIP);
	// Bottom left
	glTexCoord2f(0, 1);
	glVertex3f(-1+x, -1+y, 0);
	// Bottom right
	glTexCoord2f(1, 1);
	glVertex3f(-1+x+width, -1+y, 0);
	// Top left
	glTexCoord2f(0, 0);
	glVertex3f(-1+x, -1+y+height, 0);
	// Top right
	glTexCoord2f(1, 0);
	glVertex3f(-1+x+width, -1+y+height, 0);
	glEnd();
}

void renderInputList(InputDisplayList list, uint imageWidth, uint imageHeight, int windowWidth, int windowHeight)
{
	float renderHeight = 2*float(imageHeight)/float(windowHeight);
	float renderWidth = 2*float(imageWidth)/float(windowWidth);
	if (windowWidth > windowHeight) {
		// Render horizontally
		float x = 2-renderWidth;
		float y = 0;
		forloop(i, list.inputs.size())
		{
			renderImage(list.inputs[i].image, x, y, renderWidth, renderHeight);
			// Overlap inputs that happened on the same frame
			if (i<list.inputs.size()-1 && list.inputs[i].frameNumber == list.inputs[i+1].frameNumber) {
				y += renderHeight*0.6f;
			}
			else {
				x -= renderWidth;
				y = 0;
			}
		}
	}
	else {
		// Render vertically
		float x = 0;
		float y = 2 - renderHeight;
		forloop(i, list.inputs.size())
		{
			renderImage(list.inputs[i].image, x, y, renderWidth, renderHeight);
			// Overlap inputs that happened on the same frame
			if (i<list.inputs.size()-1 && list.inputs[i].frameNumber == list.inputs[i+1].frameNumber) {
				x += renderWidth*0.6f;
			}
			else {
				y -= renderHeight;
				x = 0;
			}
		}
	}
}

void addInputToList(InputDisplayList* mod, Texture inputImage, uint frameNumber, uint maxInputCount)
{
	InputDisplay display ={0};
	display.image = inputImage;
	display.frameNumber = frameNumber;

	if (mod->inputs.size() < maxInputCount) {
		mod->inputs.resize(mod->inputs.size()+1);
	}

	// Shift all inputs in list back
	uint i = mod->inputs.size()-1;
	while (i>0) {
		mod->inputs[i] = mod->inputs[i-1];
		--i;
	}

	// Add input at front of list
	mod->inputs[0] = display;
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

	// Setup OpenGL
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void setWindowStyle(Window* mod, bool alwaysOnTop, bool transparentBackground)
{
#ifdef WINDOW_WIN32
	if (alwaysOnTop) {
		SetWindowPos(mod->hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}

	if (transparentBackground) {
		#ifdef ENABLE_TRANSPARENCY
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
		#endif // ENABLE_TRANSPARENCY
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

int main(int argc, char** argv)
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

	Window window ={0};
	createWindow(&window);
	
	Config config ={0};
	if (argc > 1) parseConfigFile(&config, argv[1]);
	else parseConfigFile(&config, "config.txt");

	setWindowStyle(&window, config.alwaysOnTop, config.transparentBackground);

	Input input ={0};
	updateInput(&input);
	InputDisplayList inputList;

	uint frameCount = 0;
	int previousWindowWidth = 0;
	int previousWindowHeight = 0;
	bool run = true;
	while (run) {
		bool quit;
		processWindowMessages(&window, &quit);
		if (quit) run = false;

		// Resize viewport if window size changed
		int windowWidth, windowHeight;
		getWindowSize(window, &windowWidth, &windowHeight);
		if (previousWindowWidth != windowWidth|| previousWindowHeight != windowHeight) {
			glViewport(0, 0, windowWidth, windowHeight);
			previousWindowWidth = windowWidth;
			previousWindowHeight = windowHeight;
		}

		// Record inputs
		updateInput(&input);
		std::vector<InputAction> activeInputs = getActiveInputsList(input);
		forloop(activeIndex, activeInputs.size())
		{
			forloop(mapIndex, config.inputMaps.size())
			{
				if (compareInputActions(activeInputs[activeIndex], config.inputMaps[mapIndex].input))
				{
					addInputToList(&inputList, config.inputMaps[mapIndex].image, frameCount, config.maxDisplayedInputs);
				}
			}
		}

		// Render
		glClearColor(config.backgroundColor.r, config.backgroundColor.g, config.backgroundColor.b, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		renderInputList(inputList, config.imageWidth, config.imageHeight, windowWidth, windowHeight);
		
		swapBuffers(&window);
		++frameCount;
	}

	return 0;
}
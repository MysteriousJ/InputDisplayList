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

struct AxisInputAction
{
	uint axisIndex;
	float restPosition;
	float triggerPosition;
};

struct KeyboardInputAction
{
	uint keyIndex;
};

struct InputAction
{
	enum Type { Type_button, Type_hat, Type_axis, Type_keyboard };

	union {
		ButtonInputAction button;
		HatInputAction hat;
		AxisInputAction axis;
		KeyboardInputAction key;
	};

	Type type;
};

struct InputResult
{
	enum Type { Type_direction, Type_image };
	union {
		Texture image;
		uint direction;
	};
	Type type;
};

struct InputMapping
{
	InputResult result;
	InputAction input;
};

struct DirectionMapping
{
	uint direction;
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
	std::vector<DirectionMapping> directionMaps;
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
		uint axisCount;
		float* axes;
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
			delete[] input->joysticks[i].axes;
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
				input->joysticks[i].axisCount = SDL_JoystickNumAxes(input->joysticks[i].sdlJoy);
				input->joysticks[i].axes = new float[input->joysticks[i].axisCount];
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
		// Axis
		forloop(axisIndex, joystick->axisCount)
		{
			joystick->axes[axisIndex] = SDL_JoystickGetAxis(joystick->sdlJoy, axisIndex) / 32767.f;
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
		updateButton(&input->keyboard[i], isKeyDown);
	}
}

// Check if an input is currently active
bool checkInputAction(Input input, InputAction action)
{
	forloop(joystickIndex, input.joystickCount)
	{
		Input::Joystick& joystick = input.joysticks[joystickIndex];

		if (action.type == InputAction::Type_button
			&& action.button.buttonIndex < joystick.buttonCount
			&& joystick.buttons[action.button.buttonIndex].pressed)
		{
			return true;
		}

		if (action.type == InputAction::Type_hat
			&& joystick.hat & action.hat.pov)
		{
			return true;
		}

		if (action.type == InputAction::Type_axis
			&& action.axis.axisIndex < joystick.axisCount)
		{
			float axisValue = joystick.axes[action.axis.axisIndex];
			if (action.axis.triggerPosition < action.axis.restPosition
				&& axisValue <= action.axis.triggerPosition)
			{
				return true;
			}
			if (action.axis.triggerPosition > action.axis.restPosition
				&& axisValue >= action.axis.triggerPosition)
			{
				return true;
			}
		}
	}

	return false;
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

bool parseBool(std::istream& input)
{
	std::string lineBuffer;
	std::getline(input, lineBuffer);
	std::stringstream line(lineBuffer);
	std::string value;
	line >> value;
	return (value == "true");
}

uint parseUInt(std::istream& input)
{
	std::string lineBuffer;
	std::getline(input, lineBuffer);
	std::stringstream line(lineBuffer);
	uint result;
	line >> result;
	return result;
}

Color parseColor(std::istream& input)
{
	std::string lineBuffer;
	std::getline(input, lineBuffer);
	std::stringstream line(lineBuffer);
	Color result;
	line >> result.r >> result.g >> result.b;
	return result;
}

InputResult parseInputResult(std::istream& line)
{
	InputResult result;
	result.type = InputResult::Type_direction;
	std::string text;
	line >> text;
	if      (text == "left")  result.direction = SDL_HAT_LEFT;
	else if (text == "right") result.direction = SDL_HAT_RIGHT;
	else if (text == "up")    result.direction = SDL_HAT_UP;
	else if (text == "down")  result.direction = SDL_HAT_DOWN;
	else {
		result.type = InputResult::Type_image;
		createTextureFromImage(&result.image, text.c_str());
	}
	return result;
}

DirectionMapping parseDirectionMapping(std::istream& line)
{
	DirectionMapping result ={0};
	std::string direction;
	line >> direction;
	if (direction == "left")           result.direction = SDL_HAT_LEFT;
	else if (direction == "right")     result.direction = SDL_HAT_RIGHT;
	else if (direction == "up")        result.direction = SDL_HAT_UP;
	else if (direction == "down")      result.direction = SDL_HAT_DOWN;
	else if (direction == "upleft")    result.direction = SDL_HAT_LEFTUP;
	else if (direction == "downleft")  result.direction = SDL_HAT_LEFTDOWN;
	else if (direction == "upright")   result.direction = SDL_HAT_RIGHTUP;
	else if (direction == "downright") result.direction = SDL_HAT_RIGHTDOWN;
	std::string file;
	line >> file;
	createTextureFromImage(&result.image, file.c_str());
	return result;
}

InputMapping parseButtonMapping(std::istream& line)
{
	InputMapping result ={0};
	result.input.type = InputAction::Type_button;
	line >> result.input.button.buttonIndex;
	result.result = parseInputResult(line);
	return result;
}

InputMapping parseHatMapping(std::istream& line)
{
	InputMapping result ={0};
	result.input.type = InputAction::Type_hat;
	std::string direction;
	line >> direction;
	if (direction == "left")       result.input.hat.pov = SDL_HAT_LEFT;
	else if (direction == "right") result.input.hat.pov = SDL_HAT_RIGHT;
	else if (direction == "up")    result.input.hat.pov = SDL_HAT_UP;
	else if (direction == "down")  result.input.hat.pov = SDL_HAT_DOWN;
	result.result = parseInputResult(line);
	return result;
}

InputMapping parseAxisMapping(std::istream& line)
{
	InputMapping result ={0};
	result.input.type = InputAction::Type_axis;
	line >> result.input.axis.axisIndex;
	line >> result.input.axis.restPosition;
	line >> result.input.axis.triggerPosition;
	result.result = parseInputResult(line);
	return result;
}

void parseConfigFile(Config* out, const char* filePath)
{
	std::ifstream inputFile(filePath);
	// Each of these reads a line and takes the first word as the value
	out->alwaysOnTop = parseBool(inputFile);
	out->transparentBackground = parseBool(inputFile);
	out->backgroundColor = parseColor(inputFile);
	out->imageWidth = parseUInt(inputFile);
	out->imageHeight = parseUInt(inputFile);
	out->maxDisplayedInputs = parseUInt(inputFile);

	// Parse direction and input mappings
	while (!inputFile.eof()) {
		std::string lineBuffer;
		std::getline(inputFile, lineBuffer);
		std::stringstream line(lineBuffer);
		std::string inputType;
		line >> inputType;
		if (inputType == "d") out->directionMaps.push_back(parseDirectionMapping(line));
		else if (inputType == "b") out->inputMaps.push_back(parseButtonMapping(line));
		else if (inputType == "h") out->inputMaps.push_back(parseHatMapping(line));
		else if (inputType == "a") out->inputMaps.push_back(parseAxisMapping(line));
	}
}

void renderImage(Texture texture, float x, float y, float width, float height)
{
	glBindTexture(GL_TEXTURE_2D, texture.id);
	// Draw a quad with two triangles
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
		// Display list horizontally
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
		// Display list vertically
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
	uint previousDirectionInput = 0;
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
		// Directions are combined to support combinations like up-left before deciding on which image to display
		updateInput(&input);
		uint accumulatedDirection = 0;
		forloop(mapIndex, config.inputMaps.size())
		{
			if (checkInputAction(input, config.inputMaps[mapIndex].input))
			{
				if (config.inputMaps[mapIndex].result.type == InputResult::Type_direction) {
					accumulatedDirection |= config.inputMaps[mapIndex].result.direction;
				}
				else {
					addInputToList(&inputList, config.inputMaps[mapIndex].result.image, frameCount, config.maxDisplayedInputs);
				}
			}
		}
		if (accumulatedDirection != previousDirectionInput)
		{
			forloop(i, config.directionMaps.size())
			{
				if (config.directionMaps[i].direction == accumulatedDirection) {
					addInputToList(&inputList, config.directionMaps[i].image, frameCount, config.maxDisplayedInputs);
				}
			}
			previousDirectionInput = accumulatedDirection;
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
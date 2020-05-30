#include "platform.h"
#include "graphics.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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
	else if (direction == "center")    result.direction = SDL_HAT_CENTERED;
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

// Check if an input is currently active
bool checkInputAction(Input::Joystick::State joystick, InputAction action)
{
	if (action.type == InputAction::Type_button
		&& action.button.buttonIndex < joystick.buttonCount
		&& joystick.buttons[action.button.buttonIndex])
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
		float axisCurrent = joystick.axes[action.axis.axisIndex];
		if (action.axis.triggerPosition < action.axis.restPosition
			&& axisCurrent <= action.axis.triggerPosition)
		{
			return true;
		}
		if (action.axis.triggerPosition > action.axis.restPosition
			&& axisCurrent >= action.axis.triggerPosition)
		{
			return true;
		}
	}

	return false;
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

int main(int argc, char** argv)
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

	Window window ={0};
	createWindow(&window);
	setupOpenGL();
	
	Config config ={0};
	if (argc > 1) parseConfigFile(&config, argv[1]);
	else parseConfigFile(&config, "config.txt");

	setWindowStyle(&window, config.alwaysOnTop, config.transparentBackground);

	Input input = {0};
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
			InputMapping map = config.inputMaps[mapIndex];
			forloop(joystickIndex, input.joystickCount)
			{
				Input::Joystick joystick = input.joysticks[joystickIndex];
				if (checkInputAction(joystick.current, map.input))
				{
					if (map.result.type == InputResult::Type_direction) {
						accumulatedDirection |= map.result.direction;
					}
					else if (!checkInputAction(joystick.previous, map.input)) {
						// Only add if it was not active on the last frame
						addInputToList(&inputList, map.result.image, frameCount, config.maxDisplayedInputs);
					}
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
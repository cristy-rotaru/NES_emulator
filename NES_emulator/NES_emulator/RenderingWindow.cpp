#include "RenderingWindow.h"

#include <SFML/Graphics.hpp>

#include <chrono>
#include <thread>
#include <mutex>
#include <queue>

#include <iostream>

#define RW__NES_WINDOW_WIDTH (256u)
#define RW__NES_WINDOW_HEIGHT (240u)

#define RW__SCREEN_SCALE (3.0f)

#define RW__MINIMUM_FRAME_TIME (std::chrono::microseconds(10000))

#define RW__COMMNAD_NONE (0u)
#define RW__COMMAND_SET_PIXEL (1u)
#define RW__COMMAND_PUSH_FRAME (2u)
#define RW__COMMAND_SHUT_DOWN (3u)

class Screen : public sf::Drawable
{
public:
	void draw(sf::RenderTarget &target, sf::RenderStates states) const;
	sf::VertexArray vertices;
};

typedef struct
{
	uint8_t commandType;

	union
	{
		struct
		{
			std::size_t pixelX;
			std::size_t pixelY;
			uint32_t pixelColor;
		};

		bool forceFrame;
	};
}RW_command_t;

static std::chrono::high_resolution_clock::time_point RW_previousFrameTimePoint;
static bool RW_firstRender = false;

static std::thread *RW_windowOwner = nullptr;

static std::queue<RW_command_t> RW_commandQueue;
static std::queue<uint8_t> RW_eventQueue;

static std::mutex RW_commandMutex;
static std::mutex RW_eventMutex;

void RW_windowOwner__loop();

namespace RW
{
	void init()
	{
		/* Starting the window owner thread */
		RW_windowOwner = new std::thread(RW_windowOwner__loop);
		RW_windowOwner->detach();
	}

	void setPixel(std::size_t x_coordinate, std::size_t y_coordinate, uint32_t color)
	{
		std::lock_guard<std::mutex> mutexGuard(RW_commandMutex);

		RW_command_t command;
		command.commandType = RW__COMMAND_SET_PIXEL;
		command.pixelX = x_coordinate;
		command.pixelY = y_coordinate;
		command.pixelColor = color;

		RW_commandQueue.push(command);
	}

	uint8_t pollWindowEvent()
	{
		std::lock_guard<std::mutex> mutexGuard(RW_eventMutex);

		if (RW_eventQueue.empty())
		{
			return (EVENT_NONE);
		}

		uint8_t windowEvent = RW_eventQueue.front();
		RW_eventQueue.pop();

		return (windowEvent);
	}

	void redraw(bool forced)
	{
		std::lock_guard<std::mutex> mutexGuard(RW_commandMutex);

		RW_command_t command;
		command.commandType = RW__COMMAND_PUSH_FRAME;
		command.forceFrame = forced;

		RW_commandQueue.push(command);
	}

	void dispose()
	{
		std::lock_guard<std::mutex> mutexGuard(RW_commandMutex);

		RW_command_t command;
		command.commandType = RW__COMMAND_SHUT_DOWN;

		RW_commandQueue.push(command);
	}
}

void Screen::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
	target.draw(this->vertices, states);
}

/* This function is run on a separate thread */
void RW_windowOwner__loop()
{
	/* init sequence */
	sf::RenderWindow window;
	sf::Event windowEvent;
	Screen virtualScreen;

	/* Creating a window to display the virtual screen in */
	window.create(sf::VideoMode((unsigned int)(RW__NES_WINDOW_WIDTH * RW__SCREEN_SCALE), (unsigned int)(RW__NES_WINDOW_HEIGHT * RW__SCREEN_SCALE)), "NES emulator", sf::Style::Titlebar | sf::Style::Close);
	window.setVerticalSyncEnabled(true);

	/* Creating a virtual screen to write visual data to */
	virtualScreen.vertices.resize(RW__NES_WINDOW_WIDTH * RW__NES_WINDOW_HEIGHT * 6); // 6 vertices per pixel
	virtualScreen.vertices.setPrimitiveType(sf::Triangles); // each pixel will be represented by 2 triangles 
	for (std::size_t x = 0; x < RW__NES_WINDOW_WIDTH; ++x)
	{
		for (std::size_t y = 0; y < RW__NES_WINDOW_HEIGHT; ++y)
		{
			std::size_t i = (x * RW__NES_WINDOW_HEIGHT + y) * 6;
			sf::Vector2f coordinate(x * RW__SCREEN_SCALE, y * RW__SCREEN_SCALE);

			// 1st triangle
			virtualScreen.vertices[i].position = coordinate; // top-left
			virtualScreen.vertices[i].color = sf::Color::Black;

			virtualScreen.vertices[i + 1].position = coordinate + sf::Vector2f{ RW__SCREEN_SCALE, 0 }; // top-right
			virtualScreen.vertices[i + 1].color = sf::Color::Black;

			virtualScreen.vertices[i + 2].position = coordinate + sf::Vector2f{ 0, RW__SCREEN_SCALE }; // bottom-left
			virtualScreen.vertices[i + 2].color = sf::Color::Black;

			// 2nd triangle
			virtualScreen.vertices[i + 3].position = coordinate + sf::Vector2f{ RW__SCREEN_SCALE, RW__SCREEN_SCALE }; // bottom-right
			virtualScreen.vertices[i + 3].color = sf::Color::Black;

			virtualScreen.vertices[i + 4].position = coordinate + sf::Vector2f{ 0, RW__SCREEN_SCALE }; // bottom-left
			virtualScreen.vertices[i + 4].color = sf::Color::Black;

			virtualScreen.vertices[i + 5].position = coordinate + sf::Vector2f{ RW__SCREEN_SCALE, 0 }; // top-right
			virtualScreen.vertices[i + 5].color = sf::Color::Black;
		}
	}

	RW_previousFrameTimePoint = std::chrono::high_resolution_clock::now();

	/* polling loop */
	for (;;)
	{
		bool commandsAvailable = true;

		do
		{
			RW_command_t command;
			command.commandType = RW__COMMNAD_NONE;

			{
				std::lock_guard<std::mutex> mutexGuard(RW_commandMutex);

				if (RW_commandQueue.empty() == false)
				{
					command = RW_commandQueue.front();
					RW_commandQueue.pop();
				}
			}

			switch (command.commandType)
			{
				case RW__COMMNAD_NONE:
				{
					commandsAvailable = false;

					break;
				}

				case RW__COMMAND_SET_PIXEL:
				{
					std::size_t i = (command.pixelX * RW__NES_WINDOW_HEIGHT + command.pixelY) * 6;
					if (i < virtualScreen.vertices.getVertexCount())
					{
						sf::Color color(command.pixelColor);

						for (std::size_t index = i; index < i + 6; ++index)
						{
							virtualScreen.vertices[index].color = color;
						}
					}

					break;
				}

				case RW__COMMAND_PUSH_FRAME:
				{
					if (command.forceFrame || (!RW_firstRender) || (std::chrono::high_resolution_clock::now() - RW_previousFrameTimePoint >= RW__MINIMUM_FRAME_TIME))
					{
						window.draw(virtualScreen);
						window.display();

						RW_firstRender = true;
						RW_previousFrameTimePoint = std::chrono::high_resolution_clock::now();

						commandsAvailable = false; // makes sure the thread polls events after each frame
					}

					break;
				}

				case RW__COMMAND_SHUT_DOWN:
				{
					return;
				}
			}
		} while (commandsAvailable);

		if (window.pollEvent(windowEvent))
		{
			if (windowEvent.type == sf::Event::Closed)
			{
				std::lock_guard<std::mutex> mutexGuard(RW_eventMutex);

				RW_eventQueue.push(EVENT_BUTTON_CLOSE_PRESSED);
			}
		}
	}
}
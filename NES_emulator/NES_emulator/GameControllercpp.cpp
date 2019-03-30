#include "GameController.h"

#include <SFML/Window.hpp>

#define GC__BUTTON_A (0u)
#define GC__BUTTON_B (1u)
#define GC__BUTTON_SELECT (2u)
#define GC__BUTTON_START (3u)
#define GC__BUTTON_UP (4u)
#define GC__BUTTON_DOWN (5u)
#define GC__BUTTON_LEFT (6u)
#define GC__BUTTON_RIGHT (7u)

#define GC__CONTROLLER1_A (sf::Keyboard::M)
#define GC__CONTROLLER1_B (sf::Keyboard::N)
#define GC__CONTROLLER1_SELECT (sf::Keyboard::RShift)
#define GC__CONTROLLER1_START (sf::Keyboard::Return)
#define GC__CONTROLLER1_UP (sf::Keyboard::W)
#define GC__CONTROLLER1_DOWN (sf::Keyboard::S)
#define GC__CONTROLLER1_LEFT (sf::Keyboard::A)
#define GC__CONTROLLER1_RIGHT (sf::Keyboard::D)

#define GC__CONTROLLER2_A (sf::Keyboard::Numpad9)
#define GC__CONTROLLER2_B (sf::Keyboard::Numpad8)
#define GC__CONTROLLER2_SELECT (sf::Keyboard::Numpad3)
#define GC__CONTROLLER2_START (sf::Keyboard::Numpad6)
#define GC__CONTROLLER2_UP (sf::Keyboard::Up)
#define GC__CONTROLLER2_DOWN (sf::Keyboard::Down)
#define GC__CONTROLLER2_LEFT (sf::Keyboard::Left)
#define GC__CONTROLLER2_RIGHT (sf::Keyboard::Right)

static sf::Keyboard::Key GC_controller1Keys[8];
static sf::Keyboard::Key GC_controller2Keys[8];
static uint8_t GC_strobe = 0;
static uint8_t GC_buttonStatesController1 = 0;
static uint8_t GC_buttonStatesController2 = 0;

namespace GC
{
	void init()
	{
		/* Configuring key bindings */
		GC_controller1Keys[GC__BUTTON_A] = GC__CONTROLLER1_A;
		GC_controller1Keys[GC__BUTTON_B] = GC__CONTROLLER1_B;
		GC_controller1Keys[GC__BUTTON_SELECT] = GC__CONTROLLER1_SELECT;
		GC_controller1Keys[GC__BUTTON_START] = GC__CONTROLLER1_START;
		GC_controller1Keys[GC__BUTTON_UP] = GC__CONTROLLER1_UP;
		GC_controller1Keys[GC__BUTTON_DOWN] = GC__CONTROLLER1_DOWN;
		GC_controller1Keys[GC__BUTTON_LEFT] = GC__CONTROLLER1_LEFT;
		GC_controller1Keys[GC__BUTTON_RIGHT] = GC__CONTROLLER1_RIGHT;

		GC_controller2Keys[GC__BUTTON_A] = GC__CONTROLLER2_A;
		GC_controller2Keys[GC__BUTTON_B] = GC__CONTROLLER2_B;
		GC_controller2Keys[GC__BUTTON_SELECT] = GC__CONTROLLER2_SELECT;
		GC_controller2Keys[GC__BUTTON_START] = GC__CONTROLLER2_START;
		GC_controller2Keys[GC__BUTTON_UP] = GC__CONTROLLER2_UP;
		GC_controller2Keys[GC__BUTTON_DOWN] = GC__CONTROLLER2_DOWN;
		GC_controller2Keys[GC__BUTTON_LEFT] = GC__CONTROLLER2_LEFT;
		GC_controller2Keys[GC__BUTTON_RIGHT] = GC__CONTROLLER2_RIGHT;
	}

	void strobe(uint8_t s)
	{
		GC_strobe = s & 1;
		if (!GC_strobe)
		{
			GC_buttonStatesController1 = 0;
			GC_buttonStatesController2 = 0;

			for (std::size_t i = 0; i < 8; ++i)
			{
				GC_buttonStatesController1 |= (sf::Keyboard::isKeyPressed(GC_controller1Keys[i])) << i;
				GC_buttonStatesController2 |= (sf::Keyboard::isKeyPressed(GC_controller2Keys[i])) << i;
			}
		}
	}

	uint8_t readController1()
	{
		if (GC_strobe)
		{
			return ((uint8_t)sf::Keyboard::isKeyPressed(GC_controller1Keys[GC__BUTTON_A]) | 0x40); // return the state of key A
		}
		else
		{
			uint8_t toReturn = (GC_buttonStatesController1 & 0x01) | 0x40;
			GC_buttonStatesController1 >>= 1;

			return (toReturn);
		}
	}

	uint8_t readController2()
	{
		if (GC_strobe)
		{
			return ((uint8_t)sf::Keyboard::isKeyPressed(GC_controller2Keys[GC__BUTTON_A]) | 0x40); // return the state of key A
		}
		else
		{
			uint8_t toReturn = (GC_buttonStatesController2 & 0x01) | 0x40;
			GC_buttonStatesController2 >>= 1;

			return (toReturn);
		}
	}
}
/* ResidualVM - A 3D game interpreter
 *
 * ResidualVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the AUTHORS
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef STARK_SERVICES_USER_INTERFACE_H
#define STARK_SERVICES_USER_INTERFACE_H

#include "common/scummsys.h"
#include "common/str-array.h"

namespace Stark {

namespace Gfx {
class Driver;
}

class ActionMenu;
class DialogPanel;
class InventoryWindow;
class TopMenu;
class Cursor;
class FMVPlayer;
class GameWindow;
class Window;

/**
 * Facade object for interacting with the user interface from the rest of the engine
 *
 * @todo: perhaps move all window management to a new class
 * @todo: perhaps introduce a 'Screen' class or just a window with sub-windows for the game screen
 */
class UserInterface {
public:
	UserInterface(Gfx::Driver *gfx, Cursor *cursor);
	virtual ~UserInterface();

	enum Screen {
		kScreenGame,
		kScreenFMV
	};

	void init();
	void update();
	void render();
	void handleClick();
	void handleRightClick();
	void notifyShouldExit() { _exitGame = true; }
	void notifyShouldOpenInventory();
	bool shouldExit() { return _exitGame; }

	/** Start playing a FMV */
	void requestFMVPlayback(const Common::String &name);

	/** FMV playback has just ended */
	void onFMVStopped();

	/** Abort the currently playing FMV, if any */
	void skipFMV();

	/** Set the currently displayed screen */
	void changeScreen(Screen screen);

	/** Is the game screen currently displayed? */
	bool isInGameScreen() const;

	/** Can the player interact with the game world? */
	bool isInteractive() const;

	/** Allow or forbid interaction with the game world */
	void setInteractive(bool interactive);

	/** Set the selected inventory item */
	void selectInventoryItem(uint16 itemIndex);

private:
	// Game Screen windows
	ActionMenu *_actionMenu;
	DialogPanel *_dialogPanel;
	InventoryWindow *_inventoryWindow;
	TopMenu *_topMenu;
	GameWindow *_gameWindow;

	// Game screen windows array
	Common::Array<Window *> _gameScreenWindows;

	// FMV screen window
	FMVPlayer *_fmvPlayer;

	Gfx::Driver *_gfx;
	Cursor *_cursor;
	bool _exitGame;

	Screen _currentScreen;
	bool _interactive;
};

} // End of namespace Stark

#endif // STARK_SERVICES_USER_INTERFACE_H

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

#include "engines/stark/ui/screen.h"

#include "engines/stark/services/gamemessage.h"

#include "common/rect.h"
#include "common/str-array.h"
#include "common/stack.h"

namespace Common {
class SeekableReadStream;
class WriteStream;
}

namespace Graphics {
struct Surface;
}

namespace Stark {

namespace Gfx {
class Driver;
}

class DiaryIndexScreen;
class GameScreen;
class MainMenuScreen;
class SettingsMenuScreen;
class SaveMenuScreen;
class LoadMenuScreen;
class FMVMenuScreen;
class DiaryPagesScreen;
class DialogScreen;
class Cursor;
class FMVScreen;

/**
 * Facade object for interacting with the user interface from the rest of the engine
 */
class UserInterface {
public:
	explicit UserInterface(Gfx::Driver *gfx);
	virtual ~UserInterface();

	void init();
	void update();
	void render();
	void handleMouseMove(const Common::Point &pos);
	void handleMouseUp();
	void handleClick();
	void handleRightClick();
	void handleDoubleClick();
	void handleEscape();
	void notifyShouldExit() { _exitGame = true; }
	void inventoryOpen(bool open);
	bool shouldExit() { return _exitGame; }

	/** Start playing a FMV */
	void requestFMVPlayback(const Common::String &name);

	/** FMV playback has just ended */
	void onFMVStopped();

	/**
	 * Abort the currently playing FMV, if any
	 *
	 * @return true if a FMV was skipped
	 */
	bool skipFMV();

	/** Set the currently displayed screen */
	void changeScreen(Screen::Name screenName);

	/** Back to the previous displayed screen */
	void backPrevScreen();

	/** Back to the main menu screen and rest resources */
	void requestQuitToMainMenu() { _quitToMainMenu = true; }
	bool hasQuitToMainMenuRequest() { return _quitToMainMenu; }
	void performQuitToMainMenu();

	/** Is the game screen currently displayed? */
	bool isInGameScreen() const;

	/** Is the save & load menu screen currently displayed? */
	bool isInSaveLoadMenuScreen() const;

	/** Is the diary index screen currently displayed? */
	bool isInDiaryIndexScreen() const;

	/** Is the inventory panel being displayed? */
	bool isInventoryOpen() const;

	/** Can the player interact with the game world? */
	bool isInteractive() const;

	/** Allow or forbid interaction with the game world */
	void setInteractive(bool interactive);

	/** A new item has been added to the player's inventory */
	void notifyInventoryItemEnabled(uint16 itemIndex);

	/** A new entry has been added to the player's diary */
	void notifyDiaryEntryEnabled();

	/** Access the selected inventory item */
	int16 getSelectedInventoryItem() const;
	void selectInventoryItem(int16 itemIndex);

	/** Clears all the pointers to data that may be location dependent */
	void clearLocationDependentState();

	/** Open the in game options menu */
	void optionsOpen();

	/** Signal a denied interaction that occurred during a non interactive period */
	void markInteractionDenied();

	/** Was a player interaction with the world denied during this non interactive period? */
	bool wasInteractionDenied() const;

	/** The screen resolution just changed, rebuild resolution dependent data */
	void onScreenChanged();

	/** Grab a screenshot of the game screen and store it in the class context as a thumbnail */
	void saveGameScreenThumbnail();

	/** Clear the currently stored game screen thumbnail, if any */
	void freeGameScreenThumbnail();

	/** Get the currently stored game screen thumbnail, returns nullptr if there is not thumbnail stored */
	const Graphics::Surface *getGameWindowThumbnail() const;

	/** Display a message dialog, return true when the left button is pressed, and false for the right button */
	bool confirm(const Common::String &msg, const Common::String &leftBtnMsg, const Common::String &rightBtnMsg);
	bool confirm(const Common::String &msg);
	bool confirm(GameMessage::TextKey key);

	/** Directly open or close a screen */
	void toggleScreen(Screen::Name screenName);

	/** Switch to the game screen, clearing all the UI state along the way */
	void goToGameScreen();

	/** Toggle subtitles on and off */
	void requestToggleSubtitle() { _shouldToggleSubtitle = !_shouldToggleSubtitle; }
	bool hasToggleSubtitleRequest() { return _shouldToggleSubtitle; }
	void performToggleSubtitle();

	/** Cycle back or forward through inventory cursor items */
	void cycleBackInventory() { cycleInventory(false); }
	void cycleForwardInventory() { cycleInventory(true); }

	/** Scroll the inventory up or down */
	void scrollInventoryUp();
	void scrollInventoryDown();

	/** Scroll the dialog options up or down */
	void scrollDialogUp();
	void scrollDialogDown();

	/** Focus on the next or previous dialog option */
	void focusNextDialogOption();
	void focusPrevDialogOption();

	/** Select the focused dialog option */
	void selectFocusedDialogOption();

	/** Directly select a dialog option by index */
	void selectDialogOptionByIndex(uint index);

	/** Toggle the display of exit locations */
	void toggleExitDisplay();

	static const uint kThumbnailWidth = 160;
	static const uint kThumbnailHeight = 92;
	static const uint kThumbnailSize = kThumbnailWidth * kThumbnailHeight * 4;

private:
	Screen *getScreenByName(Screen::Name screenName) const;

	void cycleInventory(bool forward);

	GameScreen *_gameScreen;
	FMVScreen *_fmvScreen;
	DiaryIndexScreen *_diaryIndexScreen;
	MainMenuScreen *_mainMenuScreen;
	SettingsMenuScreen *_settingsMenuScreen;
	SaveMenuScreen *_saveMenuScreen;
	LoadMenuScreen *_loadMenuScreen;
	FMVMenuScreen *_fmvMenuScreen;
	DiaryPagesScreen *_diaryPagesScreen;
	DialogScreen *_dialogScreen;
	Screen *_currentScreen;
	Common::Stack<Screen::Name> _prevScreenNameStack;

	Gfx::Driver *_gfx;
	Cursor *_cursor;
	bool _exitGame;
	bool _quitToMainMenu;

	bool _interactive;
	bool _interactionAttemptDenied;

	bool _shouldToggleSubtitle;

	Graphics::Surface *_gameWindowThumbnail;
};

} // End of namespace Stark

#endif // STARK_SERVICES_USER_INTERFACE_H

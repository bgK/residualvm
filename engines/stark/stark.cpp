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

#include "engines/stark/stark.h"

#include "engines/stark/console.h"
#include "engines/stark/debug.h"
#include "engines/stark/resources/level.h"
#include "engines/stark/resources/location.h"
#include "engines/stark/savemetadata.h"
#include "engines/stark/scene.h"
#include "engines/stark/services/userinterface.h"
#include "engines/stark/services/archiveloader.h"
#include "engines/stark/services/dialogplayer.h"
#include "engines/stark/services/diary.h"
#include "engines/stark/services/fontprovider.h"
#include "engines/stark/services/gameinterface.h"
#include "engines/stark/services/global.h"
#include "engines/stark/services/resourceprovider.h"
#include "engines/stark/services/services.h"
#include "engines/stark/services/stateprovider.h"
#include "engines/stark/services/staticprovider.h"
#include "engines/stark/services/settings.h"
#include "engines/stark/services/gamechapter.h"
#include "engines/stark/services/gamemessage.h"
#include "engines/stark/gfx/driver.h"
#include "engines/stark/gfx/framelimiter.h"

#include "common/config-manager.h"
#include "common/debug-channels.h"
#include "common/events.h"
#include "common/random.h"
#include "common/savefile.h"
#include "common/system.h"
#include "audio/mixer.h"

namespace Stark {

StarkEngine::StarkEngine(OSystem *syst, const ADGameDescription *gameDesc) :
		Engine(syst),
		_gameDescription(gameDesc),
		_frameLimiter(nullptr),
		_console(nullptr),
		_lastClickTime(0) {
	// Add the available debug channels
	DebugMan.addDebugChannel(kDebugArchive, "Archive", "Debug the archive loading");
	DebugMan.addDebugChannel(kDebugXMG, "XMG", "Debug the loading of XMG images");
	DebugMan.addDebugChannel(kDebugXRC, "XRC", "Debug the loading of XRC resource trees");
	DebugMan.addDebugChannel(kDebugModding, "Modding", "Debug the loading of modded assets");
	DebugMan.addDebugChannel(kDebugUnknown, "Unknown", "Debug unknown values on the data");
}

StarkEngine::~StarkEngine() {
	delete StarkServices::instance().gameInterface;
	delete StarkServices::instance().diary;
	delete StarkServices::instance().dialogPlayer;
	delete StarkServices::instance().randomSource;
	delete StarkServices::instance().scene;
	delete StarkServices::instance().gfx;
	delete StarkServices::instance().staticProvider;
	delete StarkServices::instance().resourceProvider;
	delete StarkServices::instance().global;
	delete StarkServices::instance().stateProvider;
	delete StarkServices::instance().archiveLoader;
	delete StarkServices::instance().userInterface;
	delete StarkServices::instance().fontProvider;
	delete StarkServices::instance().settings;
	delete StarkServices::instance().gameChapter;
	delete StarkServices::instance().gameMessage;

	StarkServices::destroy();

	delete _console;
	delete _frameLimiter;
}

Common::Error StarkEngine::run() {
	_console = new Console();
	_frameLimiter = new Gfx::FrameLimiter(_system, ConfMan.getInt("engine_speed"));

	// Get the screen prepared
	Gfx::Driver *gfx = Gfx::Driver::create();
	gfx->init();

	// Setup the public services
	StarkServices &services = StarkServices::instance();
	services.gfx = gfx;
	services.archiveLoader = new ArchiveLoader();
	services.stateProvider = new StateProvider();
	services.global = new Global();
	services.resourceProvider = new ResourceProvider(services.archiveLoader, services.stateProvider, services.global);
	services.staticProvider = new StaticProvider(services.archiveLoader);
	services.randomSource = new Common::RandomSource("stark");
	services.fontProvider = new FontProvider();
	services.scene = new Scene(services.gfx);
	services.dialogPlayer = new DialogPlayer();
	services.diary = new Diary();
	services.gameInterface = new GameInterface();
	services.userInterface = new UserInterface(services.gfx);
	services.settings = new Settings(_mixer, _gameDescription);
	services.gameChapter = new GameChapter();
	services.gameMessage = new GameMessage();

	// Load global resources
	services.staticProvider->init();
	services.fontProvider->initFonts();

	// Apply the sound volume settings
	syncSoundSettings();

	// Initialize the UI
	services.userInterface->init();

	// Load through ResidualVM launcher
	if (ConfMan.hasKey("save_slot")) {
		loadGameState(ConfMan.getInt("save_slot"));
	}

	// Start running
	mainLoop();

	services.staticProvider->shutdown();
	services.resourceProvider->shutdown();

	return Common::kNoError;
}

void StarkEngine::mainLoop() {
	while (!shouldQuit()) {
		_frameLimiter->startFrame();

		processEvents();

		if (StarkUserInterface->shouldExit()) {
			quitGame();
			break;
		}

		StarkUserInterface->doQueuedScreenChange();

		if (StarkResourceProvider->hasLocationChangeRequest()) {
			StarkGlobal->setNormalSpeed();
			StarkResourceProvider->performLocationChange();
		}

		updateDisplayScene();

		// Swap buffers
		_frameLimiter->delayBeforeSwap();
		StarkGfx->flipBuffer();
	}
}

void StarkEngine::processEvents() {
	Common::Event e;
	while (g_system->getEventManager()->pollEvent(e)) {
		// Handle any buttons, keys and joystick operations

		if (isPaused()) {
			// Only pressing key P to resume the game is allowed when the game is paused
			if (e.type == Common::EVENT_KEYDOWN && e.kbd.keycode == Common::KEYCODE_p) {
				pauseEngine(false);
			}
			continue;
		}

		if (e.type == Common::EVENT_KEYDOWN) {
			if (e.kbdRepeat) {
				continue;
			}

			if (e.kbd.keycode == Common::KEYCODE_d && (e.kbd.hasFlags(Common::KBD_CTRL))) {
				_console->attach();
				_console->onFrame();
			} else if ((e.kbd.keycode == Common::KEYCODE_RETURN || e.kbd.keycode == Common::KEYCODE_KP_ENTER)
						&& e.kbd.hasFlags(Common::KBD_ALT)) {
					StarkGfx->toggleFullscreen();
			} else if (e.kbd.keycode == Common::KEYCODE_p) {
				if (StarkUserInterface->isInGameScreen()) {
					pauseEngine(true);
					debug("The game is paused");
				}
			} else {
				StarkUserInterface->handleKeyPress(e.kbd);
			}

		} else if (e.type == Common::EVENT_LBUTTONUP) {
			StarkUserInterface->handleMouseUp();
		} else if (e.type == Common::EVENT_MOUSEMOVE) {
			StarkUserInterface->handleMouseMove(e.mouse);
		} else if (e.type == Common::EVENT_LBUTTONDOWN) {
			StarkUserInterface->handleClick();
			if (_system->getMillis() - _lastClickTime < _doubleClickDelay) {
				StarkUserInterface->handleDoubleClick();
			}
			_lastClickTime = _system->getMillis();
		} else if (e.type == Common::EVENT_RBUTTONDOWN) {
			StarkUserInterface->handleRightClick();
		} else if (e.type == Common::EVENT_SCREEN_CHANGED) {
			onScreenChanged();
		}
	}
}

void StarkEngine::updateDisplayScene() {
	if (StarkGlobal->isFastForward()) {
		// The original engine was frame limited to 30 fps.
		// Set the frame duration to 1000 / 30 ms so that fast forward
		// skips the same amount of simulated time as the original.
		StarkGlobal->setMillisecondsPerGameloop(33);
	} else {
		StarkGlobal->setMillisecondsPerGameloop(_frameLimiter->getLastFrameDuration());
	}

	// Clear the screen
	StarkGfx->clearScreen();

	// Only update the world resources when on the game screen
	if (StarkUserInterface->isInGameScreen() && !isPaused()) {
		int frames = 0;
		do {
			// Update the game resources
			StarkGlobal->getLevel()->onGameLoop();
			StarkGlobal->getCurrent()->getLevel()->onGameLoop();
			StarkGlobal->getCurrent()->getLocation()->onGameLoop();
			frames++;

			// When the game is in fast forward mode, update
			// the game resources for multiple frames,
			// but render only once.
		} while (StarkGlobal->isFastForward() && frames < 100);
		StarkGlobal->setNormalSpeed();
	}

	// Render the current scene
	// Update the UI state before displaying the scene
	StarkUserInterface->onGameLoop();

	// Tell the UI to render, and update implicitly, if this leads to new mouse-over events.
	StarkUserInterface->render();
}

bool StarkEngine::hasFeature(EngineFeature f) const {
	return
		(f == kSupportsLoadingDuringRuntime) ||
		(f == kSupportsSavingDuringRuntime) ||
		(f == kSupportsArbitraryResolutions) ||
		(f == kSupportsRTL);
}

bool StarkEngine::canLoadGameStateCurrently() {
	return !StarkUserInterface->isInSaveLoadMenuScreen();
}

Common::Error StarkEngine::loadGameState(int slot) {
	// Open the save file
	Common::String filename = formatSaveName(_targetName.c_str(), slot);
	Common::InSaveFile *save = _saveFileMan->openForLoading(filename);
	if (!save) {
		return _saveFileMan->getError();
	}

	StateReadStream stream(save);

	// Read the header
	SaveMetadata metadata;
	Common::ErrorCode metadataErrorCode = metadata.read(&stream, filename);
	if (metadataErrorCode != Common::kNoError) {
		return metadataErrorCode;
	}

	// Reset the UI
	StarkUserInterface->skipFMV();
	StarkUserInterface->clearLocationDependentState();
	StarkUserInterface->setInteractive(true);
	StarkUserInterface->changeScreen(Screen::kScreenGame);
	StarkUserInterface->inventoryOpen(false);
	StarkUserInterface->restoreScreenHistory();

	// Clear the previous world resources
	StarkResourceProvider->shutdown();

	if (metadata.version >= 9) {
		metadata.skipGameScreenThumbnail(&stream);
	}

	// Read the resource trees state
	StarkStateProvider->readStateFromStream(&stream, metadata.version);

	// Read the diary state
	StarkDiary->readStateFromStream(&stream, metadata.version);

	// Read the location stack
	StarkResourceProvider->readLocationStack(&stream, metadata.version);

	if (stream.eos()) {
		warning("Unexpected end of file reached when reading '%s'", filename.c_str());
		return Common::kReadingFailed;
	}

	// Initialize the world resources with the loaded state
	StarkResourceProvider->initGlobal();
	StarkResourceProvider->setShouldRestoreCurrentState();
	StarkResourceProvider->requestLocationChange(metadata.levelIndex, metadata.locationIndex);

	if (metadata.version >= 9) {
		setTotalPlayTime(metadata.totalPlayTime);
	}

	return Common::kNoError;
}

bool StarkEngine::canSaveGameStateCurrently() {
	// Disallow saving when there is no level loaded or when a script is running
	// or when the save & load menu is currently displayed
	return StarkGlobal->getLevel() && StarkUserInterface->isInteractive() && !StarkUserInterface->isInSaveLoadMenuScreen();
}

Common::Error StarkEngine::saveGameState(int slot, const Common::String &desc) {
	// Ensure the state store is up to date
	StarkResourceProvider->commitActiveLocationsState();

	// Open the save file
	Common::String filename = formatSaveName(_targetName.c_str(), slot);
	Common::OutSaveFile *save = _saveFileMan->openForSaving(filename);
	if (!save) {
		return _saveFileMan->getError();
	}

	// 1. Write the header
	SaveMetadata metadata;
	metadata.description = desc;
	metadata.version = StateProvider::kSaveVersion;
	metadata.levelIndex = StarkGlobal->getCurrent()->getLevel()->getIndex();
	metadata.locationIndex = StarkGlobal->getCurrent()->getLocation()->getIndex();
	metadata.totalPlayTime = getTotalPlayTime();
	metadata.gameWindowThumbnail = StarkUserInterface->getGameWindowThumbnail();

	TimeDate timeDate;
	_system->getTimeAndDate(timeDate);
	metadata.setSaveTime(timeDate);

	metadata.write(save);
	metadata.writeGameScreenThumbnail(save);

	// 2. Write the resource trees state
	StarkStateProvider->writeStateToStream(save);

	// 3. Write the diary state
	StarkDiary->writeStateToStream(save);

	// 4. Write the location stack
	StarkResourceProvider->writeLocationStack(save);

	delete save;

	return Common::kNoError;
}

Common::String StarkEngine::formatSaveName(const char *target, int slot) {
	return Common::String::format("%s-%03d.tlj", target, slot);
}

void StarkEngine::pauseEngineIntern(bool pause) {
	Engine::pauseEngineIntern(pause);

	// This function may be called when an error occurs before the engine is fully initialized
	if (StarkGlobal && StarkGlobal->getLevel() && StarkGlobal->getCurrent()) {
		StarkGlobal->getLevel()->onEnginePause(pause);
		StarkGlobal->getCurrent()->getLevel()->onEnginePause(pause);
		StarkGlobal->getCurrent()->getLocation()->onEnginePause(pause);
	}

	if (_frameLimiter) {
		_frameLimiter->pause(pause);
	}

	// Grab a game screen thumbnail in case we need one when writing a save file
	if (StarkUserInterface && StarkUserInterface->isInGameScreen()) {
		if (pause) {
			StarkUserInterface->saveGameScreenThumbnail();
		} else {
			StarkUserInterface->freeGameScreenThumbnail();
		}
	}

	// The user may have moved the mouse or resized the window while the engine was paused
	if (!pause && StarkUserInterface) {
		onScreenChanged();
		StarkUserInterface->handleMouseMove(_eventMan->getMousePos());
	}
}

void StarkEngine::onScreenChanged() const {
	bool changed = StarkGfx->computeScreenViewport();
	if (changed) {
		StarkFontProvider->initFonts();
		StarkUserInterface->onScreenChanged();
	}
}

} // End of namespace Stark

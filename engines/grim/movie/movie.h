/* Residual - A 3D game interpreter
 *
 * Residual is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#ifndef GRIM_MOVIE_PLAYER_H
#define GRIM_MOVIE_PLAYER_H

#include "common/mutex.h"
#include "common/system.h"

#include "video/video_decoder.h"

namespace Grim {

class SaveGame;

class MoviePlayer {
protected:
	Common::String _fname;
	Common::Mutex _frameMutex;
	Video::VideoDecoder *_videoDecoder;		//< Initialize this to your needed subclass of VideoDecoder in the constructor
	Graphics::Surface *_surface, *_externalSurface;
	int32 _frame;
	bool _updateNeeded;
	int32 _speed;
	float _movieTime;
	int _channels;
	int _freq;
	bool _videoFinished;
	bool _videoPause;
	bool _videoLooping;
	int _x, _y;
	//int _width, _height;

public:
	MoviePlayer();
	virtual ~MoviePlayer() { }

	/**
	 * Loads a file for playing, and starts playing it.
	 * the default implementation calls init()/deinit() to handle
	 * any necessary setup.
	 *
	 * @param filename		the file to open
	 * @param looping		true if we want the video to loop, false otherwise
	 * @param x				the x-coordinate for the draw-position
	 * @param y				the y-coordinate for the draw-position
	 * @see	init
	 * @see stop
	 */
	virtual bool play(const char *filename, bool looping, int x, int y);
	virtual void stop();
	virtual void pause(bool p);
	virtual bool isPlaying() { return !_videoFinished; }
	virtual bool isUpdateNeeded() { return _updateNeeded; }
	virtual Graphics::Surface *getDstSurface();
	virtual int getX() { return _x; }
	virtual int getY() { return _y; }
	virtual int getFrame() { return _frame; }
	virtual void clearUpdateNeeded() { _updateNeeded = false; }
	virtual int32 getMovieTime() { return (int32)_movieTime; }

	/**
	 * Saves the state of the video to a savegame
	 *
	 * If you overload this in a subclass, call this first thing in the
	 * overloaded function
	 *
	 * @param state			the state to save to
	 */
	virtual void saveState(SaveGame *state);
	virtual void restoreState(SaveGame *state);
	
protected:
	static void timerCallback(void *ptr);
	/**
	 * Handles basic stuff per frame, like copying the latest frame to
	 * _externalBuffer, and updating the frame-counters.
	 *
	 * @return false if a frame wasnt drawn to _externalBuffer, true otherwise.
	 * @see handleFrame
	 */
	virtual bool basicHandleFrame();

	/**
	 * Frame-handling function.
	 * should check if there is a new frame to be drawn,
	 * and if possible, put it in _externalBuffer. (Setting updateNeeded in
	 * the process). A default basic handler is available, to get the basics
	 * down for VideoDecoder-based drawing.
	 *
	 * @see basicHandleFrame
	 * @see clearUpdateNeeded
	 * @see isUpdateNeeded
	 */
	virtual void handleFrame() = 0;

	/**
	 * Initialization of buffers
	 * This function is called by the default-implementation of play,
	 * and is expected to get the necessary datastructures set up for
	 * playback, as well as initializing the callback.
	 *
	 * @see deinit
	 */
	virtual void init();

	/**
	 * Closes any file/codec-handles, and resets the movie-state to
	 * a blank MoviePlayer.
	 *
	 * @see init
	 */
	virtual void deinit();
};


// Factory-like functions:

MoviePlayer *CreateMpegPlayer();
MoviePlayer *CreateSmushPlayer();
MoviePlayer *CreateBinkPlayer();
extern MoviePlayer *g_movie;

} // end of namespace Grim

#endif

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

#ifndef MYST3_RESOURCE_LOADER_H
#define MYST3_RESOURCE_LOADER_H

#include "engines/myst3/archive.h"

#include "common/array.h"
#include "common/str.h"
#include "common/str-array.h"

#include "graphics/surface.h"

namespace Myst3 {

class Renderer;
class Texture;

class ResourceLoader {
public:
	~ResourceLoader();

	void addMod(const Common::String &name);

	void addArchive(const Common::String &filename, bool mandatory);

	void loadRoomArchives(const Common::String &room);
	void unloadRoomArchives();
	const Common::String &currentRoom() const { return _currentRoom; }

	ResourceDescription getFileDescription(const Common::String &room, uint32 index, uint16 face, Archive::ResourceType type) const;
	ResourceDescriptionArray listFilesMatching(const Common::String &room, uint32 index, Archive::ResourceType type) const;

	ResourceDescription getFrameBitmap(const Common::String &room, uint16 nodeId) const;
	ResourceDescription getCubeBitmap(const Common::String &room, uint16 nodeId, uint16 faceId) const;
	ResourceDescription getMovie(const Common::String &room, uint16 movieId) const;
	ResourceDescription getStillMovie(const Common::String &room, uint16 movieId) const;
	ResourceDescription getDialogMovie(const Common::String &room, uint16 movieId) const;
	ResourceDescription getRawData(const Common::String &room, uint16 id) const;
	ResourceDescriptionArray listSpotItemImages(const Common::String &room, uint16 spotItemId) const;

	static bool checkForSubentriesSharingSameKey(const Archive::DirectoryEntry &directoryEntry,
	                                             const Archive::DirectorySubEntry &directorySubEntry);

	static Common::String computeExtractedFileName(const Archive::DirectoryEntry &directoryEntry,
	                                               const Archive::DirectorySubEntry &directorySubEntry,
	                                               bool multipleSubEntriesWithSameKey);
	static Common::String computeExtractedFileName(const Archive::DirectoryEntry &directoryEntry,
	                                               const Archive::DirectorySubEntry &directorySubEntry,
	                                               bool multipleSubEntriesWithSameKey,
	                                               const char *imagesFileExtension,
	                                               const char *cursorFileExtension,
	                                               const char *moddedImagesFileExtension);

private:
	Common::StringArray _mods;

	Common::Array<Archive *> _commonArchives;

	Common::String _currentRoom;
	Common::Array<Archive *> _roomArchives;
};

class TexDecoder {
public:
	~TexDecoder();

	bool loadStream(Common::SeekableReadStream &stream, const Common::String &name);

	const Graphics::Surface *getSurface() const { return &_outputSurface; }

private:
	Graphics::Surface _outputSurface;
};

class TextureLoader {
public:
	enum ImageFormat {
		kImageFormatJPEG,
		kImageFormatPNG,
		kImageFormatBMP,
		kImageFormatDDS,
		kImageFormatTEX
	};

	TextureLoader(Renderer &renderer);

	Texture *load(const ResourceDescription &resource, ImageFormat defaultImageFormat);
private:
	Renderer &_renderer;
	bool _loadExternalFiles;
};

class VideoLoader {
public:
	VideoLoader();

	Common::SeekableReadStream *load(const ResourceDescription &resource);

private:
	bool _loadExternalFiles;
};


} // End of namespace Myst3

#endif

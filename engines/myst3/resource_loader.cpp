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

#include "engines/myst3/resource_loader.h"

#include "engines/myst3/archive.h"
#include "engines/myst3/debug.h"
#include "engines/myst3/gfx.h"

#include "common/archive.h"
#include "common/config-manager.h"
#include "common/fs.h"

#include "image/bmp.h"
#include "image/dds.h"
#include "image/jpeg.h"
#include "image/png.h"

namespace Myst3 {

ResourceLoader::~ResourceLoader() {
	unloadRoomArchives();

	for (uint i = 0; i < _commonArchives.size(); i++) {
		delete _commonArchives[i];
	}
}

void ResourceLoader::addMod(const Common::String &name) {
	_mods.push_back(name);
	debugC(kDebugModding, "Registered mod '%s'", name.c_str());
}

void ResourceLoader::addArchive(const Common::String &filename, bool mandatory) {
	for (uint i = 0; i < _mods.size(); i++) {
		Common::String modFilename = Common::String::format("mods/%s/%s.patch", _mods[i].c_str(), filename.c_str());
		Archive *modArchive = Archive::createFromFile(modFilename, "");
		if (modArchive) {
			_commonArchives.push_back(modArchive);
			debugC(kDebugModding, "Loaded mod archive '%s'", modFilename.c_str());
		}
	}

	Archive *archive = Archive::createFromFile(filename, "");
	if (archive) {
		_commonArchives.push_back(archive);
		return;
	}

	if (mandatory) {
		error("Unable to open archive %s", filename.c_str());
	}
}

void ResourceLoader::unloadRoomArchives() {
	for (uint i = 0; i < _roomArchives.size(); i++) {
		delete _roomArchives[i];
	}
	_roomArchives.clear();
	_currentRoom.clear();
}

void ResourceLoader::loadRoomArchives(const Common::String &room) {
	unloadRoomArchives();

	for (uint i = 0; i < _mods.size(); i++) {
		Common::String modNodeFile = Common::String::format("mods/%s/%snodes.m3a.patch", _mods[i].c_str(), room.c_str());
		Archive *modNodeArchive = Archive::createFromFile(modNodeFile, room);
		if (modNodeArchive) {
			_roomArchives.push_back(modNodeArchive);
			debugC(kDebugModding, "Loaded mod archive '%s'", modNodeFile.c_str());
		}
	}

	Common::String roomFile = Common::String::format("%snodes.m3a", room.c_str());
	Archive *roomArchive = Archive::createFromFile(roomFile, room);
	if (!roomArchive) {
		error("Unable to open archive %s", roomFile.c_str());
	}

	_roomArchives.push_back(roomArchive);
	_currentRoom = room;
}

ResourceDescription ResourceLoader::getFileDescription(const Common::String &room, uint32 index, uint16 face,
                                                       Archive::ResourceType type) const {
	if (room.empty()) {
		error("No archive room name found when looking up resource %d-%d.%d", index, face, type);
	}

	// Search common archives
	for (uint archiveIndex = 0; archiveIndex < _commonArchives.size(); archiveIndex++) {
		ResourceDescription desc = _commonArchives[archiveIndex]->getDescription(room, index, face, type);
		if (desc.isValid()) {
			return desc;
		}
	}

	// Search currently loaded node archives
	for (uint archiveIndex = 0; archiveIndex < _roomArchives.size(); archiveIndex++) {
		ResourceDescription desc = _roomArchives[archiveIndex]->getDescription(room, index, face, type);
		if (desc.isValid()) {
			return desc;
		}
	}

	return ResourceDescription();
}

ResourceDescriptionArray ResourceLoader::listFilesMatching(const Common::String &room, uint32 index,
                                                           Archive::ResourceType type) const {
	if (room.empty()) {
		error("No archive room name found when looking up resource %d.%d", index, type);
	}

	for (uint archiveIndex = 0; archiveIndex < _commonArchives.size(); archiveIndex++) {
		ResourceDescriptionArray list = _commonArchives[archiveIndex]->listFilesMatching(room, index, type);
		if (!list.empty()) {
			return list;
		}
	}

	for (uint archiveIndex = 0; archiveIndex < _roomArchives.size(); archiveIndex++) {
		ResourceDescriptionArray list = _roomArchives[archiveIndex]->listFilesMatching(room, index, type);
		if (!list.empty()) {
			return list;
		}
	}

	return ResourceDescriptionArray();
}

ResourceDescription ResourceLoader::getFrameBitmap(const Common::String &room, uint16 nodeId) const {
	ResourceDescription resource = getFileDescription(room, nodeId, 0, Archive::kModdedFrame);

	if (!resource.isValid()) {
		resource = getFileDescription(room, nodeId, 1, Archive::kModdedFrame);
	}

	if (!resource.isValid()) {
		resource = getFileDescription(room, nodeId, 1, Archive::kLocalizedFrame);
	}

	if (!resource.isValid()) {
		resource = getFileDescription(room, nodeId, 0, Archive::kFrame);
	}

	if (!resource.isValid()) {
		resource = getFileDescription(room, nodeId, 1, Archive::kFrame);
	}

	if (!resource.isValid()) {
		error("Frame %d does not exist in room %s", nodeId, room.c_str());
	}

	return resource;
}

ResourceDescription ResourceLoader::getCubeBitmap(const Common::String &room, uint16 nodeId, uint16 faceId) const {
	ResourceDescription resource = getFileDescription(room, nodeId, faceId + 1, Archive::kModdedCubeFace);

	if (!resource.isValid()) {
		resource = getFileDescription(room, nodeId, faceId + 1, Archive::kCubeFace);
	}

	if (!resource.isValid())
		error("Unable to load face %d from node %d does not exist", faceId, nodeId);

	return resource;
}

ResourceDescription ResourceLoader::getMovie(const Common::String &room, uint16 movieId) const {
	ResourceDescription resource = getFileDescription(room, movieId, 0, Archive::kModdedMovie);

	if (!resource.isValid())
		resource = getFileDescription(room, movieId, 0, Archive::kMultitrackMovie);

	if (!resource.isValid())
		resource = getFileDescription(room, movieId, 0, Archive::kDialogMovie);

	if (!resource.isValid())
		resource = getFileDescription(room, movieId, 0, Archive::kStillMovie);

	if (!resource.isValid())
		resource = getFileDescription(room, movieId, 0, Archive::kMovie);

	return resource;
}

ResourceDescription ResourceLoader::getStillMovie(const Common::String &room, uint16 movieId) const {
	ResourceDescription resource = getFileDescription(room, movieId, 0, Archive::kModdedMovie);

	if (!resource.isValid())
		resource = getFileDescription(room, movieId, 0, Archive::kStillMovie);

	return resource;
}

ResourceDescription ResourceLoader::getDialogMovie(const Common::String &room, uint16 movieId) const {
	ResourceDescription resource = getFileDescription(room, movieId, 0, Archive::kModdedMovie);

	if (!resource.isValid())
		resource = getFileDescription(room, movieId, 0, Archive::kDialogMovie);

	if (!resource.isValid())
		resource = getFileDescription(room, movieId, 0, Archive::kStillMovie);

	return resource;
}

ResourceDescription ResourceLoader::getRawData(const Common::String &room, uint16 id) const {
	ResourceDescription resource = getFileDescription(room, id, 0, Archive::kModdedRawData);

	if (!resource.isValid())
		resource = getFileDescription(room, id, 0, Archive::kRawData);

	return resource;
}

ResourceDescriptionArray ResourceLoader::listSpotItemImages(const Common::String &room, uint16 spotItemId) const {
	ResourceDescriptionArray resources;
	resources.push_back(listFilesMatching(room, spotItemId, Archive::kModdedSpotItem));

	if (resources.empty()) {
		resources.push_back(listFilesMatching(room, spotItemId, Archive::kLocalizedSpotItem));
		resources.push_back(listFilesMatching(room, spotItemId, Archive::kSpotItem));
	}

	return resources;
}

bool ResourceLoader::checkForSubentriesSharingSameKey(const Archive::DirectoryEntry &directoryEntry,
                                                      const Archive::DirectorySubEntry &directorySubEntry) {
	for (uint i = 0; i < directoryEntry.subentries.size(); i++) {
		const Archive::DirectorySubEntry &otherSubEntry = directoryEntry.subentries[i];
		if (otherSubEntry.type == directorySubEntry.type
		        && otherSubEntry.face == directorySubEntry.face
		        && otherSubEntry.offset != directorySubEntry.offset) {
			return true;
		}
	}

	return false;
}

Common::String ResourceLoader::computeExtractedFileName(const Archive::DirectoryEntry &directoryEntry,
                                                        const Archive::DirectorySubEntry &directorySubEntry,
                                                        bool multipleSubEntriesWithSameKey) {
	return computeExtractedFileName(directoryEntry, directorySubEntry, multipleSubEntriesWithSameKey, "jpg", "data", "dds");
}

Common::String ResourceLoader::computeExtractedFileName(const Archive::DirectoryEntry &directoryEntry,
                                                        const Archive::DirectorySubEntry &directorySubEntry,
                                                        bool multipleSubEntriesWithSameKey,
                                                        const char *imagesFileExtension,
                                                        const char *cursorFileExtension,
                                                        const char *moddedImagesFileExtension) {
	bool printFace = true;
	Common::String extension;
	switch (directorySubEntry.type) {
	case Archive::kNumMetadata:
	case Archive::kTextMetadata:
		return ""; // These types are pure metadata and can't be extracted
	case Archive::kCubeFace:
	case Archive::kFrame:
	case Archive::kLocalizedFrame:
	case Archive::kSpotItem:
	case Archive::kLocalizedSpotItem:
		extension = imagesFileExtension;
		break;
	case Archive::kModdedCubeFace:
	case Archive::kModdedFrame:
	case Archive::kModdedSpotItem:
		extension = moddedImagesFileExtension;
		break;
	case Archive::kWaterEffectMask:
		extension = "water";
		break;
	case Archive::kLavaEffectMask:
		extension = "lava";
		break;
	case Archive::kMagneticEffectMask:
		extension = "magnet";
		break;
	case Archive::kShieldEffectMask:
		extension = "shield";
		break;
	case Archive::kMovie:
	case Archive::kStillMovie:
	case Archive::kDialogMovie:
	case Archive::kMultitrackMovie:
	case Archive::kModdedMovie:
		printFace = false;
		extension = "bik";
		break;
	case Archive::kRawData:
		printFace = false;
		extension = cursorFileExtension;
		break;
	case Archive::kModdedRawData:
		printFace = false;
		extension = moddedImagesFileExtension;
		break;
	default:
		extension = Common::String::format("%d", directorySubEntry.type);
		break;

	}

	if (printFace && multipleSubEntriesWithSameKey) {
		return Common::String::format("dump/%s-%d-%d-%d.%s", directoryEntry.roomName.c_str(), directoryEntry.index,
		                              directorySubEntry.face, directorySubEntry.offset, extension.c_str());
	}

	if (printFace && !multipleSubEntriesWithSameKey) {
		return Common::String::format("dump/%s-%d-%d.%s", directoryEntry.roomName.c_str(), directoryEntry.index,
		                              directorySubEntry.face, extension.c_str());
	}

	if (multipleSubEntriesWithSameKey) {
		return Common::String::format("dump/%s-%d-%d.%s", directoryEntry.roomName.c_str(), directoryEntry.index,
		                              directorySubEntry.offset, extension.c_str());
	}

	return Common::String::format("dump/%s-%d.%s", directoryEntry.roomName.c_str(), directoryEntry.index,
	                              extension.c_str());
}

TexDecoder::~TexDecoder() {
	_outputSurface.free();
}

bool TexDecoder::loadStream(Common::SeekableReadStream &stream, const Common::String &name) {
	uint32 magic = stream.readUint32LE();
	if (magic != MKTAG('.', 'T', 'E', 'X')) {
		warning("Invalid texture format for '%s'", name.c_str());
		return false;
	}

	stream.readUint32LE(); // unk 1
	uint32 width = stream.readUint32LE();
	uint32 height = stream.readUint32LE();
	stream.readUint32LE(); // unk 2
	stream.readUint32LE(); // unk 3

#ifdef SCUMM_BIG_ENDIAN
	Graphics::PixelFormat onDiskFormat = Graphics::PixelFormat(4, 8, 8, 8, 8, 0, 24, 16, 8);
#else
	Graphics::PixelFormat onDiskFormat = Graphics::PixelFormat(4, 8, 8, 8, 8, 8, 16, 24, 0);
#endif

	_outputSurface.create(width, height, onDiskFormat);
	stream.read(_outputSurface.getPixels(), height * _outputSurface.pitch);

	_outputSurface.convertToInPlace(Texture::getRGBAPixelFormat());

	return true;
}

static Common::SeekableReadStream *openFile(const Common::String &filename) {
	debugC(kDebugModding, "Attempting to load external file '%s'", filename.c_str());

	// FIXME: FSNode::createReadStream should not print a warning when attempting to open non existing files
	Common::FSNode fsnode = Common::FSNode(filename);
	if (!fsnode.exists()) {
		return nullptr;
	}

	Common::SeekableReadStream *externalStream = fsnode.createReadStream();
	if (externalStream) {
		debugC(kDebugModding, "Loaded external file '%s'", filename.c_str());
	}

	return externalStream;
}

TextureLoader::TextureLoader(Renderer &renderer) :
		_renderer(renderer),
		_loadExternalFiles(ConfMan.getBool("enable_external_assets")) {
}

Texture *TextureLoader::load(const ResourceDescription &resource, TextureLoader::ImageFormat defaultImageFormat) {
	ImageFormat imageFormat;
	Common::SeekableReadStream *imageStream = nullptr;
	Common::String name = Common::String::format("%s-%d-%d", resource.room().c_str(), resource.index(), resource.face());

	if (_loadExternalFiles) {
		bool multipleSubEntriesWithSameKey = ResourceLoader::checkForSubentriesSharingSameKey(resource.directoryEntry(), resource.directorySubEntry());
		name = ResourceLoader::computeExtractedFileName(resource.directoryEntry(), resource.directorySubEntry(), multipleSubEntriesWithSameKey, "dds", "dds", "dds");
		imageStream = openFile(name);
		if (imageStream) {
			imageFormat = kImageFormatDDS;
		}

		if (!imageStream) {
			name = ResourceLoader::computeExtractedFileName(resource.directoryEntry(), resource.directorySubEntry(), multipleSubEntriesWithSameKey, "png", "png", "png");
			imageStream = openFile(name);
			if (imageStream) {
				imageFormat = kImageFormatPNG;
			}
		}

		if (!imageStream) {
			name = ResourceLoader::computeExtractedFileName(resource.directoryEntry(), resource.directorySubEntry(), multipleSubEntriesWithSameKey, "jpg", "jpg", "jpg");
			imageStream = openFile(name);
			if (imageStream) {
				imageFormat = kImageFormatJPEG;
			}
		}
	}

	if (!imageStream) {
		imageStream = resource.createReadStream();
		if (resource.type() == Archive::kModdedCubeFace
		        || resource.type() == Archive::kModdedFrame
		        || resource.type() == Archive::kModdedSpotItem
		        || resource.type() == Archive::kModdedRawData) {
			imageFormat = kImageFormatDDS;
		} else {
			imageFormat = defaultImageFormat;
		}
	}

	switch (imageFormat) {
	case kImageFormatJPEG: {
		Image::JPEGDecoder jpeg;
		jpeg.setOutputPixelFormat(Texture::getRGBAPixelFormat());

		if (!jpeg.loadStream(*imageStream)) {
			error("Failed to decode JPEG %s", name.c_str());
		}
		delete imageStream;

		const Graphics::Surface *bitmap = jpeg.getSurface();
		assert(bitmap->format == Texture::getRGBAPixelFormat());

		return _renderer.createTexture(*bitmap);
	}
	case kImageFormatPNG: {
		Image::PNGDecoder decoder;

		if (!decoder.loadStream(*imageStream)) {
			error("Failed to decode PNG %s", name.c_str());
		}
		delete imageStream;

		return _renderer.createTexture(*decoder.getSurface());
	}
	case kImageFormatDDS: {
		Image::DDS decoder;

		if (!decoder.load(*imageStream, name)) {
			error("Failed to decode DDS %s", name.c_str());
		}
		delete imageStream;

		return _renderer.createTexture(decoder);

	}
	case kImageFormatTEX: {
		TexDecoder decoder;

		if (!decoder.loadStream(*imageStream, name)) {
			error("Failed to decode TEX %s", name.c_str());
		}
		delete imageStream;

		return _renderer.createTexture(*decoder.getSurface());
	}
	case kImageFormatBMP: {
		Image::BitmapDecoder decoder;
		if (!decoder.loadStream(*imageStream)) {
			error("Failed to decode BMP %s", name.c_str());
		}

		const Graphics::Surface *surfaceBGRA = decoder.getSurface();
		Graphics::Surface *surfaceRGBA = surfaceBGRA->convertTo(Texture::getRGBAPixelFormat());

		delete imageStream;

		// Apply the colorkey for transparency
		for (uint y = 0; y < surfaceRGBA->h; y++) {
			byte *pixels = (byte *)(surfaceRGBA->getBasePtr(0, y));
			for (uint x = 0; x < surfaceRGBA->w; x++) {
				byte *r = pixels + 0;
				byte *g = pixels + 1;
				byte *b = pixels + 2;
				byte *a = pixels + 3;

				if (*r == 0 && *g == 0xFF && *b == 0 && *a == 0xFF) {
					*g = 0;
					*a = 0;
				}

				pixels += 4;
			}
		}

		Texture *texture = _renderer.createTexture(*surfaceRGBA);

		surfaceRGBA->free();
		delete surfaceRGBA;

		return texture;
	}
	default:
		assert(false);
	}
}

VideoLoader::VideoLoader() :
    _loadExternalFiles(ConfMan.getBool("enable_external_assets")) {
}

Common::SeekableReadStream *VideoLoader::load(const ResourceDescription &resource) {
	Common::SeekableReadStream *binkStream = nullptr;
	if (_loadExternalFiles) {
		bool multipleSubEntriesWithSameKey = ResourceLoader::checkForSubentriesSharingSameKey(resource.directoryEntry(), resource.directorySubEntry());
		Common::String extractedFileName = ResourceLoader::computeExtractedFileName(resource.directoryEntry(), resource.directorySubEntry(), multipleSubEntriesWithSameKey);
		debugC(kDebugModding, "Attempting to load external file '%s'", extractedFileName.c_str());
		binkStream = SearchMan.createReadStreamForMember(extractedFileName);
		if (binkStream) {
			debugC(kDebugModding, "Loaded external file '%s'", extractedFileName.c_str());
		}
	}

	if (!binkStream) {
		binkStream = resource.createReadStream();
	}

	return binkStream;
}

} // End of namespace Myst3

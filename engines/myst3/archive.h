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

#ifndef MYST3_ARCHIVE_H
#define MYST3_ARCHIVE_H

#include "common/array.h"
#include "common/stream.h"

#include "math/vector3d.h"

namespace Myst3 {

class ArchiveVisitor;
class ResourceDescription;

typedef Common::Array<ResourceDescription> ResourceDescriptionArray;
typedef Common::Array<uint32> MetadataArray;

class Archive {
public:
	enum ResourceType {
		kCubeFace           =   0,
		kWaterEffectMask    =   1,
		kLavaEffectMask     =   2,
		kMagneticEffectMask =   3,
		kShieldEffectMask   =   4,
		kSpotItem           =   5,
		kFrame              =   6,
		kRawData            =   7,
		kMovie              =   8,
		kStillMovie         =  10,
		kText               =  11,
		kTextMetadata       =  12,
		kNumMetadata        =  13,
		kLocalizedSpotItem  =  69,
		kLocalizedFrame     =  70,
		kMultitrackMovie    =  72,
		kDialogMovie        =  74,
		kModdedCubeFace     = 100,
		kModdedSpotItem     = 105,
		kModdedFrame        = 106,
		kModdedRawData      = 107,
		kModdedMovie        = 108
	};

	struct DirectorySubEntry {
		uint32 offset;
		uint32 size;
		byte face;
		ResourceType type;
		MetadataArray metadata;

		DirectorySubEntry() : offset(0), size(0), face(0), type(kCubeFace) {}
	};

	struct DirectoryEntry {
		Common::String roomName;
		uint32 index;
		Common::Array<DirectorySubEntry> subentries;

		DirectoryEntry() : index(0) {}
	};

	Archive(Common::SeekableReadStream *file, const Common::String &roomName);
	~Archive();

	static Archive *createFromFile(const Common::String &filename, const Common::String &roomName);

	ResourceDescription getDescription(const Common::String &room, uint32 index, uint16 face,
	                                   ResourceType type);
	ResourceDescriptionArray listFilesMatching(const Common::String &room, uint32 index, ResourceType type);

	Common::SeekableReadStream *dumpToMemory(uint32 offset, uint32 size);
	void visit(ArchiveVisitor &visitor);

	const Common::String &getRoomName() const { return _roomName; }

private:
	Common::String _roomName;
	Common::SeekableReadStream *_file;
	Common::Array<DirectoryEntry> _directory;

	void decryptHeader(Common::SeekableReadStream &inStream, Common::WriteStream &outStream);
	void readDirectory();
	DirectorySubEntry readSubEntry(Common::ReadStream &stream);
	DirectoryEntry readEntry(Common::ReadStream &stream);
	const DirectoryEntry *getEntry(const Common::String &room, uint32 index) const;
};

class ResourceDescription {
public:
	struct SpotItemData {
		uint32 u;
		uint32 v;

		SpotItemData() : u(0), v(0) {}
	};

	struct VideoData {
		Math::Vector3d v1;
		Math::Vector3d v2;
		int32 u;
		int32 v;
		int32 width;
		int32 height;

		VideoData() : u(0), v(0), width(0), height(0) {}
	};

	ResourceDescription();
	ResourceDescription(Archive *archive, const Archive::DirectoryEntry &entry, const Archive::DirectorySubEntry &subentry);

	bool isValid() const { return _archive && _entry && _subentry; }

	Common::SeekableReadStream *createReadStream() const;

	const Common::String &room() const { return _entry->roomName; }
	uint16 index() const { return _entry->index; }
	uint16 face() const { return _subentry->face; }
	Archive::ResourceType type() const { return _subentry->type; }
	SpotItemData spotItemData() const;
	VideoData videoData() const;
	uint32 miscData(uint index) const;
	Common::String textData(uint index) const;

	const Archive::DirectoryEntry &directoryEntry() const {
		assert(isValid());
		return *_entry;
	}

	const Archive::DirectorySubEntry &directorySubEntry() const {
		assert(isValid());
		return *_subentry;
	}
private:
	Archive *_archive;
	const Archive::DirectoryEntry *_entry;
	const Archive::DirectorySubEntry *_subentry;
};

class ArchiveVisitor {
public:
	virtual ~ArchiveVisitor();

	virtual void visitArchive(Archive &archive) {}
	virtual void visitDirectoryEntry(Archive::DirectoryEntry &directoryEntry) {}
	virtual void visitDirectorySubEntry(Archive::DirectoryEntry &directoryEntry, Archive::DirectorySubEntry &directorySubEntry) {}
};

class ArchiveWriter {
public:
	ArchiveWriter(const Common::String &room);

	void addFile(const Common::String &room, uint32 index, byte face, Archive::ResourceType type,
	             const MetadataArray &metadata, const Common::String &filename, bool compress);
	void write(Common::SeekableWriteStream &outStream);

	bool empty() const { return _directory.empty(); }

private:
	struct DirectorySubEntry {
		uint32 offset;
		uint32 size;
		byte face;
		Archive::ResourceType type;
		MetadataArray metadata;
		Common::String filename;
		bool compress;

		DirectorySubEntry() : offset(0), size(0), face(0), type(Archive::kCubeFace), compress(false) {}
	};

	struct DirectoryEntry {
		Common::String roomName;
		uint32 index;
		Common::Array<DirectorySubEntry> subentries;

		DirectoryEntry() : index(0) {}
	};

	DirectoryEntry *getEntry(const Common::String &room, uint32 index);
	void writeDirectory(Common::SeekableWriteStream &outStream);
	void writeFiles(Common::SeekableWriteStream &outStream);
	void encryptHeader(uint32 *header, uint32 length);

	Common::String _room;
	Common::Array<DirectoryEntry> _directory;
};

} // End of namespace Myst3

#endif

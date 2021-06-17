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

#include "engines/myst3/archive.h"

#include "common/archive.h"
#include "common/debug.h"
#include "common/fs.h"
#include "common/memstream.h"
#include "common/substream.h"

#include "engines/myst3/lzo.h"

namespace Myst3 {

static const uint32 kLZO1X = MKTAG('L', 'Z', 'O', 'X');

Archive::Archive(Common::SeekableReadStream *file, const Common::String &roomName) :
		_roomName(roomName),
		_file(file) {
	assert(file);
	readDirectory();
}

Archive::~Archive() {
	delete _file;
}

Archive *Archive::createFromFile(const Common::String &filename, const Common::String &roomName) {
	Common::SeekableReadStream *file = SearchMan.createReadStreamForMember(filename);
	if (!file) {
		return nullptr;
	}

	return new Archive(file, roomName);
}

void Archive::decryptHeader(Common::SeekableReadStream &inStream, Common::WriteStream &outStream) {
	static const uint32 addKey = 0x3C6EF35F;
	static const uint32 multKey = 0x0019660D;

	inStream.seek(0);
	uint32 size = inStream.readUint32LE();

	bool encrypted = size > 1000000;
	
	inStream.seek(0);

	if (encrypted) {
		uint32 decryptedSize = size ^ addKey;

		uint32 currentKey = 0;
		for (uint i = 0; i < decryptedSize; i++) {
			currentKey += addKey;
			outStream.writeUint32LE(inStream.readUint32LE() ^ currentKey);
			currentKey *= multKey;
		}
	} else {
		for (uint i = 0; i < size; i++) {
			outStream.writeUint32LE(inStream.readUint32LE());
		}
	}
}

static Common::String readFixedString(Common::ReadStream &stream, uint32 length) {
	Common::String value;

	for (uint i = 0; i < length; i++) {
		value += stream.readByte();
	}

	return value;
}

static uint32 readUint24(Common::ReadStream &stream) {
	uint32 value = stream.readUint16LE();
	value |= stream.readByte() << 16;
	return value;
}

static void writeUint24(Common::WriteStream &stream, uint32 value) {
	stream.writeUint16LE(value & 0xFFFF);
	stream.writeByte(value >> 16);
}

Archive::DirectorySubEntry Archive::readSubEntry(Common::ReadStream &stream) {
	DirectorySubEntry subEntry;

	subEntry.offset = stream.readUint32LE();
	subEntry.size = stream.readUint32LE();
	uint16 metadataSize = stream.readUint16LE();
	subEntry.face = stream.readByte();
	subEntry.type = static_cast<ResourceType>(stream.readByte());

	subEntry.metadata.resize(metadataSize);
	for (uint i = 0; i < metadataSize; i++) {
		subEntry.metadata[i] = stream.readUint32LE();
	}

	return subEntry;
}

Archive::DirectoryEntry Archive::readEntry(Common::ReadStream &stream) {
	DirectoryEntry entry;
	if (_roomName.empty()) {
		entry.roomName = readFixedString(stream, 4);
	} else {
		entry.roomName = _roomName;
	}
	entry.index = readUint24(stream);

	byte subItemCount = stream.readByte();
	entry.subentries.resize(subItemCount);

	for (uint i = 0; i < subItemCount; i++) {
		entry.subentries[i] = readSubEntry(stream);
	}

	return entry;
}

void Archive::readDirectory() {
	Common::MemoryWriteStreamDynamic buf(DisposeAfterUse::YES);
	decryptHeader(*_file, buf);

	Common::MemoryReadStream directory(buf.getData(), buf.size());
	/* uint32 directorySize = */ directory.readUint32LE();

	while (directory.pos() + 4 < directory.size()) {
		_directory.push_back(readEntry(directory));
	}
}

void Archive::visit(ArchiveVisitor &visitor) {
	visitor.visitArchive(*this);

	for (uint i = 0; i < _directory.size(); i++) {
		visitor.visitDirectoryEntry(_directory[i]);

		for (uint j = 0; j < _directory[i].subentries.size(); j++) {
			visitor.visitDirectorySubEntry(_directory[i], _directory[i].subentries[j]);
		}
	}
}

Common::SeekableReadStream *Archive::dumpToMemory(uint32 offset, uint32 size) {
	_file->seek(offset);

	byte *data = (byte *)malloc(size);
	_file->read(data, size);

	uint32 signature = READ_LE_UINT32(data);
	if (signature == kLZO1X) {
		uint32 uncompressedSize = READ_LE_UINT32(data + 4);
		byte *uncompressed = (byte *)malloc(uncompressedSize);

		size_t uncompressedWritten = 0;
		LzoResult decompressResult = lzoDecompress(data + 8, size - 8, uncompressed, uncompressedSize, uncompressedWritten);
		if (decompressResult != LzoResult::Success) {
			error("Unable to decompress at offset %d", offset);
		}

		assert(uncompressedWritten == uncompressedSize);

		free(data);
		return new Common::MemoryReadStream(uncompressed, uncompressedSize, DisposeAfterUse::YES);
	}

	return new Common::MemoryReadStream(data, size, DisposeAfterUse::YES);
}

const Archive::DirectoryEntry *Archive::getEntry(const Common::String &room, uint32 index) const {
	for (uint i = 0; i < _directory.size(); i++) {
		const DirectoryEntry &entry = _directory[i];
		if (entry.index == index && entry.roomName == room) {
			return &entry;
		}
	}

	return nullptr;
}

ResourceDescription Archive::getDescription(const Common::String &room, uint32 index, uint16 face,
                                                 ResourceType type) {
	const DirectoryEntry *entry = getEntry(room, index);
	if (!entry) {
		return ResourceDescription();
	}

	for (uint i = 0; i < entry->subentries.size(); i++) {
		const DirectorySubEntry &subentry = entry->subentries[i];
		if (subentry.face == face && subentry.type == type) {
			return ResourceDescription(this, *entry, subentry);
		}
	}

	return ResourceDescription();
}

ResourceDescriptionArray Archive::listFilesMatching(const Common::String &room, uint32 index, ResourceType type) {
	const DirectoryEntry *entry = getEntry(room, index);
	if (!entry) {
		return ResourceDescriptionArray();
	}

	ResourceDescriptionArray list;
	for (uint i = 0; i < entry->subentries.size(); i++) {
		const DirectorySubEntry &subentry = entry->subentries[i];
		if (subentry.type == type) {
			list.push_back(ResourceDescription(this, *entry, subentry));
		}
	}

	return list;
}

ResourceDescription::ResourceDescription() :
		_archive(nullptr),
		_entry(nullptr),
		_subentry(nullptr) {
}

ResourceDescription::ResourceDescription(Archive *archive, const Archive::DirectoryEntry &entry, const Archive::DirectorySubEntry &subentry) :
		_archive(archive),
		_entry(&entry),
		_subentry(&subentry) {
}

Common::SeekableReadStream *ResourceDescription::createReadStream() const {
	return _archive->dumpToMemory(_subentry->offset, _subentry->size);
}

ResourceDescription::SpotItemData ResourceDescription::spotItemData() const {
	assert(_subentry->type == Archive::kSpotItem
	       || _subentry->type == Archive::kLocalizedSpotItem
	       || _subentry->type == Archive::kModdedSpotItem);

	SpotItemData spotItemData;
	spotItemData.u = _subentry->metadata[0];
	spotItemData.v = _subentry->metadata[1];

	return spotItemData;
}

ResourceDescription::VideoData ResourceDescription::videoData() const {
	VideoData videoData;

	if (_subentry->type == Archive::kMovie
	    || _subentry->type == Archive::kMultitrackMovie
	    || _subentry->type == Archive::kModdedMovie) {
		videoData.v1.setValue(0, static_cast<int32>(_subentry->metadata[0]) * 0.000001f);
		videoData.v1.setValue(1, static_cast<int32>(_subentry->metadata[1]) * 0.000001f);
		videoData.v1.setValue(2, static_cast<int32>(_subentry->metadata[2]) * 0.000001f);

		videoData.v2.setValue(0, static_cast<int32>(_subentry->metadata[3]) * 0.000001f);
		videoData.v2.setValue(1, static_cast<int32>(_subentry->metadata[4]) * 0.000001f);
		videoData.v2.setValue(2, static_cast<int32>(_subentry->metadata[5]) * 0.000001f);

		videoData.u      = static_cast<int32>(_subentry->metadata[6]);
		videoData.v      = static_cast<int32>(_subentry->metadata[7]);
		videoData.width  = static_cast<int32>(_subentry->metadata[8]);
		videoData.height = static_cast<int32>(_subentry->metadata[9]);
	}

	return videoData;
}

uint32 ResourceDescription::miscData(uint index) const {
	assert(_subentry->type == Archive::kNumMetadata || _subentry->type == Archive::kTextMetadata);

	if (index == 0) {
		return _subentry->offset;
	} else if (index == 1) {
		return _subentry->size;
	} else {
		return _subentry->metadata[index - 2];
	}
}

Common::String ResourceDescription::textData(uint index) const {
	assert(_subentry->type == Archive::kTextMetadata);

	uint8 key = 35;
	uint8 cnt = 0;
	uint8 decrypted[89];
	memset(decrypted, 0, sizeof(decrypted));

	uint8 *out = &decrypted[0];
	while (cnt / 4 < (_subentry->metadata.size() + 2) && cnt < 89) {
		// XORed text stored in little endian 32 bit words
		*out++ = (miscData(cnt / 4) >> (8 * (3 - (cnt % 4)))) ^ key++;
		cnt++;
	}

	// decrypted contains a null separated string array
	// extract the wanted one
	cnt = 0;
	int i = 0;
	Common::String text;
	while (cnt <= index && i < 89) {
		if (cnt == index)
			text += decrypted[i];

		if (!decrypted[i])
			cnt++;

		i++;
	}

	return text;
}

ArchiveVisitor::~ArchiveVisitor() {
}

ArchiveWriter::ArchiveWriter(const Common::String &room) :
		_room(room) {
}

void ArchiveWriter::addFile(const Common::String &room, uint32 index, byte face, Archive::ResourceType type,
                            const MetadataArray &metadata, const Common::String &filename, bool compress) {
	if (!_room.empty()) {
		assert(room == _room);
	}

	DirectoryEntry *entry = getEntry(room, index);
	if (!entry) {
		DirectoryEntry newEntry;
		newEntry.roomName = room;
		newEntry.index    = index;

		_directory.push_back(newEntry);

		entry = &_directory.back();
	}

	DirectorySubEntry newSubEntry;
	newSubEntry.face     = face;
	newSubEntry.type     = type;
	newSubEntry.filename = filename;
	newSubEntry.compress = compress;

	if (type == Archive::kNumMetadata || type == Archive::kTextMetadata) {
		assert(filename.empty());
		assert(!metadata.empty());

		newSubEntry.offset = metadata[0];
		if (metadata.size() >= 2) {
			newSubEntry.size = metadata[1];
		}
		if (metadata.size() >= 3) {
			newSubEntry.metadata = MetadataArray(&metadata[2], metadata.size() - 2);
		}
	} else {
		assert(!filename.empty());
		newSubEntry.metadata = metadata;
	}

	entry->subentries.push_back(newSubEntry);
}

void ArchiveWriter::write(Common::SeekableWriteStream &outStream) {
	// Compute the size of the directory
	Common::MemoryWriteStreamDynamic directorySizingTempBuffer(DisposeAfterUse::YES);
	writeDirectory(directorySizingTempBuffer);

	uint32 directoryBufferSize = sizeof(uint32);                // directorySize
	directoryBufferSize += directorySizingTempBuffer.size();    // directory
	directoryBufferSize += sizeof(uint32);                      // checksum

	// Write the data files to the output stream,
	//  saving the offsets to the in-memory directory along the way
	outStream.seek(directoryBufferSize);
	writeFiles(outStream);

	// Write the directory to a temporary buffer
	byte *directoryBuffer = new byte[directoryBufferSize];
	memset(directoryBuffer, 0, directoryBufferSize);

	Common::SeekableMemoryWriteStream directoryStream(directoryBuffer, directoryBufferSize);
	directoryStream.writeUint32LE(directoryBufferSize / 4);
	writeDirectory(directoryStream);

	// Encrypt the directory
	encryptHeader((uint32 *)directoryBuffer, directoryBufferSize / 4);

	// Write the directory to the output file
	outStream.seek(0);
	outStream.write(directoryBuffer, directoryBufferSize);
}

ArchiveWriter::DirectoryEntry *ArchiveWriter::getEntry(const Common::String &room, uint32 index) {
	for (uint i = 0; i < _directory.size(); i++) {
		DirectoryEntry &entry = _directory[i];
		if (entry.index == index && entry.roomName == room) {
			return &entry;
		}
	}

	return nullptr;
}

void ArchiveWriter::writeDirectory(Common::SeekableWriteStream &outStream) {
	for (uint entryIndex = 0; entryIndex < _directory.size(); entryIndex++) {
		DirectoryEntry &entry = _directory[entryIndex];
		if (_room.empty()) {
			outStream.writeString(entry.roomName);
		}

		writeUint24(outStream, entry.index);
		outStream.writeByte(entry.subentries.size());

		for (uint subentryIndex = 0; subentryIndex < entry.subentries.size(); subentryIndex++) {
			const DirectorySubEntry &directorySubEntry = entry.subentries[subentryIndex];

			outStream.writeUint32LE(directorySubEntry.offset);
			outStream.writeUint32LE(directorySubEntry.size);
			outStream.writeUint16LE(directorySubEntry.metadata.size());
			outStream.writeByte(directorySubEntry.face);
			outStream.writeByte(directorySubEntry.type);

			for (uint i = 0; i < directorySubEntry.metadata.size(); i++) {
				outStream.writeUint32LE(directorySubEntry.metadata[i]);
			}
		}
	}
}

void ArchiveWriter::writeFiles(Common::SeekableWriteStream &outStream) {
	for (uint entryIndex = 0; entryIndex < _directory.size(); entryIndex++) {
		DirectoryEntry &entry = _directory[entryIndex];
		for (uint subentryIndex = 0; subentryIndex < entry.subentries.size(); subentryIndex++) {
			DirectorySubEntry &directorySubEntry = entry.subentries[subentryIndex];
			if (directorySubEntry.filename.empty()) continue;

			Common::FSNode fileToInclude = Common::FSNode(directorySubEntry.filename);
			Common::SeekableReadStream *readStream = fileToInclude.createReadStream();
			if (!readStream) {
				error("Unable to open file '%s'", directorySubEntry.filename.c_str());
			}

			directorySubEntry.offset = outStream.pos();

			if (directorySubEntry.compress) {
				uint uncompressedSize = readStream->size();
				byte *uncompressed = new byte[uncompressedSize];
				readStream->read(uncompressed, uncompressedSize);

				uint compressBufferSize = lzoCompressWorstSize(uncompressedSize);
				byte *compressed = new byte[compressBufferSize];

				size_t compressedSize = 0;
				LzoResult compressResult = lzoCompress(uncompressed, uncompressedSize, compressed, compressBufferSize, compressedSize);
				if (compressResult != LzoResult::Success) {
					error("Unable to LZO compress '%s'", directorySubEntry.filename.c_str());
				}

				delete[] uncompressed;

				outStream.writeUint32LE(kLZO1X);
				outStream.writeUint32LE(uncompressedSize);
				uint32 written = outStream.write(compressed, compressedSize);
				assert(written == (uint32)compressedSize);

				delete[] compressed;
			} else {
				uint32 written = outStream.writeStream(readStream);
				assert(written == (uint32)readStream->size());
			}

			directorySubEntry.size = outStream.pos() - directorySubEntry.offset;

			delete readStream;
		}
	}
}

void ArchiveWriter::encryptHeader(uint32 *header, uint32 length) {
	static const uint32 addKey  = 0x3C6EF35F;
	static const uint32 multKey = 0x0019660D;

	uint32 checksum   = 0;
	uint32 currentKey = 0;
	for (uint i = 0; i < length - 1; i++) {
		checksum   += header[i];

		currentKey *= multKey;
		currentKey += addKey;

		header[i]  ^= currentKey;
	}

	header[length - 1] = checksum ^ currentKey;
}

} // End of namespace Myst3

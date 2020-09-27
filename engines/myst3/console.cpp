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

#include "engines/myst3/console.h"

#include "engines/myst3/archive.h"
#include "engines/myst3/database.h"
#include "engines/myst3/effects.h"
#include "engines/myst3/inventory.h"
#include "engines/myst3/resource_loader.h"
#include "engines/myst3/script.h"
#include "engines/myst3/state.h"

#include "common/file.h"
#include "common/md5.h"

namespace Myst3 {

Console::Console(Myst3Engine *vm) : GUI::Debugger(), _vm(vm) {
	registerCmd("infos",         WRAP_METHOD(Console, Cmd_Infos));
	registerCmd("lookAt",        WRAP_METHOD(Console, Cmd_LookAt));
	registerCmd("initScript",    WRAP_METHOD(Console, Cmd_InitScript));
	registerCmd("var",           WRAP_METHOD(Console, Cmd_Var));
	registerCmd("listNodes",     WRAP_METHOD(Console, Cmd_ListNodes));
	registerCmd("run",           WRAP_METHOD(Console, Cmd_Run));
	registerCmd("runOp",         WRAP_METHOD(Console, Cmd_RunOp));
	registerCmd("go",            WRAP_METHOD(Console, Cmd_Go));
	registerCmd("extract",       WRAP_METHOD(Console, Cmd_Extract));
	registerCmd("fillInventory", WRAP_METHOD(Console, Cmd_FillInventory));
	registerCmd("dumpArchive",   WRAP_METHOD(Console, Cmd_DumpArchive));
	registerCmd("modArchive",    WRAP_METHOD(Console, Cmd_ModArchive));
	registerCmd("dumpMasks",     WRAP_METHOD(Console, Cmd_DumpMasks));
}

Console::~Console() {
}

void Console::describeScript(const Common::Array<Opcode> &script) {
	for (uint j = 0; j < script.size(); j++) {
		debugPrintf("%s", _vm->_scriptEngine->describeOpcode(script[j]).c_str());
	}
}

bool Console::Cmd_Infos(int argc, const char **argv) {
	uint16 nodeId = _vm->_state->getLocationNode();
	uint32 roomId = _vm->_state->getLocationRoom();
	uint32 ageID = _vm->_state->getLocationAge();

	if (argc >= 2) {
		nodeId = atoi(argv[1]);
	}

	if (argc >= 3) {
		RoomKey roomKey = _vm->_db->getRoomKey(argv[2]);
		if (roomKey.roomID == 0 || roomKey.ageID == 0) {
			debugPrintf("Unknown room name %s\n", argv[2]);
			return true;
		}

		roomId = roomKey.roomID;
		ageID = roomKey.ageID;
	}

	NodePtr nodeData = _vm->_db->getNodeData(nodeId, roomId, ageID);

	if (!nodeData) {
		debugPrintf("No node with id %d\n", nodeId);
		return true;
	}

	Common::String roomName = _vm->_db->getRoomName(roomId, ageID);

	debugPrintf("node: %s %d    ", roomName.c_str(), nodeId);

	for (uint i = 0; i < nodeData->scripts.size(); i++) {
		debugPrintf("\ninit %d > %s (%s)\n", i,
				_vm->_state->describeCondition(nodeData->scripts[i].condition).c_str(),
				_vm->_state->evaluate(nodeData->scripts[i].condition) ? "true" : "false");

		describeScript(nodeData->scripts[i].script);
	}

	for (uint i = 0; i < nodeData->hotspots.size(); i++) {
		debugPrintf("\nhotspot %d > %s (%s)\n", i,
				_vm->_state->describeCondition(nodeData->hotspots[i].condition).c_str(),
				_vm->_state->evaluate(nodeData->hotspots[i].condition) ? "true" : "false");

		for(uint j = 0; j < nodeData->hotspots[i].rects.size(); j++) {
			PolarRect &rect = nodeData->hotspots[i].rects[j];

			debugPrintf("    rect > pitch: %d heading: %d width: %d height: %d\n",
					rect.centerPitch, rect.centerHeading, rect.width, rect.height);
		}

		describeScript(nodeData->hotspots[i].script);
	}

	for (uint i = 0; i < nodeData->soundScripts.size(); i++) {
		debugPrintf("\nsound %d > %s (%s)\n", i,
				_vm->_state->describeCondition(nodeData->soundScripts[i].condition).c_str(),
				_vm->_state->evaluate(nodeData->soundScripts[i].condition) ? "true" : "false");

		describeScript(nodeData->soundScripts[i].script);
	}

	for (uint i = 0; i < nodeData->backgroundSoundScripts.size(); i++) {
		debugPrintf("\nbackground sound %d > %s (%s)\n", i,
				_vm->_state->describeCondition(nodeData->backgroundSoundScripts[i].condition).c_str(),
				_vm->_state->evaluate(nodeData->backgroundSoundScripts[i].condition) ? "true" : "false");

		describeScript(nodeData->backgroundSoundScripts[i].script);
	}

	return true;
}

bool Console::Cmd_LookAt(int argc, const char **argv) {
	if (argc != 1 && argc != 3) {
		debugPrintf("Usage :\n");
		debugPrintf("lookAt pitch heading\n");
		return true;
	}

	float pitch = _vm->_state->getLookAtPitch();
	float heading = _vm->_state->getLookAtHeading();

	debugPrintf("pitch: %d heading: %d\n", (int)pitch, (int)heading);

	if (argc >= 3){
		_vm->_state->lookAt(atof(argv[1]), atof(argv[2]));
		return false;
	}

	return true;
}

bool Console::Cmd_InitScript(int argc, const char **argv) {
	describeScript(_vm->_db->getNodeInitScript());

	return true;
}


bool Console::Cmd_Var(int argc, const char **argv) {

	if (argc != 2 && argc != 3) {
		debugPrintf("Usage :\n");
		debugPrintf("var variable : Display var value\n");
		debugPrintf("var variable value : Change var value\n");
		return true;
	}

	uint16 var = atoi(argv[1]);
	if (var < 1 || var > 2047) {
		debugPrintf("Variable out of range %d\n", var);
		return true;
	}	
	uint32 value = _vm->_state->getVar(var);

	if (argc == 3) {
		value = atoi(argv[2]);
		_vm->_state->setVar(var, value);
	}

	debugPrintf("%s: %d\n", _vm->_state->describeVar(var).c_str(), value);

	return true;
}

bool Console::Cmd_ListNodes(int argc, const char **argv) {
	uint32 roomID = _vm->_state->getLocationRoom();
	uint32 ageID = _vm->_state->getLocationAge();

	if (argc == 2) {
		RoomKey roomKey = _vm->_db->getRoomKey(argv[1]);
		if (roomKey.roomID == 0 || roomKey.ageID == 0) {
			debugPrintf("Unknown room name %s\n", argv[1]);
			return true;
		}

		roomID = roomKey.roomID;
		ageID = roomKey.ageID;
	}

	debugPrintf("Nodes:\n");

	Common::Array<uint16> list = _vm->_db->listRoomNodes(roomID, ageID);
	for (uint i = 0; i < list.size(); i++) {
		debugPrintf("%d\n", list[i]);
	}

	return true;
}

bool Console::Cmd_Run(int argc, const char **argv) {
	uint16 nodeId = _vm->_state->getLocationNode();
	uint32 roomId = _vm->_state->getLocationRoom();
	uint32 ageId = _vm->_state->getLocationAge();

	if (argc >= 2) {
		nodeId = atoi(argv[1]);
	}

	if (argc >= 3) {
		RoomKey roomKey = _vm->_db->getRoomKey(argv[2]);

		if (roomKey.roomID == 0 || roomKey.ageID == 0) {
			debugPrintf("Unknown room name %s\n", argv[2]);
			return true;
		}

		roomId = roomKey.roomID;
		ageId = roomKey.ageID;
	}

	_vm->runScriptsFromNode(nodeId, roomId, ageId);

	return false;
}

bool Console::Cmd_RunOp(int argc, const char **argv) {
	if (argc < 2) {
		debugPrintf("Usage :\n");
		debugPrintf("runOp [opcode] [argument 1] [argument 2] ... : Run specified command\n");
		return true;
	}

	Opcode op;
	op.op = atoi(argv[1]);

	for (int i = 2; i < argc; i++) {
		op.args.push_back(atoi(argv[i]));
	}

	debugPrintf("Running opcode :\n");
	debugPrintf("%s\n", _vm->_scriptEngine->describeOpcode(op).c_str());

	_vm->_scriptEngine->runSingleOp(op);

	return false;
}

bool Console::Cmd_Go(int argc, const char **argv) {
	if (argc != 3) {
		debugPrintf("Usage :\n");
		debugPrintf("go [room name] [node id] : Go to node\n");
		return true;
	}

	RoomKey roomKey = _vm->_db->getRoomKey(argv[1]);
	if (roomKey.roomID == 0 || roomKey.ageID == 0) {
		debugPrintf("Unknown room name %s\n", argv[1]);
		return true;
	}

	uint16 nodeId = atoi(argv[2]);

	_vm->_state->setLocationNextAge(roomKey.ageID);
	_vm->_state->setLocationNextRoom(roomKey.roomID);
	_vm->_state->setLocationNextNode(nodeId);

	_vm->goToNode(0, kTransitionFade);

	return false;
}

bool Console::Cmd_Extract(int argc, const char **argv) {
	if (argc != 5) {
		debugPrintf("Extract a file from the game's archives\n");
		debugPrintf("Usage :\n");
		debugPrintf("extract [room] [node id] [face number] [object type]\n");
		return true;
	}

	// Room names are uppercase
	Common::String room = Common::String(argv[1]);
	room.toUppercase();

	uint16 id = atoi(argv[2]);
	uint16 face = atoi(argv[3]);
	Archive::ResourceType type = (Archive::ResourceType) atoi(argv[4]);

	ResourceDescription desc = _vm->_resourceLoader->getFileDescription(room, id, face, type);

	if (!desc.isValid()) {
		debugPrintf("File with room %s, id %d, face %d and type %d does not exist\n", room.c_str(), id, face, type);
		return true;
	}

	Common::SeekableReadStream *s = desc.createReadStream();
	Common::String filename = Common::String::format("node%s_%d_face%d.%d", room.c_str(), id, face, type);
	Common::DumpFile f;
	f.open(filename);

	uint8 *buf = new uint8[s->size()];

	s->read(buf, s->size());
	f.write(buf, s->size());

	delete[] buf;

	f.close();

	delete s;

	debugPrintf("File '%s' successfully written\n", filename.c_str());

	return true;
}

bool Console::Cmd_FillInventory(int argc, const char **argv) {
	_vm->_inventory->addAll();
	return false;
}

class DumpingArchiveVisitor : public ArchiveVisitor {
public:
	DumpingArchiveVisitor() :
			_archive(nullptr) {
	}

	void visitArchive(Archive &archive) override {
		_archive = &archive;
	}

	void visitDirectorySubEntry(Archive::DirectoryEntry &directoryEntry, Archive::DirectorySubEntry &directorySubEntry) override {
		Common::String fileName = ResourceLoader::computeExtractedFileName(directoryEntry, directorySubEntry);
		if (fileName.empty()) return;

		debug("Extracted %s", fileName.c_str());

		Common::DumpFile outFile;
		if (!outFile.open(fileName, true))
			error("Unable to open file '%s' for writing", fileName.c_str());

		Common::SeekableReadStream *memoryStream = _archive->dumpToMemory(directorySubEntry.offset, directorySubEntry.size);
		outFile.writeStream(memoryStream);
		delete memoryStream;

		outFile.close();
	}

private:
	Archive *_archive;
};

bool Console::Cmd_DumpArchive(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("Extract all the files from a game archive\n");
		debugPrintf("The destination folder, named 'dump', is in the location ResidualVM was launched from\n");
		debugPrintf("Usage :\n");
		debugPrintf("dumpArchive [file name]\n");
		return true;
	}

	// Is the archive multi-room
	Common::String temp = Common::String(argv[1]);
	temp.toUppercase();

	bool multiRoom = !temp.hasSuffix(".M3A");
	if (!multiRoom) {
		temp = Common::String(argv[1], 4);
		temp.toUppercase();
	}

	Archive *archive = Archive::createFromFile(argv[1], multiRoom ? "" : temp);
	if (!archive) {
		debugPrintf("Can't open archive with name '%s'\n", argv[1]);
		return true;
	}

	DumpingArchiveVisitor dumper;
	archive->visit(dumper);

	delete archive;

	return true;
}

class ModdingArchiveVisitor : public ArchiveVisitor {
public:
	ModdingArchiveVisitor(ArchiveWriter &archiveWriter, bool compress, GUI::Debugger &debugger) :
			_archive(nullptr),
			_archiveWriter(archiveWriter),
			_compress(compress),
			_debugger(debugger) {
	}

	void visitArchive(Archive &archive) override {
		_archive = &archive;
	}

	void visitDirectorySubEntry(Archive::DirectoryEntry &directoryEntry, Archive::DirectorySubEntry &directorySubEntry) override {
		Archive::ResourceType moddedType = directorySubEntry.type;
		bool compress = false;
		switch (directorySubEntry.type) {
		case Archive::kCubeFace:
			moddedType = Archive::kModdedCubeFace;
			compress = true;
			break;
		case Archive::kSpotItem:
		case Archive::kLocalizedSpotItem:
			moddedType = Archive::kModdedSpotItem;
			compress = true;
			break;
		case Archive::kFrame:
		case Archive::kLocalizedFrame:
			moddedType = Archive::kModdedFrame;
			compress = true;
			break;
		case Archive::kRawData:
			moddedType = Archive::kModdedRawData;
			compress = true;
			break;
		case Archive::kMovie:
		case Archive::kStillMovie:
		case Archive::kDialogMovie:
		case Archive::kMultitrackMovie:
			moddedType = Archive::kModdedMovie;
			break;
		default:
			break;
		}

		Archive::DirectorySubEntry moddedDirectorySubEntry = directorySubEntry;
		moddedDirectorySubEntry.type = moddedType;

		Common::String fileName = ResourceLoader::computeExtractedFileName(directoryEntry, moddedDirectorySubEntry);
		if (fileName.empty()) return;

		Common::FSNode extractedFile = Common::FSNode(fileName);
		if (!extractedFile.exists()) return;

		// Checksum the original and modded files
		Common::SeekableReadStream *originalStream = _archive->dumpToMemory(directorySubEntry.offset, directorySubEntry.size);
		Common::String originalMd5 = Common::computeStreamMD5AsString(*originalStream);
		delete originalStream;

		Common::SeekableReadStream *moddedStream = extractedFile.createReadStream();
		Common::String moddedMd5 = Common::computeStreamMD5AsString(*moddedStream);
		delete moddedStream;

		// Ignore files that have not changed
		if (moddedMd5 == originalMd5) return;

		MetadataArray moddedMetadata = directorySubEntry.metadata;

		// We need to store the original video size to be able
		//  to compute a scaling ratio when rendering.
		if (directorySubEntry.type == Archive::kStillMovie
		    || directorySubEntry.type == Archive::kDialogMovie) {
			assert(moddedMetadata.empty());

			Common::SeekableReadStream *binkStream = _archive->dumpToMemory(directorySubEntry.offset, directorySubEntry.size);
			Video::BinkDecoder bink;
			bink.loadStream(binkStream);

			moddedMetadata.resize(10);
			moddedMetadata[8] = bink.getWidth();
			moddedMetadata[9] = bink.getHeight();
		}

		_debugger.debugPrintf("Adding '%s' to the modded archive (md5sum %s)\n", fileName.c_str(), moddedMd5.c_str());

		_archiveWriter.addFile(
		            directoryEntry.roomName,
		            directoryEntry.index,
		            directorySubEntry.face,
		            moddedType,
		            moddedMetadata,
		            fileName,
		            _compress && compress
		);
	}

private:
	Archive *_archive;
	ArchiveWriter &_archiveWriter;
	bool _compress;
	GUI::Debugger &_debugger;
};

bool Console::Cmd_ModArchive(int argc, const char **argv) {
	if (argc != 2 && argc != 3) {
		debugPrintf("Build a new game archive from a folder of modded files.\n");
		debugPrintf("The source folder, must be named 'dump', and be located in the location ResidualVM was launched from\n");
		debugPrintf("Usage :\n");
		debugPrintf("modArchive [file name] [compress]\n");
		return true;
	}

	// Is the archive multi-room
	Common::String temp = Common::String(argv[1]);
	if (temp.size() < 4) {
		debugPrintf("Invalid file name '%s'\n", argv[1]);
		return true;
	}

	Common::String room;
	if (temp.hasSuffixIgnoreCase(".m3a")) {
		room = Common::String(argv[1], 4);
		room.toUppercase();
	}

	bool compress = true;
	if (argc >= 3) {
		if (!Common::parseBool(argv[2], compress)) {
			debugPrintf("Invalid boolean value '%s'\n", argv[2]);
			return true;
		}
	}

	Archive *archive = Archive::createFromFile(argv[1], room);
	if (!archive) {
		debugPrintf("Can't open archive with name '%s'\n", argv[1]);
		return true;
	}

	ArchiveWriter archiveWriter(room);
	ModdingArchiveVisitor moddingVisitor(archiveWriter, compress, *this);
	archive->visit(moddingVisitor);
	delete archive;

	if (archiveWriter.empty()) {
		debugPrintf("No modded files were found to put in the archive\n");
		return true;
	}

	Common::String outFileName = Common::String::format("%s.patch", argv[1]);

	Common::DumpFile outFile;
	if (!outFile.open(outFileName, true))
		error("Unable to open file '%s' for writing", outFileName.c_str());

	archiveWriter.write(outFile);

	debugPrintf("The mod archive '%s' has been written\n", outFileName.c_str());

	return true;
}

bool Console::Cmd_DumpMasks(int argc, const char **argv) {
	if (argc != 1 && argc != 2) {
		debugPrintf("Extract the masks of the faces of a cube node.\n");
		debugPrintf("The destination folder, named 'dump', must exist.\n");
		debugPrintf("Usage :\n");
		debugPrintf("dumpMasks [node]\n");
		return true;
	}

	uint16 nodeId = _vm->_state->getLocationNode();
	uint32 roomId = _vm->_state->getLocationRoom();
	uint32 ageID  = _vm->_state->getLocationAge();

	if (argc >= 2) {
		nodeId = atoi(argv[1]);
	}

	Common::String roomName = _vm->_db->getRoomName(roomId, ageID);

	debugPrintf("Extracting masks for node %s %d:\n", roomName.c_str(), nodeId);

	for (uint i = 0; i < 6; i++) {
		bool water = dumpFaceMask(roomName, nodeId, i, Archive::kWaterEffectMask);
		if (water)
			debugPrintf("Face %d: water OK\n", i);

		bool lava = dumpFaceMask(roomName, nodeId, i, Archive::kLavaEffectMask);
		if (lava)
			debugPrintf("Face %d: lava OK\n", i);

		bool magnet = dumpFaceMask(roomName, nodeId, i, Archive::kMagneticEffectMask);
		if (magnet)
			debugPrintf("Face %d: magnet OK\n", i);

		if (!water && !lava && !magnet)
			debugPrintf("Face %d: No mask found\n", i);
	}

	return true;
}

bool Console::dumpFaceMask(const Common::String &room, uint16 index, int face, Archive::ResourceType type) {
	ResourceDescription maskDesc = _vm->_resourceLoader->getFileDescription(room, index, face, type);

	if (!maskDesc.isValid())
		return false;

	Common::SeekableReadStream *maskStream = maskDesc.createReadStream();

	Effect::FaceMask *mask = Effect::loadMask(maskStream);

	delete maskStream;

	Common::DumpFile outFile;
	outFile.open(Common::String::format("dump/%s-%d-%d.masku_%d", room.c_str(), index, face, type), true);
	outFile.write(mask->surface->getPixels(), mask->surface->pitch * mask->surface->h);
	outFile.close();

	delete mask;

	return true;
}

} // End of namespace Myst3

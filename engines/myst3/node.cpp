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

#include "engines/myst3/effects.h"
#include "engines/myst3/node.h"
#include "engines/myst3/resource_loader.h"
#include "engines/myst3/myst3.h"

#include "common/debug.h"
#include "common/rect.h"

namespace Myst3 {

Node::Node(const Common::String &room, uint16 id, Node::Type type) :
		_room(room),
		_id(id),
		_type(type) {
}

void Node::addEffect(Effect *effect) {
	if (effect) {
		_effects.push_back(effect);
	}
}

Node::~Node() {
	for (uint i = 0; i < _effects.size(); i++) {
		delete _effects[i];
	}
	_effects.clear();
}

} // end of namespace Myst3

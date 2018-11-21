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

#include "engines/stark/scene.h"

#include "engines/stark/gfx/driver.h"
#include "engines/stark/gfx/renderentry.h"

#include "engines/stark/services/services.h"
#include "engines/stark/services/global.h"
#include "engines/stark/resources/floor.h"
#include "engines/stark/resources/floorface.h"
#include "engines/stark/resources/item.h"

#include "common/system.h"

#include "math/glmath.h"

#include "graphics/opengl/shader.h" // HACK: I just want to see something

namespace Stark {

Scene::Scene(Gfx::Driver *gfx) :
		_gfx(gfx),
		_fov(45.0),
		_nearClipPlane(100.0),
		_farClipPlane(64000.0),
		_fadeLevel(1.0),
		_floatOffset(0.0),
		_maxShadowLength(0.075f) {
	static const char* attributes[] = { "pos", "face", "enabled", nullptr };
	_shader = OpenGL::Shader::fromFiles("stark_floor", attributes);
}

Scene::~Scene() {
	delete _shader;
}

void Scene::initCamera(const Math::Vector3d &position, const Math::Vector3d &lookDirection,
		float fov, Common::Rect viewSize, float nearClipPlane, float farClipPlane) {
	_cameraPosition = position;
	_cameraLookDirection = lookDirection;
	_fov = fov;
	_viewSize = viewSize;
	_nearClipPlane = nearClipPlane;
	_farClipPlane = farClipPlane;

	_viewMatrix = Math::makeLookAtMatrix(_cameraPosition, _cameraPosition + _cameraLookDirection, Math::Vector3d(0.0, 0.0, 1.0));
	_viewMatrix.transpose(); // Math::makeLookAtMatrix outputs transposed matrices ...
	_viewMatrix.translate(-_cameraPosition);

	setSwayAngle(0);
	setFadeLevel(1.0);
	setFloatOffset(0);
}

void Scene::scrollCamera(const Common::Rect &viewport) {
	_viewport = viewport;

	float xmin, xmax, ymin, ymax;
	computeClippingRect(&xmin, &xmax, &ymin, &ymax);

	// The amounts by which translate to clipping planes to account for one pixel
	// of camera scrolling movement
	float scollXFactor = (xmax - xmin) / _viewport.width();
	float scollYFactor = (ymax - ymin) / _viewport.height();

	int32 distanceToRight = _viewport.right - _viewSize.width();
	int32 distanceToTop = -_viewport.top;

	xmin += distanceToRight * scollXFactor;
	xmax += distanceToRight * scollXFactor;
	ymin += distanceToTop * scollYFactor;
	ymax += distanceToTop * scollYFactor;

	_projectionMatrix = Math::makeFrustumMatrix(xmin, xmax, ymin, ymax, _nearClipPlane, _farClipPlane);
	_projectionMatrix.transpose(); // Math::makeFrustumMatrix outputs transposed matrices ...
}

void Scene::computeClippingRect(float *xmin, float *xmax, float *ymin, float *ymax) {
	float aspectRatio = _viewSize.width() / (float) _viewSize.height();
	float xmaxValue = _nearClipPlane * tan(_fov * M_PI / 360.0);
	float ymaxValue = xmaxValue / aspectRatio;

	float xminValue = xmaxValue - 2 * xmaxValue * (_viewport.width() / (float) _viewSize.width());
	float yminValue = ymaxValue - 2 * ymaxValue * (_viewport.height() / (float) _viewSize.height());

	if (xmin) *xmin = xminValue;
	if (xmax) *xmax = xmaxValue;
	if (ymin) *ymin = yminValue;
	if (ymax) *ymax = ymaxValue;
}

Math::Ray Scene::makeRayFromMouse(const Common::Point &mouse) const {
	Common::Rect gameViewport = _gfx->gameViewport();

	Math::Vector4d in;
	in.x() = (mouse.x - gameViewport.left) * 2 / (float) gameViewport.width() - 1.0;
	in.y() = (gameViewport.bottom - mouse.y) * 2 / (float) gameViewport.height() - 1.0;
	in.z() = 1.0;
	in.w() = 1.0;

	Math::Matrix4 view = _viewMatrix;
	view.translate(_cameraPosition);

	Math::Matrix4 A = _projectionMatrix * view;
	A.inverse();

	Math::Vector4d out = A * in;

	Math::Vector3d origin = _cameraPosition;
	Math::Vector3d direction = Math::Vector3d(out.x(), out.y(), out.z());
	direction.normalize();

	return Math::Ray(origin, direction);
}

Common::Point Scene::convertPosition3DToGameScreenOriginal(const Math::Vector3d &obj) const {
	Math::Vector4d in;
	in.set(obj.x(), obj.y(), obj.z(), 1.0);

	Math::Vector4d out = _projectionMatrix * _viewMatrix * in;

	out.x() /= out.w();
	out.y() /= out.w();

	Common::Point point;
	point.x = (1 + out.x()) * Gfx::Driver::kGameViewportWidth / 2;
	point.y = -Gfx::Driver::kTopBorderHeight + Gfx::Driver::kOriginalHeight - (1 + out.y()) * Gfx::Driver::kGameViewportHeight / 2;

	return point;
}

void Scene::setFadeLevel(float fadeLevel) {
	_fadeLevel = fadeLevel;
}

float Scene::getFadeLevel() const {
	return _fadeLevel;
}

void Scene::setSwayAngle(const Math::Angle &angle) {
	_swayAngle = angle;
}

Math::Angle Scene::getSwayAngle() const {
	return _swayAngle;
}

Math::Vector3d Scene::getSwayDirection() const {
	// Actor sway is always along the camera direction, so that
	// the rotation is not affected by the direction they are facing.
	return _cameraLookDirection;
}

void Scene::setFloatOffset(float floatOffset) {
	_floatOffset = floatOffset;
}

float Scene::getFloatOffset() const {
	return _floatOffset;
}

void Scene::drawFloor() {
	struct Attributes {
		Math::Vector3d pos;
		float face;
		float enabled;
	};

	Resources::Floor *floor = StarkGlobal->getCurrent()->getFloor();
	if (!floor) return;

	Common::Array<Resources::FloorFace *> faces = floor->listChildren<Resources::FloorFace>();

	Attributes *attributes = new Attributes[faces.size() * 3];
	for (uint i = 0; i < faces.size(); i++) {
		if (!faces[i]->hasVertices()) continue;

		bool enabled = faces[i]->isEnabled();

		for (uint j = 0; j < 3; j++) {
			attributes[3 * i + j].pos = floor->getVertex(faces[i]->getVertexIndex(j));
			attributes[3 * i + j].face = i;
			attributes[3 * i + j].enabled = enabled;
		}
	}

	uint32 vbo = OpenGL::Shader::createBuffer(GL_ARRAY_BUFFER, sizeof(float) * 5 * 3 * faces.size(), &attributes[0]);
	delete[] attributes;

	Math::Matrix4 mvp = _projectionMatrix * _viewMatrix;
	mvp.transpose();

	int32 aprilFace = -1;

	Resources::GlobalItemTemplate *april = StarkGlobal->getApril();
	if (april) {
		Resources::ModelItem *a2 = Resources::Object::cast<Resources::ModelItem>(april->getSceneInstance());
		aprilFace = a2->getFloorFaceIndex();
	}

	_shader->enableVertexAttribute("pos", vbo, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
	_shader->enableVertexAttribute("face", vbo, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 3 * sizeof(float));
	_shader->enableVertexAttribute("enabled", vbo, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 4 * sizeof(float));
	_shader->use(true);
	_shader->setUniform("mvp", mvp);
	_shader->setUniform("aprilFace", aprilFace);
	_shader->setUniform("color", Math::Vector3d(1.0, 0, 0));
	_shader->setUniform("aprilColor", Math::Vector3d(0, 1.0, 0));
	_shader->setUniform("enabledColor", Math::Vector3d(0, 0, 1.0));

	glBindTexture(GL_TEXTURE_2D, 0);
	glDrawArrays(GL_TRIANGLES, 0, 3 * faces.size());

	glUseProgram(0);

	OpenGL::Shader::freeBuffer(vbo);
}

} // End of namespace Stark

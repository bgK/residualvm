MODULE := engines/myst3

MODULE_OBJS := \
	ambient.o \
	archive.o \
	console.o \
	cursor.o \
	database.o \
	detection.o \
	effects.o \
	gfx.o \
	gfx_opengl.o \
	gfx_opengl_shaders.o \
	gfx_opengl_texture.o \
	gfx_tinygl.o \
	gfx_tinygl_texture.o \
	hotspot.o \
	inventory.o \
	lzo.o \
	menu.o \
	movie.o \
	myst3.o \
	node.o \
	node_software.o \
	puzzles.o \
	resource_loader.o \
	scene.o \
	script.o \
	sound.o \
	state.o \
	subtitles.o \
	transition.o

# This module can be built as a plugin
ifeq ($(ENABLE_MYST3), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

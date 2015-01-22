# Makefile to build and install the SDL library

top_builddir = .
srcdir  = /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend
objects = build
prefix = /usr/local
exec_prefix = ${prefix}
bindir	= ${exec_prefix}/bin
libdir  = ${exec_prefix}/lib
includedir = ${prefix}/include
datarootdir = ${prefix}/share
datadir	= ${datarootdir}
auxdir	= build-scripts
distpath = $(srcdir)/..
distdir = SDL2-2.0.3
distfile = $(distdir).tar.gz


SHELL	= /bin/bash
CC      = gcc
INCLUDE = -Iinclude -I/home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/include
CFLAGS  = -fPIC  -DUSING_GENERATED_CONFIG_H
EXTRA_CFLAGS = -Iinclude -I/home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/include  -mmmx -m3dnow -msse -msse2 -fvisibility=hidden  -D_REENTRANT -DHAVE_LINUX_VERSION_H -Wall
LDFLAGS = 
EXTRA_LDFLAGS =  -Wl,--no-undefined -lm -ldl -lpthread -lrt
LIBTOOL = $(SHELL) $(top_builddir)/libtool
INSTALL = /usr/bin/install -c
AR	= ar
RANLIB	= ranlib
WINDRES	= :

TARGET  = libSDL2.la
OBJECTS = $(objects)/SDL.lo $(objects)/SDL_assert.lo $(objects)/SDL_error.lo $(objects)/SDL_hints.lo $(objects)/SDL_log.lo $(objects)/SDL_atomic.lo $(objects)/SDL_spinlock.lo $(objects)/SDL_audio.lo $(objects)/SDL_audiocvt.lo $(objects)/SDL_audiodev.lo $(objects)/SDL_audiotypecvt.lo $(objects)/SDL_mixer.lo $(objects)/SDL_wave.lo $(objects)/SDL_cpuinfo.lo $(objects)/SDL_dynapi.lo $(objects)/SDL_clipboardevents.lo $(objects)/SDL_dropevents.lo $(objects)/SDL_events.lo $(objects)/SDL_gesture.lo $(objects)/SDL_keyboard.lo $(objects)/SDL_mouse.lo $(objects)/SDL_quit.lo $(objects)/SDL_touch.lo $(objects)/SDL_windowevents.lo $(objects)/SDL_rwops.lo $(objects)/SDL_haptic.lo $(objects)/SDL_gamecontroller.lo $(objects)/SDL_joystick.lo $(objects)/e_atan2.lo $(objects)/e_log.lo $(objects)/e_pow.lo $(objects)/e_rem_pio2.lo $(objects)/e_sqrt.lo $(objects)/k_cos.lo $(objects)/k_rem_pio2.lo $(objects)/k_sin.lo $(objects)/s_atan.lo $(objects)/s_copysign.lo $(objects)/s_cos.lo $(objects)/s_fabs.lo $(objects)/s_floor.lo $(objects)/s_scalbn.lo $(objects)/s_sin.lo $(objects)/SDL_power.lo $(objects)/SDL_d3dmath.lo $(objects)/SDL_render.lo $(objects)/SDL_yuv_mmx.lo $(objects)/SDL_yuv_sw.lo $(objects)/SDL_render_d3d.lo $(objects)/SDL_render_d3d11.lo $(objects)/SDL_render_gl.lo $(objects)/SDL_shaders_gl.lo $(objects)/SDL_render_gles.lo $(objects)/SDL_render_gles2.lo $(objects)/SDL_shaders_gles2.lo $(objects)/SDL_render_psp.lo $(objects)/SDL_blendfillrect.lo $(objects)/SDL_blendline.lo $(objects)/SDL_blendpoint.lo $(objects)/SDL_drawline.lo $(objects)/SDL_drawpoint.lo $(objects)/SDL_render_sw.lo $(objects)/SDL_rotate.lo $(objects)/SDL_getenv.lo $(objects)/SDL_iconv.lo $(objects)/SDL_malloc.lo $(objects)/SDL_qsort.lo $(objects)/SDL_stdlib.lo $(objects)/SDL_string.lo $(objects)/SDL_thread.lo $(objects)/SDL_timer.lo $(objects)/SDL_RLEaccel.lo $(objects)/SDL_blit.lo $(objects)/SDL_blit_0.lo $(objects)/SDL_blit_1.lo $(objects)/SDL_blit_A.lo $(objects)/SDL_blit_N.lo $(objects)/SDL_blit_auto.lo $(objects)/SDL_blit_copy.lo $(objects)/SDL_blit_slow.lo $(objects)/SDL_bmp.lo $(objects)/SDL_clipboard.lo $(objects)/SDL_egl.lo $(objects)/SDL_fillrect.lo $(objects)/SDL_pixels.lo $(objects)/SDL_rect.lo $(objects)/SDL_shape.lo $(objects)/SDL_stretch.lo $(objects)/SDL_surface.lo $(objects)/SDL_video.lo $(objects)/SDL_nullevents.lo $(objects)/SDL_nullframebuffer.lo $(objects)/SDL_nullvideo.lo $(objects)/SDL_diskaudio.lo $(objects)/SDL_dummyaudio.lo $(objects)/SDL_sysloadso.lo $(objects)/SDL_dspaudio.lo $(objects)/SDL_x11clipboard.lo $(objects)/SDL_x11dyn.lo $(objects)/SDL_x11events.lo $(objects)/SDL_x11framebuffer.lo $(objects)/SDL_x11keyboard.lo $(objects)/SDL_x11messagebox.lo $(objects)/SDL_x11modes.lo $(objects)/SDL_x11mouse.lo $(objects)/SDL_x11opengl.lo $(objects)/SDL_x11opengles.lo $(objects)/SDL_x11shape.lo $(objects)/SDL_x11touch.lo $(objects)/SDL_x11video.lo $(objects)/SDL_x11window.lo $(objects)/SDL_x11xinput2.lo $(objects)/edid-parse.lo $(objects)/imKStoUCS.lo $(objects)/SDL_systhread.lo $(objects)/SDL_syssem.lo $(objects)/SDL_sysmutex.lo $(objects)/SDL_syscond.lo $(objects)/SDL_systls.lo $(objects)/SDL_sysjoystick.lo $(objects)/SDL_syshaptic.lo $(objects)/SDL_syspower.lo $(objects)/SDL_sysfilesystem.lo $(objects)/SDL_systimer.lo $(objects)/SDL_evdev.lo
VERSION_OBJECTS = 

SDLMAIN_TARGET = libSDL2main.a
SDLMAIN_OBJECTS = $(objects)/SDL_dummy_main.o

SDLTEST_TARGET = libSDL2_test.a
SDLTEST_OBJECTS = $(objects)/SDL_test_assert.o $(objects)/SDL_test_common.o $(objects)/SDL_test_compare.o $(objects)/SDL_test_crc32.o $(objects)/SDL_test_font.o $(objects)/SDL_test_fuzzer.o $(objects)/SDL_test_harness.o $(objects)/SDL_test_imageBlit.o $(objects)/SDL_test_imageBlitBlend.o $(objects)/SDL_test_imageFace.o $(objects)/SDL_test_imagePrimitives.o $(objects)/SDL_test_imagePrimitivesBlend.o $(objects)/SDL_test_log.o $(objects)/SDL_test_md5.o $(objects)/SDL_test_random.o

SRC_DIST = *.txt acinclude Android.mk autogen.sh android-project build-scripts cmake configure configure.in debian include Makefile.* sdl2-config.in sdl2.m4 sdl2.pc.in SDL2.spec.in src test VisualC.html VisualC Xcode Xcode-iOS
GEN_DIST = SDL2.spec

HDRS = \
	SDL.h \
	SDL_assert.h \
	SDL_atomic.h \
	SDL_audio.h \
    SDL_bits.h \
	SDL_blendmode.h \
	SDL_clipboard.h \
	SDL_cpuinfo.h \
	SDL_endian.h \
	SDL_error.h \
	SDL_events.h \
	SDL_filesystem.h \
	SDL_gamecontroller.h \
	SDL_gesture.h \
	SDL_haptic.h \
	SDL_hints.h \
	SDL_joystick.h \
	SDL_keyboard.h \
	SDL_keycode.h \
	SDL_loadso.h \
	SDL_log.h \
	SDL_main.h \
	SDL_messagebox.h \
	SDL_mouse.h \
	SDL_mutex.h \
	SDL_name.h \
	SDL_opengl.h \
	SDL_opengles.h \
	SDL_opengles2.h \
	SDL_pixels.h \
	SDL_platform.h \
	SDL_power.h \
	SDL_quit.h \
	SDL_rect.h \
	SDL_render.h \
	SDL_rwops.h \
	SDL_scancode.h \
	SDL_shape.h \
	SDL_stdinc.h \
	SDL_surface.h \
	SDL_system.h \
	SDL_syswm.h \
	SDL_thread.h \
	SDL_timer.h \
	SDL_touch.h \
	SDL_types.h \
	SDL_version.h \
	SDL_video.h \
	begin_code.h \
	close_code.h

SDLTEST_HDRS = $(shell ls $(srcdir)/include | fgrep SDL_test)

LT_AGE      = 2
LT_CURRENT  = 2
LT_RELEASE  = 2.0
LT_REVISION = 1
LT_LDFLAGS  = -no-undefined -rpath $(DESTDIR)$(libdir) -release $(LT_RELEASE) -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

all: $(srcdir)/configure Makefile $(objects) $(objects)/$(TARGET) $(objects)/$(SDLMAIN_TARGET) $(objects)/$(SDLTEST_TARGET)

$(srcdir)/configure: $(srcdir)/configure.in
	@echo "Warning, configure.in is out of date"
	#(cd $(srcdir) && sh autogen.sh && sh configure)
	@sleep 3

Makefile: $(srcdir)/Makefile.in
	$(SHELL) config.status $@

Makefile.in:;

$(objects):
	$(SHELL) $(auxdir)/mkinstalldirs $@

update-revision:
	$(SHELL) $(auxdir)/updaterev.sh

.PHONY: all update-revision install install-bin install-hdrs install-lib install-data uninstall uninstall-bin uninstall-hdrs uninstall-lib uninstall-data clean distclean dist $(OBJECTS:.lo=.d)

$(objects)/$(TARGET): $(OBJECTS) $(VERSION_OBJECTS)
	$(LIBTOOL) --tag=CC --mode=link $(CC) -o $@ $(OBJECTS) $(VERSION_OBJECTS) $(LDFLAGS) $(EXTRA_LDFLAGS) $(LT_LDFLAGS)

$(objects)/$(SDLMAIN_TARGET): $(SDLMAIN_OBJECTS)
	$(AR) cru $@ $(SDLMAIN_OBJECTS)
	$(RANLIB) $@

$(objects)/$(SDLTEST_TARGET): $(SDLTEST_OBJECTS)
	$(AR) cru $@ $(SDLTEST_OBJECTS)
	$(RANLIB) $@

install: all install-bin install-hdrs install-lib install-data
install-bin:
	$(SHELL) $(auxdir)/mkinstalldirs $(DESTDIR)$(bindir)
	$(INSTALL) -m 755 sdl2-config $(DESTDIR)$(bindir)/sdl2-config
install-hdrs: update-revision
	$(SHELL) $(auxdir)/mkinstalldirs $(DESTDIR)$(includedir)/SDL2
	for file in $(HDRS) $(SDLTEST_HDRS); do \
	    $(INSTALL) -m 644 $(srcdir)/include/$$file $(DESTDIR)$(includedir)/SDL2/$$file; \
	done
	$(INSTALL) -m 644 include/SDL_config.h $(DESTDIR)$(includedir)/SDL2/SDL_config.h
	if test -f include/SDL_revision.h; then \
	    $(INSTALL) -m 644 include/SDL_revision.h $(DESTDIR)$(includedir)/SDL2/SDL_revision.h; \
	else \
	    $(INSTALL) -m 644 $(srcdir)/include/SDL_revision.h $(DESTDIR)$(includedir)/SDL2/SDL_revision.h; \
	fi

install-lib: $(objects) $(objects)/$(TARGET) $(objects)/$(SDLMAIN_TARGET) $(objects)/$(SDLTEST_TARGET)
	$(SHELL) $(auxdir)/mkinstalldirs $(DESTDIR)$(libdir)
	$(LIBTOOL) --mode=install $(INSTALL) $(objects)/$(TARGET) $(DESTDIR)$(libdir)/$(TARGET)
	$(INSTALL) -m 644 $(objects)/$(SDLMAIN_TARGET) $(DESTDIR)$(libdir)/$(SDLMAIN_TARGET)
	$(RANLIB) $(DESTDIR)$(libdir)/$(SDLMAIN_TARGET)
	$(INSTALL) -m 644 $(objects)/$(SDLTEST_TARGET) $(DESTDIR)$(libdir)/$(SDLTEST_TARGET)
	$(RANLIB) $(DESTDIR)$(libdir)/$(SDLTEST_TARGET)
install-data:
	$(SHELL) $(auxdir)/mkinstalldirs $(DESTDIR)$(datadir)/aclocal
	$(INSTALL) -m 644 $(srcdir)/sdl2.m4 $(DESTDIR)$(datadir)/aclocal/sdl2.m4
	$(SHELL) $(auxdir)/mkinstalldirs $(DESTDIR)$(libdir)/pkgconfig
	$(INSTALL) -m 644 sdl2.pc $(DESTDIR)$(libdir)/pkgconfig

uninstall: uninstall-bin uninstall-hdrs uninstall-lib uninstall-data
uninstall-bin:
	rm -f $(DESTDIR)$(bindir)/sdl2-config
uninstall-hdrs:
	for file in $(HDRS) $(SDLTEST_HDRS); do \
	    rm -f $(DESTDIR)$(includedir)/SDL2/$$file; \
	done
	rm -f $(DESTDIR)$(includedir)/SDL2/SDL_config.h
	rm -f $(DESTDIR)$(includedir)/SDL2/SDL_revision.h
	-rmdir $(DESTDIR)$(includedir)/SDL2
uninstall-lib:
	$(LIBTOOL) --mode=uninstall rm -f $(DESTDIR)$(libdir)/$(TARGET)
	rm -f $(DESTDIR)$(libdir)/$(SDLMAIN_TARGET)
	rm -f $(DESTDIR)$(libdir)/$(SDLTEST_TARGET)
uninstall-data:
	rm -f $(DESTDIR)$(datadir)/aclocal/sdl2.m4
	rm -f $(DESTDIR)$(libdir)/pkgconfig/sdl2.pc

clean:
	rm -rf $(objects)
	if test -f test/Makefile; then (cd test; $(MAKE) $@); fi

distclean: clean
	rm -f Makefile Makefile.rules sdl2-config
	rm -f config.status config.cache config.log libtool
	rm -rf $(srcdir)/autom4te*
	find $(srcdir) \( \
	    -name '*~' -o \
	    -name '*.bak' -o \
	    -name '*.old' -o \
	    -name '*.rej' -o \
	    -name '*.orig' -o \
	    -name '.#*' \) \
	    -exec rm -f {} \;
	if test -f test/Makefile; then (cd test; $(MAKE) $@); fi

dist $(distfile):
	$(SHELL) $(auxdir)/mkinstalldirs $(distdir)
	(cd $(srcdir); tar cf - $(SRC_DIST)) | (cd $(distdir); tar xf -)
	tar cf - $(GEN_DIST) | (cd $(distdir); tar xf -)
	find $(distdir) \( \
	    -name '*~' -o \
	    -name '*.bak' -o \
	    -name '*.old' -o \
	    -name '*.rej' -o \
	    -name '*.orig' -o \
	    -name '.#*' \) \
	    -exec rm -f {} \;
	if test -f $(distdir)/test/Makefile; then (cd $(distdir)/test && make distclean); fi
	(cd $(distdir); build-scripts/updaterev.sh)
	tar cvf - $(distdir) | gzip --best >$(distfile)
	rm -rf $(distdir)

rpm: $(distfile)
	rpmbuild -ta $?

# Build rules for objects
-include $(OBJECTS:.lo=.d)

# Special dependency for SDL.c, since it depends on SDL_revision.h
/home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/SDL.c: update-revision

$(objects)/SDL.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/SDL.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_assert.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/SDL_assert.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_error.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/SDL_error.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_hints.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/SDL_hints.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_log.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/SDL_log.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_atomic.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/atomic/SDL_atomic.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_spinlock.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/atomic/SDL_spinlock.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_audio.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/audio/SDL_audio.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_audiocvt.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/audio/SDL_audiocvt.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_audiodev.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/audio/SDL_audiodev.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_audiotypecvt.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/audio/SDL_audiotypecvt.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_mixer.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/audio/SDL_mixer.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_wave.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/audio/SDL_wave.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_cpuinfo.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/cpuinfo/SDL_cpuinfo.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_dynapi.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/dynapi/SDL_dynapi.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_clipboardevents.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/events/SDL_clipboardevents.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_dropevents.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/events/SDL_dropevents.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_events.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/events/SDL_events.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_gesture.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/events/SDL_gesture.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_keyboard.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/events/SDL_keyboard.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_mouse.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/events/SDL_mouse.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_quit.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/events/SDL_quit.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_touch.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/events/SDL_touch.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_windowevents.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/events/SDL_windowevents.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_rwops.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/file/SDL_rwops.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_haptic.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/haptic/SDL_haptic.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_gamecontroller.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/joystick/SDL_gamecontroller.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_joystick.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/joystick/SDL_joystick.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/e_atan2.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/libm/e_atan2.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/e_log.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/libm/e_log.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/e_pow.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/libm/e_pow.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/e_rem_pio2.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/libm/e_rem_pio2.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/e_sqrt.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/libm/e_sqrt.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/k_cos.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/libm/k_cos.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/k_rem_pio2.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/libm/k_rem_pio2.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/k_sin.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/libm/k_sin.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/s_atan.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/libm/s_atan.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/s_copysign.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/libm/s_copysign.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/s_cos.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/libm/s_cos.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/s_fabs.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/libm/s_fabs.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/s_floor.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/libm/s_floor.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/s_scalbn.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/libm/s_scalbn.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/s_sin.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/libm/s_sin.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_power.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/power/SDL_power.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_d3dmath.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/SDL_d3dmath.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_render.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/SDL_render.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_yuv_mmx.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/SDL_yuv_mmx.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_yuv_sw.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/SDL_yuv_sw.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_render_d3d.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/direct3d/SDL_render_d3d.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_render_d3d11.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/direct3d11/SDL_render_d3d11.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_render_gl.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/opengl/SDL_render_gl.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_shaders_gl.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/opengl/SDL_shaders_gl.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_render_gles.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/opengles/SDL_render_gles.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_render_gles2.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/opengles2/SDL_render_gles2.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_shaders_gles2.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/opengles2/SDL_shaders_gles2.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_render_psp.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/psp/SDL_render_psp.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_blendfillrect.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/software/SDL_blendfillrect.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_blendline.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/software/SDL_blendline.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_blendpoint.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/software/SDL_blendpoint.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_drawline.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/software/SDL_drawline.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_drawpoint.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/software/SDL_drawpoint.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_render_sw.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/software/SDL_render_sw.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_rotate.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/render/software/SDL_rotate.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_getenv.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/stdlib/SDL_getenv.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_iconv.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/stdlib/SDL_iconv.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_malloc.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/stdlib/SDL_malloc.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_qsort.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/stdlib/SDL_qsort.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_stdlib.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/stdlib/SDL_stdlib.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_string.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/stdlib/SDL_string.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_thread.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/thread/SDL_thread.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_timer.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/timer/SDL_timer.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_RLEaccel.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_RLEaccel.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_blit.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_blit.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_blit_0.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_blit_0.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_blit_1.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_blit_1.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_blit_A.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_blit_A.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_blit_N.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_blit_N.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_blit_auto.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_blit_auto.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_blit_copy.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_blit_copy.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_blit_slow.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_blit_slow.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_bmp.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_bmp.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_clipboard.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_clipboard.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_egl.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_egl.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_fillrect.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_fillrect.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_pixels.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_pixels.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_rect.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_rect.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_shape.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_shape.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_stretch.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_stretch.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_surface.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_surface.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_video.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/SDL_video.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_nullevents.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/dummy/SDL_nullevents.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_nullframebuffer.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/dummy/SDL_nullframebuffer.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_nullvideo.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/dummy/SDL_nullvideo.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_diskaudio.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/audio/disk/SDL_diskaudio.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_dummyaudio.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/audio/dummy/SDL_dummyaudio.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_sysloadso.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/loadso/dlopen/SDL_sysloadso.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_dspaudio.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/audio/dsp/SDL_dspaudio.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_x11clipboard.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/SDL_x11clipboard.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_x11dyn.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/SDL_x11dyn.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_x11events.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/SDL_x11events.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_x11framebuffer.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/SDL_x11framebuffer.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_x11keyboard.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/SDL_x11keyboard.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_x11messagebox.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/SDL_x11messagebox.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_x11modes.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/SDL_x11modes.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_x11mouse.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/SDL_x11mouse.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_x11opengl.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/SDL_x11opengl.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_x11opengles.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/SDL_x11opengles.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_x11shape.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/SDL_x11shape.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_x11touch.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/SDL_x11touch.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_x11video.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/SDL_x11video.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_x11window.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/SDL_x11window.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_x11xinput2.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/SDL_x11xinput2.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/edid-parse.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/edid-parse.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/imKStoUCS.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/video/x11/imKStoUCS.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_systhread.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/thread/pthread/SDL_systhread.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_syssem.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/thread/pthread/SDL_syssem.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_sysmutex.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/thread/pthread/SDL_sysmutex.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_syscond.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/thread/pthread/SDL_syscond.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_systls.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/thread/pthread/SDL_systls.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_sysjoystick.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/joystick/linux/SDL_sysjoystick.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_syshaptic.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/haptic/linux/SDL_syshaptic.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_syspower.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/power/linux/SDL_syspower.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_sysfilesystem.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/filesystem/unix/SDL_sysfilesystem.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_systimer.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/timer/unix/SDL_systimer.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_evdev.lo: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/core/linux/SDL_evdev.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@


$(objects)/SDL_dummy_main.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/main/dummy/SDL_dummy_main.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

$(objects)/SDL_test_assert.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/test/SDL_test_assert.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@ 
$(objects)/SDL_test_common.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/test/SDL_test_common.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@ 
$(objects)/SDL_test_compare.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/test/SDL_test_compare.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@ 
$(objects)/SDL_test_crc32.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/test/SDL_test_crc32.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@ 
$(objects)/SDL_test_font.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/test/SDL_test_font.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@ 
$(objects)/SDL_test_fuzzer.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/test/SDL_test_fuzzer.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@ 
$(objects)/SDL_test_harness.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/test/SDL_test_harness.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@ 
$(objects)/SDL_test_imageBlit.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/test/SDL_test_imageBlit.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@ 
$(objects)/SDL_test_imageBlitBlend.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/test/SDL_test_imageBlitBlend.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@ 
$(objects)/SDL_test_imageFace.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/test/SDL_test_imageFace.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@ 
$(objects)/SDL_test_imagePrimitives.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/test/SDL_test_imagePrimitives.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@ 
$(objects)/SDL_test_imagePrimitivesBlend.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/test/SDL_test_imagePrimitivesBlend.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@ 
$(objects)/SDL_test_log.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/test/SDL_test_log.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@ 
$(objects)/SDL_test_md5.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/test/SDL_test_md5.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@ 
$(objects)/SDL_test_random.o: /home/rcl/Perforce/RCL_kubmonster_1/UE4/Engine/Source/ThirdParty/SDL2/SDL-gui-backend/build/SDL-gui-backend/src/test/SDL_test_random.c
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(CFLAGS) $(EXTRA_CFLAGS) -MMD -MT $@ -c $< -o $@

#
# Cross Platform Makefile
# Compatible with MSYS2/MINGW, Ubuntu 14.04.1 and Mac OS X
#
# You will need GLFW (http://www.glfw.org):
# Linux:
#   apt-get install libglfw-dev
# Mac OS X:
#   brew install glfw
# MSYS2:
#   pacman -S --noconfirm --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-glfw
#

#CXX = g++
#CXX = clang++

EXE = midi2osc
SOURCES = main.cpp imgui_impl_glfw_gl3.cpp
SOURCES += imgui/imgui.cpp imgui/imgui_draw.cpp
SOURCES += imgui/gl3w/GL/gl3w.c
OBJS = $(addsuffix .o, $(basename $(notdir $(SOURCES))))

UNAME_S := $(shell uname -s)


ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	LIBS = -lGL `pkg-config --static --libs glfw3`

	CXXFLAGS = -Iimgui/ -Iimgui/gl3w `pkg-config --cflags glfw3`
	CXXFLAGS += -Wall -Wformat -O2 
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	LIBS = -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	#LIBS += -L/usr/local/lib -lglfw3
	LIBS += -L/usr/local/lib -lglfw3 -lportmidi -headerpad_max_install_names

	CXXFLAGS = -Iimgui/ -Iimgui/gl3w -I/usr/local/include -Wno-c++11-extensions
	CXXFLAGS += -Wall -Wformat -O2
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
   ECHO_MESSAGE = "Windows"
   LIBS = -lglfw3 -lgdi32 -lopengl32 -limm32

   CXXFLAGS = -Iimgui/ -Iimgui/gl3w `pkg-config --cflags glfw3`
   CXXFLAGS += -Wall -Wformat
   CFLAGS = $(CXXFLAGS)
endif



%.o:%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o:imgui/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o:imgui/gl3w/GL/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

all: $(EXE)
	@echo Build complete for $(ECHO_MESSAGE)

$(EXE): $(OBJS) $(EXE).icns Info.plist 
	$(CXX) -o $@ $(OBJS) $(CXXFLAGS) $(LIBS)
	mkdir -p midi2osc.app/Contents/MacOS/
	mkdir -p midi2osc.app/Contents/Resources/
	cp midi2osc midi2osc.app/Contents/MacOS/
	cp Info.plist midi2osc.app/Contents/
	cp $(EXE).icns midi2osc.app/Contents/Resources/
	-cp /usr/local/opt/glfw3/lib/libglfw3.3.dylib midi2osc.app/Contents/MacOS/
	-cp /usr/local/opt/portmidi/lib/libportmidi.dylib midi2osc.app/Contents/MacOS/
	install_name_tool -change /usr/local/opt/glfw3/lib/libglfw3.3.dylib @executable_path/libglfw3.3.dylib midi2osc.app/Contents/MacOS/midi2osc
	install_name_tool -change /usr/local/opt/portmidi/lib/libportmidi.dylib @executable_path/libportmidi.dylib midi2osc.app/Contents/MacOS/midi2osc

clean:
	rm -f $(EXE) $(OBJS)
	rm -f $(EXE).icns

$(EXE).icns: $(EXE)Icon.png
	rm -rf $(EXE).iconset
	mkdir $(EXE).iconset
	sips -z 16 16     $(EXE)Icon.png --out $(EXE).iconset/icon_16x16.png
	sips -z 32 32     $(EXE)Icon.png --out $(EXE).iconset/icon_16x16@2x.png
	sips -z 32 32     $(EXE)Icon.png --out $(EXE).iconset/icon_32x32.png
	sips -z 64 64     $(EXE)Icon.png --out $(EXE).iconset/icon_32x32@2x.png
	sips -z 128 128   $(EXE)Icon.png --out $(EXE).iconset/icon_128x128.png
	sips -z 256 256   $(EXE)Icon.png --out $(EXE).iconset/icon_128x128@2x.png
	sips -z 256 256   $(EXE)Icon.png --out $(EXE).iconset/icon_256x256.png
	sips -z 512 512   $(EXE)Icon.png --out $(EXE).iconset/icon_256x256@2x.png
	sips -z 512 512   $(EXE)Icon.png --out $(EXE).iconset/icon_512x512.png
	sips -z 1024 1024 $(EXE)Icon.png --out $(EXE).iconset/icon_512x512@2x.png
	#cp $(EXE)Icon.png $(EXE).iconset/icon_512x512@2x.png
	iconutil -c icns -o $(EXE).icns $(EXE).iconset
	rm -r $(EXE).iconset

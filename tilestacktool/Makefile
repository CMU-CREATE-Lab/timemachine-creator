COMMANDS=

CPP_UTILS_DIR=../cpp-utils

include ../Makefile.common

ifeq ($(OS),windows)
TILESTACKTOOL = tilestacktool.exe
else
TILESTACKTOOL = tilestacktool
endif

ifeq ($(OS),osx)
PLATFORM_CXX_FLAGS=-mmacosx-version-min=10.5 
endif

ifeq ($(OS),linux)
PLATFORM_CXX_FLAGS=-static
endif

build: $(TILESTACKTOOL)

test: selftest patp4_1x1_small

DATASETS=../datasets
PATP4S=$(DATASETS)/patp

selftest: $(TILESTACKTOOL)
	./tilestacktool --selftest

patp4_1x1_small: $(TILESTACKTOOL)
	./tilestacktool --tilesize 256 --image2tiles testresults/$@/patp0.data/tiles kro $(DATASETS)/$@/patp0.jpg
	./tilestacktool --tilesize 256 --image2tiles testresults/$@/patp1.data/tiles kro $(DATASETS)/$@/patp1.jpg
	./tilestacktool --tilesize 256 --image2tiles testresults/$@/patp2.data/tiles kro $(DATASETS)/$@/patp2.jpg
	./tilestacktool --tilesize 256 --image2tiles testresults/$@/patp3.data/tiles kro $(DATASETS)/$@/patp3.jpg
	./tilestacktool --loadtiles testresults/$@/patp?.data/tiles/r0.kro --create-parent-directories --save testresults/$@/transpose/r0.ts2
	./tilestacktool --loadtiles testresults/$@/patp?.data/tiles/r1.kro --create-parent-directories --save testresults/$@/transpose/r1.ts2
	./tilestacktool --loadtiles testresults/$@/patp?.data/tiles/r2.kro --create-parent-directories --save testresults/$@/transpose/r2.ts2
	./tilestacktool --loadtiles testresults/$@/patp?.data/tiles/r3.kro --create-parent-directories --save testresults/$@/transpose/r3.ts2
	$(call CP,testresults/$@/patp0.data/tiles/r.json,testresults/$@/transpose/r.json)
	./tilestacktool --path2stack 256 256 '{"frames":{"start":0, "end":3} ,"bounds":{"xmin":0, "ymin":0, "width":512, "height":512}}' testresults/$@/transpose --save testresults/$@/transpose/r.ts2
	./tilestacktool --path2stack 200 150 '{"frames":{"start":0, "end":3} ,"bounds":{"xmin":150, "ymin":200, "width":400, "height":300}}' testresults/$@/transpose --writevideo testresults/$@/testvid.mp4 1 24
	./tilestacktool --path2stack 1088 624 '{"frames":{"start":0,"end":3},"bounds":{"xmin":0,"ymin":0,"width":1088,"height":624}}' testresults/$@/transpose

units:
	g++ -g -I /opt/local/include -I . -Wall unit_tests/test_GPTileIdx.cpp GPTileIdx.cpp -o unit_tests/test_GPTileIdx
	unit_tests/test_GPTileIdx

test-modis: tilestacktool
	./tilestacktool --load modis/r12.ts2 --viz 0 3000 1 --writehtml r12.html
	./tilestacktool --load modis/r12.ts2 --viz 0 3000 1 --save modis/r12.8.ts2
	./tilestacktool --load modis/r12.8.ts2 --writehtml modis/r12.8.html
	./tilestacktool --load modis/r12.8.ts2 --writevideo modis/r12.8.mp4 1 26

SOURCES = tilestacktool.cpp io.cpp io_streamfile.cpp Tilestack.cpp $(CPP_UTILS_DIR)/cpp-utils.cpp png_util.cpp ImageReader.cpp ImageWriter.cpp GPTileIdx.cpp jsoncpp/json_reader.cpp jsoncpp/json_value.cpp jsoncpp/json_writer.cpp $(COMMANDS)

ZLIB_DIR = dependencies/zlib

ifeq ($(OS),windows)

ZLIB = $(ZLIB_DIR)/zlib.lib

$(ZLIB):
	(cd $(ZLIB_DIR) && $(call CP,zconf.h.original,zconf.h) && nmake /f win32\\Makefile.msc)

zlib-clean:
	(cd $(ZLIB_DIR) && nmake /f win32\\Makefile.msc clean)

else

ZLIB = $(ZLIB_DIR)/libz.a

$(ZLIB):
	(cd $(ZLIB_DIR); cp zconf.h.original zconf.h; ./configure; make)

zlib-clean:
	make -C $(ZLIB_DIR) clean
	rm -rf $(ZLIB_DIR)/lib $(ZLIB_DIR)/include

endif

zlib: $(ZLIB)

LIBPNG_DIR = dependencies/libpng

ifeq ($(OS),windows)

LIBPNG = $(LIBPNG_DIR)/libpng.lib

$(LIBPNG): $(ZLIB)
	(cd $(LIBPNG_DIR) && nmake /f scripts\\makefile.vcwin32)

libpng-clean:
	(cd $(LIBPNG_DIR) && nmake /f scripts\\makefile.vcwin32 clean)

else

LIBPNG = $(LIBPNG_DIR)/lib/libpng.a

$(LIBPNG):
	(cd $(LIBPNG_DIR); ./configure --prefix=`pwd`; make install)

libpng-clean:
	rm -rf $(LIBPNG_DIR)/lib $(LIBPNG_DIR)/include
	make -C $(LIBPNG_DIR) clean

endif

libpng: $(LIBPNG)

LIBJPEG_DIR = dependencies/jpeg-8d

ifeq ($(OS),windows)

LIBJPEG = $(LIBJPEG_DIR)/libjpeg.lib

$(LIBJPEG): $(LIBJPEG_DIR)/jconfig.h
	(cd $(LIBJPEG_DIR) && nmake /f Makefile.vc nodebug=1)

# Configure for Visual Studio 10
$(LIBJPEG_DIR)/jconfig.h:
	(cd $(LIBJPEG_DIR) && nmake /f Makefile.vc setup-v10)

libjpeg-clean:
	(cd $(LIBJPEG_DIR) && nmake /f Makefile.vc clean)

else

LIBJPEG = $(LIBJPEG_DIR)/lib/libjpeg.a

$(LIBJPEG):
	(cd $(LIBJPEG_DIR); ./configure --prefix=`pwd`; make install)

libjpeg-clean:
	rm -rf $(LIBJPEG_DIR)/lib $(LIBJPEG_DIR)/include
	make -C $(LIBJPEG_DIR) clean

endif

libjpeg: $(LIBJPEG)

tilestacktool.exe: $(SOURCES) $(ZLIB) $(LIBPNG) $(LIBJPEG)
	cl /EHsc /Ox /MT /Ijsoncpp /I$(LIBJPEG_DIR) /I$(LIBPNG_DIR) /I../cpp-utils /Fetilestacktool.exe $(SOURCES) $(ZLIB) $(LIBPNG) $(LIBJPEG) /link /NODEFAULTLIB:LIBCMT

tilestacktool: $(SOURCES) $(LIBPNG) $(ZLIB) $(LIBJPEG)
	g++ $(PLATFORM_CXX_FLAGS) -O3 -g -Ijsoncpp -I$(LIBJPEG_DIR) -I$(LIBPNG_DIR) -I$(CPP_UTILS_DIR) -Wall $^ -o $@

clean: zlib-clean libpng-clean libjpeg-clean
	$(call RM_R,testresults)
	$(call RM,*.o)
	$(call RM,*.obj)
	$(call RM,$(TILESTACKTOOL))

COMMANDS=

CPP_UTILS_DIR=../cpp_utils

include ../Makefile.common

ifeq ($(OS),windows)
TILESTACKTOOL = tilestacktool.exe
else
TILESTACKTOOL = tilestacktool
OPTIMIZATION = -O3
endif

ifeq ($(OS),osx)
PLATFORM_CXX_FLAGS=-mmacosx-version-min=10.5 
endif

ifeq ($(OS),linux)
PLATFORM_CXX_FLAGS=-static
endif

build: $(TILESTACKTOOL)

test: selftest patp4_1x1_small

test-clean:
	$(call RM_R,testresults)

DATASETS=../datasets
PATP4S=$(DATASETS)/patp

selftest: $(TILESTACKTOOL)
	./tilestacktool --selftest

patp4_1x1_small: $(TILESTACKTOOL)
	./tilestacktool --tilesize 256 --image2tiles testresults/$@/patp0.data/tiles kro $(DATASETS)/$@/patp0.jpg
	./tilestacktool --tilesize 256 --image2tiles testresults/$@/patp1.data/tiles kro $(DATASETS)/$@/patp1.jpg
	./tilestacktool --tilesize 256 --image2tiles testresults/$@/patp2.data/tiles kro $(DATASETS)/$@/patp2.jpg
	./tilestacktool --tilesize 256 --image2tiles testresults/$@/patp3.data/tiles kro $(DATASETS)/$@/patp3.jpg
	./tilestacktool --delete-source-tiles --loadtiles testresults/$@/patp?.data/tiles/r0.kro --create-parent-directories --save testresults/$@/transpose/r0.ts2
	./tilestacktool --delete-source-tiles --loadtiles testresults/$@/patp?.data/tiles/r1.kro --create-parent-directories --save testresults/$@/transpose/r1.ts2
	./tilestacktool --delete-source-tiles --loadtiles testresults/$@/patp?.data/tiles/r2.kro --create-parent-directories --save testresults/$@/transpose/r2.ts2
	./tilestacktool --delete-source-tiles --loadtiles testresults/$@/patp?.data/tiles/r3.kro --create-parent-directories --save testresults/$@/transpose/r3.ts2
	$(call CP,testresults/$@/patp0.data/tiles/r.json,testresults/$@/transpose/r.json)
	./tilestacktool --path2stack-downsize 256 256 '{"frames":{"start":0, "end":3} ,"bounds":{"xmin":0, "ymin":0, "width":512, "height":512}}' testresults/$@/transpose --save testresults/$@/transpose/r.ts2
	./tilestacktool --path2stack 200 150 '{"frames":{"start":0, "end":3} ,"bounds":{"xmin":150, "ymin":200, "width":400, "height":300}}' testresults/$@/transpose --writevideo testresults/$@/testvid.mp4 1 24
	./tilestacktool --path2stack 200 150 '{"frames":{"start":0, "end":3} ,"bounds":{"xmin":150, "ymin":200, "width":400, "height":300}}' testresults/$@/transpose --prependleader 10 --writevideo testresults/$@/testvid-with-leader.mp4 1 24
	./tilestacktool --path2stack 1088 624 '{"frames":{"start":0,"end":3},"bounds":{"xmin":0,"ymin":0,"width":1088,"height":624}}' testresults/$@/transpose
	./tilestacktool --path2stack 200 150 '{"frames":{"start":0, "end":3} ,"bounds":{"xmin":150, "ymin":200, "width":400, "height":300}}' testresults/$@/transpose --viz '{"scale":0.3}' --writevideo testresults/$@/testvid_scale_0.3.mp4 1 24
	./tilestacktool --path2stack 200 150 '{"frames":{"start":0, "end":3} ,"bounds":{"xmin":150, "ymin":200, "width":400, "height":300}}' testresults/$@/transpose --viz '{"scale":[1,0,0]}' --writevideo testresults/$@/testvid_scale_1_0_0.mp4 1 24
	./tilestacktool --path2stack 200 150 '{"frames":{"start":0, "end":3} ,"bounds":{"xmin":150, "ymin":200, "width":400, "height":300}}' testresults/$@/transpose --viz '{"gamma":2}' --writevideo testresults/$@/testvid_gamma_2.mp4 1 24
	./tilestacktool --path2stack 200 150 '{"frames":{"start":0, "end":3} ,"bounds":{"xmin":150, "ymin":200, "width":400, "height":300}}' testresults/$@/transpose --viz '{"gamma":[1,2,1]}' --writevideo testresults/$@/testvid_gamma_1_2_1.mp4 1 24

test-overlay: patp4_1x1_small
	./tilestacktool \
	    --path2stack 1920 1080 '{"frames":{"start":0,"end":3},"bounds":{"xmin":0,"ymin":48,"width":512,"height":288}}' testresults/patp4_1x1_small/transpose \
	    --tilestackinfo \
	    --viz '{"scale":0.3}' \
	    --tilestackinfo \
	    --path2overlay 1920 1080 '{"frames":{"start":0,"end":3},"bounds":{"xmin":0,"ymin":48,"width":512,"height":288}}' overlay-test.html \
	    --tilestackinfo \
	    --composite \
	    --writevideo testresults/overlay.mp4 1 24

test-modis: tilestacktool
	./tilestacktool --load modis/r12.ts2 --viz '{"scale":0.0853}' --writehtml r12.html
	./tilestacktool --load modis/r12.ts2 --viz '{"scale":0.0853}' --save modis/r12.8.ts2
	./tilestacktool --load modis/r12.8.ts2 --writehtml modis/r12.8.html
	./tilestacktool --load modis/r12.8.ts2 --writevideo modis/r12.8.mp4 1 26

interpolation: $(TILESTACKTOOL)
	./tilestacktool --tilesize 256 --image2tiles testresults/$@/0.data/tiles kro InterpolationTest.png
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r00.kro --create-parent-directories --save testresults/$@/stacks/r00.ts2
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r01.kro --create-parent-directories --save testresults/$@/stacks/r01.ts2
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r02.kro --create-parent-directories --save testresults/$@/stacks/r02.ts2
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r03.kro --create-parent-directories --save testresults/$@/stacks/r03.ts2
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r10.kro --create-parent-directories --save testresults/$@/stacks/r10.ts2
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r11.kro --create-parent-directories --save testresults/$@/stacks/r11.ts2
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r12.kro --create-parent-directories --save testresults/$@/stacks/r12.ts2
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r13.kro --create-parent-directories --save testresults/$@/stacks/r13.ts2
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r20.kro --create-parent-directories --save testresults/$@/stacks/r20.ts2
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r21.kro --create-parent-directories --save testresults/$@/stacks/r21.ts2
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r22.kro --create-parent-directories --save testresults/$@/stacks/r22.ts2
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r23.kro --create-parent-directories --save testresults/$@/stacks/r23.ts2
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r30.kro --create-parent-directories --save testresults/$@/stacks/r30.ts2
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r31.kro --create-parent-directories --save testresults/$@/stacks/r31.ts2
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r32.kro --create-parent-directories --save testresults/$@/stacks/r32.ts2
	./tilestacktool --loadtiles testresults/$@/0.data/tiles/r33.kro --create-parent-directories --save testresults/$@/stacks/r33.ts2
	$(call CP,testresults/$@/0.data/tiles/r.json,testresults/$@/stacks/r.json)
	./tilestacktool --path2stack-downsize 256 256 '{"frames":{"start":0, "end":0} ,"bounds":{"xmin":  0, "ymin":  0, "width": 512, "height": 512}}' testresults/$@/stacks --save testresults/$@/stacks/r0.ts2
	./tilestacktool --path2stack-downsize 256 256 '{"frames":{"start":0, "end":0} ,"bounds":{"xmin":512, "ymin":  0, "width": 512, "height": 512}}' testresults/$@/stacks --save testresults/$@/stacks/r1.ts2
	./tilestacktool --path2stack-downsize 256 256 '{"frames":{"start":0, "end":0} ,"bounds":{"xmin":  0, "ymin":512, "width": 512, "height": 512}}' testresults/$@/stacks --save testresults/$@/stacks/r2.ts2
	./tilestacktool --path2stack-downsize 256 256 '{"frames":{"start":0, "end":0} ,"bounds":{"xmin":512, "ymin":512, "width": 512, "height": 512}}' testresults/$@/stacks --save testresults/$@/stacks/r3.ts2
	./tilestacktool --path2stack-downsize 256 256 '{"frames":{"start":0, "end":0} ,"bounds":{"xmin":  0, "ymin":  0, "width":1024, "height":1024}}' testresults/$@/stacks --save testresults/$@/stacks/r.ts2
	./tilestacktool --path2stack-downsize 256 256 '{"frames":{"start":0, "end":0} ,"bounds":{"xmin":383, "ymin":383, "width": 256, "height": 256}}' testresults/$@/stacks --writevideo testresults/$@/foo0.mp4 1 24 # 4
	./tilestacktool --path2stack-downsize 256 256 '{"frames":{"start":0, "end":0} ,"bounds":{"xmin":255, "ymin":255, "width": 512, "height": 512}}' testresults/$@/stacks --writevideo testresults/$@/foo1.mp4 1 24 # 9
	./tilestacktool --path2stack-downsize 256 256 '{"frames":{"start":0, "end":0} ,"bounds":{"xmin":  0, "ymin":  0, "width":1024, "height":1024}}' testresults/$@/stacks --writevideo testresults/$@/foo2.mp4 1 24 # 4
	./tilestacktool --path2stack 256 256 '{"frames":{"start":0, "end":0} ,"bounds":{"xmin":383, "ymin":383, "width": 256, "height": 256}}' testresults/$@/stacks --writevideo testresults/$@/foo0.mp4 1 24 # 4
	./tilestacktool --path2stack 256 256 '{"frames":{"start":0, "end":0} ,"bounds":{"xmin":255, "ymin":255, "width": 512, "height": 512}}' testresults/$@/stacks --writevideo testresults/$@/foo1.mp4 1 24 # 4
	./tilestacktool --path2stack 256 256 '{"frames":{"start":0, "end":0} ,"bounds":{"xmin":  0, "ymin":  0, "width":1024, "height":1024}}' testresults/$@/stacks --writevideo testresults/$@/foo2.mp4 1 24 # 1
	./tilestacktool --path2stack 1024 1024 '{"frames":{"start":0, "end":0} ,"bounds":{"xmin":  0, "ymin":  0, "width":1024, "height":1024}}' testresults/$@/stacks --writehtml testresults/$@/1024.html
	./tilestacktool --path2stack 512 512 '{"frames":{"start":0, "end":0} ,"bounds":{"xmin":  0, "ymin":  0, "width":1024, "height":1024}}' testresults/$@/stacks --writehtml testresults/$@/512.html
	./tilestacktool --path2stack 256 256 '{"frames":{"start":0, "end":0} ,"bounds":{"xmin":  0, "ymin":  0, "width":1024, "height":1024}}' testresults/$@/stacks --writehtml testresults/$@/256.html

LEADERDEST = testresults/leaders

LEADERS=$(LEADERDEST)/leader_1088x624_24_60.mp4 \
        $(LEADERDEST)/leader_768x432_24_120.mp4 \
        $(LEADERDEST)/leader_1088x624_26_70.mp4 \
	$(LEADERDEST)/leader_768x432_26_140.mp4 \
        $(LEADERDEST)/leader_1088x624_28_80.mp4 \
        $(LEADERDEST)/leader_768x432_28_160.mp4 \
        $(LEADERDEST)/leader_1088x624_30_90.mp4 \
        $(LEADERDEST)/leader_768x432_30_180.mp4

leaders: $(LEADERS)
	ls -l $(LEADERDEST)

leaders.clean:
	rm -rf $(LEADERDEST)

leaders.rebuild:
	$(MAKE) leaders.clean
	$(MAKE) leaders

leader_dims = $(word 2,$(subst _, ,$(basename $(1))))
leader_compression = $(word 3,$(subst _, ,$(basename $(1))))
leader_nframes = $(word 4,$(subst _, ,$(basename $(1))))

leader_width = $(word 1,$(subst x, ,$(call leader_dims,$(1))))
leader_height = $(word 2,$(subst x, ,$(call leader_dims,$(1))))

leader_%.mp4: $(TILESTACKTOOL)
	mkdir -p $(LEADERDEST)
	./tilestacktool --blackstack 1 $(call leader_width,$@) $(call leader_height,$@) 3 8 --prependleader $(call leader_nframes,$@) --writevideo $@ 10 $(call leader_compression,$@)

SOURCES = tilestacktool.cpp warp.cpp io.cpp io_streamfile.cpp Tilestack.cpp $(CPP_UTILS_DIR)/cpp_utils.cpp png_util.cpp ImageReader.cpp ImageWriter.cpp GPTileIdx.cpp jsoncpp/json_reader.cpp jsoncpp/json_value.cpp jsoncpp/json_writer.cpp qt-faststart.cpp SimpleZlib.cpp $(COMMANDS)

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
	(cd $(ZLIB_DIR) && cp zconf.h.original zconf.h && cp Makefile.original Makefile && ./configure && make)

zlib-clean:
	rm -rf $(ZLIB_DIR)/lib $(ZLIB_DIR)/include $(ZLIB_DIR)/Makefile $(ZLIB)

zlib: $(ZLIB)

endif

LIBPNG_DIR = dependencies/libpng

ifeq ($(OS),windows)

LIBPNG = $(LIBPNG_DIR)/libpng.lib

$(LIBPNG): $(ZLIB)
	(cd $(LIBPNG_DIR) && nmake /f scripts\\makefile.vcwin32)

libpng-clean:
	(cd $(LIBPNG_DIR) && nmake /f scripts\\makefile.vcwin32 clean)

else

LIBPNG = $(LIBPNG_DIR)/lib/libpng.a

$(LIBPNG): $(ZLIB)
	(cd $(LIBPNG_DIR); CFLAGS=-I`pwd`/../zlib LDFLAGS=-L`pwd`/../zlib ./configure --prefix=`pwd` --enable-shared=no; make install)

libpng-clean:
	rm -rf $(LIBPNG_DIR)/lib $(LIBPNG_DIR)/include $(LIBPNG)
	-make -C $(LIBPNG_DIR) clean

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
	rm -rf $(LIBJPEG_DIR)/lib $(LIBJPEG_DIR)/include $(LIBJPEG)
	-make -C $(LIBJPEG_DIR) clean

endif

libjpeg: $(LIBJPEG)

tilestacktool.exe: $(SOURCES) $(ZLIB) $(LIBPNG) $(LIBJPEG)
	cl /EHsc /Ox /MT /Ijsoncpp /I$(ZLIB_DIR) /I$(LIBJPEG_DIR) /I$(LIBPNG_DIR) /I../cpp_utils /Fetilestacktool.exe $(SOURCES) $(ZLIB) $(LIBPNG) $(LIBJPEG) /link /NODEFAULTLIB:LIBCMT

tilestacktool: $(SOURCES) $(LIBPNG) $(ZLIB) $(LIBJPEG)
	g++ $(PLATFORM_CXX_FLAGS) $(OPTIMIZATION) -g -Ijsoncpp -I$(ZLIB_DIR) -I$(LIBJPEG_DIR) -I$(LIBPNG_DIR) -I$(CPP_UTILS_DIR) -Wall $^ -o $@

units: test_GPTileIdx test_SimpleZlib

test_%: unit_tests/test_%.cpp $(CPP_UTILS_DIR)/cpp_utils.cpp SimpleZlib.cpp GPTileIdx.cpp $(LIBPNG) $(ZLIB) $(LIBJPEG) 
	g++ $(PLATFORM_CXX_FLAGS) -g -Ijsoncpp -I. -I$(LIBJPEG_DIR) -I$(LIBPNG_DIR) -I$(CPP_UTILS_DIR) -Wall $^ -o unit_tests/$@
	unit_tests/$@

clean: zlib-clean libpng-clean libjpeg-clean test-clean
	$(call RM,*.o)
	$(call RM,*.obj)
	$(call RM,$(TILESTACKTOOL))

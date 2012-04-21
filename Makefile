test: test-image2tiles test-modis

test-image2tiles: tilestacktool
	./tilestacktool --tilesize 512 --image2tiles test-patp-out kro test-patp/1.jpg

units:
	g++ -g -I /opt/local/include -I . -Wall unit_tests/test_GPTileIdx.cpp GPTileIdx.cpp -o unit_tests/test_GPTileIdx
	unit_tests/test_GPTileIdx

test-modis: tilestacktool
	./tilestacktool --load modis/r12.ts2 --viz 0 3000 1 --writehtml r12.html
	./tilestacktool --load modis/r12.ts2 --viz 0 3000 1 --save modis/r12.8.ts2
	./tilestacktool --load modis/r12.8.ts2 --writehtml modis/r12.8.html
	./tilestacktool --load modis/r12.8.ts2 --writevideo modis/r12.8.mp4 1 26

tilestacktool: tilestacktool.cpp cpp-utils/cpp-utils.cpp png_util.cpp ImageReader.cpp ImageWriter.cpp GPTileIdx.cpp
	g++ -g -I/opt/local/include -Icpp-utils -Wall $^ -o $@ -L/opt/local/lib -lpng -ljpeg


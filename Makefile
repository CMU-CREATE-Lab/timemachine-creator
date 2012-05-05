test: test-patp test-image2tiles

test-patp: tilestacktool
	./tilestacktool --tilesize 256 --image2tiles patp/out/patp0.data/tiles kro patp/patp0.jpg
	./tilestacktool --tilesize 256 --image2tiles patp/out/patp1.data/tiles kro patp/patp1.jpg
	./tilestacktool --tilesize 256 --image2tiles patp/out/patp2.data/tiles kro patp/patp2.jpg
	./tilestacktool --tilesize 256 --image2tiles patp/out/patp3.data/tiles kro patp/patp3.jpg
	./tilestacktool --loadtiles patp/out/patp{0,1,2,3}.data/tiles/r0.kro --create-parent-directories --save patp/out/transpose/r0.ts2
	./tilestacktool --loadtiles patp/out/patp{0,1,2,3}.data/tiles/r1.kro --create-parent-directories --save patp/out/transpose/r1.ts2
	./tilestacktool --loadtiles patp/out/patp{0,1,2,3}.data/tiles/r2.kro --create-parent-directories --save patp/out/transpose/r2.ts2
	./tilestacktool --loadtiles patp/out/patp{0,1,2,3}.data/tiles/r3.kro --create-parent-directories --save patp/out/transpose/r3.ts2
	cp patp/out/patp0.data/tiles/r.json patp/out/transpose/r.json
	./tilestacktool --path2stack 256 256 '{"frames":{"start":0, "end":3} ,"bounds":{"xmin":0, "ymin":0, "width":512, "height":512}}' patp/out/transpose --save patp/out/transpose/r.ts2
	./tilestacktool --path2stack 200 150 '{"frames":{"start":0, "end":3} ,"bounds":{"xmin":150, "ymin":200, "width":400, "height":300}}' patp/out/transpose --writevideo patp/out/testvid.mp4 1 24

debug-patp: test-patp
	./tilestacktool --load patp/out/transpose/r0.ts2 --writehtml patp/out/transpose/r0.html
	./tilestacktool --load patp/out/transpose/r2.ts2 --writehtml patp/out/transpose/r2.html
	./tilestacktool --load patp/out/transpose/r.ts2 --writehtml patp/out/transpose/r.html

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

tilestacktool: tilestacktool.cpp cpp-utils/cpp-utils.cpp png_util.cpp ImageReader.cpp ImageWriter.cpp GPTileIdx.cpp jsoncpp/json_reader.cpp jsoncpp/json_value.cpp jsoncpp/json_writer.cpp
	g++ -g -Ijsoncpp -I/opt/local/include -Icpp-utils -Wall $^ -o $@ -L/opt/local/lib -lpng -ljpeg

clean:
	rm -rf *.o patp/out

COMMANDS=

build: tilestacktool

test: test-patp4s

test-patp4s: tilestacktool
	./tilestacktool --tilesize 256 --image2tiles testresults/patp4s/patp0.data/tiles kro patp4s/patp0.jpg
	./tilestacktool --tilesize 256 --image2tiles testresults/patp4s/patp1.data/tiles kro patp4s/patp1.jpg
	./tilestacktool --tilesize 256 --image2tiles testresults/patp4s/patp2.data/tiles kro patp4s/patp2.jpg
	./tilestacktool --tilesize 256 --image2tiles testresults/patp4s/patp3.data/tiles kro patp4s/patp3.jpg
	./tilestacktool --loadtiles testresults/patp4s/patp{0,1,2,3}.data/tiles/r0.kro --create-parent-directories --save testresults/patp4s/transpose/r0.ts2
	./tilestacktool --loadtiles testresults/patp4s/patp{0,1,2,3}.data/tiles/r1.kro --create-parent-directories --save testresults/patp4s/transpose/r1.ts2
	./tilestacktool --loadtiles testresults/patp4s/patp{0,1,2,3}.data/tiles/r2.kro --create-parent-directories --save testresults/patp4s/transpose/r2.ts2
	./tilestacktool --loadtiles testresults/patp4s/patp{0,1,2,3}.data/tiles/r3.kro --create-parent-directories --save testresults/patp4s/transpose/r3.ts2
	cp testresults/patp4s/patp0.data/tiles/r.json testresults/patp4s/transpose/r.json
	./tilestacktool --path2stack 256 256 '{"frames":{"start":0, "end":3} ,"bounds":{"xmin":0, "ymin":0, "width":512, "height":512}}' testresults/patp4s/transpose --save testresults/patp4s/transpose/r.ts2
	./tilestacktool --path2stack 200 150 '{"frames":{"start":0, "end":3} ,"bounds":{"xmin":150, "ymin":200, "width":400, "height":300}}' testresults/patp4s/transpose --writevideo testresults/patp4s/testvid.mp4 1 24
	./tilestacktool --path2stack 1088 624 '{"frames":{"start":0,"end":3},"bounds":{"xmin":0,"ymin":0,"width":1088,"height":624}}' testresults/patp4s/transpose

debug-patp: test-patp
	./tilestacktool --load testresults/patp4s/transpose/r0.ts2 --writehtml testresults/patp4s/transpose/r0.html
	./tilestacktool --load testresults/patp4s/transpose/r2.ts2 --writehtml testresults/patp4s/transpose/r2.html
	./tilestacktool --load testresults/patp4s/transpose/r.ts2 --writehtml testresults/patp4s/transpose/r.html

units:
	g++ -g -I /opt/local/include -I . -Wall unit_tests/test_GPTileIdx.cpp GPTileIdx.cpp -o unit_tests/test_GPTileIdx
	unit_tests/test_GPTileIdx

test-modis: tilestacktool
	./tilestacktool --load modis/r12.ts2 --viz 0 3000 1 --writehtml r12.html
	./tilestacktool --load modis/r12.ts2 --viz 0 3000 1 --save modis/r12.8.ts2
	./tilestacktool --load modis/r12.8.ts2 --writehtml modis/r12.8.html
	./tilestacktool --load modis/r12.8.ts2 --writevideo modis/r12.8.mp4 1 26

tilestacktool: tilestacktool.cpp io.cpp io_streamfile.cpp Tilestack.cpp cpp-utils/cpp-utils.cpp png_util.cpp ImageReader.cpp ImageWriter.cpp GPTileIdx.cpp jsoncpp/json_reader.cpp jsoncpp/json_value.cpp jsoncpp/json_writer.cpp $(COMMANDS)
	g++ -O3 -g -Ijsoncpp -I/opt/local/include -Icpp-utils -Wall $^ -o $@ -L/opt/local/lib -lpng -ljpeg

clean:
	rm -rf tilestacktool *.o testresults

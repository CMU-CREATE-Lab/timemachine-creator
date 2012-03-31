test1: tilestacktool
	./tilestacktool --load test/r12.ts2 --viz 0 3000 1 --writehtml r12.html
	./tilestacktool --load test/r12.ts2 --viz 0 3000 1 --save test/r12.8.ts2
	./tilestacktool --load test/r12.8.ts2 --writehtml r12.8.html
	./tilestacktool --load test/r12.8.ts2 --writevideo r12.8.mp4 1 26

tilestacktool: tilestacktool.cpp utils.cpp png_util.cpp
	g++ -g -I/opt/local/include -Wall $^ -o $@ -L/opt/local/lib -lpng


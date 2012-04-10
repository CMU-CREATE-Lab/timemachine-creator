//



// TODO: 
//    factor out Tilestack from TilestackReader
//    demand paged Tilestack vs single memory block

DONE
Show 16-bit tilestack
--read 16.ts --viz16to8 min max gamma --writehtml 8.html
--read: reads in demand-page mode
--viz16to8:  creates test 8-bit fully populated, then fills
--writehtml: writes 8-bit RGBA pngs

DONE
Visualize 16-bit to 8-bit tilestack
--read 16.ts --viz16to8 [params] --write 8.ts
--read: reads in demand-page mode
--viz16to8:  creates test 8-bit fully populated, then fills
--write:  writes fully-populated 

DONE
Show 8-bit tilestack
--read 8.ts --writehtml 8.html
--read: reads in demand-page mode
--writehtml: writes 8-bit RGBA pngs

DONE
Create video from tilestack
--read 8.ts
--writevideo 8.mp4 [fps] [Fill]

Capture 1K Landsat
Make video
Fill holes
Make video

Capture 1K MODIS
Make video
Fill holes
Make video

fill holes and visualize
--read 16.ts --fillholes --viz16to8 [params] --write 8.ts
--read: reads in demand-page mode
--fillholes:  creates dest 16-bit fully populated, then fills
--viz16to8:  creates test 8-bit fully populated, then fills
--write:  writes fully-populated 

Create video
--makevideo jsonfile quadtree-prefix
Reads 8-bit tilestacks in demand-page mode, as needed
Blits to larger-than-needed image
Bicubic interpolation

Make combined tile for quadtree
--read 0_8.ts --read 1_8.ts --read 2_8.ts --read 3_8.ts --combine --write parent_8.ts
--read: reads in demand-page mode
--combine:  creates 8-bit fully populated, then fills
--write:  writes fully-populated 

--writehtml 8.html

--loadtiles src0.jpg src1.jpg ... srcN.jpg

--image2tiles dest src.{jpg,tif,png}

When creating tilestacks, use whatever automatically-extractable timestamp is available.  Otherwise, use sequence #.
GEE: timestamp from database
Stitcher: timestamp from stitch
- Only works if timestamp is valid.  Might not be.
- Can we read this efficiently?

What happens if we add a new picture:
 at the end of old pictures?  Append.  This just works.
 in the middle of old pictures?  Need to recompile.  That's OK.

ct.rb knows which frames contributed to all tilestacks
So let's maintain timestamps separately from tilestacks, except in case where we 

ct.rb can (a) trust timestamps from tilestacks (e.g. GEE), or (b) know how to independently get timestamps (e.g. stitch)
Future: spare tilestacks that use timestamps as IDs







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


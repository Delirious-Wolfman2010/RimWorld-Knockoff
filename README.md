TEXTURES FOLDER  (keep this "textures" folder next to main.cpp / play.command)

All 73 PNGs here are original art made by make_textures.py (run `python3 make_textures.py` to regenerate them;
needs: pip install pillow numpy scipy). The game draws textures/<name>.png when a file exists and falls back to
built-in shapes when it doesn't, so you can replace any single file with your own art at any time.
Any size works (PNG with transparency; 64x64 recommended), drawn at 32x32 on screen.

TERRAIN:   terrain_water terrain_sand terrain_grass terrain_rock
FLOORS:    floor_wood floor_stone floor_steel
CROPS:     crop_<rice|potato|corn|healroot>_<0..3>   (0 = seedling, 3 = ready to harvest)
OBJECTS:   tree boulder steelvein berrybush wall door campfire stove researchbench smithy machiningtable
           sleepspot bed table stool windturbine solarpanel turret trap horseshoes chesstable
           torchlamp standinglamp battery generator sandbags
           Material variants are checked first: wall_wood wall_stone wall_steel, door_wood door_stone door_steel
PEOPLE:    pawn_body (greyscale-white, tinted with the colonist's shirt colour), pawn_head (untinted),
           pawn_down_body, pawn_down_head, raider, trader
ANIMALS:   deer muffalo boar wolf
WORLD MAP: world_deepwater world_water world_sand world_grass world_forest world_mountain
           world_settlement_0 (Outlander) world_settlement_1 (Tribal) world_settlement_2 (Pirate) world_settlement_destroyed

Everything should be ready to run, just open up the C++ folder, and double click the play.command file. If that doesn't work, try double clicking on the game executable, though that is less stable.
\
\
You should put: \
output.ppm \
perlin_noise.png \
simplex_fractal.png \
in a folder named output, which will be located in your whole game file. 
\
\
the game folder should look like: \
output(folder) \
main.cpp \
stb_image_write.h \
play.command \
game(executable)
\
\
\
to play the game, just run the play.command file. \
thanks for playing

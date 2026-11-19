TEXTURES FOLDER  (put this "textures" folder next to main.cpp / play.command)

The game draws textures/<name>.png if the file exists, otherwise it falls back to its built-in shapes.
You can add textures one at a time. Any size works (PNG with transparency, 64x64 recommended), drawn at 32x32 on screen.

TERRAIN:   terrain_water terrain_sand terrain_grass terrain_rock
FLOORS:    floor_wood floor_stone floor_steel
CROPS:     crop_<rice|potato|corn|healroot>_<0..3>   (0 = seedling, 3 = ready to harvest)
OBJECTS:   tree boulder steelvein berrybush wall door campfire stove researchbench smithy machiningtable
           sleepspot bed table stool windturbine solarpanel turret trap horseshoes chesstable
           torchlamp standinglamp battery generator sandbags
           Material variants are optional and checked first: wall_wood wall_stone wall_steel, door_wood, bed_stone, ...
PEOPLE:    pawn (drawn tinted with the colonist's shirt colour, so make it light/greyscale), pawn_down
           raider trader deer muffalo boar wolf
WORLD MAP: world_deepwater world_water world_sand world_grass world_forest world_mountain
           world_settlement_0 (Outlander) world_settlement_1 (Tribal) world_settlement_2 (Pirate) world_settlement_destroyed
#include "raylib.h"
#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <random>
#include <string>
#include <set>
#include <utility>
#include <queue>
typedef std::vector<int> IntList;
typedef std::vector<float> FloatList;
typedef std::vector<double> DoubleList;
typedef std::vector<DoubleList> DoubleGrid;
typedef std::vector<const char*> StrList;
typedef std::vector<Color> ColorList;
typedef std::set<int> IntSet;
typedef std::pair<int, int> GridCoord;
typedef std::vector<GridCoord> CoordList;
typedef std::priority_queue<GridCoord, CoordList, std::greater<GridCoord>> PathQueue;
class SimplexFBM {
private:
    IntList perm;
    const double F2 = 0.5 * (std::sqrt(3.0) - 1.0);
    const double G2 = (3.0 - std::sqrt(3.0)) / 6.0;
    const DoubleGrid grad2 = {{1,1},{-1,1},{1,-1},{-1,-1},{1,0},{-1,0},{0,1},{0,-1}};
    double dot(int gIndex, double x, double y) const { return grad2[gIndex].at(0) * x + grad2[gIndex].at(1) * y; }
public:
    SimplexFBM(unsigned int seed = 1337) {
        perm.resize(256);
        std::iota(perm.begin(), perm.end(), 0);
        std::default_random_engine engine(seed);
        std::shuffle(perm.begin(), perm.end(), engine);
        perm.insert(perm.end(), perm.begin(), perm.end()); }
    double noise2D(double xin, double yin) const {
        double s = (xin + yin) * F2;
        int i = (int)std::floor(xin + s);
        int j = (int)std::floor(yin + s);
        double t = (i + j) * G2;
        double x0 = xin - (i - t), y0 = yin - (j - t);
        int i1 = (x0 > y0) ? 1 : 0, j1 = (x0 > y0) ? 0 : 1;
        double x1 = x0 - i1 + G2, y1 = y0 - j1 + G2;
        double x2 = x0 - 1.0 + 2.0 * G2, y2 = y0 - 1.0 + 2.0 * G2;
        int ii = i & 255, jj = j & 255;
        int gi0 = perm[ii + perm[jj]] % 8, gi1 = perm[ii + i1 + perm[jj + j1]] % 8, gi2 = perm[ii + 1 + perm[jj + 1]] % 8;
        double t0 = 0.5 - x0 * x0 - y0 * y0;
        double n0 = (t0 < 0) ? 0.0 : (t0 * t0 * t0 * t0 * dot(gi0, x0, y0));
        double t1 = 0.5 - x1 * x1 - y1 * y1;
        double n1 = (t1 < 0) ? 0.0 : (t1 * t1 * t1 * t1 * dot(gi1, x1, y1));
        double t2 = 0.5 - x2 * x2 - y2 * y2;
        double n2 = (t2 < 0) ? 0.0 : (t2 * t2 * t2 * t2 * dot(gi2, x2, y2));
        return 70.0 * (n0 + n1 + n2); }
    double fractal(double x, double y, int octaves = 4) const {
        double total = 0.0, frequency = 1.0, amplitude = 1.0, maxValue = 0.0;
        for (int i = 0; i < octaves; ++i) {
            total += noise2D(x * frequency, y * frequency) * amplitude;
            maxValue += amplitude;
            amplitude *= 0.5;
            frequency *= 2.0; }
        return total / maxValue; }};
unsigned int HashTile(int wx, int wy, unsigned int seed) {
    unsigned int h = seed;
    h ^= (unsigned int)wx * 374761393u;
    h ^= (unsigned int)wy * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16); }
enum class GameMode { WORLD_EXPLORE, COLONY_SIM };
enum class MaterialType { WOOD, STONE, STEEL };
enum class BaseTerrain { WATER, SAND, GRASS, ROCK_GROUND, TILLED_SOIL };
enum class FloorType { NONE, WOOD_FLOOR, STONE_TILE };
enum class ObjectType { NONE, TREE, BOULDER, BERRY_BUSH, WALL, DOOR, STOVE, RESEARCH_BENCH, BED, TABLE, CHAIR, WINDMILL, SOLAR_PANEL, HORSESHOES, CHESS_TABLE };
enum class CropType { NONE, RICE, POTATO, CORN, HEALROOT };
enum class TaskType { HARVEST_PROP, HARVEST_CROP, TILL_SOIL, SOW_CROP, BUILD_FLOOR, BUILD_OBJECT, BUILD_ROOF, BUILD_CONDUIT, DECONSTRUCT_OR_REMOVE, COOK_MEAL, RESEARCH_WORK };
enum class ResearchId { ELECTRICITY = 0, SOLAR_POWER, STONE_CUTTING, COMPLEX_FURNITURE, DEVILSTRAND_CROPS, COUNT };
enum class SkillId { CONSTRUCTION = 0, PLANTS, MINING, COOKING, INTELLECTUAL, ARTISTIC, COUNT };
struct ResearchNode {
    std::string title;
    std::string desc;
    int cost;
    int progress;
    int prereq;
    int uiX;
    int uiY; };
struct LocalCell {
    BaseTerrain terrain = BaseTerrain::GRASS;
    FloorType floor = FloorType::NONE;
    ObjectType obj = ObjectType::NONE;
    MaterialType objMat = MaterialType::WOOD;
    bool hasRoof = false;
    bool hasConduit = false;
    CropType crop = CropType::NONE;
    float cropGrowth = 0.0f; };
struct Job {
    int id;
    TaskType type;
    int tx;
    int ty;
    FloorType targetFloor = FloorType::NONE;
    ObjectType targetObj = ObjectType::NONE;
    MaterialType mat = MaterialType::WOOD;
    CropType targetCrop = CropType::NONE;
    float progress = 0.0f;
    float workRequired = 1.2f;
    int assignedPawn = -1; };
struct Pawn {
    std::string name;
    int x;
    int y;
    Color shirtColor;
    float moveTimer = 0.0f;
    int jobIndex = -1;
    float pathTimer = 0.0f;
    IntSet failedJobs;
    std::string activity = "Idle";
    float hunger = 100.0f;
    float energy = 100.0f;
    float recreation = 85.0f;
    float mood = 80.0f;
    bool isSleeping = false;
    bool wantsSleep = false;
    int targetBedX = -1;
    int targetBedY = -1;
    bool doingRecreation = false;
    int targetRecX = -1;
    int targetRecY = -1;
    float researchAccum = 0.0f;
    IntList skillLevel;
    IntList skillPassion; // 0 = None (35%), 1 = Minor * (100%), 2 = Major ** (150%)
    FloatList skillXP; };
typedef std::vector<Pawn> PawnList;
typedef std::vector<Job> JobList;
typedef std::vector<LocalCell> CellRow;
typedef std::vector<CellRow> CellGrid;
typedef std::vector<ResearchNode> ResearchList;
// RimWorld Skill Helpers: XP per level = (level + 1) * 1000, Passion learn rates = 0.35 / 1.00 / 1.50
float GetSkillWorkSpeed(int level) {
    return 0.50f + (float)std::clamp(level, 0, 20) * 0.0625f; }
void GrantSkillXP(Pawn& p, SkillId sid, float baseXP, std::string& statusOut) {
    int idx = (int)sid;
    if (idx < 0 || idx >= (int)p.skillLevel.size()) return;
    if (p.skillLevel[idx] >= 20) return;
    float passionMult = (p.skillPassion[idx] == 2) ? 1.50f : (p.skillPassion[idx] == 1 ? 1.00f : 0.35f);
    p.skillXP[idx] += baseXP * passionMult;
    float xpNeeded = (float)((p.skillLevel[idx] + 1) * 1000);
    while (p.skillXP[idx] >= xpNeeded && p.skillLevel[idx] < 20) {
        p.skillXP[idx] -= xpNeeded;
        p.skillLevel[idx]++;
        const StrList sNames = {"Construction", "Plants", "Mining", "Cooking", "Intellectual", "Artistic"};
        statusOut = p.name + TextFormat(" leveled up %s to %d!", sNames[idx], p.skillLevel[idx]);
        xpNeeded = (float)((p.skillLevel[idx] + 1) * 1000); }}
void InitRimWorldSkills(Pawn& p) {
    int count = (int)SkillId::COUNT;
    p.skillLevel.assign(count, 0);
    p.skillPassion.assign(count, 0);
    p.skillXP.assign(count, 0.0f);
    for (int i = 0; i < count; ++i) {
        // RimWorld-like bell-curve base generation (sum of 3 rolls -> peaks around 4-9, max 15)
        int roll = GetRandomValue(0, 5) + GetRandomValue(0, 5) + GetRandomValue(0, 5);
        p.skillLevel[i] = std::clamp(roll, 0, 15);
        int pRoll = GetRandomValue(1, 100);
        if (pRoll <= 12) p.skillPassion[i] = 2;      // 12% Major passion (**)
        else if (pRoll <= 38) p.skillPassion[i] = 1; // 26% Minor passion (*)
        else p.skillPassion[i] = 0; }}               // 62% No passion
void DrawWorldTileIcon(int px, int py, int size, double h, unsigned int detailHash, float time) {
    if (h < 0.42) {
        DrawRectangle(px, py, size, size, Color{24, 68, 140, 255});
        int waveOffset = (int)(std::sin(time * 2.5f + px * 0.1f) * 2.0f);
        DrawLine(px + 6 + waveOffset, py + size / 2, px + size - 6 + waveOffset, py + size / 2, Color{65, 130, 210, 255});
    } else if (h < 0.48) {
        DrawRectangle(px, py, size, size, Color{48, 135, 205, 255});
        int waveOffset = (int)(std::cos(time * 3.0f + py * 0.1f) * 2.0f);
        DrawLine(px + 8 + waveOffset, py + size / 2 + 2, px + size - 8 + waveOffset, py + size / 2 + 2, Color{150, 215, 255, 255});
    } else if (h < 0.53) {
        DrawRectangle(px, py, size, size, Color{232, 208, 148, 255});
        if (detailHash % 3 == 0) {
            DrawCircle(px + 10, py + 12, 1.5f, Color{190, 160, 100, 255});
            DrawCircle(px + 20, py + 22, 1.5f, Color{190, 160, 100, 255}); }
    } else if (h < 0.75) {
        DrawRectangle(px, py, size, size, Color{66, 158, 72, 255});
        if (detailHash % 4 == 0 || h > 0.65) {
            DrawRectangle(px + size / 2 - 2, py + size - 8, 4, 6, Color{102, 66, 40, 255});
            DrawTriangle(Vector2{(float)(px + size / 2), (float)(py + 5)}, Vector2{(float)(px + 6), (float)(py + size - 8)}, Vector2{(float)(px + size - 6), (float)(py + size - 8)}, Color{32, 102, 42, 255});
            DrawTriangle(Vector2{(float)(px + size / 2), (float)(py + 3)}, Vector2{(float)(px + 8), (float)(py + size - 13)}, Vector2{(float)(px + size - 8), (float)(py + size - 13)}, Color{44, 130, 54, 255}); }
    } else {
        DrawRectangle(px, py, size, size, Color{105, 110, 118, 255});
        DrawTriangle(Vector2{(float)(px + size / 2), (float)(py + 3)}, Vector2{(float)(px + 3), (float)(py + size - 3)}, Vector2{(float)(px + size - 3), (float)(py + size - 3)}, Color{75, 80, 88, 255});
        DrawTriangle(Vector2{(float)(px + size / 2), (float)(py + 3)}, Vector2{(float)(px + size / 2 - 6), (float)(py + 12)}, Vector2{(float)(px + size / 2 + 6), (float)(py + 12)}, WHITE); }}
void DrawLocalCell(int px, int py, int size, const LocalCell& cell, float time, bool showRoofOverlay) {
    if (cell.terrain == BaseTerrain::WATER) {
        DrawRectangle(px, py, size, size, Color{38, 115, 185, 255});
        int waveOffset = (int)(std::sin(time * 2.5f + px * 0.1f) * 2.0f);
        DrawLine(px + 6 + waveOffset, py + size / 2, px + size - 6 + waveOffset, py + size / 2, Color{120, 195, 245, 255});
    } else if (cell.terrain == BaseTerrain::SAND) {
        DrawRectangle(px, py, size, size, Color{232, 208, 148, 255});
    } else if (cell.terrain == BaseTerrain::GRASS) {
        DrawRectangle(px, py, size, size, Color{72, 165, 78, 255});
DrawPixel(px + 8, py + 10, Color{55, 135, 60, 255});
DrawPixel(px + 22, py + 24, Color{90, 185, 95, 255});
} else if (cell.terrain == BaseTerrain::ROCK_GROUND) {
DrawRectangle(px, py, size, size, Color{125, 130, 138, 255});
} else if (cell.terrain == BaseTerrain::TILLED_SOIL) {
DrawRectangle(px, py, size, size, Color{102, 68, 42, 255});
for (int row = 5; row < size; row += 7) DrawLine(px + 3, py + row, px + size - 3, py + row, Color{76, 48, 28, 255}); }
if (cell.floor == FloorType::WOOD_FLOOR) {
DrawRectangle(px, py, size, size, Color{168, 122, 74, 255});
DrawLine(px, py + size / 2, px + size, py + size / 2, Color{130, 90, 50, 255});
DrawRectangleLines(px, py, size, size, Color{140, 98, 55, 255});
} else if (cell.floor == FloorType::STONE_TILE) {
DrawRectangle(px, py, size, size, Color{165, 170, 178, 255});
DrawRectangleLines(px, py, size, size, Color{120, 125, 132, 255});
DrawLine(px + size / 2, py, px + size / 2, py + size, Color{130, 135, 142, 255});
DrawLine(px, py + size / 2, px + size, py + size / 2, Color{130, 135, 142, 255}); }
if (cell.hasConduit) {
DrawRectangle(px + size / 2 - 2, py, 4, size, Color{45, 150, 235, 200});
DrawRectangle(px, py + size / 2 - 2, size, 4, Color{45, 150, 235, 200});
DrawCircle(px + size / 2, py + size / 2, 3.0f, YELLOW); }
if (cell.crop != CropType::NONE) {
Color cropCol = LIME;
if (cell.crop == CropType::RICE) cropCol = Color{175, 225, 95, 255};
else if (cell.crop == CropType::POTATO) cropCol = Color{95, 185, 75, 255};
else if (cell.crop == CropType::CORN) cropCol = Color{240, 210, 55, 255};
else if (cell.crop == CropType::HEALROOT) cropCol = Color{80, 220, 185, 255};
float r = 3.0f + cell.cropGrowth * 6.5f;
DrawCircle(px + 10, py + 11, r * 0.65f, cropCol);
DrawCircle(px + 22, py + 11, r * 0.65f, cropCol);
DrawCircle(px + 10, py + 22, r * 0.65f, cropCol);
DrawCircle(px + 22, py + 22, r * 0.65f, cropCol);
if (cell.cropGrowth >= 1.0f) DrawCircleLines(px + size / 2, py + size / 2, 11.0f, GOLD); }
Color matCol = (cell.objMat == MaterialType::WOOD) ? Color{145, 95, 48, 255} : Color{130, 136, 145, 255};
Color matDark = (cell.objMat == MaterialType::WOOD) ? Color{85, 50, 20, 255} : Color{70, 74, 82, 255};
if (cell.obj == ObjectType::TREE) {
DrawRectangle(px + size / 2 - 2, py + size - 8, 4, 6, Color{102, 66, 40, 255});
DrawTriangle(Vector2{(float)(px + size / 2), (float)(py + 4)}, Vector2{(float)(px + 5), (float)(py + size - 8)}, Vector2{(float)(px + size - 5), (float)(py + size - 8)}, Color{28, 96, 38, 255});
DrawTriangle(Vector2{(float)(px + size / 2), (float)(py + 2)}, Vector2{(float)(px + 8), (float)(py + size - 14)}, Vector2{(float)(px + size - 8), (float)(py + size - 14)}, Color{44, 135, 54, 255});
} else if (cell.obj == ObjectType::BOULDER) {
DrawCircle(px + size / 2, py + size / 2 + 2, 10.0f, Color{90, 95, 102, 255});
DrawCircle(px + size / 2 - 2, py + size / 2, 7.0f, Color{135, 140, 148, 255});
DrawCircleLines(px + size / 2, py + size / 2 + 2, 10.0f, Color{55, 58, 64, 255});
} else if (cell.obj == ObjectType::BERRY_BUSH) {
DrawCircle(px + size / 2, py + size / 2 + 2, 9.0f, Color{38, 118, 46, 255});
DrawCircle(px + size / 2 - 3, py + size / 2, 2.5f, RED);
DrawCircle(px + size / 2 + 4, py + size / 2 + 2, 2.5f, RED);
DrawCircle(px + size / 2, py + size / 2 + 5, 2.5f, RED);
} else if (cell.obj == ObjectType::WALL) {
DrawRectangle(px + 1, py + 1, size - 2, size - 2, matCol);
DrawRectangleLines(px + 1, py + 1, size - 2, size - 2, matDark);
DrawLine(px + 2, py + size / 2, px + size - 2, py + size / 2, matDark);
} else if (cell.obj == ObjectType::DOOR) {
DrawRectangle(px + 2, py + 10, size - 4, size - 20, matCol);
DrawRectangleLines(px + 2, py + 10, size - 4, size - 20, matDark);
DrawCircle(px + size - 7, py + size / 2, 2.0f, GOLD);
} else if (cell.obj == ObjectType::STOVE) {
DrawRectangle(px + 4, py + 5, size - 8, size - 10, Color{65, 70, 78, 255});
DrawRectangleLines(px + 4, py + 5, size - 8, size - 10, BLACK);
DrawCircle(px + 11, py + 13, 3.0f, ORANGE);
DrawCircle(px + 21, py + 13, 3.0f, RED);
} else if (cell.obj == ObjectType::RESEARCH_BENCH) {
DrawRectangle(px + 3, py + 6, size - 6, size - 12, matCol);
DrawRectangleLines(px + 3, py + 6, size - 6, size - 12, matDark);
DrawRectangle(px + 7, py + 9, 8, 6, SKYBLUE);
DrawCircle(px + 21, py + 16, 3.0f, WHITE);
} else if (cell.obj == ObjectType::BED) {
DrawRectangle(px + 5, py + 3, size - 10, size - 6, matCol);
DrawRectangle(px + 7, py + 5, size - 14, 6, WHITE);
DrawRectangle(px + 7, py + 12, size - 14, size - 17, Color{60, 125, 195, 255});
} else if (cell.obj == ObjectType::TABLE) {
DrawRectangle(px + 4, py + 4, size - 8, size - 8, matCol);
DrawRectangleLines(px + 4, py + 4, size - 8, size - 8, matDark);
} else if (cell.obj == ObjectType::CHAIR) {
DrawRectangle(px + 9, py + 9, size - 18, size - 18, matCol);
DrawRectangleLines(px + 9, py + 9, size - 18, size - 18, matDark);
} else if (cell.obj == ObjectType::WINDMILL) {
DrawCircle(px + size / 2, py + size / 2, 7.0f, LIGHTGRAY);
float ang = time * 3.5f;
int cx = px + size / 2, cy = py + size / 2;
DrawLineEx(Vector2{(float)cx, (float)cy}, Vector2{cx + std::cos(ang) * 14.0f, cy + std::sin(ang) * 14.0f}, 3.0f, WHITE);
DrawLineEx(Vector2{(float)cx, (float)cy}, Vector2{cx + std::cos(ang + 2.09f) * 14.0f, cy + std::sin(ang + 2.09f) * 14.0f}, 3.0f, WHITE);
DrawLineEx(Vector2{(float)cx, (float)cy}, Vector2{cx + std::cos(ang + 4.18f) * 14.0f, cy + std::sin(ang + 4.18f) * 14.0f}, 3.0f, WHITE);
} else if (cell.obj == ObjectType::SOLAR_PANEL) {
DrawRectangle(px + 3, py + 4, size - 6, size - 8, Color{25, 55, 120, 255});
DrawRectangleLines(px + 3, py + 4, size - 6, size - 8, SKYBLUE);
DrawLine(px + size / 2, py + 4, px + size / 2, py + size - 4, SKYBLUE);
DrawLine(px + 3, py + size / 2, px + size - 3, py + size / 2, SKYBLUE);
} else if (cell.obj == ObjectType::HORSESHOES) {
DrawLineEx(Vector2{(float)(px + size / 2), (float)(py + 8)}, Vector2{(float)(px + size / 2), (float)(py + size - 6)}, 3.0f, Color{130, 82, 42, 255});
DrawCircleLines(px + size / 2, py + 12, 5.0f, LIGHTGRAY);
} else if (cell.obj == ObjectType::CHESS_TABLE) {
DrawRectangle(px + 4, py + 4, size - 8, size - 8, matCol);
DrawRectangleLines(px + 4, py + 4, size - 8, size - 8, matDark);
DrawRectangle(px + 9, py + 9, 7, 7, WHITE);
DrawRectangle(px + 16, py + 9, 7, 7, BLACK);
DrawRectangle(px + 9, py + 16, 7, 7, BLACK);
DrawRectangle(px + 16, py + 16, 7, 7, WHITE); }
if (cell.hasRoof && showRoofOverlay) {
DrawRectangle(px, py, size, size, Color{110, 75, 40, 110});
DrawRectangleLines(px, py, size, size, Color{180, 140, 80, 160}); }}
void DrawPawnIcon(int cx, int cy, Color shirtColor, const std::string& name, bool isWorking, bool isSleeping, bool isSelected) {
if (isSelected) DrawCircleLines(cx, cy + 2, 14.0f, GOLD);
if (isSleeping) {
DrawCircle(cx - 3, cy + 2, 8.0f, shirtColor);
DrawCircleLines(cx - 3, cy + 2, 8.0f, BLACK);
DrawCircle(cx + 6, cy + 2, 5.5f, Color{255, 218, 180, 255});
DrawCircleLines(cx + 6, cy + 2, 5.5f, BLACK);
DrawText("Zzz", cx + 4, cy - 14, 12, SKYBLUE);
} else {
DrawCircle(cx, cy + 4, 9.0f, shirtColor);
DrawCircleLines(cx, cy + 4, 9.0f, BLACK);
DrawCircle(cx, cy - 5, 6.0f, Color{255, 218, 180, 255});
DrawCircleLines(cx, cy - 5, 6.0f, BLACK);
if (isWorking) DrawText("*", cx + 8, cy - 12, 16, YELLOW); }
int textW = MeasureText(name.c_str(), 10);
DrawRectangle(cx - textW / 2 - 2, cy + 14, textW + 4, 12, Color{0, 0, 0, 170});
DrawText(name.c_str(), cx - textW / 2, cy + 15, 10, isSelected ? GOLD : WHITE); }
void SyncPawnIndices(PawnList& pawns, const JobList& jobs) {
for (auto& p : pawns) p.jobIndex = -1;
for (size_t j = 0; j < jobs.size(); ++j) {
if (jobs[j].assignedPawn >= 0 && jobs[j].assignedPawn < (int)pawns.size()) pawns[jobs[j].assignedPawn].jobIndex = (int)j; }}
GridCoord FindNextStep(const CellGrid& grid, int startX, int startY, int targetX, int targetY, bool exactTile) {
int rows = (int)grid.size();
int cols = (int)grid.size();
auto isBlocked = [&](int x, int y) {
if (x < 0 || x >= cols || y < 0 || y >= rows) return true;
const LocalCell& c = grid[y][x];
if (c.terrain == BaseTerrain::WATER) return true;
if (c.obj == ObjectType::WALL || c.obj == ObjectType::WINDMILL || c.obj == ObjectType::SOLAR_PANEL) return true;
return false; };
auto getTileCost = [&](int x, int y) {
const LocalCell& c = grid[y][x];
if (c.obj == ObjectType::DOOR) return 5;
if (c.floor != FloorType::NONE) return 7;
if (c.obj == ObjectType::TREE || c.obj == ObjectType::BOULDER) return 18;
return 10; };
IntList dist(rows * cols, 999999);
IntList parent(rows * cols, -1);
PathQueue pq;
int startIdx = startY * cols + startX;
dist[startIdx] = 0;
pq.push(GridCoord{0, startIdx});
int goalIdx = -1;
const IntList dx8 = {0, 0, -1, 1, -1, 1, -1, 1};
const IntList dy8 = {-1, 1, 0, 0, -1, -1, 1, 1};
while (!pq.empty()) {
int d = pq.top().first;
int curr = pq.top().second;
pq.pop();
if (d > dist[curr]) continue;
int cx = curr % cols, cy = curr / cols;
int cheb = std::max(std::abs(cx - targetX), std::abs(cy - targetY));
if ((exactTile && cheb == 0) || (!exactTile && cheb <= 1)) { goalIdx = curr; break; }
for (int i = 0; i < 8; ++i) {
int nx = cx + dx8[i], ny = cy + dy8[i];
if (isBlocked(nx, ny)) continue;
if (i >= 4 && (isBlocked(cx + dx8[i], cy) || isBlocked(cx, cy + dy8[i]))) continue;
int stepCost = getTileCost(nx, ny);
if (i >= 4) stepCost = (stepCost * 14) / 10;
int nextIdx = ny * cols + nx;
if (dist[curr] + stepCost < dist[nextIdx]) {
dist[nextIdx] = dist[curr] + stepCost;
parent[nextIdx] = curr;
pq.push(GridCoord{dist[nextIdx], nextIdx}); }}}
if (goalIdx == -1) return GridCoord{-1, -1};
int trace = goalIdx;
while (parent[trace] != -1 && parent[trace] != startIdx) trace = parent[trace];
return GridCoord{trace % cols, trace / cols}; }
int main() {
const int tileSize = 32, viewCols = 50, viewRows = 29, scrollMargin = 8, topBarHeight = 72, bottomBarHeight = 96;
const int screenWidth = viewCols * tileSize, screenHeight = viewRows * tileSize + topBarHeight;
InitWindow(screenWidth, screenHeight, "RimWorld-Style Colony & World Sim");
SetWindowPosition(40, 25);
SetTargetFPS(60);
std::random_device rd;
std::mt19937 seedRng(rd());
std::uniform_int_distribution seedDist(1000, 999999);
unsigned int seed = seedDist(seedRng);
SimplexFBM gen(seed);
GameMode mode = GameMode::WORLD_EXPLORE;
std::string statusMessage = "Explore World Map with WASD/Arrows. Press ENTER on land to ZOOM IN and settle!";
auto getWorldElevation = [&](int wx, int wy) { return (gen.fractal(wx * 0.09, wy * 0.09, 4) + 1.0) * 0.5; };
auto isWorldWalkable = [&](int wx, int wy) { double h = getWorldElevation(wx, wy); return (h >= 0.42 && h < 0.75); };
int playerX = 0, playerY = 0;
bool foundSpawn = false;
for (int radius = 0; radius < 200 && !foundSpawn; ++radius) {
for (int dy = -radius; dy <= radius && !foundSpawn; ++dy) {
for (int dx = -radius; dx <= radius && !foundSpawn; ++dx) {
double h = getWorldElevation(dx, dy);
if (h >= 0.48 && h < 0.75) { playerX = dx; playerY = dy; foundSpawn = true; }}}}
int camX = playerX - viewCols / 2, camY = playerY - viewRows / 2;
float moveCooldown = 0.0f;
const int localCols = 90, localRows = 70;
CellGrid localMap(localRows, CellRow(localCols));
int settledWorldX = 0, settledWorldY = 0;
std::string settledBiomeName = "Temperate Forest";
int wood = 80, stone = 45, rawFood = 40, cookedFood = 15;
int powerGenerated = 0, powerConsumed = 0, nextJobId = 1;
int gameDay = 1;
float gameHour = 8.0f;
float windOutputPerMill = 145.0f;
float windChangeTimer = 0.0f;
bool showRoofOverlay = true;
int selectedPawnIdx = -1;
ResearchList researchTree = {
{"Electricity", "Unlocks Windmills, Stove & Power Conduits", 60, 0, -1, 60, 120},
{"Solar Power", "Unlocks Solar Panels (100W 06:30-18:00)", 90, 0, 0, 360, 120},
{"Stonecutting", "Unlocks Stone Walls & Stone Tile Floors", 50, 0, -1, 60, 260},
{"Complex Furniture", "Unlocks Beds, Tables, Chairs & Chess", 70, 0, 2, 360, 260},
{"Devilstrand", "Unlocks medicinal Healroot farming", 80, 0, -1, 660, 190}};
int activeResearchIdx = 0;
bool researchScreenOpen = false;
PawnList pawns;
JobList jobs;
int mainTab = 0, buildSubTab = 0, subToolIdx = 0;
MaterialType selectedMat = MaterialType::WOOD;
auto isResearchDone = [&](int idx) { return (idx < 0 || idx >= (int)researchTree.size()) ? true : (researchTree[idx].progress >= researchTree[idx].cost); };
auto isLocalWalkable = [&](int lx, int ly) {
if (lx < 0 || lx >= localCols || ly < 0 || ly >= localRows) return false;
const LocalCell& c = localMap[ly][lx];
if (c.terrain == BaseTerrain::WATER) return false;
if (c.obj == ObjectType::WALL || c.obj == ObjectType::WINDMILL || c.obj == ObjectType::SOLAR_PANEL) return false;
return true; };
auto hasJobAt = [&](int lx, int ly) {
for (const auto& j : jobs) if (j.tx == lx && j.ty == ly) return true;
return false; };
auto rollWindOutput = [&](int dummy) {
(void)dummy;
int r = GetRandomValue(1, 100);
if (r <= 4) return 0.0f;
if (r <= 14) return (float)GetRandomValue(20, 69);
if (r <= 88) return (float)GetRandomValue(70, 200);
return (float)GetRandomValue(201, 250); };
auto enterZoomedColony = [&](int wx, int wy) {
mode = GameMode::COLONY_SIM;
settledWorldX = wx; settledWorldY = wy;
wood = 80; stone = 45; rawFood = 40; cookedFood = 15;
gameDay = 1; gameHour = 8.0f;
windOutputPerMill = rollWindOutput(0);
windChangeTimer = 0.0f;
selectedPawnIdx = -1;
jobs.clear(); pawns.clear();
mainTab = 0; buildSubTab = 0; subToolIdx = 0;
researchScreenOpen = false;
double worldElev = getWorldElevation(wx, wy);
unsigned int sectorSeed = HashTile(wx, wy, seed + 5555);
SimplexFBM localNoise(sectorSeed);
settledBiomeName = (worldElev < 0.53) ? "Coastal Beach" : (worldElev > 0.66 ? "Mountain Foothills" : "Temperate Forest");
for (int y = 0; y < localRows; ++y) {
for (int x = 0; x < localCols; ++x) {
LocalCell cell;
double n = (localNoise.fractal(x * 0.08, y * 0.08, 3) + 1.0) * 0.5;
unsigned int h = HashTile(x, y, sectorSeed);
double adjusted = n * 0.65 + worldElev * 0.35;
if (adjusted < 0.36) cell.terrain = BaseTerrain::WATER;
else if (adjusted < 0.43) cell.terrain = BaseTerrain::SAND;
else if (adjusted > 0.72) { cell.terrain = BaseTerrain::ROCK_GROUND; if (h % 4 == 0) cell.obj = ObjectType::BOULDER; }
else {
cell.terrain = BaseTerrain::GRASS;
int roll = h % 100;
int treeChance = (worldElev >= 0.53 && worldElev <= 0.66) ? 18 : 8;
if (roll < treeChance) cell.obj = ObjectType::TREE;
else if (roll < treeChance + 4) cell.obj = ObjectType::BOULDER;
else if (roll < treeChance + 7) cell.obj = ObjectType::BERRY_BUSH; }
localMap[y][x] = cell; }}
int centerX = localCols / 2, centerY = localRows / 2;
for (int dy = -3; dy <= 3; ++dy) {
for (int dx = -3; dx <= 3; ++dx) {
int lx = centerX + dx, ly = centerY + dy;
if (lx >= 0 && lx < localCols && ly >= 0 && ly < localRows) {
if (localMap[ly][lx].terrain == BaseTerrain::WATER) localMap[ly][lx].terrain = BaseTerrain::GRASS;
localMap[ly][lx].obj = ObjectType::NONE; }}}
const StrList names = {"Alice", "Bob", "Maya"};
const ColorList colors = {Color{210, 55, 55, 255}, Color{55, 130, 220, 255}, Color{220, 165, 45, 255}};
for (int i = 0; i < 3; ++i) {
Pawn p;
p.name = names[i]; p.x = centerX + (i - 1); p.y = centerY; p.shirtColor = colors[i];
InitRimWorldSkills(p);
pawns.push_back(p); }
int visibleMapRows = (screenHeight - topBarHeight - bottomBarHeight) / tileSize;
camX = centerX - viewCols / 2;
camY = centerY - visibleMapRows / 2;
statusMessage = "Colony founded! Click any colonist to view their Needs & RimWorld Skills."; };
while (!WindowShouldClose()) {
float dt = GetFrameTime();
float time = (float)GetTime();
if (moveCooldown > 0.0f) moveCooldown -= dt;
bool allSleeping = false;
if (mode == GameMode::COLONY_SIM && !pawns.empty()) {
allSleeping = true;
for (const auto& p : pawns) {
if (!p.isSleeping) { allSleeping = false; break; }}}
float hoursPerRealSec = allSleeping ? 0.25f : 0.025f;
float gameHoursElapsed = dt * hoursPerRealSec;
float gameMinutesElapsed = gameHoursElapsed * 60.0f;
gameHour += gameHoursElapsed;
if (gameHour >= 24.0f) { gameHour -= 24.0f; gameDay++; }
if (mode == GameMode::WORLD_EXPLORE) {
bool tapUp = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP), tapDown = IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN);
bool tapLeft = IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT), tapRight = IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT);
bool holdUp = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP), holdDown = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
bool holdLeft = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT), holdRight = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
int nextX = playerX, nextY = playerY;
bool shouldMove = false;
if (tapUp || tapDown || tapLeft || tapRight) {
moveCooldown = 0.22f; shouldMove = true;
if (tapUp) nextY--; else if (tapDown) nextY++; else if (tapLeft) nextX--; else if (tapRight) nextX++;
} else if (moveCooldown <= 0.0f && (holdUp || holdDown || holdLeft || holdRight)) {
moveCooldown = 0.10f; shouldMove = true;
if (holdUp) nextY--; else if (holdDown) nextY++; else if (holdLeft) nextX--; else if (holdRight) nextX++; }
if (shouldMove) {
if (!isWorldWalkable(nextX, nextY)) statusMessage = "Blocked! Deep ocean and steep mountains cannot be settled.";
else {
playerX = nextX; playerY = nextY;
if (playerX - camX < scrollMargin) camX = playerX - scrollMargin;
else if (playerX - camX >= viewCols - scrollMargin) camX = playerX - (viewCols - scrollMargin - 1);
if (playerY - camY < scrollMargin) camY = playerY - scrollMargin;
else if (playerY - camY >= viewRows - scrollMargin) camY = playerY - (viewRows - scrollMargin - 1);
double h = getWorldElevation(playerX, playerY);
const char* biome = (h < 0.53) ? "Coastal Beach" : (h > 0.66 ? "Mountain Foothills" : "Temperate Forest");
statusMessage = std::string("Biome: ") + biome + " | Press ENTER to zoom in and settle here!"; }}
if (IsKeyPressed(KEY_ENTER)) enterZoomedColony(playerX, playerY);
} else if (mode == GameMode::COLONY_SIM) {
int visibleMapRows = (screenHeight - topBarHeight - bottomBarHeight) / tileSize;
windChangeTimer += gameHoursElapsed;
if (windChangeTimer >= 2.5f) {
windChangeTimer = 0.0f;
windOutputPerMill = rollWindOutput(0); }
float solarOutputPerPanel = 0.0f;
if (gameHour >= 5.0f && gameHour < 6.5f) solarOutputPerPanel = 100.0f * ((gameHour - 5.0f) / 1.5f);
else if (gameHour >= 6.5f && gameHour <= 18.0f) solarOutputPerPanel = 100.0f;
else if (gameHour > 18.0f && gameHour <= 19.0f) solarOutputPerPanel = 100.0f * (1.0f - (gameHour - 18.0f));
int genPower = 0, usePower = 0;
bool isDaytime = (gameHour >= 6.0f && gameHour <= 19.0f);
for (int y = 0; y < localRows; ++y) {
for (int x = 0; x < localCols; ++x) {
LocalCell& c = localMap[y][x];
if (c.crop != CropType::NONE && c.cropGrowth < 1.0f) {
float rate = isDaytime ? 0.06f : 0.01f;
if (c.crop == CropType::RICE) rate *= 1.35f;
else if (c.crop == CropType::CORN) rate *= 0.75f;
else if (c.crop == CropType::HEALROOT) rate *= 0.65f;
c.cropGrowth = std::min(1.0f, c.cropGrowth + rate * gameHoursElapsed); }
if (c.obj == ObjectType::WINDMILL) genPower += (int)std::round(windOutputPerMill);
if (c.obj == ObjectType::SOLAR_PANEL) genPower += (int)std::round(solarOutputPerPanel);
if (c.obj == ObjectType::STOVE) usePower += 200;
if (c.obj == ObjectType::RESEARCH_BENCH) usePower += 150; }}
powerGenerated = genPower; powerConsumed = usePower;
bool hasPower = (powerGenerated >= powerConsumed && powerGenerated > 0);
if (hasPower && rawFood >= 5 && cookedFood < 25) {
for (int y = 0; y < localRows; ++y) {
for (int x = 0; x < localCols; ++x) {
if (localMap[y][x].obj == ObjectType::STOVE && !hasJobAt(x, y)) {
jobs.push_back(Job{nextJobId++, TaskType::COOK_MEAL, x, y, FloorType::NONE, ObjectType::NONE, MaterialType::WOOD, CropType::NONE, 0.0f, 1.5f, -1}); }}}}
if (!researchScreenOpen && moveCooldown <= 0.0f) {
bool movedCam = false;
if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) { camY--; movedCam = true; }
if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) { camY++; movedCam = true; }
if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) { camX--; movedCam = true; }
if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) { camX++; movedCam = true; }
if (movedCam) {
camX = std::clamp(camX, 0, std::max(0, localCols - viewCols));
camY = std::clamp(camY, 0, std::max(0, localRows - visibleMapRows));
moveCooldown = 0.06f; }}
if (IsKeyPressed(KEY_R)) showRoofOverlay = !showRoofOverlay;
if (IsKeyPressed(KEY_ESCAPE)) {
if (researchScreenOpen) researchScreenOpen = false;
else if (selectedPawnIdx != -1) selectedPawnIdx = -1;
else {
mode = GameMode::WORLD_EXPLORE;
camX = playerX - viewCols / 2; camY = playerY - viewRows / 2;
statusMessage = "Zoomed back out to World Map. Press ENTER on any land tile to settle."; }}
Vector2 mouse = GetMousePosition();
int mapBottomY = screenHeight - bottomBarHeight;
if (researchScreenOpen) {
if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
if (mouse.x >= 1340 && mouse.x <= 1420 && mouse.y >= 110 && mouse.y <= 145) researchScreenOpen = false;
for (int i = 0; i < (int)researchTree.size(); ++i) {
ResearchNode& node = researchTree[i];
int bx = 180 + node.uiX, by = 130 + node.uiY;
if (mouse.x >= bx && mouse.x <= bx + 250 && mouse.y >= by && mouse.y <= by + 95) {
bool prereqMet = (node.prereq == -1) || isResearchDone(node.prereq);
if (prereqMet && !isResearchDone(i)) { activeResearchIdx = i; statusMessage = "Active Research set to: " + node.title; }
else if (!prereqMet) statusMessage = "Prerequisite research required first!"; }}}
} else if (selectedPawnIdx != -1 && mouse.x >= 12 && mouse.x <= 362 && mouse.y >= mapBottomY - 246 && mouse.y <= mapBottomY - 10) {
if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
int px = 12, py = mapBottomY - 246;
if (mouse.x >= px + 322 && mouse.x <= px + 342 && mouse.y >= py + 8 && mouse.y <= py + 28) selectedPawnIdx = -1; }
} else if (mouse.y >= mapBottomY && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
int mainRowY = mapBottomY + 52, subRowY = mapBottomY + 4;
if (mouse.y >= mainRowY) {
int tabW = screenWidth / 4;
int clickedMain = (int)mouse.x / tabW;
if (clickedMain == 3) researchScreenOpen = !researchScreenOpen;
else if (clickedMain >= 0 && clickedMain <= 2) { mainTab = clickedMain; subToolIdx = 0; researchScreenOpen = false; }
} else if (mouse.y >= subRowY) {
if (mainTab == 0) {
int idx = (int)(mouse.x - 10) / 208;
if (idx >= 0 && idx <= 1) subToolIdx = idx;
} else if (mainTab == 1) {
if (mouse.x >= 10 && mouse.x < 126) { buildSubTab = 0; subToolIdx = 0; }
else if (mouse.x >= 130 && mouse.x < 246) { buildSubTab = 1; subToolIdx = 0; }
else if (mouse.x >= 250 && mouse.x < 366) { buildSubTab = 2; subToolIdx = 0; }
else if (mouse.x >= 370 && mouse.x < 486) { buildSubTab = 3; subToolIdx = 0; }
else if (mouse.x >= 500) {
int clickedItem = (int)(mouse.x - 500) / 188;
int maxItems = (buildSubTab == 0) ? 4 : (buildSubTab == 1 ? 5 : (buildSubTab == 2 ? 3 : 2));
if (clickedItem >= 0 && clickedItem < maxItems) {
bool switchable = (buildSubTab == 0 && clickedItem < 3) || (buildSubTab == 1 && clickedItem >= 1 && clickedItem <= 4) || (buildSubTab == 3 && clickedItem == 1);
if (subToolIdx == clickedItem && switchable) {
if (selectedMat == MaterialType::WOOD) {
if (isResearchDone((int)ResearchId::STONE_CUTTING)) { selectedMat = MaterialType::STONE; statusMessage = "Switched material to STONE."; }
else statusMessage = "Research 'Stonecutting' first to build with Stone!";
} else { selectedMat = MaterialType::WOOD; statusMessage = "Switched material to WOOD."; }
} else subToolIdx = clickedItem; }}
} else if (mainTab == 2) {
int idx = (int)(mouse.x - 10) / 198;
if (idx >= 0 && idx <= 4) {
if (idx == 4 && !isResearchDone((int)ResearchId::DEVILSTRAND_CROPS)) statusMessage = "Research 'Devilstrand' first to plant Healroot!";
else subToolIdx = idx; }}}
} else if (!researchScreenOpen && mouse.y > topBarHeight && mouse.y < mapBottomY) {
int vx = (int)mouse.x / tileSize, vy = (int)(mouse.y - topBarHeight) / tileSize;
int lx = camX + vx, ly = camY + vy;
bool clickedPawn = false;
if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
for (size_t i = 0; i < pawns.size(); ++i) {
if (pawns[i].x == lx && pawns[i].y == ly) {
selectedPawnIdx = (int)i;
clickedPawn = true;
break; }}}
if (!clickedPawn && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && lx >= 0 && lx < localCols && ly >= 0 && ly < localRows) {
LocalCell& cell = localMap[ly][lx];
if (mainTab == 0) {
if (subToolIdx == 0 && !hasJobAt(lx, ly)) {
if (cell.crop != CropType::NONE && cell.cropGrowth >= 1.0f) jobs.push_back(Job{nextJobId++, TaskType::HARVEST_CROP, lx, ly, FloorType::NONE, ObjectType::NONE, MaterialType::WOOD, CropType::NONE, 0.0f, 1.0f, -1});
else if (cell.obj == ObjectType::TREE || cell.obj == ObjectType::BOULDER || cell.obj == ObjectType::BERRY_BUSH) jobs.push_back(Job{nextJobId++, TaskType::HARVEST_PROP, lx, ly, FloorType::NONE, ObjectType::NONE, MaterialType::WOOD, CropType::NONE, 0.0f, 1.2f, -1});
} else if (subToolIdx == 1) {
bool removedJob = false;
for (size_t i = 0; i < jobs.size(); ++i) {
if (jobs[i].tx == lx && jobs[i].ty == ly) {
if (jobs[i].type == TaskType::BUILD_FLOOR || jobs[i].type == TaskType::BUILD_OBJECT || jobs[i].type == TaskType::BUILD_CONDUIT) {
if (jobs[i].mat == MaterialType::WOOD) wood += 5; else stone += 5; }
jobs.erase(jobs.begin() + i);
SyncPawnIndices(pawns, jobs);
removedJob = true; break; }}
if (!removedJob && !hasJobAt(lx, ly)) {
if (cell.obj != ObjectType::NONE && cell.obj != ObjectType::TREE && cell.obj != ObjectType::BOULDER && cell.obj != ObjectType::BERRY_BUSH) jobs.push_back(Job{nextJobId++, TaskType::DECONSTRUCT_OR_REMOVE, lx, ly, FloorType::NONE, ObjectType::NONE, MaterialType::WOOD, CropType::NONE, 0.0f, 1.0f, -1});
else if (cell.hasRoof || cell.hasConduit || cell.floor != FloorType::NONE) jobs.push_back(Job{nextJobId++, TaskType::DECONSTRUCT_OR_REMOVE, lx, ly, FloorType::NONE, ObjectType::NONE, MaterialType::WOOD, CropType::NONE, 0.0f, 0.8f, -1}); }}
} else if (mainTab == 1 && !hasJobAt(lx, ly) && cell.terrain != BaseTerrain::WATER) {
bool pawnHere = false;
for (const auto& p : pawns) if (p.x == lx && p.y == ly) pawnHere = true;
if (buildSubTab == 0) {
if (subToolIdx == 0) {
FloorType ft = (selectedMat == MaterialType::WOOD) ? FloorType::WOOD_FLOOR : FloorType::STONE_TILE;
if (cell.floor != ft) {
if (selectedMat == MaterialType::WOOD && wood >= 2) { wood -= 2; jobs.push_back(Job{nextJobId++, TaskType::BUILD_FLOOR, lx, ly, ft, ObjectType::NONE, MaterialType::WOOD, CropType::NONE, 0.0f, 0.8f, -1}); }
else if (selectedMat == MaterialType::STONE && stone >= 2) { stone -= 2; jobs.push_back(Job{nextJobId++, TaskType::BUILD_FLOOR, lx, ly, ft, ObjectType::NONE, MaterialType::STONE, CropType::NONE, 0.0f, 1.0f, -1}); }
else statusMessage = "Need 2 Wood or Stone to lay flooring!"; }
} else if (subToolIdx == 1 && !pawnHere && cell.obj == ObjectType::NONE) {
if (selectedMat == MaterialType::WOOD && wood >= 5) { wood -= 5; jobs.push_back(Job{nextJobId++, TaskType::BUILD_OBJECT, lx, ly, FloorType::NONE, ObjectType::WALL, MaterialType::WOOD, CropType::NONE, 0.0f, 1.3f, -1}); }
else if (selectedMat == MaterialType::STONE && stone >= 5) { stone -= 5; jobs.push_back(Job{nextJobId++, TaskType::BUILD_OBJECT, lx, ly, FloorType::NONE, ObjectType::WALL, MaterialType::STONE, CropType::NONE, 0.0f, 1.8f, -1}); }
else statusMessage = "Need 5 Wood or Stone for a Wall!";
} else if (subToolIdx == 2 && cell.obj == ObjectType::NONE) {
if (selectedMat == MaterialType::WOOD && wood >= 5) { wood -= 5; jobs.push_back(Job{nextJobId++, TaskType::BUILD_OBJECT, lx, ly, FloorType::NONE, ObjectType::DOOR, MaterialType::WOOD, CropType::NONE, 0.0f, 1.2f, -1}); }
else if (selectedMat == MaterialType::STONE && stone >= 5) { stone -= 5; jobs.push_back(Job{nextJobId++, TaskType::BUILD_OBJECT, lx, ly, FloorType::NONE, ObjectType::DOOR, MaterialType::STONE, CropType::NONE, 0.0f, 1.6f, -1}); }
else statusMessage = "Need 5 Wood or Stone for a Door!";
} else if (subToolIdx == 3 && !cell.hasRoof) {
jobs.push_back(Job{nextJobId++, TaskType::BUILD_ROOF, lx, ly, FloorType::NONE, ObjectType::NONE, MaterialType::WOOD, CropType::NONE, 0.0f, 0.7f, -1}); }
} else if (buildSubTab == 1 && cell.obj == ObjectType::NONE) {
if (subToolIdx == 0) {
if (!isResearchDone((int)ResearchId::ELECTRICITY)) statusMessage = "Research 'Electricity' first to build an Electric Stove!";
else if (stone >= 15) { stone -= 15; jobs.push_back(Job{nextJobId++, TaskType::BUILD_OBJECT, lx, ly, FloorType::NONE, ObjectType::STOVE, MaterialType::STONE, CropType::NONE, 0.0f, 2.0f, -1}); }
else statusMessage = "Need 15 Stone for a Stove!";
} else if (subToolIdx == 1) {
if (wood >= 15) { wood -= 15; jobs.push_back(Job{nextJobId++, TaskType::BUILD_OBJECT, lx, ly, FloorType::NONE, ObjectType::RESEARCH_BENCH, selectedMat, CropType::NONE, 0.0f, 2.0f, -1}); }
else statusMessage = "Need 15 Wood for a Research Bench!";
} else if (subToolIdx >= 2 && subToolIdx <= 4) {
if (!isResearchDone((int)ResearchId::COMPLEX_FURNITURE)) statusMessage = "Research 'Complex Furniture' first to build Beds, Tables & Chairs!";
else {
ObjectType fObj = (subToolIdx == 2) ? ObjectType::BED : (subToolIdx == 3 ? ObjectType::TABLE : ObjectType::CHAIR);
int cost = (subToolIdx == 2) ? 10 : (subToolIdx == 3 ? 8 : 5);
if (selectedMat == MaterialType::WOOD && wood >= cost) { wood -= cost; jobs.push_back(Job{nextJobId++, TaskType::BUILD_OBJECT, lx, ly, FloorType::NONE, fObj, MaterialType::WOOD, CropType::NONE, 0.0f, 1.4f, -1}); }
else if (selectedMat == MaterialType::STONE && stone >= cost) { stone -= cost; jobs.push_back(Job{nextJobId++, TaskType::BUILD_OBJECT, lx, ly, FloorType::NONE, fObj, MaterialType::STONE, CropType::NONE, 0.0f, 1.8f, -1}); }
else statusMessage = TextFormat("Need %d Wood or Stone!", cost); }}
} else if (buildSubTab == 2) {
if (!isResearchDone((int)ResearchId::ELECTRICITY)) statusMessage = "Research 'Electricity' first to build Power infrastructure!";
else if (subToolIdx == 0 && !cell.hasConduit) {
if (wood >= 1) { wood -= 1; jobs.push_back(Job{nextJobId++, TaskType::BUILD_CONDUIT, lx, ly, FloorType::NONE, ObjectType::NONE, MaterialType::WOOD, CropType::NONE, 0.0f, 0.5f, -1}); }
else statusMessage = "Need 1 Wood per Power Conduit!";
} else if (subToolIdx == 1 && !pawnHere && cell.obj == ObjectType::NONE) {
if (wood >= 20 && stone >= 10) { wood -= 20; stone -= 10; jobs.push_back(Job{nextJobId++, TaskType::BUILD_OBJECT, lx, ly, FloorType::NONE, ObjectType::WINDMILL, MaterialType::STONE, CropType::NONE, 0.0f, 2.5f, -1}); }
else statusMessage = "Windmill costs 20 Wood + 10 Stone!";
} else if (subToolIdx == 2 && !pawnHere && cell.obj == ObjectType::NONE) {
if (!isResearchDone((int)ResearchId::SOLAR_POWER)) statusMessage = "Research 'Solar Power' first!";
else if (stone >= 25) { stone -= 25; jobs.push_back(Job{nextJobId++, TaskType::BUILD_OBJECT, lx, ly, FloorType::NONE, ObjectType::SOLAR_PANEL, MaterialType::STONE, CropType::NONE, 0.0f, 2.5f, -1}); }
else statusMessage = "Solar Panel costs 25 Stone!"; }
} else if (buildSubTab == 3 && cell.obj == ObjectType::NONE) {
if (subToolIdx == 0) {
if (wood >= 10) { wood -= 10; jobs.push_back(Job{nextJobId++, TaskType::BUILD_OBJECT, lx, ly, FloorType::NONE, ObjectType::HORSESHOES, MaterialType::WOOD, CropType::NONE, 0.0f, 1.0f, -1}); }
else statusMessage = "Need 10 Wood to build a Horseshoes Pin!";
} else if (subToolIdx == 1) {
if (!isResearchDone((int)ResearchId::COMPLEX_FURNITURE)) statusMessage = "Research 'Complex Furniture' first to build a Chess Table!";
else if (selectedMat == MaterialType::WOOD && wood >= 15) { wood -= 15; jobs.push_back(Job{nextJobId++, TaskType::BUILD_OBJECT, lx, ly, FloorType::NONE, ObjectType::CHESS_TABLE, MaterialType::WOOD, CropType::NONE, 0.0f, 1.5f, -1}); }
else if (selectedMat == MaterialType::STONE && stone >= 15) { stone -= 15; jobs.push_back(Job{nextJobId++, TaskType::BUILD_OBJECT, lx, ly, FloorType::NONE, ObjectType::CHESS_TABLE, MaterialType::STONE, CropType::NONE, 0.0f, 1.8f, -1}); }
else statusMessage = "Need 15 Wood or Stone to build a Chess Table!"; }}
} else if (mainTab == 2 && !hasJobAt(lx, ly)) {
if (subToolIdx == 0) {
if (cell.terrain == BaseTerrain::GRASS && cell.floor == FloorType::NONE && cell.obj == ObjectType::NONE) jobs.push_back(Job{nextJobId++, TaskType::TILL_SOIL, lx, ly, FloorType::NONE, ObjectType::NONE, MaterialType::WOOD, CropType::NONE, 0.0f, 1.0f, -1});
} else if (subToolIdx >= 1 && subToolIdx <= 4) {
if (cell.terrain == BaseTerrain::TILLED_SOIL && cell.crop == CropType::NONE && cell.obj == ObjectType::NONE) {
CropType ct = (subToolIdx == 1) ? CropType::RICE : (subToolIdx == 2 ? CropType::POTATO : (subToolIdx == 3 ? CropType::CORN : CropType::HEALROOT));
jobs.push_back(Job{nextJobId++, TaskType::SOW_CROP, lx, ly, FloorType::NONE, ObjectType::NONE, MaterialType::WOOD, ct, 0.0f, 0.9f, -1});
} else if (cell.terrain != BaseTerrain::TILLED_SOIL) statusMessage = "Till the grass first using 'Till Soil' before planting crops!"; }}}}
CoordList benches;
CoordList recObjects;
for (int y = 0; y < localRows; ++y) {
for (int x = 0; x < localCols; ++x) {
if (localMap[y][x].obj == ObjectType::RESEARCH_BENCH) benches.push_back(GridCoord{x, y});
if (localMap[y][x].obj == ObjectType::HORSESHOES || localMap[y][x].obj == ObjectType::CHESS_TABLE) recObjects.push_back(GridCoord{x, y}); }}
bool hasHigherPriorityJob = false;
for (const auto& j : jobs) if (j.type != TaskType::RESEARCH_WORK) { hasHigherPriorityJob = true; break; }
for (size_t pIdx = 0; pIdx < pawns.size(); ++pIdx) {
Pawn& pawn = pawns[pIdx];
pawn.hunger -= gameHoursElapsed * (100.0f / 24.0f);
if (pawn.hunger <= 80.0f) {
if (cookedFood > 0) {
cookedFood -= 1;
pawn.hunger = std::min(100.0f, pawn.hunger + 20.0f);
statusMessage = pawn.name + " ate a cooked meal (-1 Meal).";
} else if (rawFood > 0) {
int bite = std::min(rawFood, 10);
rawFood -= bite;
pawn.hunger = std::min(100.0f, pawn.hunger + bite * 2.0f);
statusMessage = pawn.name + TextFormat(" ate %d raw food (low nutrition).", bite); }
} else if (pawn.hunger < 0.0f) pawn.hunger = 0.0f;
if (!pawn.doingRecreation) pawn.recreation = std::max(0.0f, pawn.recreation - gameHoursElapsed * 3.0f);
pawn.mood = std::clamp(35.0f + pawn.hunger * 0.22f + pawn.energy * 0.22f + pawn.recreation * 0.21f, 0.0f, 100.0f);
bool isNight = (gameHour >= 21.0f || gameHour < 6.0f);
if (pawn.isSleeping) {
pawn.doingRecreation = false;
bool inBed = (localMap[pawn.y][pawn.x].obj == ObjectType::BED);
float recoveryRate = inBed ? 15.0f : 10.5f;
pawn.energy = std::min(100.0f, pawn.energy + gameHoursElapsed * recoveryRate);
pawn.activity = "Sleeping";
if (gameHour >= 6.0f && gameHour < 7.0f) {
pawn.energy = std::max(pawn.energy, 90.0f);
pawn.isSleeping = false;
pawn.wantsSleep = false;
pawn.targetBedX = -1;
pawn.targetBedY = -1;
pawn.activity = "Idle"; }
continue;
} else {
pawn.energy -= gameHoursElapsed * (100.0f / 16.0f);
if (pawn.energy < 0.0f) pawn.energy = 0.0f;
if ((isNight && pawn.energy < 70.0f) || pawn.energy <= 15.0f) pawn.wantsSleep = true; }
if (pawn.wantsSleep) {
pawn.doingRecreation = false;
if (pawn.jobIndex != -1) {
int jIdx = pawn.jobIndex;
if (jIdx >= 0 && jIdx < (int)jobs.size()) jobs[jIdx].assignedPawn = -1;
pawn.jobIndex = -1; }
if (pawn.targetBedX == -1) {
int bestBedDist = 999999;
for (int y = 0; y < localRows; ++y) {
for (int x = 0; x < localCols; ++x) {
if (localMap[y][x].obj == ObjectType::BED) {
bool bedClaimed = false;
for (size_t k = 0; k < pawns.size(); ++k)
if (k != pIdx && ((pawns[k].x == x && pawns[k].y == y) || (pawns[k].targetBedX == x && pawns[k].targetBedY == y))) bedClaimed = true;
if (!bedClaimed) {
int d = std::abs(x - pawn.x) + std::abs(y - pawn.y);
if (d < bestBedDist) { bestBedDist = d; pawn.targetBedX = x; pawn.targetBedY = y; }}}}}}
if (pawn.targetBedX == -1 || (pawn.x == pawn.targetBedX && pawn.y == pawn.targetBedY)) {
pawn.isSleeping = true;
pawn.activity = "Sleeping"; }
continue; }
// Cancel recreation immediately if a higher-priority work order arrives
if (pawn.doingRecreation && hasHigherPriorityJob) {
pawn.doingRecreation = false;
pawn.targetRecX = -1; pawn.targetRecY = -1; }
if (pawn.jobIndex == -1 && !pawn.doingRecreation) {
int bestJob = -1, bestDist = 999999;
for (size_t jIdx = 0; jIdx < jobs.size(); ++jIdx) {
if (jobs[jIdx].assignedPawn == -1 && !pawn.failedJobs.count(jobs[jIdx].id)) {
int dist = std::abs(jobs[jIdx].tx - pawn.x) + std::abs(jobs[jIdx].ty - pawn.y);
if (dist < bestDist) { bestDist = dist; bestJob = (int)jIdx; }}}
if (bestJob == -1 && !pawn.failedJobs.empty()) pawn.failedJobs.clear();
if (bestJob == -1 && !benches.empty() && !isResearchDone(activeResearchIdx)) {
int chosenBench = -1, bestBenchDist = 999999;
for (size_t b = 0; b < benches.size(); ++b) {
int bx = benches[b].first, by = benches[b].second;
bool benchInUse = false;
for (const auto& j : jobs) {
if (j.type == TaskType::RESEARCH_WORK && j.tx == bx && j.ty == by) { benchInUse = true; break; }}
if (!benchInUse) {
int d = std::abs(bx - pawn.x) + std::abs(by - pawn.y);
if (d < bestBenchDist) { bestBenchDist = d; chosenBench = (int)b; }}}
if (chosenBench != -1) {
jobs.push_back(Job{nextJobId++, TaskType::RESEARCH_WORK, benches[chosenBench].first, benches[chosenBench].second, FloorType::NONE, ObjectType::NONE, MaterialType::WOOD, CropType::NONE, 0.0f, 9999.0f, -1});
bestJob = (int)jobs.size() - 1; }}
if (bestJob != -1) {
pawn.jobIndex = bestJob; pawn.pathTimer = 0.0f; jobs[bestJob].assignedPawn = (int)pIdx;
} else if (pawn.recreation < 85.0f) {
// Only do recreation when pawn would otherwise be idle!
pawn.doingRecreation = true;
pawn.targetRecX = -1; pawn.targetRecY = -1;
int bestRecDist = 999999;
for (size_t r = 0; r < recObjects.size(); ++r) {
int rx = recObjects[r].first, ry = recObjects[r].second;
int d = std::abs(rx - pawn.x) + std::abs(ry - pawn.y);
if (d < bestRecDist) { bestRecDist = d; pawn.targetRecX = rx; pawn.targetRecY = ry; }}}}}
for (size_t pIdx = 0; pIdx < pawns.size(); ++pIdx) {
Pawn& pawn = pawns[pIdx];
if (pawn.isSleeping) continue;
pawn.moveTimer -= dt;
if (pawn.wantsSleep && pawn.targetBedX != -1) {
pawn.activity = "Going to bed";
if (pawn.moveTimer <= 0.0f) {
pawn.moveTimer = 0.18f;
GridCoord step = FindNextStep(localMap, pawn.x, pawn.y, pawn.targetBedX, pawn.targetBedY, true);
if (step.first == -1) { pawn.targetBedX = -1; pawn.isSleeping = true; }
else { pawn.x = step.first; pawn.y = step.second; }}
continue; }
if (pawn.jobIndex >= 0 && pawn.jobIndex < (int)jobs.size()) {
Job& job = jobs[pawn.jobIndex];
int dx = job.tx - pawn.x, dy = job.ty - pawn.y;
int dist = std::max(std::abs(dx), std::abs(dy));
if (dist <= 1) {
pawn.pathTimer = 0.0f;
if (job.type == TaskType::RESEARCH_WORK) {
if (hasHigherPriorityJob) {
int finishedIdx = pawn.jobIndex;
pawn.jobIndex = -1; pawn.researchAccum = 0.0f;
jobs.erase(jobs.begin() + finishedIdx);
SyncPawnIndices(pawns, jobs);
continue; }
pawn.activity = "Researching";
int intelLvl = pawn.skillLevel[(int)SkillId::INTELLECTUAL];
float intelSpeed = GetSkillWorkSpeed(intelLvl);
pawn.researchAccum += gameMinutesElapsed * intelSpeed;
GrantSkillXP(pawn, SkillId::INTELLECTUAL, dt * 85.0f, statusMessage);
while (pawn.researchAccum >= 1.0f && !isResearchDone(activeResearchIdx)) {
pawn.researchAccum -= 1.0f;
researchTree[activeResearchIdx].progress += 1; }
if (isResearchDone(activeResearchIdx)) {
statusMessage = "RESEARCH COMPLETE: " + researchTree[activeResearchIdx].title + "!";
int finishedIdx = pawn.jobIndex;
pawn.jobIndex = -1; pawn.activity = "Idle";
jobs.erase(jobs.begin() + finishedIdx);
SyncPawnIndices(pawns, jobs); }
} else {
pawn.activity = (job.type == TaskType::COOK_MEAL) ? "Cooking" : "Working";
SkillId usedSkill = SkillId::CONSTRUCTION;
if (job.type == TaskType::HARVEST_CROP || job.type == TaskType::TILL_SOIL || job.type == TaskType::SOW_CROP ||
(job.type == TaskType::HARVEST_PROP && localMap[job.ty][job.tx].obj != ObjectType::BOULDER)) {
usedSkill = SkillId::PLANTS;
} else if (job.type == TaskType::HARVEST_PROP && localMap[job.ty][job.tx].obj == ObjectType::BOULDER) {
usedSkill = SkillId::MINING;
} else if (job.type == TaskType::COOK_MEAL) {
usedSkill = SkillId::COOKING; }
float workSpeed = GetSkillWorkSpeed(pawn.skillLevel[(int)usedSkill]);
job.progress += dt * workSpeed;
GrantSkillXP(pawn, usedSkill, dt * 110.0f, statusMessage);
if (job.progress >= job.workRequired) {
LocalCell& tc = localMap[job.ty][job.tx];
if (job.type == TaskType::HARVEST_PROP) {
if (tc.obj == ObjectType::TREE) { wood += 15; statusMessage = pawn.name + " chopped wood (+15)."; }
else if (tc.obj == ObjectType::BOULDER) { stone += 15; statusMessage = pawn.name + " mined stone (+15)."; }
else if (tc.obj == ObjectType::BERRY_BUSH) { rawFood += 15; statusMessage = pawn.name + " picked berries (+15 Raw)."; }
tc.obj = ObjectType::NONE;
} else if (job.type == TaskType::HARVEST_CROP) {
int yield = (tc.crop == CropType::CORN) ? 35 : 22;
rawFood += yield;
tc.crop = CropType::NONE; tc.cropGrowth = 0.0f;
statusMessage = pawn.name + TextFormat(" harvested crops (+%d Raw Food).", yield);
} else if (job.type == TaskType::TILL_SOIL) tc.terrain = BaseTerrain::TILLED_SOIL;
else if (job.type == TaskType::SOW_CROP) { tc.crop = job.targetCrop; tc.cropGrowth = 0.05f; }
else if (job.type == TaskType::BUILD_FLOOR) tc.floor = job.targetFloor;
else if (job.type == TaskType::BUILD_OBJECT) { tc.obj = job.targetObj; tc.objMat = job.mat; }
else if (job.type == TaskType::BUILD_ROOF) tc.hasRoof = true;
else if (job.type == TaskType::BUILD_CONDUIT) tc.hasConduit = true;
else if (job.type == TaskType::COOK_MEAL) {
int batch = std::min(rawFood, 5);
rawFood -= batch;
cookedFood += batch;
statusMessage = pawn.name + TextFormat(" cooked %d meals at the stove.", batch);
} else if (job.type == TaskType::DECONSTRUCT_OR_REMOVE) {
if (tc.obj != ObjectType::NONE) tc.obj = ObjectType::NONE;
else if (tc.hasRoof) tc.hasRoof = false;
else if (tc.hasConduit) tc.hasConduit = false;
else if (tc.floor != FloorType::NONE) tc.floor = FloorType::NONE; }
int finishedIdx = pawn.jobIndex;
pawn.jobIndex = -1; pawn.activity = "Idle";
jobs.erase(jobs.begin() + finishedIdx);
SyncPawnIndices(pawns, jobs); }}
} else {
if (job.type == TaskType::RESEARCH_WORK && hasHigherPriorityJob) {
int finishedIdx = pawn.jobIndex;
pawn.jobIndex = -1;
jobs.erase(jobs.begin() + finishedIdx);
SyncPawnIndices(pawns, jobs);
continue; }
pawn.pathTimer += dt;
if (pawn.pathTimer >= 30.0f) {
statusMessage = pawn.name + " timed out reaching target - switching tasks.";
pawn.failedJobs.insert(job.id);
job.assignedPawn = -1; pawn.jobIndex = -1; pawn.pathTimer = 0.0f;
continue; }
if (pawn.moveTimer <= 0.0f) {
pawn.activity = "Moving";
pawn.moveTimer = 0.18f;
bool needExact = (job.type == TaskType::BUILD_FLOOR || job.type == TaskType::BUILD_CONDUIT);
GridCoord nextStep = FindNextStep(localMap, pawn.x, pawn.y, job.tx, job.ty, needExact);
if (nextStep.first == -1) {
statusMessage = pawn.name + " cannot reach target (blocked by walls/water) - switching tasks.";
pawn.failedJobs.insert(job.id);
job.assignedPawn = -1; pawn.jobIndex = -1; pawn.pathTimer = 0.0f;
continue;
} else { pawn.x = nextStep.first; pawn.y = nextStep.second; }}}
} else if (pawn.doingRecreation) {
if (pawn.targetRecX != -1 && localMap[pawn.targetRecY][pawn.targetRecX].obj != ObjectType::NONE) {
int dist = std::max(std::abs(pawn.targetRecX - pawn.x), std::abs(pawn.targetRecY - pawn.y));
if (dist <= 1) {
ObjectType rObj = localMap[pawn.targetRecY][pawn.targetRecX].obj;
pawn.activity = (rObj == ObjectType::CHESS_TABLE) ? "Playing Chess" : "Playing Horseshoes";
float recPerHour = (rObj == ObjectType::CHESS_TABLE) ? 85.0f : 55.0f;
pawn.recreation = std::min(100.0f, pawn.recreation + gameHoursElapsed * recPerHour);
if (rObj == ObjectType::CHESS_TABLE) GrantSkillXP(pawn, SkillId::INTELLECTUAL, dt * 45.0f, statusMessage);
if (pawn.recreation >= 99.5f) { pawn.doingRecreation = false; pawn.activity = "Idle"; }
} else if (pawn.moveTimer <= 0.0f) {
pawn.activity = "Going to play";
pawn.moveTimer = 0.18f;
GridCoord step = FindNextStep(localMap, pawn.x, pawn.y, pawn.targetRecX, pawn.targetRecY, false);
if (step.first == -1) { pawn.targetRecX = -1; }
else { pawn.x = step.first; pawn.y = step.second; }}
} else {
// No proper recreation available -> Go for a walk (+40% recreation per in-game hour)
pawn.activity = "Going for a walk";
pawn.recreation = std::min(100.0f, pawn.recreation + gameHoursElapsed * 40.0f);
if (pawn.moveTimer <= 0.0f) {
pawn.moveTimer = 0.45f;
int nx = pawn.x + GetRandomValue(-1, 1), ny = pawn.y + GetRandomValue(-1, 1);
if (std::abs(nx - localCols / 2) <= 22 && std::abs(ny - localRows / 2) <= 18 && isLocalWalkable(nx, ny)) { pawn.x = nx; pawn.y = ny; }}
if (pawn.recreation >= 99.5f) { pawn.doingRecreation = false; pawn.activity = "Idle"; }}
} else {
pawn.activity = "Idle";
if (pawn.moveTimer <= 0.0f) {
pawn.moveTimer = GetRandomValue(12, 26) * 0.1f;
int nx = pawn.x + GetRandomValue(-1, 1), ny = pawn.y + GetRandomValue(-1, 1);
if (std::abs(nx - localCols / 2) <= 18 && std::abs(ny - localRows / 2) <= 15 && isLocalWalkable(nx, ny)) { pawn.x = nx; pawn.y = ny; }}}}}
BeginDrawing();
ClearBackground(Color{20, 24, 30, 255});
if (mode == GameMode::WORLD_EXPLORE) {
for (int vy = 0; vy < viewRows; ++vy)
for (int vx = 0; vx < viewCols; ++vx)
DrawWorldTileIcon(vx * tileSize, vy * tileSize + topBarHeight, tileSize, getWorldElevation(camX + vx, camY + vy), HashTile(camX + vx, camY + vy, seed), time);
int screenPlayerX = (playerX - camX) * tileSize + tileSize / 2, screenPlayerY = (playerY - camY) * tileSize + topBarHeight + tileSize / 2;
DrawCircle(screenPlayerX, screenPlayerY + 2, 10.0f, Color{210, 45, 45, 255});
DrawCircleLines(screenPlayerX, screenPlayerY + 2, 10.0f, WHITE);
DrawCircle(screenPlayerX, screenPlayerY - 4, 6.0f, Color{255, 215, 175, 255});
DrawRectangle(screenPlayerX - 8, screenPlayerY - 8, 16, 3, Color{120, 75, 35, 255});
} else {
int visibleMapRows = (screenHeight - topBarHeight - bottomBarHeight) / tileSize;
for (int vy = 0; vy < visibleMapRows; ++vy)
for (int vx = 0; vx < viewCols; ++vx)
if (camX + vx >= 0 && camX + vx < localCols && camY + vy >= 0 && camY + vy < localRows)
DrawLocalCell(vx * tileSize, vy * tileSize + topBarHeight, tileSize, localMap[camY + vy][camX + vx], time, showRoofOverlay);
for (const auto& job : jobs) {
if (job.type == TaskType::RESEARCH_WORK || job.type == TaskType::COOK_MEAL) continue;
int vx = job.tx - camX, vy = job.ty - camY;
if (vx >= 0 && vx < viewCols && vy >= 0 && vy < visibleMapRows) {
int px = vx * tileSize, py = vy * tileSize + topBarHeight;
Color mc = (job.type == TaskType::HARVEST_PROP || job.type == TaskType::HARVEST_CROP) ? ORANGE : SKYBLUE;
if (job.type == TaskType::DECONSTRUCT_OR_REMOVE) mc = RED;
if (job.type == TaskType::TILL_SOIL || job.type == TaskType::SOW_CROP) mc = LIME;
DrawRectangleLinesEx(Rectangle{(float)(px + 2), (float)(py + 2), (float)(tileSize - 4), (float)(tileSize - 4)}, 2.0f, mc);
if (job.progress > 0.0f) DrawRectangle(px + 4, py + tileSize - 7, (int)((tileSize - 8) * std::min(1.0f, job.progress / job.workRequired)), 4, LIME); }}
for (size_t i = 0; i < pawns.size(); ++i) {
int vx = pawns[i].x - camX, vy = pawns[i].y - camY;
if (vx >= 0 && vx < viewCols && vy >= 0 && vy < visibleMapRows)
DrawPawnIcon(vx * tileSize + tileSize / 2, vy * tileSize + topBarHeight + tileSize / 2, pawns[i].shirtColor, pawns[i].name, pawns[i].activity == "Working" || pawns[i].activity == "Researching" || pawns[i].activity == "Cooking", pawns[i].isSleeping, selectedPawnIdx == (int)i); }
float darkness = 0.0f;
if (gameHour < 5.0f || gameHour > 21.0f) darkness = 0.48f;
else if (gameHour < 7.0f) darkness = 0.48f * (1.0f - (gameHour - 5.0f) / 2.0f);
else if (gameHour > 19.0f) darkness = 0.48f * ((gameHour - 19.0f) / 2.0f);
int mapBottomY = screenHeight - bottomBarHeight;
if (darkness > 0.01f) DrawRectangle(0, topBarHeight, screenWidth, mapBottomY - topBarHeight, Color{10, 15, 35, (unsigned char)(darkness * 255)});
Vector2 mouse = GetMousePosition();
if (!researchScreenOpen && mouse.y > topBarHeight && mouse.y < mapBottomY)
DrawRectangleLines(((int)mouse.x / tileSize) * tileSize, ((int)(mouse.y - topBarHeight) / tileSize) * tileSize + topBarHeight, tileSize, tileSize, GOLD);
DrawRectangle(0, mapBottomY, screenWidth, bottomBarHeight, Color{22, 26, 34, 255});
DrawLine(0, mapBottomY, screenWidth, mapBottomY, Color{70, 80, 95, 255});
int subRowY = mapBottomY + 4, mainRowY = mapBottomY + 52;
const char* matLabel = (selectedMat == MaterialType::WOOD) ? "Wood" : "Stone";
if (mainTab == 0) {
const StrList ordNames = {"Harvest / Chop / Mine", "Cancel / Deconstruct"};
for (int i = 0; i < 2; ++i) {
int bx = 10 + i * 208; bool act = (subToolIdx == i);
DrawRectangle(bx, subRowY, 200, 42, act ? Color{55, 80, 115, 255} : Color{32, 38, 48, 255});
DrawRectangleLines(bx, subRowY, 200, 42, act ? GOLD : GRAY);
DrawText(ordNames[i], bx + 12, subRowY + 13, 14, act ? GOLD : WHITE); }
} else if (mainTab == 1) {
const StrList subNames = {"Construction", "Production", "Power", "Recreation"};
for (int s = 0; s < 4; ++s) {
int bx = 10 + s * 116; bool act = (buildSubTab == s);
DrawRectangle(bx, subRowY, 112, 42, act ? Color{70, 95, 65, 255} : Color{32, 38, 48, 255});
DrawRectangleLines(bx, subRowY, 112, 42, act ? LIME : GRAY);
DrawText(subNames[s], bx + 10, subRowY + 13, 13, act ? LIME : WHITE); }
if (buildSubTab == 0) {
const StrList cItems = {"Floor (2)", "Wall (5)", "Door (5)", "Build Roof"};
for (int i = 0; i < 4; ++i) {
int bx = 500 + i * 188; bool act = (subToolIdx == i);
DrawRectangle(bx, subRowY, 180, 42, act ? Color{55, 80, 115, 255} : Color{32, 38, 48, 255});
DrawRectangleLines(bx, subRowY, 180, 42, act ? GOLD : GRAY);
DrawText(cItems[i], bx + 10, subRowY + 6, 14, act ? GOLD : WHITE);
if (i < 3) DrawText(TextFormat("Mat: %s (Click toggle)", matLabel), bx + 10, subRowY + 24, 11, LIGHTGRAY);
else DrawText("Toggle View: [R]", bx + 10, subRowY + 24, 11, LIGHTGRAY); }
} else if (buildSubTab == 1) {
const StrList pItems = {"Electric Stove", "Research Bench", "Bed (10)", "Table (8)", "Chair (5)"};
for (int i = 0; i < 5; ++i) {
int bx = 500 + i * 188; bool act = (subToolIdx == i);
DrawRectangle(bx, subRowY, 180, 42, act ? Color{55, 80, 115, 255} : Color{32, 38, 48, 255});
DrawRectangleLines(bx, subRowY, 180, 42, act ? GOLD : GRAY);
DrawText(pItems[i], bx + 10, subRowY + 6, 14, act ? GOLD : WHITE);
if (i == 0) DrawText("15 Stone (Cooks 1:1)", bx + 10, subRowY + 24, 11, LIGHTGRAY);
else if (i == 1) DrawText("15 Wood", bx + 10, subRowY + 24, 11, LIGHTGRAY);
else DrawText(TextFormat("Mat: %s (Click toggle)", matLabel), bx + 10, subRowY + 24, 11, LIGHTGRAY); }
} else if (buildSubTab == 2) {
const StrList eItems = {"Power Conduit", "Windmill", "Solar Panel"};
const StrList eCosts = {"1 Wood (No block)", "20W + 10S (0-250W)", "25 Stone (0-100W Day)"};
for (int i = 0; i < 3; ++i) {
int bx = 500 + i * 188; bool act = (subToolIdx == i);
DrawRectangle(bx, subRowY, 180, 42, act ? Color{55, 80, 115, 255} : Color{32, 38, 48, 255});
DrawRectangleLines(bx, subRowY, 180, 42, act ? GOLD : GRAY);
DrawText(eItems[i], bx + 10, subRowY + 6, 14, act ? GOLD : WHITE);
DrawText(eCosts[i], bx + 10, subRowY + 24, 11, LIGHTGRAY); }
} else if (buildSubTab == 3) {
const StrList rItems = {"Horseshoes Pin", "Chess Table"};
for (int i = 0; i < 2; ++i) {
int bx = 500 + i * 188; bool act = (subToolIdx == i);
DrawRectangle(bx, subRowY, 180, 42, act ? Color{55, 80, 115, 255} : Color{32, 38, 48, 255});
DrawRectangleLines(bx, subRowY, 180, 42, act ? GOLD : GRAY);
DrawText(rItems[i], bx + 10, subRowY + 6, 14, act ? GOLD : WHITE);
if (i == 0) DrawText("10 Wood (+55% Rec/hr)", bx + 10, subRowY + 24, 11, LIGHTGRAY);
else DrawText(TextFormat("15 %s (+85%% Rec/hr)", matLabel), bx + 10, subRowY + 24, 11, LIGHTGRAY); }}
} else if (mainTab == 2) {
const StrList gItems = {"Till Soil", "Plant Rice", "Plant Potato", "Plant Corn", "Plant Healroot"};
const StrList gSub = {"Prepares grass", "Fast growth", "Balanced yield", "High yield", "Req: Devilstrand"};
for (int i = 0; i < 5; ++i) {
int bx = 10 + i * 198; bool act = (subToolIdx == i);
DrawRectangle(bx, subRowY, 190, 42, act ? Color{50, 90, 60, 255} : Color{32, 38, 48, 255});
DrawRectangleLines(bx, subRowY, 190, 42, act ? LIME : GRAY);
DrawText(gItems[i], bx + 10, subRowY + 6, 14, act ? LIME : WHITE);
DrawText(gSub[i], bx + 10, subRowY + 24, 11, LIGHTGRAY); }}
const StrList mainTitles = {"ORDERS", "BUILD", "GROW", "RESEARCH"};
int tabW = screenWidth / 4;
for (int m = 0; m < 4; ++m) {
int tx = m * tabW;
bool active = (m == 3) ? researchScreenOpen : (mainTab == m && !researchScreenOpen);
DrawRectangle(tx + 2, mainRowY, tabW - 4, 40, active ? Color{60, 85, 120, 255} : Color{28, 33, 42, 255});
DrawRectangleLines(tx + 2, mainRowY, tabW - 4, 40, active ? GOLD : Color{60, 70, 85, 255});
int tw = MeasureText(mainTitles[m], 16);
DrawText(mainTitles[m], tx + (tabW - tw) / 2, mainRowY + 12, 16, active ? GOLD : WHITE); }
// Bottom-Left Player Needs & RimWorld Skills Window
if (selectedPawnIdx >= 0 && selectedPawnIdx < (int)pawns.size() && !researchScreenOpen) {
const Pawn& sp = pawns[selectedPawnIdx];
int px = 12, py = mapBottomY - 246, pw = 350, ph = 236;
DrawRectangle(px, py, pw, ph, Color{22, 27, 36, 242});
DrawRectangleLinesEx(Rectangle{(float)px, (float)py, (float)pw, (float)ph}, 2.0f, GOLD);
DrawText(TextFormat("%s  (%s)", sp.name.c_str(), sp.activity.c_str()), px + 12, py + 10, 15, GOLD);
DrawRectangle(px + pw - 28, py + 8, 20, 20, MAROON);
DrawText("X", px + pw - 22, py + 10, 14, WHITE);
DrawLine(px + 10, py + 34, px + pw - 10, py + 34, Color{60, 70, 85, 255});
auto drawNeedBar = [&](int bx, int by, const char* label, float val, Color barCol) {
DrawText(label, bx, by, 12, LIGHTGRAY);
DrawRectangle(bx + 84, by + 1, 195, 13, Color{14, 17, 22, 255});
DrawRectangle(bx + 85, by + 2, (int)(193.0f * std::clamp(val / 100.0f, 0.0f, 1.0f)), 11, barCol);
DrawRectangleLines(bx + 84, by + 1, 195, 13, GRAY);
DrawText(TextFormat("%d%%", (int)val), bx + 286, by, 12, WHITE); };
Color moodCol = (sp.mood > 60.0f) ? LIME : (sp.mood > 30.0f ? ORANGE : RED);
drawNeedBar(px + 12, py + 42, "Mood:", sp.mood, moodCol);
drawNeedBar(px + 12, py + 64, "Food:", sp.hunger, SKYBLUE);
drawNeedBar(px + 12, py + 86, "Recreation:", sp.recreation, GOLD);
drawNeedBar(px + 12, py + 108, "Sleep:", sp.energy, PURPLE);
DrawLine(px + 10, py + 130, px + pw - 10, py + 130, Color{60, 70, 85, 255});
DrawText("SKILLS  (* Minor 100%  |  ** Major 150%)", px + 12, py + 136, 11, GOLD);
const StrList sLabels = {"Construction", "Plants", "Mining", "Cooking", "Intellectual", "Artistic"};
for (int s = 0; s < (int)SkillId::COUNT; ++s) {
int col = s % 2, row = s / 2;
int sx = px + 12 + col * 168, sy = py + 155 + row * 24;
const char* pMark = (sp.skillPassion[s] == 2) ? "**" : (sp.skillPassion[s] == 1 ? "" : "");
Color sCol = (sp.skillPassion[s] == 2) ? GOLD : (sp.skillPassion[s] == 1 ? SKYBLUE : RAYWHITE);
DrawText(TextFormat("%s: %d%s", sLabels[s], sp.skillLevel[s], pMark), sx, sy, 12, sCol); }}
if (researchScreenOpen) {
DrawRectangle(140, 100, 1320, 740, Color{20, 24, 32, 245});
DrawRectangleLinesEx(Rectangle{140, 100, 1320, 740}, 2.0f, GOLD);
DrawText("COLONY RESEARCH TREE (1 pt per in-game minute per Research Bench)", 170, 118, 20, GOLD);
DrawRectangle(1340, 110, 80, 35, MAROON);
DrawText("CLOSE", 1353, 120, 15, WHITE);
for (int i = 0; i < (int)researchTree.size(); ++i) {
int pre = researchTree[i].prereq;
if (pre != -1) DrawLineEx(Vector2{(float)(180 + researchTree[pre].uiX + 250), (float)(130 + researchTree[pre].uiY + 47)}, Vector2{(float)(180 + researchTree[i].uiX), (float)(130 + researchTree[i].uiY + 47)}, 3.0f, isResearchDone(pre) ? LIME : GRAY); }
for (int i = 0; i < (int)researchTree.size(); ++i) {
const ResearchNode& node = researchTree[i];
int bx = 180 + node.uiX, by = 130 + node.uiY;
bool done = isResearchDone(i), selected = (activeResearchIdx == i && !done), locked = (node.prereq != -1 && !isResearchDone(node.prereq));
Color bg = done ? Color{35, 85, 50, 255} : (selected ? Color{55, 80, 125, 255} : (locked ? Color{30, 32, 38, 255} : Color{42, 48, 60, 255}));
Color border = done ? LIME : (selected ? GOLD : (locked ? DARKGRAY : LIGHTGRAY));
DrawRectangle(bx, by, 250, 95, bg);
DrawRectangleLinesEx(Rectangle{(float)bx, (float)by, 250.0f, 95.0f}, 2.0f, border);
DrawText(node.title.c_str(), bx + 10, by + 10, 16, done ? LIME : WHITE);
DrawText(node.desc.c_str(), bx + 10, by + 32, 11, LIGHTGRAY);
const char* statusTxt = done ? "COMPLETED" : (locked ? "LOCKED" : TextFormat("Progress: %d / %d", node.progress, node.cost));
DrawText(statusTxt, bx + 10, by + 68, 13, done ? LIME : (selected ? GOLD : RAYWHITE)); }}}
DrawRectangle(0, 0, screenWidth, topBarHeight, Color{25, 30, 38, 255});
int hourInt = (int)gameHour, minInt = (int)((gameHour - hourInt) * 60.0f);
const char* timeStr = allSleeping ? TextFormat("[SLEEP 10X SPEED]  DAY %d  |  %02d:%02d", gameDay, hourInt, minInt) : TextFormat("DAY %d  |  %02d:%02d", gameDay, hourInt, minInt);
DrawText(timeStr, screenWidth - MeasureText(timeStr, 18) - 20, 12, 18, allSleeping ? GOLD : SKYBLUE);
if (mode == GameMode::WORLD_EXPLORE) {
DrawText(TextFormat("WORLD MAP  |  COORDS: (%d, %d)  |  SEED: %u  |  [ENTER] Zoom In & Settle", playerX, playerY, seed), 18, 12, 18, GOLD);
DrawText(statusMessage.c_str(), 18, 42, 16, RAYWHITE);
} else {
DrawText(TextFormat("SECTOR (%d,%d) [%s] | WOOD: %d | STONE: %d | RAW: %d | MEALS: %d | POWER: %dW/%dW | [ESC] World",
settledWorldX, settledWorldY, settledBiomeName.c_str(), wood, stone, rawFood, cookedFood, powerGenerated, powerConsumed), 18, 12, 15, LIME);
DrawText(statusMessage.c_str(), 18, 42, 15, RAYWHITE); }
EndDrawing(); }
CloseWindow();
return 0; }
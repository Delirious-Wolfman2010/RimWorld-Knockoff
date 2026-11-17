#include "raylib.h"
#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <random>
#include <string>
#include <set>
#include <map>
#include <utility>
#include <queue>
#include <functional>
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
static const int localCols = 90, localRows = 70;
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
        IntList base = perm;
        perm.insert(perm.end(), base.begin(), base.end()); }
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
// ============================ ENUMS ============================
enum class GameMode { WORLD_EXPLORE, COLONY_SIM };
enum class MaterialType { WOOD, STONE, STEEL };
enum class BaseTerrain { WATER, SAND, GRASS, ROCK_GROUND };
enum class FloorType { NONE, WOOD_FLOOR, STONE_TILE, STEEL_TILE };
enum class ObjectType { NONE, TREE, BOULDER, STEEL_VEIN, BERRY_BUSH, WALL, DOOR, CAMPFIRE, STOVE, RESEARCH_BENCH, SMITHY, MACHINING, SLEEP_SPOT, BED, TABLE, STOOL, WINDMILL, SOLAR_PANEL, TURRET, TRAP, HORSESHOES, CHESS_TABLE, LAMP_TORCH, LAMP_STANDING, BATTERY, GENERATOR, SANDBAGS };
enum class CropType { NONE, RICE, POTATO, CORN, HEALROOT };
enum class TaskType { CHOP, MINE, PICK_BUSH, HARVEST_CROP, SOW_CROP, BUILD_FLOOR, BUILD_OBJECT, BUILD_ROOF, REMOVE_ROOF, BUILD_CONDUIT, DECONSTRUCT, COOK_MEAL, RESEARCH_WORK, CRAFT, HUNT };
enum class WorkType { HUNTING = 0, CONSTRUCTION, GROWING, PLANT_CUTTING, MINING, COOKING, CRAFTING, RESEARCHING, COUNT };
enum class SkillId { CONSTRUCTION = 0, PLANTS, MINING, COOKING, INTELLECTUAL, SHOOTING, MELEE, CRAFTING, COUNT };
enum class CType { RAIDER, TRADER, DEER, MUFFALO, BOAR, WOLF };
enum class ToolKind { MINE, CHOP, HARVEST, HUNT, DECON, CANCEL, ZONE_GROW, ZONE_CLEAR, ROOF, ROOF_REMOVE, BUILD };
enum ResearchId { R_ELECTRICITY = 0, R_BATTERIES, R_SOLAR, R_ANALYZER, R_STONECUT, R_FURNITURE, R_FORTIFY, R_SMITHING, R_BLADES, R_TRAPS, R_TOOLS, R_MACHINING, R_FIREARMS, R_TURRETS, R_DEVIL, R_FARMING, R_COUNT };
enum SchedAct { SA_SLEEP = 0, SA_WORK, SA_JOY, SA_ANY };
// ============================ DATA TABLES ============================
struct WeaponDef { const char* name; bool ranged; float dmg; float range; float cd; };
static const int WEAPON_COUNT = 8;
static const WeaponDef WEAPONS[WEAPON_COUNT] = {
    {"Fists", false, 5.0f, 1.5f, 1.2f}, {"Knife", false, 9.0f, 1.5f, 1.0f}, {"Club", false, 11.0f, 1.5f, 1.2f},
    {"Mace", false, 16.0f, 1.5f, 1.4f}, {"Longsword", false, 20.0f, 1.5f, 1.2f}, {"Revolver", true, 11.0f, 11.0f, 1.1f},
    {"Bolt-action rifle", true, 17.0f, 15.0f, 1.8f}, {"Pump shotgun", true, 20.0f, 7.0f, 1.5f}};
struct RecipeDef { const char* name; int weapon; int bench; int wood; int steel; float work; int research; };
static const int RECIPE_COUNT = 7;
static const RecipeDef RECIPES[RECIPE_COUNT] = {
    {"Knife", 1, 0, 0, 15, 5.0f, R_SMITHING}, {"Club", 2, 0, 25, 0, 4.0f, R_SMITHING}, {"Mace", 3, 0, 0, 35, 8.0f, R_SMITHING}, {"Longsword", 4, 0, 0, 55, 10.0f, R_BLADES},
    {"Revolver", 5, 1, 0, 40, 8.0f, R_MACHINING}, {"Bolt-action rifle", 6, 1, 10, 60, 11.0f, R_FIREARMS}, {"Pump shotgun", 7, 1, 8, 50, 10.0f, R_FIREARMS}};
static const int TRADE_COUNT = 12;
static const int TRADE_BASE[TRADE_COUNT] = {1, 2, 5, 1, 3, 18, 12, 60, 100, 80, 140, 120};
static const char* TradeName(int i) {
    static const char* n[5] = {"Wood", "Stone", "Steel", "Raw food", "Meals"};
    return i < 5 ? n[i] : WEAPONS[i - 4].name; }
static const char* WORK_NAMES[8] = {"Hunting", "Construction", "Growing", "Plant Cutting", "Mining", "Cooking", "Crafting", "Research"};
static const char* SKILL_NAMES[8] = {"Construction", "Plants", "Mining", "Cooking", "Intellectual", "Shooting", "Melee", "Crafting"};
struct ResearchNode { std::string title; std::string desc; int cost; int progress; int prereq; int uiX; int uiY; };
struct ToolDef {
    const char* name = ""; const char* sub = ""; int cat = 0; ToolKind kind = ToolKind::BUILD;
    ObjectType obj = ObjectType::NONE; FloorType floor = FloorType::NONE; int buildKind = 0; // 0 object, 1 floor, 2 conduit
    int wood = 0, stone = 0, steel = 0; bool matChoice = false; int matAmt = 0; float work = 1.0f; int research = -1;
    int crop = 0; bool blocks = false; MaterialType fixedMat = MaterialType::WOOD; };
struct LocalCell {
    BaseTerrain terrain = BaseTerrain::GRASS; FloorType floor = FloorType::NONE;
    ObjectType obj = ObjectType::NONE; MaterialType objMat = MaterialType::WOOD;
    bool hasRoof = false; bool hasConduit = false; CropType crop = CropType::NONE; float cropGrowth = 0.0f;
    int zoneCrop = 0; float hp = 0.0f; int bedOwner = -1; float aux = 0.0f; };
struct Job {
    int id = 0; TaskType type = TaskType::CHOP; int tx = 0; int ty = 0;
    FloorType targetFloor = FloorType::NONE; ObjectType targetObj = ObjectType::NONE; MaterialType mat = MaterialType::WOOD;
    CropType targetCrop = CropType::NONE; float progress = 0.0f; float workRequired = 1.2f;
    int assignedPawn = -1; int forcedPawn = -1; int cw = 0, cs = 0, cst = 0; int recipe = -1; int targetId = -1; };
struct TradeStock { std::string who; int silver = 0; std::vector<int> qty = std::vector<int>(TRADE_COUNT, 0); };
struct Pawn {
    std::string name; int x = 0; int y = 0; Color shirtColor = Color{200, 200, 200, 255};
    float moveTimer = 0.0f; int jobId = -1; float pathTimer = 0.0f; IntSet failedJobs; std::string activity = "Idle";
    float hunger = 100.0f, energy = 100.0f, recreation = 85.0f, mood = 80.0f, hp = 100.0f, maxHp = 100.0f;
    bool isSleeping = false, downed = false, dead = false, drafted = false, doingRecreation = false;
    int targetBedX = -1, targetBedY = -1, bedX = -1, bedY = -1, targetRecX = -1, targetRecY = -1;
    float researchAccum = 0.0f, atkTimer = 0.0f, thinkTimer = 0.0f; int weapon = 0;
    int gotoX = -1, gotoY = -1, forcedAttackId = -1;
    IntList skillLevel, skillPassion; FloatList skillXP; IntList workPrio; IntList schedule; };
struct Critter {
    int id = 0; CType type = CType::DEER; std::string name; int x = 0, y = 0; float hp = 50.0f, maxHp = 50.0f;
    int weapon = 0; float atkTimer = 0.0f, moveTimer = 0.0f, stayHours = 0.0f, meleeDmg = 0.0f;
    bool hostile = false, hunted = false, leaving = false, gone = false; TradeStock stock; };
typedef std::vector<Pawn> PawnList;
typedef std::vector<Job> JobList;
typedef std::vector<Critter> CritterList;
typedef std::vector<LocalCell> CellRow;
typedef std::vector<CellRow> CellGrid;
typedef std::vector<ResearchNode> ResearchList;
struct ColonyState {
    CellGrid map; PawnList pawns; JobList jobs; CritterList critters;
    int wood = 80, stone = 45, steel = 40, silver = 120, rawFood = 40, cookedFood = 15;
    int weaponInv[WEAPON_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0};
    int billTarget[RECIPE_COUNT] = {0, 0, 0, 0, 0, 0, 0};
    int nextJobId = 1, nextCritterId = 1, powerGen = 0, powerUse = 0;
    float windOut = 145.0f, windTimer = 0.0f, batteryCharge = 0.0f, fuelAccum = 0.0f; bool onBattery = false;
    double nextRaidTime = 0.0, nextTraderTime = 0.0, nextAnimalTime = 0.0;
    int camX = 0, camY = 0, worldX = 0, worldY = 0; std::string biome = "Temperate Forest"; };
struct Shot { float x1, y1, x2, y2, t; Color col; };
struct CtxItem { std::string label; int action; int a; int b; };
static int& TradeQtyRef(ColonyState& C, int i) {
    if (i == 0) return C.wood; if (i == 1) return C.stone; if (i == 2) return C.steel;
    if (i == 3) return C.rawFood; if (i == 4) return C.cookedFood; return C.weaponInv[i - 4]; }
static void ToolCost(const ToolDef& t, MaterialType m, int& w, int& s, int& st) {
    w = t.wood; s = t.stone; st = t.steel;
    if (t.matChoice) { if (m == MaterialType::WOOD) w += t.matAmt; else if (m == MaterialType::STONE) s += t.matAmt; else st += t.matAmt; }}
static float StructHp(ObjectType o, MaterialType m) {
    if (o == ObjectType::WALL) return m == MaterialType::WOOD ? 70.0f : (m == MaterialType::STONE ? 150.0f : 200.0f);
    if (o == ObjectType::DOOR) return m == MaterialType::WOOD ? 60.0f : (m == MaterialType::STONE ? 100.0f : 140.0f);
    return 50.0f; }
static WorkType WorkTypeOf(TaskType t) {
    switch (t) {
        case TaskType::CHOP: return WorkType::PLANT_CUTTING;
        case TaskType::MINE: return WorkType::MINING;
        case TaskType::PICK_BUSH: case TaskType::HARVEST_CROP: case TaskType::SOW_CROP: return WorkType::GROWING;
        case TaskType::COOK_MEAL: return WorkType::COOKING;
        case TaskType::RESEARCH_WORK: return WorkType::RESEARCHING;
        case TaskType::CRAFT: return WorkType::CRAFTING;
        case TaskType::HUNT: return WorkType::HUNTING;
        default: return WorkType::CONSTRUCTION; }}
static SkillId SkillOfTask(TaskType t) {
    switch (t) {
        case TaskType::CHOP: case TaskType::PICK_BUSH: case TaskType::HARVEST_CROP: case TaskType::SOW_CROP: return SkillId::PLANTS;
        case TaskType::MINE: return SkillId::MINING;
        case TaskType::COOK_MEAL: return SkillId::COOKING;
        case TaskType::RESEARCH_WORK: return SkillId::INTELLECTUAL;
        case TaskType::CRAFT: return SkillId::CRAFTING;
        default: return SkillId::CONSTRUCTION; }}
static std::map<std::string, Texture2D> g_tex; static std::set<std::string> g_texMissing;
// Draws textures/<name>.png scaled to w*h. Returns false (so the caller draws the built-in shapes) if the file doesn't exist.
static bool DrawTex(const std::string& name, int x, int y, int w, int h, Color tint = Color{255, 255, 255, 255}) {
    auto it = g_tex.find(name);
    if (it == g_tex.end()) {
        if (g_texMissing.count(name)) return false;
        std::string path = "textures/" + name + ".png";
        if (!FileExists(path.c_str())) { g_texMissing.insert(name); return false; }
        Texture2D t = LoadTexture(path.c_str());
        if (t.id == 0) { g_texMissing.insert(name); return false; }
        it = g_tex.emplace(name, t).first; }
    const Texture2D& t = it->second;
    DrawTexturePro(t, Rectangle{0.0f, 0.0f, (float)t.width, (float)t.height}, Rectangle{(float)x, (float)y, (float)w, (float)h}, Vector2{0.0f, 0.0f}, 0.0f, tint);
    return true; }
static void UnloadAllTex() { for (auto& kv : g_tex) UnloadTexture(kv.second); g_tex.clear(); }
static std::string ObjTexName(ObjectType o) {
    switch (o) {
        case ObjectType::TREE: return "tree"; case ObjectType::BOULDER: return "boulder"; case ObjectType::STEEL_VEIN: return "steelvein"; case ObjectType::BERRY_BUSH: return "berrybush";
        case ObjectType::WALL: return "wall"; case ObjectType::DOOR: return "door"; case ObjectType::CAMPFIRE: return "campfire"; case ObjectType::STOVE: return "stove";
        case ObjectType::RESEARCH_BENCH: return "researchbench"; case ObjectType::SMITHY: return "smithy"; case ObjectType::MACHINING: return "machiningtable";
        case ObjectType::SLEEP_SPOT: return "sleepspot"; case ObjectType::BED: return "bed"; case ObjectType::TABLE: return "table"; case ObjectType::STOOL: return "stool";
        case ObjectType::WINDMILL: return "windturbine"; case ObjectType::SOLAR_PANEL: return "solarpanel"; case ObjectType::TURRET: return "turret"; case ObjectType::TRAP: return "trap";
        case ObjectType::HORSESHOES: return "horseshoes"; case ObjectType::CHESS_TABLE: return "chesstable"; case ObjectType::LAMP_TORCH: return "torchlamp";
        case ObjectType::LAMP_STANDING: return "standinglamp"; case ObjectType::BATTERY: return "battery"; case ObjectType::GENERATOR: return "generator"; case ObjectType::SANDBAGS: return "sandbags";
        default: return ""; }}
static float Dist(int x1, int y1, int x2, int y2) { return std::sqrt((float)((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2))); }
static bool MouseIn(Vector2 m, int x, int y, int w, int h) { return m.x >= x && m.x < x + w && m.y >= y && m.y < y + h; }
static void DrawButton(int x, int y, int w, int h, const char* label, bool active, Color accent, int fs = 14) {
    DrawRectangle(x, y, w, h, active ? Color{55, 80, 115, 255} : Color{32, 38, 48, 255});
    DrawRectangleLines(x, y, w, h, active ? accent : GRAY);
    int tw = MeasureText(label, fs);
    DrawText(label, x + (w - tw) / 2, y + (h - fs) / 2, fs, active ? accent : WHITE); }
float GetSkillWorkSpeed(int level) { return 0.50f + (float)std::clamp(level, 0, 20) * 0.0625f; }
void GrantSkillXP(Pawn& p, SkillId sid, float baseXP, std::string& statusOut) {
    int idx = (int)sid;
    if (idx < 0 || idx >= (int)p.skillLevel.size() || p.skillLevel[idx] >= 20) return;
    float passionMult = (p.skillPassion[idx] == 2) ? 1.50f : (p.skillPassion[idx] == 1 ? 1.00f : 0.35f);
    p.skillXP[idx] += baseXP * passionMult;
    float xpNeeded = (float)((p.skillLevel[idx] + 1) * 1000);
    while (p.skillXP[idx] >= xpNeeded && p.skillLevel[idx] < 20) {
        p.skillXP[idx] -= xpNeeded; p.skillLevel[idx]++;
        statusOut = p.name + TextFormat(" leveled up %s to %d!", SKILL_NAMES[idx], p.skillLevel[idx]);
        xpNeeded = (float)((p.skillLevel[idx] + 1) * 1000); }}
void InitPawnData(Pawn& p) {
    int count = (int)SkillId::COUNT;
    p.skillLevel.assign(count, 0); p.skillPassion.assign(count, 0); p.skillXP.assign(count, 0.0f);
    for (int i = 0; i < count; ++i) {
        int roll = GetRandomValue(0, 5) + GetRandomValue(0, 5) + GetRandomValue(0, 5);
        p.skillLevel[i] = std::clamp(roll, 0, 15);
        int pRoll = GetRandomValue(1, 100);
        p.skillPassion[i] = (pRoll <= 12) ? 2 : (pRoll <= 38 ? 1 : 0); }
    p.workPrio = {3, 2, 3, 3, 3, 2, 3, 4};
    p.schedule.assign(24, SA_ANY);
    for (int h = 0; h < 24; ++h) { if (h >= 22 || h < 6) p.schedule[h] = SA_SLEEP; else if (h >= 20) p.schedule[h] = SA_JOY; }}
// ============================ DRAWING ============================
void DrawWorldTileIcon(int px, int py, int size, double h, unsigned int detailHash, float time) {
    if (DrawTex(h < 0.42 ? "world_deepwater" : (h < 0.48 ? "world_water" : (h < 0.53 ? "world_sand" : (h < 0.75 ? "world_grass" : "world_mountain"))), px, py, size, size)) {
        if (h >= 0.53 && h < 0.75 && (detailHash % 4 == 0 || h > 0.65)) DrawTex("world_forest", px, py, size, size);
        return; }
    if (h < 0.42) {
        DrawRectangle(px, py, size, size, Color{24, 68, 140, 255});
        int w = (int)(std::sin(time * 2.5f + px * 0.1f) * 2.0f);
        DrawLine(px + 6 + w, py + size / 2, px + size - 6 + w, py + size / 2, Color{65, 130, 210, 255});
    } else if (h < 0.48) {
        DrawRectangle(px, py, size, size, Color{48, 135, 205, 255});
        int w = (int)(std::cos(time * 3.0f + py * 0.1f) * 2.0f);
        DrawLine(px + 8 + w, py + size / 2 + 2, px + size - 8 + w, py + size / 2 + 2, Color{150, 215, 255, 255});
    } else if (h < 0.53) {
        DrawRectangle(px, py, size, size, Color{232, 208, 148, 255});
        if (detailHash % 3 == 0) { DrawCircle(px + 10, py + 12, 1.5f, Color{190, 160, 100, 255}); DrawCircle(px + 20, py + 22, 1.5f, Color{190, 160, 100, 255}); }
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
void DrawSettlementIcon(int px, int py, int size, int type, bool destroyed) {
    if (DrawTex(destroyed ? std::string("world_settlement_destroyed") : "world_settlement_" + std::to_string(type), px, py, size, size)) return;
    Color c = destroyed ? Color{70, 70, 70, 255} : (type == 0 ? Color{70, 130, 220, 255} : (type == 1 ? Color{210, 160, 60, 255} : Color{200, 50, 50, 255}));
    DrawRectangle(px + 5, py + 12, size - 10, size - 16, c);
    DrawTriangle(Vector2{(float)(px + size / 2), (float)(py + 3)}, Vector2{(float)(px + 2), (float)(py + 13)}, Vector2{(float)(px + size - 2), (float)(py + 13)}, Color{90, 60, 40, 255});
    DrawRectangleLines(px + 5, py + 12, size - 10, size - 16, BLACK);
    if (destroyed) DrawLine(px + 4, py + 4, px + size - 4, py + size - 4, RED); }
void DrawLocalCell(int px, int py, int size, const LocalCell& cell, float time, bool showRoofOverlay) {
    static const char* terrNames[4] = {"terrain_water", "terrain_sand", "terrain_grass", "terrain_rock"};
    bool terrTex = DrawTex(terrNames[(int)cell.terrain], px, py, size, size);
    if (terrTex) {}
    else if (cell.terrain == BaseTerrain::WATER) {
        DrawRectangle(px, py, size, size, Color{38, 115, 185, 255});
        int w = (int)(std::sin(time * 2.5f + px * 0.1f) * 2.0f);
        DrawLine(px + 6 + w, py + size / 2, px + size - 6 + w, py + size / 2, Color{120, 195, 245, 255});
    } else if (cell.terrain == BaseTerrain::SAND) {
        DrawRectangle(px, py, size, size, Color{232, 208, 148, 255});
    } else if (cell.terrain == BaseTerrain::GRASS) {
        DrawRectangle(px, py, size, size, Color{72, 165, 78, 255});
        DrawPixel(px + 8, py + 10, Color{55, 135, 60, 255}); DrawPixel(px + 22, py + 24, Color{90, 185, 95, 255});
    } else { DrawRectangle(px, py, size, size, Color{125, 130, 138, 255}); }
    static const char* floorNames[4] = {"", "floor_wood", "floor_stone", "floor_steel"};
    bool floorTex = cell.floor != FloorType::NONE && DrawTex(floorNames[(int)cell.floor], px, py, size, size);
    if (floorTex) {}
    else if (cell.floor == FloorType::WOOD_FLOOR) {
        DrawRectangle(px, py, size, size, Color{168, 122, 74, 255});
        DrawLine(px, py + size / 2, px + size, py + size / 2, Color{130, 90, 50, 255});
        DrawRectangleLines(px, py, size, size, Color{140, 98, 55, 255});
    } else if (cell.floor == FloorType::STONE_TILE) {
        DrawRectangle(px, py, size, size, Color{165, 170, 178, 255});
        DrawRectangleLines(px, py, size, size, Color{120, 125, 132, 255});
        DrawLine(px + size / 2, py, px + size / 2, py + size, Color{130, 135, 142, 255});
        DrawLine(px, py + size / 2, px + size, py + size / 2, Color{130, 135, 142, 255});
    } else if (cell.floor == FloorType::STEEL_TILE) {
        DrawRectangle(px, py, size, size, Color{150, 160, 175, 255});
        DrawRectangleLines(px, py, size, size, Color{100, 110, 125, 255});
        DrawCircle(px + 5, py + 5, 1.5f, Color{90, 100, 115, 255}); DrawCircle(px + size - 5, py + size - 5, 1.5f, Color{90, 100, 115, 255}); }
    if (cell.zoneCrop > 0) {
        DrawRectangle(px, py, size, size, Color{70, 190, 90, 45});
        DrawRectangleLines(px, py, size, size, Color{110, 220, 120, 120}); }
    if (cell.hasConduit) {
        DrawRectangle(px + size / 2 - 2, py, 4, size, Color{45, 150, 235, 200});
        DrawRectangle(px, py + size / 2 - 2, size, 4, Color{45, 150, 235, 200});
        DrawCircle(px + size / 2, py + size / 2, 3.0f, YELLOW); }
    static const char* cropNames[5] = {"", "rice", "potato", "corn", "healroot"};
    if (cell.crop != CropType::NONE && DrawTex(std::string("crop_") + cropNames[(int)cell.crop] + "_" + std::to_string(std::min(3, (int)(cell.cropGrowth * 4.0f))), px, py, size, size)) {}
    else if (cell.crop != CropType::NONE) {
        Color cropCol = LIME;
        if (cell.crop == CropType::RICE) cropCol = Color{175, 225, 95, 255};
        else if (cell.crop == CropType::POTATO) cropCol = Color{95, 185, 75, 255};
        else if (cell.crop == CropType::CORN) cropCol = Color{240, 210, 55, 255};
        else if (cell.crop == CropType::HEALROOT) cropCol = Color{80, 220, 185, 255};
        float r = 3.0f + cell.cropGrowth * 6.5f;
        DrawCircle(px + 10, py + 11, r * 0.65f, cropCol); DrawCircle(px + 22, py + 11, r * 0.65f, cropCol);
        DrawCircle(px + 10, py + 22, r * 0.65f, cropCol); DrawCircle(px + 22, py + 22, r * 0.65f, cropCol);
        if (cell.cropGrowth >= 1.0f) DrawCircleLines(px + size / 2, py + size / 2, 11.0f, GOLD); }
    Color matCol = (cell.objMat == MaterialType::WOOD) ? Color{145, 95, 48, 255} : (cell.objMat == MaterialType::STONE ? Color{130, 136, 145, 255} : Color{170, 185, 205, 255});
    Color matDark = (cell.objMat == MaterialType::WOOD) ? Color{85, 50, 20, 255} : (cell.objMat == MaterialType::STONE ? Color{70, 74, 82, 255} : Color{95, 108, 128, 255});
    int cx = px + size / 2, cy = py + size / 2;
    static const char* matSuffix[3] = {"_wood", "_stone", "_steel"};
    bool objTex = false;
    if (cell.obj != ObjectType::NONE) { std::string base = ObjTexName(cell.obj); objTex = DrawTex(base + matSuffix[(int)cell.objMat], px, py, size, size) || DrawTex(base, px, py, size, size); }
    if (!objTex) switch (cell.obj) {
        case ObjectType::TREE:
            DrawRectangle(px + size / 2 - 2, py + size - 8, 4, 6, Color{102, 66, 40, 255});
            DrawTriangle(Vector2{(float)cx, (float)(py + 4)}, Vector2{(float)(px + 5), (float)(py + size - 8)}, Vector2{(float)(px + size - 5), (float)(py + size - 8)}, Color{28, 96, 38, 255});
            DrawTriangle(Vector2{(float)cx, (float)(py + 2)}, Vector2{(float)(px + 8), (float)(py + size - 14)}, Vector2{(float)(px + size - 8), (float)(py + size - 14)}, Color{44, 135, 54, 255}); break;
        case ObjectType::BOULDER:
            DrawCircle(cx, cy + 2, 10.0f, Color{90, 95, 102, 255}); DrawCircle(cx - 2, cy, 7.0f, Color{135, 140, 148, 255});
            DrawCircleLines(cx, cy + 2, 10.0f, Color{55, 58, 64, 255}); break;
        case ObjectType::STEEL_VEIN:
            DrawCircle(cx, cy + 2, 10.0f, Color{80, 88, 100, 255}); DrawCircle(cx - 2, cy, 7.0f, Color{120, 130, 145, 255});
            DrawCircle(cx - 4, cy - 2, 2.0f, Color{150, 195, 245, 255}); DrawCircle(cx + 3, cy + 3, 2.0f, Color{150, 195, 245, 255}); DrawCircle(cx + 4, cy - 3, 1.5f, WHITE);
            DrawCircleLines(cx, cy + 2, 10.0f, Color{45, 52, 64, 255}); break;
        case ObjectType::BERRY_BUSH:
            DrawCircle(cx, cy + 2, 9.0f, Color{38, 118, 46, 255}); DrawCircle(cx - 3, cy, 2.5f, RED); DrawCircle(cx + 4, cy + 2, 2.5f, RED); DrawCircle(cx, cy + 5, 2.5f, RED); break;
        case ObjectType::WALL:
            DrawRectangle(px + 1, py + 1, size - 2, size - 2, matCol); DrawRectangleLines(px + 1, py + 1, size - 2, size - 2, matDark);
            DrawLine(px + 2, py + size / 2, px + size - 2, py + size / 2, matDark); break;
        case ObjectType::DOOR:
            DrawRectangle(px + 2, py + 10, size - 4, size - 20, matCol); DrawRectangleLines(px + 2, py + 10, size - 4, size - 20, matDark);
            DrawCircle(px + size - 7, cy, 2.0f, GOLD); break;
        case ObjectType::CAMPFIRE:
            DrawCircle(cx, cy + 4, 10.0f, Color{85, 85, 90, 255}); DrawCircle(cx, cy + 4, 6.5f, Color{45, 35, 30, 255});
            DrawTriangle(Vector2{(float)cx, (float)(cy - 9)}, Vector2{(float)(cx - 6), (float)(cy + 5)}, Vector2{(float)(cx + 6), (float)(cy + 5)}, ORANGE);
            DrawTriangle(Vector2{(float)cx, (float)(cy - 3)}, Vector2{(float)(cx - 3), (float)(cy + 5)}, Vector2{(float)(cx + 3), (float)(cy + 5)}, YELLOW); break;
        case ObjectType::STOVE:
            DrawRectangle(px + 4, py + 5, size - 8, size - 10, Color{65, 70, 78, 255}); DrawRectangleLines(px + 4, py + 5, size - 8, size - 10, BLACK);
            DrawCircle(px + 11, py + 13, 3.0f, ORANGE); DrawCircle(px + 21, py + 13, 3.0f, RED); break;
        case ObjectType::RESEARCH_BENCH:
            DrawRectangle(px + 3, py + 6, size - 6, size - 12, matCol); DrawRectangleLines(px + 3, py + 6, size - 6, size - 12, matDark);
            DrawRectangle(px + 7, py + 9, 8, 6, SKYBLUE); DrawCircle(px + 21, py + 16, 3.0f, WHITE); break;
        case ObjectType::SMITHY:
            DrawRectangle(px + 4, py + 9, size - 8, 6, Color{70, 74, 82, 255}); DrawRectangle(px + 10, py + 15, size - 20, 10, Color{55, 58, 66, 255});
            DrawRectangle(px + 7, py + 25, size - 14, 4, Color{40, 42, 48, 255}); DrawTriangle(Vector2{(float)(px + 2), (float)(py + 12)}, Vector2{(float)(px + 6), (float)(py + 9)}, Vector2{(float)(px + 6), (float)(py + 15)}, Color{70, 74, 82, 255}); break;
        case ObjectType::MACHINING:
            DrawRectangle(px + 3, py + 5, size - 6, size - 10, Color{70, 78, 92, 255}); DrawRectangleLines(px + 3, py + 5, size - 6, size - 10, BLACK);
            DrawCircle(cx, cy, 7.0f, Color{120, 128, 142, 255}); DrawCircleLines(cx, cy, 7.0f, BLACK); DrawCircle(cx, cy, 2.5f, ORANGE); break;
        case ObjectType::SLEEP_SPOT:
            DrawRectangle(px + 7, py + 5, size - 14, size - 10, Color{170, 150, 120, 255}); DrawRectangleLines(px + 7, py + 5, size - 14, size - 10, Color{110, 95, 70, 255}); break;
        case ObjectType::BED:
            DrawRectangle(px + 5, py + 3, size - 10, size - 6, matCol); DrawRectangle(px + 7, py + 5, size - 14, 6, WHITE);
            DrawRectangle(px + 7, py + 12, size - 14, size - 17, Color{60, 125, 195, 255}); break;
        case ObjectType::TABLE:
            DrawRectangle(px + 4, py + 4, size - 8, size - 8, matCol); DrawRectangleLines(px + 4, py + 4, size - 8, size - 8, matDark); break;
        case ObjectType::STOOL:
            DrawCircle(cx, cy, 7.0f, matCol); DrawCircleLines(cx, cy, 7.0f, matDark); break;
        case ObjectType::WINDMILL: {
            DrawCircle(cx, cy, 7.0f, LIGHTGRAY); float ang = time * 3.5f;
            for (int k = 0; k < 3; ++k) DrawLineEx(Vector2{(float)cx, (float)cy}, Vector2{cx + std::cos(ang + k * 2.09f) * 14.0f, cy + std::sin(ang + k * 2.09f) * 14.0f}, 3.0f, WHITE);
            break; }
        case ObjectType::SOLAR_PANEL:
            DrawRectangle(px + 3, py + 4, size - 6, size - 8, Color{25, 55, 120, 255}); DrawRectangleLines(px + 3, py + 4, size - 6, size - 8, SKYBLUE);
            DrawLine(cx, py + 4, cx, py + size - 4, SKYBLUE); DrawLine(px + 3, cy, px + size - 3, cy, SKYBLUE); break;
        case ObjectType::TURRET:
            DrawRectangle(px + 5, py + 5, size - 10, size - 10, Color{60, 66, 76, 255}); DrawRectangleLines(px + 5, py + 5, size - 10, size - 10, BLACK);
            DrawCircle(cx, cy, 6.0f, GRAY); DrawRectangle(cx, cy - 2, 12, 4, DARKGRAY); break;
        case ObjectType::TRAP:
            DrawRectangleLines(px + 6, py + 6, size - 12, size - 12, GRAY);
            for (int k = 0; k < 4; ++k) DrawTriangle(Vector2{(float)(px + 8 + k * 4), (float)(py + 18)}, Vector2{(float)(px + 10 + k * 4), (float)(py + 10)}, Vector2{(float)(px + 12 + k * 4), (float)(py + 18)}, LIGHTGRAY);
            break;
        case ObjectType::HORSESHOES:
            DrawLineEx(Vector2{(float)cx, (float)(py + 8)}, Vector2{(float)cx, (float)(py + size - 6)}, 3.0f, Color{130, 82, 42, 255}); DrawCircleLines(cx, py + 12, 5.0f, LIGHTGRAY); break;
        case ObjectType::CHESS_TABLE:
            DrawRectangle(px + 4, py + 4, size - 8, size - 8, matCol); DrawRectangleLines(px + 4, py + 4, size - 8, size - 8, matDark);
            DrawRectangle(px + 9, py + 9, 7, 7, WHITE); DrawRectangle(px + 16, py + 9, 7, 7, BLACK); DrawRectangle(px + 9, py + 16, 7, 7, BLACK); DrawRectangle(px + 16, py + 16, 7, 7, WHITE); break;
        case ObjectType::LAMP_TORCH:
            DrawRectangle(cx - 2, cy - 2, 4, 14, Color{110, 75, 40, 255});
            DrawTriangle(Vector2{(float)cx, (float)(cy - 12)}, Vector2{(float)(cx - 5), (float)(cy - 1)}, Vector2{(float)(cx + 5), (float)(cy - 1)}, ORANGE);
            DrawTriangle(Vector2{(float)cx, (float)(cy - 7)}, Vector2{(float)(cx - 2), (float)(cy - 1)}, Vector2{(float)(cx + 2), (float)(cy - 1)}, YELLOW); break;
        case ObjectType::LAMP_STANDING:
            DrawRectangle(cx - 1, cy - 2, 3, 14, Color{90, 95, 105, 255}); DrawCircle(cx, cy - 6, 6.0f, Color{255, 240, 170, 255}); DrawCircleLines(cx, cy - 6, 6.0f, Color{120, 110, 70, 255}); break;
        case ObjectType::BATTERY:
            DrawRectangle(px + 6, py + 7, size - 12, size - 12, Color{60, 66, 76, 255}); DrawRectangleLines(px + 6, py + 7, size - 12, size - 12, BLACK);
            DrawRectangle(cx - 3, py + 4, 6, 4, Color{150, 155, 165, 255}); DrawRectangle(px + 9, py + 12, size - 18, 4, LIME); break;
        case ObjectType::GENERATOR:
            DrawRectangle(px + 3, py + 6, size - 6, size - 10, Color{120, 80, 50, 255}); DrawRectangleLines(px + 3, py + 6, size - 6, size - 10, BLACK);
            DrawCircle(cx, cy + 1, 6.0f, Color{70, 74, 82, 255}); DrawCircle(cx, cy + 1, 2.5f, ORANGE); DrawRectangle(px + 8, py + 2, 4, 5, Color{60, 60, 66, 255}); break;
        case ObjectType::SANDBAGS:
            DrawRectangle(px + 3, py + 12, 12, 8, Color{185, 165, 110, 255}); DrawRectangle(px + 15, py + 12, 12, 8, Color{175, 155, 100, 255}); DrawRectangle(px + 9, py + 5, 12, 8, Color{190, 170, 115, 255});
            DrawRectangleLines(px + 3, py + 12, 12, 8, Color{110, 95, 60, 255}); DrawRectangleLines(px + 15, py + 12, 12, 8, Color{110, 95, 60, 255}); DrawRectangleLines(px + 9, py + 5, 12, 8, Color{110, 95, 60, 255}); break;
        default: break; }
    if (cell.hasRoof && showRoofOverlay) {
        DrawRectangle(px, py, size, size, Color{110, 75, 40, 110}); DrawRectangleLines(px, py, size, size, Color{180, 140, 80, 160}); }}
void DrawPawnIcon(int cx, int cy, Color shirt, const std::string& name, bool isWorking, bool isSleeping, bool isSelected, bool drafted, bool downed, int weapon, float hpFrac) {
    if (isSelected) DrawCircleLines(cx, cy + 2, 14.0f, GOLD);
    bool lying = isSleeping || downed;
    bool pt = DrawTex(lying ? "pawn_down_body" : "pawn_body", cx - 16, cy - 16, 32, 32, shirt);
    if (pt) {
        DrawTex(lying ? "pawn_down_head" : "pawn_head", cx - 16, cy - 16, 32, 32);
        if (drafted) DrawCircleLines(cx, cy + 2, 12.0f, ORANGE);
        if (isWorking) DrawText("*", cx + 8, cy - 14, 16, YELLOW);
    } else if (isSleeping || downed) {
        DrawCircle(cx - 3, cy + 2, 8.0f, shirt); DrawCircleLines(cx - 3, cy + 2, 8.0f, BLACK);
        DrawCircle(cx + 6, cy + 2, 5.5f, Color{255, 218, 180, 255}); DrawCircleLines(cx + 6, cy + 2, 5.5f, BLACK);
        DrawText(downed ? "!" : "Zzz", cx + 4, cy - 14, 12, downed ? RED : SKYBLUE);
    } else {
        DrawCircle(cx, cy + 4, 9.0f, shirt); DrawCircleLines(cx, cy + 4, 9.0f, BLACK);
        DrawCircle(cx, cy - 5, 6.0f, Color{255, 218, 180, 255}); DrawCircleLines(cx, cy - 5, 6.0f, BLACK);
        if (weapon > 0) { Color wc = WEAPONS[weapon].ranged ? Color{60, 60, 66, 255} : Color{200, 200, 210, 255};
            DrawLineEx(Vector2{(float)(cx + 6), (float)(cy + 4)}, Vector2{(float)(cx + (WEAPONS[weapon].ranged ? 15 : 12)), (float)(cy + (WEAPONS[weapon].ranged ? 2 : -4))}, 3.0f, wc); }
        if (drafted) DrawCircleLines(cx, cy + 2, 12.0f, ORANGE);
        if (isWorking) DrawText("*", cx + 8, cy - 14, 16, YELLOW); }
    if (hpFrac < 0.999f) { DrawRectangle(cx - 12, cy - 18, 24, 4, BLACK); DrawRectangle(cx - 11, cy - 17, (int)(22 * std::clamp(hpFrac, 0.0f, 1.0f)), 2, RED); }
    int textW = MeasureText(name.c_str(), 10);
    DrawRectangle(cx - textW / 2 - 2, cy + 14, textW + 4, 12, Color{0, 0, 0, 170});
    DrawText(name.c_str(), cx - textW / 2, cy + 15, 10, isSelected ? GOLD : WHITE); }
void DrawCritterIcon(int cx, int cy, const Critter& c) {
    Color body = Color{150, 105, 60, 255}; float r = 8.0f;
    switch (c.type) {
        case CType::DEER: body = Color{170, 120, 70, 255}; r = 8.0f; break;
        case CType::MUFFALO: body = Color{95, 70, 55, 255}; r = 12.0f; break;
        case CType::BOAR: body = Color{80, 70, 65, 255}; r = 9.0f; break;
        case CType::WOLF: body = Color{140, 145, 155, 255}; r = 8.0f; break;
        case CType::RAIDER: body = Color{150, 30, 30, 255}; break;
        case CType::TRADER: body = Color{150, 70, 190, 255}; break; }
    static const char* cTex[6] = {"raider", "trader", "deer", "muffalo", "boar", "wolf"};
    bool ctx2 = DrawTex(cTex[(int)c.type], cx - 16, cy - 16, 32, 32);
    if (ctx2) {}
    else if (c.type == CType::RAIDER || c.type == CType::TRADER) {
        DrawCircle(cx, cy + 4, 9.0f, body); DrawCircleLines(cx, cy + 4, 9.0f, BLACK);
        DrawCircle(cx, cy - 5, 6.0f, Color{240, 200, 165, 255}); DrawCircleLines(cx, cy - 5, 6.0f, BLACK);
        if (c.type == CType::TRADER) DrawText("$", cx + 8, cy - 14, 14, GOLD);
        else if (c.weapon > 0) DrawLineEx(Vector2{(float)(cx + 6), (float)(cy + 4)}, Vector2{(float)(cx + 14), (float)(cy - 2)}, 3.0f, DARKGRAY);
    } else {
        DrawCircle(cx, cy + 3, r, body); DrawCircleLines(cx, cy + 3, r, BLACK); DrawCircle(cx + (int)r - 2, cy - 2, 4.0f, body); DrawCircleLines(cx + (int)r - 2, cy - 2, 4.0f, BLACK);
        if (c.type == CType::DEER) { DrawLine(cx + 6, cy - 6, cx + 4, cy - 12, Color{200, 180, 140, 255}); DrawLine(cx + 10, cy - 6, cx + 12, cy - 12, Color{200, 180, 140, 255}); }
        if (c.hostile) DrawText("!", cx - 2, cy - 18, 14, RED); }
    if (c.hp < c.maxHp) { DrawRectangle(cx - 12, cy - 18, 24, 4, BLACK); DrawRectangle(cx - 11, cy - 17, (int)(22 * std::clamp(c.hp / c.maxHp, 0.0f, 1.0f)), 2, c.hostile ? RED : LIME); }
    if (c.type == CType::RAIDER || c.type == CType::TRADER) {
        int tw = MeasureText(c.name.c_str(), 10); DrawRectangle(cx - tw / 2 - 2, cy + 14, tw + 4, 12, Color{0, 0, 0, 170}); DrawText(c.name.c_str(), cx - tw / 2, cy + 15, 10, c.type == CType::RAIDER ? Color{255, 120, 120, 255} : Color{220, 170, 255, 255}); }}
// ============================ PATHFINDING ============================
GridCoord FindNextStep(const CellGrid& grid, int startX, int startY, int targetX, int targetY, bool exactTile, bool breach = false) {
    int rows = (int)grid.size(), cols = (int)grid[0].size();
    if (startX < 0 || startX >= cols || startY < 0 || startY >= rows) return GridCoord{-1, -1};
    auto isBlocked = [&](int x, int y) {
        if (x < 0 || x >= cols || y < 0 || y >= rows) return true;
        const LocalCell& c = grid[y][x];
        if (c.terrain == BaseTerrain::WATER) return true;
        if (c.obj == ObjectType::WALL) return !breach;
        if (c.obj == ObjectType::WINDMILL || c.obj == ObjectType::SOLAR_PANEL || c.obj == ObjectType::TURRET) return true;
        return false; };
    auto getTileCost = [&](int x, int y) {
        const LocalCell& c = grid[y][x];
        if (c.obj == ObjectType::WALL) return 60;
        if (c.obj == ObjectType::TRAP) return breach ? 10 : 200;
        if (c.obj == ObjectType::SANDBAGS) return 25;
        if (c.obj == ObjectType::DOOR) return 5;
        if (c.floor != FloorType::NONE) return 7;
        if (c.obj == ObjectType::TREE || c.obj == ObjectType::BOULDER || c.obj == ObjectType::STEEL_VEIN) return 18;
        return 10; };
    IntList dist(rows * cols, 999999), parent(rows * cols, -1);
    PathQueue pq;
    int startIdx = startY * cols + startX;
    dist[startIdx] = 0; pq.push(GridCoord{0, startIdx});
    int goalIdx = -1;
    const IntList dx8 = {0, 0, -1, 1, -1, 1, -1, 1}, dy8 = {-1, 1, 0, 0, -1, -1, 1, 1};
    while (!pq.empty()) {
        int d = pq.top().first, curr = pq.top().second; pq.pop();
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
            if (dist[curr] + stepCost < dist[nextIdx]) { dist[nextIdx] = dist[curr] + stepCost; parent[nextIdx] = curr; pq.push(GridCoord{dist[nextIdx], nextIdx}); }}}
    if (goalIdx == -1) return GridCoord{-1, -1};
    int trace = goalIdx;
    if (trace == startIdx) return GridCoord{startX, startY};
    while (parent[trace] != -1 && parent[trace] != startIdx) trace = parent[trace];
    return GridCoord{trace % cols, trace / cols}; }
// ============================ MAIN ============================
int main() {
    const int tileSize = 32, viewCols = 50, viewRows = 29, scrollMargin = 8, topBarHeight = 72, bottomBarHeight = 140;
    const int screenWidth = viewCols * tileSize, screenHeight = viewRows * tileSize + topBarHeight;
    const int mapBottomY = screenHeight - bottomBarHeight, visibleMapRows = (mapBottomY - topBarHeight) / tileSize;
    const int PX = 100, PY = 90, PW = 1400, PH = 760;
    InitWindow(screenWidth, screenHeight, "RimWorld-Style Colony & World Sim");
    SetExitKey(KEY_NULL);
    SetWindowPosition(40, 25);
    SetTargetFPS(60);
    std::random_device rd; std::mt19937 seedRng(rd()); std::uniform_int_distribution<int> seedDist(1000, 999999);
    unsigned int seed = (unsigned int)seedDist(seedRng);
    SimplexFBM gen(seed);
    GameMode mode = GameMode::WORLD_EXPLORE;
    std::string statusMessage = "Explore the World Map with WASD/Arrows. ENTER on land to settle (or re-enter a colony). T = trade, X = raid at settlements.";
    auto getWorldElevation = [&](int wx, int wy) { return (gen.fractal(wx * 0.09, wy * 0.09, 4) + 1.0) * 0.5; };
    auto isWorldWalkable = [&](int wx, int wy) { double h = getWorldElevation(wx, wy); return (h >= 0.42 && h < 0.75); };
    auto settlementTypeAt = [&](int wx, int wy) -> int {
        unsigned int h = HashTile(wx, wy, seed + 777u);
        if (h % 61 != 0) return -1;
        double e = getWorldElevation(wx, wy);
        if (e < 0.48 || e >= 0.75) return -1;
        if (std::abs(wx) < 4 && std::abs(wy) < 4) return -1;
        return (int)((h / 61) % 3); };
    auto settlementName = [&](int wx, int wy, int type) -> std::string {
        static const char* first[6] = {"Oak", "Stone", "Raven", "Ash", "Iron", "Mill"};
        static const char* second[6] = {"haven", "ford", "hold", "reach", "stead", "crossing"};
        unsigned int h = HashTile(wx, wy, seed + 991u);
        std::string n = std::string(first[h % 6]) + second[(h / 6) % 6];
        return n + (type == 0 ? " (Outlander Union)" : (type == 1 ? " (Tribal Confederacy)" : " (Pirate Gang)")); };
    int playerX = 0, playerY = 0; bool foundSpawn = false;
    for (int radius = 0; radius < 200 && !foundSpawn; ++radius)
        for (int dy = -radius; dy <= radius && !foundSpawn; ++dy)
            for (int dx = -radius; dx <= radius && !foundSpawn; ++dx) {
                double h = getWorldElevation(dx, dy);
                if (h >= 0.48 && h < 0.75) { playerX = dx; playerY = dy; foundSpawn = true; }}
    int wcamX = playerX - viewCols / 2, wcamY = playerY - viewRows / 2, camX = 0, camY = 0;
    float moveCooldown = 0.0f, scanTimer = 0.0f;
    ColonyState C; bool haveCur = false; GridCoord curKey{0, 0};
    std::map<GridCoord, ColonyState> colonies; std::set<GridCoord> destroyedSettlements; std::map<GridCoord, TradeStock> settlementStocks;
    int gameDay = 1, speed = 1, lastSpeed = 1; float gameHour = 8.0f;
    bool showRoofOverlay = true;
    int selectedPawnIdx = -1, selTileX = -1, selTileY = -1;
    int openPanel = 0; // 0 none, 1 work, 2 schedule, 3 assign, 4 research, 5 trade
    int selCat = -1, selTool = -1, schedBrush = SA_WORK;
    MaterialType selMat = MaterialType::WOOD;
    bool dragging = false; int dragX0 = 0, dragY0 = 0;
    bool ctxOpen = false; int ctxX = 0, ctxY = 0; std::vector<CtxItem> ctx;
    int tradeCritterId = -1; bool tradeSettle = false; GridCoord tradeKey{0, 0};
    std::vector<Shot> shots;
    ResearchList researchTree = {
        {"Electricity", "Stove, Wind Turbine, Conduit, Lamp, Generator", 800, 0, -1, 0, 0},
        {"Batteries", "Unlocks the Battery (stores power)", 450, 0, 0, 300, 0},
        {"Solar Power", "Unlocks Solar Generators", 900, 0, 0, 600, 0},
        {"Multi-analyzer", "Research is 50% faster", 1200, 0, 0, 900, 0},
        {"Stonecutting", "Stone building & Stone Tile", 350, 0, -1, 0, 135},
        {"Complex Furniture", "Unlocks the Chess Table", 450, 0, 4, 300, 135},
        {"Fortification", "Unlocks Sandbags (cover)", 300, 0, 4, 600, 135},
        {"Smithing", "Unlocks Smithy, Knife, Club, Mace", 300, 0, -1, 0, 270},
        {"Blade Forging", "Unlocks the Longsword", 500, 0, 7, 300, 270},
        {"Trapmaking", "Unlocks Deadfall Traps", 350, 0, 7, 600, 270},
        {"Improved Tools", "All colony work 15% faster", 600, 0, 7, 900, 270},
        {"Machining", "Machining Table, Revolver", 900, 0, 7, 0, 405},
        {"Firearms", "Unlocks Rifle & Shotgun", 800, 0, 11, 300, 405},
        {"Mini-turrets", "Unlocks the Mini-turret", 900, 0, 11, 600, 405},
        {"Devilstrand", "Unlocks Healroot growing zones", 700, 0, -1, 0, 540},
        {"Advanced Farming", "Crops grow 25% faster", 600, 0, 14, 300, 540}};
    int activeResearchIdx = 0;
    auto isResearchDone = [&](int idx) { return (idx < 0 || idx >= (int)researchTree.size()) ? true : (researchTree[idx].progress >= researchTree[idx].cost); };
    // ---------------- Architect tool table ----------------
    const StrList catNames = {"Orders", "Zone", "Structure", "Production", "Furniture", "Power", "Security", "Joy", "Floors"};
    std::vector<ToolDef> tools;
    auto addTool = [&](const char* n, const char* sub, int cat, ToolKind k) -> ToolDef& {
        tools.push_back(ToolDef()); ToolDef& t = tools.back(); t.name = n; t.sub = sub; t.cat = cat; t.kind = k; return t; };
    addTool("Mine", "Designate rock & ore", 0, ToolKind::MINE);
    addTool("Chop wood", "Designate trees", 0, ToolKind::CHOP);
    addTool("Harvest", "Bushes & ripe crops", 0, ToolKind::HARVEST);
    addTool("Hunt", "Mark animals", 0, ToolKind::HUNT);
    addTool("Deconstruct", "Remove buildings", 0, ToolKind::DECON);
    addTool("Cancel", "Cancel orders", 0, ToolKind::CANCEL);
    { auto& t = addTool("Rice zone", "Fast growth", 1, ToolKind::ZONE_GROW); t.crop = 1; }
    { auto& t = addTool("Potato zone", "Balanced yield", 1, ToolKind::ZONE_GROW); t.crop = 2; }
    { auto& t = addTool("Corn zone", "High yield", 1, ToolKind::ZONE_GROW); t.crop = 3; }
    { auto& t = addTool("Healroot zone", "Needs Devilstrand", 1, ToolKind::ZONE_GROW); t.crop = 4; t.research = R_DEVIL; }
    addTool("Clear zone", "Remove growing zone", 1, ToolKind::ZONE_CLEAR);
    addTool("Roof area", "Build roofs", 1, ToolKind::ROOF);
    addTool("Remove roof", "Remove roofs", 1, ToolKind::ROOF_REMOVE);
    { auto& t = addTool("Wall", "Blocks movement", 2, ToolKind::BUILD); t.obj = ObjectType::WALL; t.matChoice = true; t.matAmt = 5; t.work = 1.4f; t.blocks = true; }
    { auto& t = addTool("Door", "Pawns pass freely", 2, ToolKind::BUILD); t.obj = ObjectType::DOOR; t.matChoice = true; t.matAmt = 5; t.work = 1.2f; }
    { auto& t = addTool("Campfire", "30 Wood, cooks, no power", 3, ToolKind::BUILD); t.obj = ObjectType::CAMPFIRE; t.wood = 30; t.work = 1.5f; }
    { auto& t = addTool("Electric stove", "15 Stone, 200W", 3, ToolKind::BUILD); t.obj = ObjectType::STOVE; t.stone = 15; t.work = 2.0f; t.research = R_ELECTRICITY; t.fixedMat = MaterialType::STONE; }
    { auto& t = addTool("Research bench", "15 of material", 3, ToolKind::BUILD); t.obj = ObjectType::RESEARCH_BENCH; t.matChoice = true; t.matAmt = 15; t.work = 2.0f; }
    { auto& t = addTool("Smithy", "30 Steel 20 Stone", 3, ToolKind::BUILD); t.obj = ObjectType::SMITHY; t.steel = 30; t.stone = 20; t.work = 2.5f; t.research = R_SMITHING; t.fixedMat = MaterialType::STEEL; }
    { auto& t = addTool("Machining table", "60 Steel, 150W", 3, ToolKind::BUILD); t.obj = ObjectType::MACHINING; t.steel = 60; t.work = 3.0f; t.research = R_MACHINING; t.fixedMat = MaterialType::STEEL; }
    { auto& t = addTool("Sleeping spot", "3 Wood", 4, ToolKind::BUILD); t.obj = ObjectType::SLEEP_SPOT; t.wood = 3; t.work = 0.5f; }
    { auto& t = addTool("Bed", "10 of material", 4, ToolKind::BUILD); t.obj = ObjectType::BED; t.matChoice = true; t.matAmt = 10; t.work = 1.4f; }
    { auto& t = addTool("Table", "8 of material", 4, ToolKind::BUILD); t.obj = ObjectType::TABLE; t.matChoice = true; t.matAmt = 8; t.work = 1.4f; }
    { auto& t = addTool("Stool", "3 of material", 4, ToolKind::BUILD); t.obj = ObjectType::STOOL; t.matChoice = true; t.matAmt = 3; t.work = 0.8f; }
    { auto& t = addTool("Torch lamp", "8 Wood, lights area", 4, ToolKind::BUILD); t.obj = ObjectType::LAMP_TORCH; t.wood = 8; t.work = 0.6f; }
    { auto& t = addTool("Standing lamp", "10 Steel, 30W", 4, ToolKind::BUILD); t.obj = ObjectType::LAMP_STANDING; t.steel = 10; t.work = 0.8f; t.research = R_ELECTRICITY; t.fixedMat = MaterialType::STEEL; }
    { auto& t = addTool("Battery", "25 Steel, stores power", 5, ToolKind::BUILD); t.obj = ObjectType::BATTERY; t.steel = 25; t.work = 1.5f; t.research = R_BATTERIES; t.fixedMat = MaterialType::STEEL; }
    { auto& t = addTool("Fuel generator", "40 Steel, burns wood, 1000W", 5, ToolKind::BUILD); t.obj = ObjectType::GENERATOR; t.steel = 40; t.work = 2.5f; t.research = R_ELECTRICITY; t.blocks = true; t.fixedMat = MaterialType::STEEL; }
    { auto& t = addTool("Power conduit", "1 Wood", 5, ToolKind::BUILD); t.buildKind = 2; t.wood = 1; t.work = 0.5f; t.research = R_ELECTRICITY; }
    { auto& t = addTool("Wind turbine", "20 Wood 10 Stone", 5, ToolKind::BUILD); t.obj = ObjectType::WINDMILL; t.wood = 20; t.stone = 10; t.work = 2.5f; t.research = R_ELECTRICITY; t.blocks = true; t.fixedMat = MaterialType::STONE; }
    { auto& t = addTool("Solar generator", "25 Stone", 5, ToolKind::BUILD); t.obj = ObjectType::SOLAR_PANEL; t.stone = 25; t.work = 2.5f; t.research = R_SOLAR; t.blocks = true; t.fixedMat = MaterialType::STONE; }
    { auto& t = addTool("Mini-turret", "50 Steel", 6, ToolKind::BUILD); t.obj = ObjectType::TURRET; t.steel = 50; t.work = 3.0f; t.research = R_TURRETS; t.blocks = true; t.fixedMat = MaterialType::STEEL; }
    { auto& t = addTool("Deadfall trap", "15 Steel, hits raiders", 6, ToolKind::BUILD); t.obj = ObjectType::TRAP; t.steel = 15; t.work = 1.0f; t.research = R_TRAPS; t.fixedMat = MaterialType::STEEL; }
    { auto& t = addTool("Sandbags", "6 Wood, cover & slows", 6, ToolKind::BUILD); t.obj = ObjectType::SANDBAGS; t.wood = 6; t.work = 0.8f; t.research = R_FORTIFY; }
    { auto& t = addTool("Horseshoes pin", "10 Wood", 7, ToolKind::BUILD); t.obj = ObjectType::HORSESHOES; t.wood = 10; t.work = 1.0f; }
    { auto& t = addTool("Chess table", "15 of material", 7, ToolKind::BUILD); t.obj = ObjectType::CHESS_TABLE; t.matChoice = true; t.matAmt = 15; t.work = 1.5f; t.research = R_FURNITURE; }
    { auto& t = addTool("Wooden floor", "2 Wood", 8, ToolKind::BUILD); t.buildKind = 1; t.floor = FloorType::WOOD_FLOOR; t.wood = 2; t.work = 0.8f; }
    { auto& t = addTool("Stone tile", "2 Stone", 8, ToolKind::BUILD); t.buildKind = 1; t.floor = FloorType::STONE_TILE; t.stone = 2; t.work = 1.0f; t.research = R_STONECUT; t.fixedMat = MaterialType::STONE; }
    { auto& t = addTool("Metal tile", "3 Steel", 8, ToolKind::BUILD); t.buildKind = 1; t.floor = FloorType::STEEL_TILE; t.steel = 3; t.work = 1.0f; t.fixedMat = MaterialType::STEEL; }
    // ---------------- Colony helper lambdas ----------------
    auto inMap = [&](int x, int y) { return x >= 0 && x < localCols && y >= 0 && y < localRows; };
    auto isBedObj = [&](ObjectType o) { return o == ObjectType::BED || o == ObjectType::SLEEP_SPOT; };
    auto isWalkable = [&](int x, int y) {
        if (!inMap(x, y)) return false;
        const LocalCell& c = C.map[y][x];
        if (c.terrain == BaseTerrain::WATER) return false;
        if (c.obj == ObjectType::WALL || c.obj == ObjectType::WINDMILL || c.obj == ObjectType::SOLAR_PANEL || c.obj == ObjectType::TURRET) return false;
        return true; };
    auto findJob = [&](int id) { for (size_t i = 0; i < C.jobs.size(); ++i) if (C.jobs[i].id == id) return (int)i; return -1; };
    auto jobIdxAt = [&](int x, int y) {
        for (size_t i = 0; i < C.jobs.size(); ++i) {
            const Job& j = C.jobs[i];
            if (j.tx == x && j.ty == y && j.type != TaskType::COOK_MEAL && j.type != TaskType::RESEARCH_WORK && j.type != TaskType::CRAFT && j.type != TaskType::HUNT) return (int)i; }
        return -1; };
    auto releaseJob = [&](Pawn& p) {
        int ji = findJob(p.jobId);
        if (ji >= 0) C.jobs[ji].assignedPawn = -1;
        p.jobId = -1; };
    auto refundJob = [&](const Job& j) { C.wood += j.cw; C.stone += j.cs; C.steel += j.cst; };
    auto eraseJobAt = [&](int ji) {
        int id = C.jobs[ji].id;
        for (auto& p : C.pawns) if (p.jobId == id) p.jobId = -1;
        C.jobs.erase(C.jobs.begin() + ji); };
    auto purgeAutoJobs = [&](int x, int y) {
        for (int i = (int)C.jobs.size() - 1; i >= 0; --i) {
            Job& j = C.jobs[i];
            if (j.tx == x && j.ty == y && (j.type == TaskType::COOK_MEAL || j.type == TaskType::RESEARCH_WORK || j.type == TaskType::CRAFT)) {
                if (j.type == TaskType::CRAFT) refundJob(j);
                eraseJobAt(i); }}};
    auto addJob = [&](TaskType t, int x, int y, float work) -> Job& {
        C.jobs.push_back(Job()); Job& j = C.jobs.back(); j.id = C.nextJobId++; j.type = t; j.tx = x; j.ty = y; j.workRequired = work; return j; };
    auto lineClear = [&](int x0, int y0, int x1, int y1) {
        int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy, x = x0, y = y0;
        while (!(x == x1 && y == y1)) {
            if (!(x == x0 && y == y0) && inMap(x, y) && C.map[y][x].obj == ObjectType::WALL) return false;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x += sx; }
            if (e2 <= dx) { err += dx; y += sy; }}
        return true; };
    auto critterAt = [&](int x, int y) -> Critter* { for (auto& c : C.critters) if (c.x == x && c.y == y && c.hp > 0 && !c.gone) return &c; return nullptr; };
    auto pawnAtTile = [&](int x, int y) { for (const auto& p : C.pawns) if (!p.dead && p.x == x && p.y == y) return true; return critterAt(x, y) != nullptr; };
    auto aliveCount = [&]() { int n = 0; for (const auto& p : C.pawns) if (!p.dead) n++; return n; };
    auto hasPower = [&]() { return (C.powerGen >= C.powerUse && C.powerGen > 0) || C.onBattery; };
    auto damagePawn = [&](Pawn& p, float dmg) {
        if (p.dead) return;
        p.hp -= dmg;
        if (p.hp <= 0.0f && !p.downed) {
            p.downed = true; p.drafted = false; p.isSleeping = false; p.doingRecreation = false; p.gotoX = -1;
            int ji = findJob(p.jobId); if (ji >= 0) C.jobs[ji].assignedPawn = -1; p.jobId = -1;
            statusMessage = p.name + " is down!"; }
        if (p.hp <= -40.0f) {
            p.dead = true; p.downed = false; statusMessage = p.name + " has died.";
            if (p.bedX >= 0 && inMap(p.bedX, p.bedY)) C.map[p.bedY][p.bedX].bedOwner = -1;
            p.bedX = -1; p.bedY = -1; }};
    auto damageCritter = [&](Critter& c, float dmg) {
        c.hp -= dmg;
        if (c.type == CType::BOAR || c.type == CType::WOLF) c.hostile = true; };
    auto rangedHit = [&](int skill, float d) { return std::clamp(0.62f + skill * 0.025f - d * 0.025f, 0.15f, 0.95f); };
    auto pawnAttack = [&](Pawn& p, Critter& c) {
        if (p.atkTimer > 0.0f) return;
        const WeaponDef& wd = WEAPONS[p.weapon];
        int sk = wd.ranged ? (int)SkillId::SHOOTING : (int)SkillId::MELEE;
        float d = Dist(p.x, p.y, c.x, c.y);
        float chance = wd.ranged ? rangedHit(p.skillLevel[sk], d) : std::clamp(0.7f + p.skillLevel[sk] * 0.015f, 0.5f, 0.95f);
        p.atkTimer = wd.cd / (0.75f + p.skillLevel[sk] * 0.03f);
        GrantSkillXP(p, (SkillId)sk, 120.0f, statusMessage);
        if (wd.ranged) shots.push_back(Shot{(float)p.x, (float)p.y, (float)c.x, (float)c.y, 0.12f, YELLOW});
        if (wd.ranged && C.map[c.y][c.x].obj == ObjectType::SANDBAGS) chance *= 0.6f;
        if (GetRandomValue(1, 1000) <= (int)(chance * 1000)) damageCritter(c, wd.dmg * (0.8f + GetRandomValue(0, 40) * 0.01f)); };
    auto edgeSpawn = [&](int& ox, int& oy) {
        for (int t = 0; t < 80; ++t) {
            int side = GetRandomValue(0, 3), x, y;
            if (side == 0) { x = GetRandomValue(0, localCols - 1); y = 0; } else if (side == 1) { x = GetRandomValue(0, localCols - 1); y = localRows - 1; }
            else if (side == 2) { x = 0; y = GetRandomValue(0, localRows - 1); } else { x = localCols - 1; y = GetRandomValue(0, localRows - 1); }
            if (isWalkable(x, y)) { ox = x; oy = y; return; }}
        ox = 2; oy = 2; C.map[2][2].terrain = BaseTerrain::GRASS; C.map[2][2].obj = ObjectType::NONE; };
    auto makeStock = [&](int type, unsigned int h, const char* who) {
        TradeStock s; s.who = who; s.silver = 300 + (int)(h % 500);
        s.qty[0] = 40 + (int)(h % 60); s.qty[1] = 30 + (int)((h >> 3) % 50); s.qty[2] = 20 + (int)((h >> 5) % 50);
        s.qty[3] = 50 + (int)((h >> 7) % 60); s.qty[4] = 10 + (int)((h >> 9) % 20);
        for (int w = 1; w < WEAPON_COUNT; ++w) { bool gun = WEAPONS[w].ranged; if (gun && type == 1) continue; s.qty[4 + w] = (int)((h >> (11 + w)) % 3); }
        return s; };
    auto spawnCritter = [&](CType t, int x, int y) -> Critter& {
        C.critters.push_back(Critter()); Critter& c = C.critters.back(); c.id = C.nextCritterId++; c.type = t; c.x = x; c.y = y;
        switch (t) {
            case CType::DEER: c.name = "Deer"; c.maxHp = 40; break;
            case CType::MUFFALO: c.name = "Muffalo"; c.maxHp = 120; c.meleeDmg = 14; break;
            case CType::BOAR: c.name = "Boar"; c.maxHp = 60; c.meleeDmg = 14; break;
            case CType::WOLF: c.name = "Wolf"; c.maxHp = 55; c.meleeDmg = 12; c.hostile = true; break;
            case CType::RAIDER: c.name = "Raider"; c.maxHp = 75; c.hostile = true; break;
            case CType::TRADER: c.name = "Trader"; c.maxHp = 60; break; }
        c.hp = c.maxHp; return c; };
    auto spawnWildlife = [&](int n) {
        for (int i = 0; i < n; ++i) for (int t = 0; t < 40; ++t) {
            int x = GetRandomValue(0, localCols - 1), y = GetRandomValue(0, localRows - 1);
            if (C.map[y][x].terrain != BaseTerrain::GRASS || C.map[y][x].obj != ObjectType::NONE || Dist(x, y, localCols / 2, localRows / 2) < 10.0f) continue;
            int r = GetRandomValue(1, 100);
            spawnCritter(r <= 45 ? CType::DEER : (r <= 75 ? CType::MUFFALO : CType::BOAR), x, y); break; }};
    auto spawnRaid = [&]() {
        int alive = aliveCount(); if (alive == 0) return;
        int n = std::clamp(1 + (int)(gameDay * 0.5f) + GetRandomValue(0, 1), 1, 10);
        n = std::min(n, alive * 2 + 1);
        int sx, sy; edgeSpawn(sx, sy);
        for (int i = 0; i < n; ++i) {
            int x = std::clamp(sx + GetRandomValue(-2, 2), 0, localCols - 1), y = std::clamp(sy + GetRandomValue(-2, 2), 0, localRows - 1);
            if (!isWalkable(x, y)) { x = sx; y = sy; }
            Critter& c = spawnCritter(CType::RAIDER, x, y);
            int r = GetRandomValue(1, 100);
            if (gameDay < 6) c.weapon = (r <= 35) ? 1 : (r <= 65 ? 2 : 5);
            else c.weapon = (r <= 15) ? 1 : (r <= 30 ? 3 : (r <= 50 ? 5 : (r <= 75 ? 6 : 7)));
            c.name = "Raider"; }
        statusMessage = TextFormat("RAID! %d raiders approach from the map edge!", n); };
    auto spawnTrader = [&]() {
        for (const auto& c : C.critters) if (c.type == CType::TRADER && !c.gone) return;
        int sx, sy; edgeSpawn(sx, sy);
        Critter& c = spawnCritter(CType::TRADER, sx, sy);
        c.stayHours = 12.0f; c.name = "Trader";
        c.stock = makeStock(0, HashTile(gameDay, C.nextCritterId, seed + 31u), "Orbital Trader");
        statusMessage = "A trade caravan has arrived. Click the Trader to trade!"; };
    auto saveCurrent = [&]() { if (haveCur) { C.camX = camX; C.camY = camY; colonies[curKey] = C; }};
    auto generateColony = [&](int wx, int wy) {
        C = ColonyState();
        C.map.assign(localRows, CellRow(localCols));
        C.worldX = wx; C.worldY = wy;
        double worldElev = getWorldElevation(wx, wy);
        unsigned int sectorSeed = HashTile(wx, wy, seed + 5555);
        SimplexFBM localNoise(sectorSeed);
        C.biome = (worldElev < 0.53) ? "Coastal Beach" : (worldElev > 0.66 ? "Mountain Foothills" : "Temperate Forest");
        for (int y = 0; y < localRows; ++y) for (int x = 0; x < localCols; ++x) {
            LocalCell cell;
            double n = (localNoise.fractal(x * 0.08, y * 0.08, 3) + 1.0) * 0.5;
            unsigned int h = HashTile(x, y, sectorSeed);
            double adjusted = n * 0.65 + worldElev * 0.35;
            if (adjusted < 0.36) cell.terrain = BaseTerrain::WATER;
            else if (adjusted < 0.43) cell.terrain = BaseTerrain::SAND;
            else if (adjusted > 0.72) { cell.terrain = BaseTerrain::ROCK_GROUND; if (h % 4 == 0) cell.obj = (h % 13 == 0) ? ObjectType::STEEL_VEIN : ObjectType::BOULDER; }
            else {
                cell.terrain = BaseTerrain::GRASS;
                int roll = h % 100, treeChance = (worldElev >= 0.53 && worldElev <= 0.66) ? 18 : 8;
                if (roll < treeChance) cell.obj = ObjectType::TREE;
                else if (roll < treeChance + 4) cell.obj = (h % 11 == 0) ? ObjectType::STEEL_VEIN : ObjectType::BOULDER;
                else if (roll < treeChance + 7) cell.obj = ObjectType::BERRY_BUSH; }
            C.map[y][x] = cell; }
        int centerX = localCols / 2, centerY = localRows / 2;
        for (int dy = -3; dy <= 3; ++dy) for (int dx = -3; dx <= 3; ++dx) {
            int lx = centerX + dx, ly = centerY + dy;
            if (C.map[ly][lx].terrain == BaseTerrain::WATER) C.map[ly][lx].terrain = BaseTerrain::GRASS;
            C.map[ly][lx].obj = ObjectType::NONE; }
        C.weaponInv[5] = 1; C.weaponInv[1] = 2; C.weaponInv[2] = 1;
        const StrList names = {"Alice", "Bob", "Maya"};
        const ColorList colors = {Color{210, 55, 55, 255}, Color{55, 130, 220, 255}, Color{220, 165, 45, 255}};
        for (int i = 0; i < 3; ++i) {
            Pawn p; p.name = names[i]; p.x = centerX + (i - 1); p.y = centerY; p.shirtColor = colors[i];
            InitPawnData(p); C.pawns.push_back(p); }
        spawnWildlife(14);
        double nowH = (gameDay - 1) * 24.0 + gameHour;
        C.nextRaidTime = nowH + 96.0; C.nextTraderTime = nowH + 40.0; C.nextAnimalTime = nowH + 6.0;
        camX = centerX - viewCols / 2; camY = centerY - visibleMapRows / 2; };
    auto enterColony = [&](int wx, int wy) {
        saveCurrent();
        GridCoord key{wx, wy};
        if (colonies.count(key)) {
            C = colonies[key]; camX = C.camX; camY = C.camY;
            double nowH = (gameDay - 1) * 24.0 + gameHour;
            C.nextRaidTime = std::max(C.nextRaidTime, nowH + 6.0);
            statusMessage = "Returned to your colony. Everything is exactly as you left it.";
        } else { generateColony(wx, wy); statusMessage = "Colony founded! Pick tools from the Architect menu; nothing happens on click until you choose one."; }
        curKey = key; haveCur = true; mode = GameMode::COLONY_SIM;
        selectedPawnIdx = -1; selTileX = selTileY = -1; selTool = -1; selCat = -1; openPanel = 0; dragging = false; ctxOpen = false; shots.clear(); };
    // ---- job picking ----
    auto pickJob = [&](size_t pIdx, bool excludeResearch) -> int {
        Pawn& p = C.pawns[pIdx]; int best = -1; long bestScore = 1000000000L;
        for (size_t ji = 0; ji < C.jobs.size(); ++ji) {
            const Job& j = C.jobs[ji];
            if (j.assignedPawn != -1 || p.failedJobs.count(j.id)) continue;
            if (j.forcedPawn != -1 && j.forcedPawn != (int)pIdx) continue;
            if (excludeResearch && j.type == TaskType::RESEARCH_WORK) continue;
            bool forced = (j.forcedPawn == (int)pIdx);
            int prio = p.workPrio[(int)WorkTypeOf(j.type)];
            if (prio == 0 && !forced) continue;
            long d = std::abs(j.tx - p.x) + std::abs(j.ty - p.y);
            long score = forced ? d - 1000000L : (long)prio * 100000L + d;
            if (score < bestScore) { bestScore = score; best = (int)ji; }}
        return best; };
    auto applyTool = [&](const ToolDef& t, int x, int y) {
        if (!inMap(x, y)) return;
        LocalCell& c = C.map[y][x];
        switch (t.kind) {
            case ToolKind::MINE:
                if ((c.obj == ObjectType::BOULDER || c.obj == ObjectType::STEEL_VEIN) && jobIdxAt(x, y) < 0) addJob(TaskType::MINE, x, y, c.obj == ObjectType::STEEL_VEIN ? 2.4f : 1.6f);
                break;
            case ToolKind::CHOP:
                if (c.obj == ObjectType::TREE && jobIdxAt(x, y) < 0) addJob(TaskType::CHOP, x, y, 1.6f);
                break;
            case ToolKind::HARVEST:
                if (jobIdxAt(x, y) >= 0) break;
                if (c.obj == ObjectType::BERRY_BUSH) addJob(TaskType::PICK_BUSH, x, y, 0.8f);
                else if (c.crop != CropType::NONE && c.cropGrowth >= 1.0f) addJob(TaskType::HARVEST_CROP, x, y, 1.0f);
                break;
            case ToolKind::HUNT: { Critter* cr = critterAt(x, y); if (cr && cr->type != CType::RAIDER && cr->type != CType::TRADER) { cr->hunted = true; statusMessage = std::string("Hunting ") + cr->name + " designated."; } break; }
            case ToolKind::DECON:
                if (jobIdxAt(x, y) >= 0) break;
                if (c.obj != ObjectType::NONE && c.obj != ObjectType::TREE && c.obj != ObjectType::BOULDER && c.obj != ObjectType::STEEL_VEIN && c.obj != ObjectType::BERRY_BUSH) addJob(TaskType::DECONSTRUCT, x, y, 1.0f);
                else if (c.hasConduit || c.floor != FloorType::NONE) addJob(TaskType::DECONSTRUCT, x, y, 0.8f);
                break;
            case ToolKind::CANCEL: {
                int ji = jobIdxAt(x, y);
                if (ji >= 0) { refundJob(C.jobs[ji]); eraseJobAt(ji); }
                if (Critter* cr = critterAt(x, y)) cr->hunted = false;
                break; }
            case ToolKind::ZONE_GROW:
                if (t.research >= 0 && !isResearchDone(t.research)) { statusMessage = "Research '" + researchTree[t.research].title + "' first!"; break; }
                if ((c.terrain == BaseTerrain::GRASS || c.terrain == BaseTerrain::SAND) && c.obj == ObjectType::NONE && c.floor == FloorType::NONE) c.zoneCrop = t.crop;
                break;
            case ToolKind::ZONE_CLEAR:
                c.zoneCrop = 0;
                { int ji = jobIdxAt(x, y); if (ji >= 0 && C.jobs[ji].type == TaskType::SOW_CROP) eraseJobAt(ji); }
                break;
            case ToolKind::ROOF:
                if (!c.hasRoof && c.terrain != BaseTerrain::WATER && jobIdxAt(x, y) < 0) addJob(TaskType::BUILD_ROOF, x, y, 0.7f);
                break;
            case ToolKind::ROOF_REMOVE:
                if (c.hasRoof && jobIdxAt(x, y) < 0) addJob(TaskType::REMOVE_ROOF, x, y, 0.6f);
                break;
            case ToolKind::BUILD: {
                if (t.research >= 0 && !isResearchDone(t.research)) { statusMessage = "Research '" + researchTree[t.research].title + "' first!"; break; }
                if (t.matChoice && selMat == MaterialType::STONE && !isResearchDone(R_STONECUT)) { statusMessage = "Research 'Stonecutting' first to build with Stone!"; break; }
                if (jobIdxAt(x, y) >= 0 || c.terrain == BaseTerrain::WATER) break;
                if (t.buildKind == 0 && c.obj != ObjectType::NONE) break;
                if (t.buildKind == 0 && t.blocks && pawnAtTile(x, y)) break;
                if (t.buildKind == 1 && c.floor == t.floor) break;
                if (t.buildKind == 2 && c.hasConduit) break;
                int cw, cs, cst; ToolCost(t, selMat, cw, cs, cst);
                if (C.wood < cw || C.stone < cs || C.steel < cst) { statusMessage = TextFormat("Need %d Wood, %d Stone, %d Steel for %s!", cw, cs, cst, t.name); break; }
                C.wood -= cw; C.stone -= cs; C.steel -= cst;
                Job& j = addJob(t.buildKind == 0 ? TaskType::BUILD_OBJECT : (t.buildKind == 1 ? TaskType::BUILD_FLOOR : TaskType::BUILD_CONDUIT), x, y, t.work * (t.matChoice && selMat != MaterialType::WOOD ? 1.3f : 1.0f));
                j.targetObj = t.obj; j.targetFloor = t.floor; j.mat = t.matChoice ? selMat : t.fixedMat; j.cw = cw; j.cs = cs; j.cst = cst;
                c.zoneCrop = 0;
                break; }}};
    auto doSelectCategory = [&](int cat) { selCat = (selCat == cat) ? -1 : cat; selTool = -1; };
    // trade helpers
    auto getTradeStock = [&]() -> TradeStock* {
        if (tradeSettle) { auto it = settlementStocks.find(tradeKey); return it == settlementStocks.end() ? nullptr : &it->second; }
        for (auto& c : C.critters) if (c.id == tradeCritterId && !c.gone) return &c.stock;
        return nullptr; };
    auto raidSettlement = [&](GridCoord key, int type) {
        if (!haveCur) { statusMessage = "You have no colonists to send."; return; }
        float att = 0.0f; int n = 0;
        for (auto& p : C.pawns) if (!p.dead && !p.downed) { const WeaponDef& w = WEAPONS[p.weapon]; att += 8.0f + w.dmg * (w.ranged ? 1.4f : 1.0f) + p.skillLevel[w.ranged ? (int)SkillId::SHOOTING : (int)SkillId::MELEE] * 1.5f; n++; }
        if (n == 0) { statusMessage = "No fit colonists available to raid."; return; }
        unsigned int h = HashTile(key.first, key.second, seed + 777u);
        float def = 40.0f + (h % 5) * 14.0f + (type == 2 ? 25.0f : 0.0f);
        bool win = GetRandomValue(1, 1000) <= (int)(att / (att + def) * 1000.0f);
        for (auto& p : C.pawns) if (!p.dead && !p.downed && GetRandomValue(1, 100) <= (win ? 20 : 55)) p.hp = std::max(5.0f, p.hp - GetRandomValue(15, 60));
        if (win) {
            int silver = 100 + GetRandomValue(0, 250), steel = GetRandomValue(10, 60), wood = GetRandomValue(0, 40);
            C.silver += silver; C.steel += steel; C.wood += wood;
            if (GetRandomValue(1, 100) <= 50) C.weaponInv[GetRandomValue(1, WEAPON_COUNT - 1)]++;
            destroyedSettlements.insert(key);
            statusMessage = TextFormat("RAID SUCCESS! Looted %d silver, %d steel, %d wood. Settlement destroyed.", silver, steel, wood);
        } else statusMessage = "Raid FAILED! Your colonists retreated wounded."; };
    // ======================== MAIN LOOP ========================
    while (!WindowShouldClose()) {
        float dt = GetFrameTime(); float time = (float)GetTime();
        const float speedMult[4] = {0.0f, 1.0f, 3.0f, 6.0f};
        float sdt = dt * speedMult[speed];
        if (moveCooldown > 0.0f) moveCooldown -= dt;
        Vector2 mouse = GetMousePosition();
        bool lPress = IsMouseButtonPressed(MOUSE_LEFT_BUTTON), rPress = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON), lRel = IsMouseButtonReleased(MOUSE_LEFT_BUTTON), lDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
        bool allSleeping = false;
        if (mode == GameMode::COLONY_SIM && !C.pawns.empty()) {
            allSleeping = true; bool any = false;
            for (const auto& p : C.pawns) { if (p.dead) continue; any = true; if (!p.isSleeping) { allSleeping = false; break; }}
            if (!any) allSleeping = false; }
        float gameHoursElapsed = sdt * (allSleeping ? 0.25f : 0.025f);
        float gameMinutesElapsed = gameHoursElapsed * 60.0f;
        gameHour += gameHoursElapsed;
        if (gameHour >= 24.0f) { gameHour -= 24.0f; gameDay++; }
        double nowH = (gameDay - 1) * 24.0 + gameHour;
        if (IsKeyPressed(KEY_SPACE)) { if (speed == 0) speed = lastSpeed; else { lastSpeed = speed; speed = 0; }}
        if (IsKeyPressed(KEY_ONE)) { speed = 1; lastSpeed = 1; }
        if (IsKeyPressed(KEY_TWO)) { speed = 2; lastSpeed = 2; }
        if (IsKeyPressed(KEY_THREE)) { speed = 3; lastSpeed = 3; }
        // ======================= WORLD MODE INPUT =======================
        if (mode == GameMode::WORLD_EXPLORE) {
            if (openPanel == 5) {
                if (IsKeyPressed(KEY_ESCAPE)) openPanel = 0;
            } else {
                bool tapUp = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP), tapDown = IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN);
                bool tapLeft = IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT), tapRight = IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT);
                bool holdUp = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP), holdDown = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
                bool holdLeft = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT), holdRight = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
                int nextX = playerX, nextY = playerY; bool shouldMove = false;
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
                        if (playerX - wcamX < scrollMargin) wcamX = playerX - scrollMargin;
                        else if (playerX - wcamX >= viewCols - scrollMargin) wcamX = playerX - (viewCols - scrollMargin - 1);
                        if (playerY - wcamY < scrollMargin) wcamY = playerY - scrollMargin;
                        else if (playerY - wcamY >= viewRows - scrollMargin) wcamY = playerY - (viewRows - scrollMargin - 1);
                        double h = getWorldElevation(playerX, playerY);
                        const char* biome = (h < 0.53) ? "Coastal Beach" : (h > 0.66 ? "Mountain Foothills" : "Temperate Forest");
                        int st = settlementTypeAt(playerX, playerY);
                        if (st >= 0) statusMessage = settlementName(playerX, playerY, st) + (destroyedSettlements.count(GridCoord{playerX, playerY}) ? " - DESTROYED" : (st == 2 ? " - hostile!  [X] Raid" : "  [T] Trade   [X] Raid"));
                        else if (colonies.count(GridCoord{playerX, playerY})) statusMessage = "Your colony is here. Press ENTER to return to it.";
                        else statusMessage = std::string("Biome: ") + biome + " | Press ENTER to zoom in and settle here!"; }}
                GridCoord here{playerX, playerY}; int st = settlementTypeAt(playerX, playerY);
                if (IsKeyPressed(KEY_ENTER)) { if (st >= 0) statusMessage = "You cannot settle on top of another settlement."; else enterColony(playerX, playerY); }
                if (IsKeyPressed(KEY_T)) {
                    if (st < 0) statusMessage = "No settlement here to trade with.";
                    else if (destroyedSettlements.count(here)) statusMessage = "That settlement has been destroyed.";
                    else if (st == 2) statusMessage = "The pirates refuse to trade with you!";
                    else if (!haveCur) statusMessage = "You have no colony stockpile to trade from.";
                    else {
                        if (!settlementStocks.count(here)) settlementStocks[here] = makeStock(st, HashTile(here.first, here.second, seed + 123u), settlementName(playerX, playerY, st).c_str());
                        tradeSettle = true; tradeKey = here; openPanel = 5; }}
                if (IsKeyPressed(KEY_X)) {
                    if (st < 0) statusMessage = "No settlement here to raid.";
                    else if (destroyedSettlements.count(here)) statusMessage = "That settlement has already been destroyed.";
                    else raidSettlement(here, st); }}
        } else {
        // ======================= COLONY MODE INPUT =======================
            if (IsKeyPressed(KEY_O)) showRoofOverlay = !showRoofOverlay;
            if (IsKeyPressed(KEY_R) && selectedPawnIdx >= 0 && selectedPawnIdx < (int)C.pawns.size() && !C.pawns[selectedPawnIdx].dead && !C.pawns[selectedPawnIdx].downed) {
                Pawn& sp = C.pawns[selectedPawnIdx]; sp.drafted = !sp.drafted; sp.gotoX = -1; sp.forcedAttackId = -1;
                if (sp.drafted) { releaseJob(sp); sp.doingRecreation = false; } statusMessage = sp.name + (sp.drafted ? " drafted." : " released from draft."); }
            if (IsKeyPressed(KEY_ESCAPE)) {
                if (ctxOpen) ctxOpen = false;
                else if (openPanel != 0) openPanel = 0;
                else if (dragging) dragging = false;
                else if (selTool != -1) selTool = -1;
                else if (selCat != -1) selCat = -1;
                else if (selectedPawnIdx != -1) selectedPawnIdx = -1;
                else if (selTileX != -1) selTileX = selTileY = -1;
                else { saveCurrent(); mode = GameMode::WORLD_EXPLORE; wcamX = playerX - viewCols / 2; wcamY = playerY - viewRows / 2; statusMessage = "Zoomed out. Your colony stays here. ENTER on its tile returns to it."; }}
            if (mode == GameMode::COLONY_SIM) {
            if (openPanel == 0 && !ctxOpen && moveCooldown <= 0.0f) {
                bool moved = false;
                if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) { camY--; moved = true; }
                if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) { camY++; moved = true; }
                if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) { camX--; moved = true; }
                if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) { camX++; moved = true; }
                if (moved) { camX = std::clamp(camX, 0, std::max(0, localCols - viewCols)); camY = std::clamp(camY, 0, std::max(0, localRows - visibleMapRows)); moveCooldown = 0.06f; }}
            std::vector<int> alive; for (int i = 0; i < (int)C.pawns.size(); ++i) if (!C.pawns[i].dead) alive.push_back(i);
            const int tabW = screenWidth / 6;
            bool handled = false;
            // ---- bottom bar ----
            if (mouse.y >= mapBottomY) {
                handled = true;
                if (lPress) {
                    int itemsY = mapBottomY + 4, catY = mapBottomY + 52, tabY = mapBottomY + 96;
                    if (mouse.y >= tabY) {
                        int ct = (int)mouse.x / tabW;
                        if (ct == 0) { openPanel = 0; }
                        else if (ct >= 1 && ct <= 4) { openPanel = (openPanel == ct) ? 0 : ct; selTool = -1; }
                        else if (ct == 5) { saveCurrent(); mode = GameMode::WORLD_EXPLORE; wcamX = playerX - viewCols / 2; wcamY = playerY - viewRows / 2; statusMessage = "Zoomed out to the World Map."; }
                    } else if (mouse.y >= catY && openPanel == 0) {
                        int ci = (int)(mouse.x - 10) / 150; if (ci >= 0 && ci < (int)catNames.size() && mouse.x >= 10) doSelectCategory(ci);
                    } else if (mouse.y >= itemsY && openPanel == 0 && selCat >= 0) {
                        int slot = (int)(mouse.x - 10) / 176, k = 0;
                        for (int ti = 0; ti < (int)tools.size(); ++ti) if (tools[ti].cat == selCat) {
                            if (k == slot && mouse.x >= 10) {
                                if (selTool == ti && tools[ti].matChoice) {
                                    if (selMat == MaterialType::WOOD) { if (isResearchDone(R_STONECUT)) { selMat = MaterialType::STONE; statusMessage = "Material: STONE."; } else { selMat = MaterialType::STEEL; statusMessage = "Material: STEEL (Stonecutting not researched)."; }}
                                    else if (selMat == MaterialType::STONE) { selMat = MaterialType::STEEL; statusMessage = "Material: STEEL."; }
                                    else { selMat = MaterialType::WOOD; statusMessage = "Material: WOOD."; }
                                } else { selTool = ti; selTileX = selTileY = -1; selectedPawnIdx = -1; }
                                break; }
                            k++; }}}
            }
            // ---- panels ----
            else if (openPanel != 0) {
                handled = true;
                bool lp = lPress, rp = rPress;
                if (openPanel == 1 && (lp || rp)) {
                    for (size_t r = 0; r < alive.size(); ++r) for (int w = 0; w < 8; ++w) {
                        int bx = PX + 230 + w * 130, by = PY + 90 + (int)r * 44;
                        if (MouseIn(mouse, bx, by, 120, 36)) { int& v = C.pawns[alive[r]].workPrio[w]; v = lp ? (v + 1) % 5 : (v + 4) % 5; }}
                } else if (openPanel == 2) {
                    for (int b = 0; b < 4; ++b) if (lPress && MouseIn(mouse, PX + 170 + b * 130, PY + 50, 120, 30)) schedBrush = b;
                    if (lDown) for (size_t r = 0; r < alive.size(); ++r) for (int h = 0; h < 24; ++h)
                        if (MouseIn(mouse, PX + 170 + h * 50, PY + 130 + (int)r * 46, 46, 40)) C.pawns[alive[r]].schedule[h] = schedBrush;
                } else if (openPanel == 3 && lPress) {
                    for (size_t r = 0; r < alive.size(); ++r) {
                        int pi = alive[r]; Pawn& p = C.pawns[pi]; int by = PY + 90 + (int)r * 50;
                        if (MouseIn(mouse, PX + 200, by, 420, 40)) {
                            std::vector<GridCoord> beds; beds.push_back(GridCoord{-1, -1});
                            for (int y = 0; y < localRows; ++y) for (int x = 0; x < localCols; ++x) if (isBedObj(C.map[y][x].obj) && (C.map[y][x].bedOwner == -1 || C.map[y][x].bedOwner == pi)) beds.push_back(GridCoord{x, y});
                            size_t cur = 0; for (size_t k = 0; k < beds.size(); ++k) if (beds[k].first == p.bedX && beds[k].second == p.bedY) cur = k;
                            GridCoord nb = beds[(cur + 1) % beds.size()];
                            if (p.bedX >= 0 && inMap(p.bedX, p.bedY)) C.map[p.bedY][p.bedX].bedOwner = -1;
                            p.bedX = nb.first; p.bedY = nb.second; if (nb.first >= 0) C.map[nb.second][nb.first].bedOwner = pi; p.targetBedX = -1; p.targetBedY = -1; }
                        if (MouseIn(mouse, PX + 640, by, 420, 40)) {
                            int w = p.weapon;
                            for (int step = 1; step <= WEAPON_COUNT; ++step) { int cand = (w + step) % WEAPON_COUNT; if (cand == 0 || C.weaponInv[cand] > 0) { w = cand; break; }}
                            if (w != p.weapon) { if (p.weapon > 0) C.weaponInv[p.weapon]++; if (w > 0) C.weaponInv[w]--; p.weapon = w; }}}
                } else if (openPanel == 4 && lPress) {
                    if (MouseIn(mouse, PX + PW - 110, PY + 10, 90, 35)) openPanel = 0;
                    for (int i = 0; i < (int)researchTree.size(); ++i) {
                        ResearchNode& node = researchTree[i]; int bx = PX + 40 + node.uiX, by = PY + 70 + node.uiY;
                        if (MouseIn(mouse, bx, by, 250, 95)) {
                            bool prereqMet = (node.prereq == -1) || isResearchDone(node.prereq);
                            if (prereqMet && !isResearchDone(i)) { activeResearchIdx = i; statusMessage = "Active Research set to: " + node.title; }
                            else if (!prereqMet) statusMessage = "Prerequisite research required first!"; }}
                } else if (openPanel == 5 && lPress) {
                    TradeStock* ts = getTradeStock();
                    if (!ts) openPanel = 0;
                    else {
                        if (MouseIn(mouse, PX + PW - 110, PY + 10, 90, 35)) { openPanel = 0; tradeCritterId = -1; }
                        for (int i = 0; i < TRADE_COUNT; ++i) {
                            int ry = PY + 110 + i * 40; int buyP = std::max(1, (int)std::ceil(TRADE_BASE[i] * 1.4)), sellP = std::max(1, (int)(TRADE_BASE[i] * 0.5));
                            for (int b = 0; b < 4; ++b) if (MouseIn(mouse, PX + 560 + b * 90, ry, 82, 32)) {
                                int n = (b % 2 == 0) ? 1 : 10;
                                if (b < 2) { n = std::min(n, ts->qty[i]); n = std::min(n, C.silver / buyP); if (n > 0) { C.silver -= n * buyP; ts->silver += n * buyP; ts->qty[i] -= n; TradeQtyRef(C, i) += n; }}
                                else { n = std::min(n, TradeQtyRef(C, i)); n = std::min(n, ts->silver / sellP); if (n > 0) { C.silver += n * sellP; ts->silver -= n * sellP; ts->qty[i] += n; TradeQtyRef(C, i) -= n; }}}}}}
                if (mouse.y < PY && lPress) { /* clicking above the panel ignored */ }
            }
            // ---- context menu ----
            if (!handled && ctxOpen) {
                handled = true;
                if (lPress) {
                    for (size_t i = 0; i < ctx.size(); ++i) if (MouseIn(mouse, ctxX, ctxY + (int)i * 26, 300, 26)) {
                        CtxItem it = ctx[i];
                        if (selectedPawnIdx >= 0 && selectedPawnIdx < (int)C.pawns.size()) {
                            Pawn& sp = C.pawns[selectedPawnIdx];
                            if (it.action == 1) { releaseJob(sp); sp.gotoX = it.a; sp.gotoY = it.b; sp.doingRecreation = false; }
                            else if (it.action == 2) { int ji = findJob(it.a); if (ji >= 0) { Job& j = C.jobs[ji]; if (j.assignedPawn >= 0 && j.assignedPawn != selectedPawnIdx) C.pawns[j.assignedPawn].jobId = -1; j.assignedPawn = -1; j.forcedPawn = selectedPawnIdx; releaseJob(sp); sp.gotoX = -1; sp.doingRecreation = false; }}
                            else if (it.action == 3) { int ji = jobIdxAt(it.a, it.b); if (ji >= 0) { Job& j = C.jobs[ji]; j.forcedPawn = selectedPawnIdx; j.assignedPawn = -1; releaseJob(sp); sp.gotoX = -1; sp.doingRecreation = false; }}
                            else if (it.action == 6) {
                                LocalCell& dc = C.map[it.b][it.a]; ToolDef td; 
                                td.kind = (dc.obj == ObjectType::TREE) ? ToolKind::CHOP : ((dc.obj == ObjectType::BOULDER || dc.obj == ObjectType::STEEL_VEIN) ? ToolKind::MINE : ToolKind::HARVEST);
                                applyTool(td, it.a, it.b);
                                int ji = jobIdxAt(it.a, it.b); if (ji >= 0) { Job& j = C.jobs[ji]; j.forcedPawn = selectedPawnIdx; j.assignedPawn = -1; releaseJob(sp); sp.gotoX = -1; sp.doingRecreation = false; }}
                            else if (it.action == 4) { sp.forcedAttackId = it.a; sp.gotoX = -1; }
                            else if (it.action == 5) { for (auto& c : C.critters) if (c.id == it.a) c.hunted = true; sp.forcedAttackId = it.a; sp.gotoX = -1; }}
                    }
                    ctxOpen = false; }
                else if (rPress) ctxOpen = false;
            }
            // ---- colonist bar & panels on map ----
            if (!handled && mouse.y < topBarHeight + 36 && mouse.y >= topBarHeight + 2) {
                for (size_t k = 0; k < alive.size(); ++k) if (MouseIn(mouse, 8 + (int)k * 126, topBarHeight + 4, 120, 30)) {
                    handled = true;
                    if (lPress) { selectedPawnIdx = alive[k]; selTileX = selTileY = -1; camX = std::clamp(C.pawns[alive[k]].x - viewCols / 2, 0, std::max(0, localCols - viewCols)); camY = std::clamp(C.pawns[alive[k]].y - visibleMapRows / 2, 0, std::max(0, localRows - visibleMapRows)); }}}
            if (!handled && selectedPawnIdx >= 0 && selectedPawnIdx < (int)C.pawns.size() && MouseIn(mouse, 12, mapBottomY - 300, 350, 290)) {
                handled = true;
                if (lPress) {
                    Pawn& sp = C.pawns[selectedPawnIdx];
                    if (MouseIn(mouse, 12 + 322, mapBottomY - 300 + 8, 20, 20)) selectedPawnIdx = -1;
                    else if (MouseIn(mouse, 12 + 230, mapBottomY - 300 + 8, 84, 22) && !sp.downed) { sp.drafted = !sp.drafted; sp.gotoX = -1; sp.forcedAttackId = -1; if (sp.drafted) { releaseJob(sp); sp.doingRecreation = false; }}}}
            if (!handled && selTileX >= 0 && inMap(selTileX, selTileY) && (C.map[selTileY][selTileX].obj == ObjectType::SMITHY || C.map[selTileY][selTileX].obj == ObjectType::MACHINING) && MouseIn(mouse, 12, mapBottomY - 230, 400, 220)) {
                handled = true;
                if (lPress) {
                    int bench = (C.map[selTileY][selTileX].obj == ObjectType::SMITHY) ? 0 : 1; int row = 0;
                    if (MouseIn(mouse, 12 + 372, mapBottomY - 230 + 6, 20, 20)) selTileX = selTileY = -1;
                    for (int r = 0; r < RECIPE_COUNT; ++r) if (RECIPES[r].bench == bench) {
                        int ry = mapBottomY - 230 + 40 + row * 36;
                        if (isResearchDone(RECIPES[r].research) && MouseIn(mouse, 12 + 240, ry, 40, 28)) C.billTarget[r] = std::max(0, C.billTarget[r] - 1);
                        if (isResearchDone(RECIPES[r].research) && MouseIn(mouse, 12 + 330, ry, 40, 28)) C.billTarget[r] = std::min(20, C.billTarget[r] + 1);
                        row++; }}}
            // ---- map ----
            if (!handled && mouse.y > topBarHeight && mouse.y < mapBottomY) {
                int vx = (int)mouse.x / tileSize, vy = (int)(mouse.y - topBarHeight) / tileSize, lx = camX + vx, ly = camY + vy;
                bool onMap = inMap(lx, ly) && vy < visibleMapRows;
                if (rPress) {
                    if (dragging) dragging = false;
                    else if (selTool != -1) { selTool = -1; statusMessage = "Action cancelled."; }
                    else if (selectedPawnIdx >= 0 && selectedPawnIdx < (int)C.pawns.size() && onMap && !C.pawns[selectedPawnIdx].dead && !C.pawns[selectedPawnIdx].downed) {
                        Pawn& sp = C.pawns[selectedPawnIdx]; LocalCell& tc = C.map[ly][lx];
                        Critter* cr = critterAt(lx, ly);
                        if (sp.drafted) {
                            if (cr && cr->hostile) sp.forcedAttackId = cr->id; else if (isWalkable(lx, ly)) { sp.gotoX = lx; sp.gotoY = ly; }
                        } else {
                            ctx.clear();
                            if (cr && cr->hostile) ctx.push_back(CtxItem{"Attack " + cr->name, 4, cr->id, 0});
                            else if (cr && cr->type != CType::TRADER) ctx.push_back(CtxItem{"Hunt " + cr->name + " (" + sp.name + ")", 5, cr->id, 0});
                            int ji = jobIdxAt(lx, ly);
                            if (ji >= 0) ctx.push_back(CtxItem{"Prioritize this job (" + sp.name + ")", 2, C.jobs[ji].id, 0});
                            else if (tc.obj == ObjectType::TREE) ctx.push_back(CtxItem{"Prioritize chopping (" + sp.name + ")", 6, lx, ly});
                            else if (tc.obj == ObjectType::BOULDER || tc.obj == ObjectType::STEEL_VEIN) ctx.push_back(CtxItem{"Prioritize mining (" + sp.name + ")", 6, lx, ly});
                            else if (tc.obj == ObjectType::BERRY_BUSH || (tc.crop != CropType::NONE && tc.cropGrowth >= 1.0f)) ctx.push_back(CtxItem{"Prioritize harvesting (" + sp.name + ")", 6, lx, ly});
                            if (isWalkable(lx, ly)) ctx.push_back(CtxItem{"Go here (" + sp.name + ")", 1, lx, ly});
                            if (!ctx.empty()) { ctxOpen = true; ctxX = std::min((int)mouse.x, screenWidth - 310); ctxY = std::min((int)mouse.y, screenHeight - (int)ctx.size() * 26 - 4); }}}}
                else if (selTool != -1) {
                    const ToolDef& td = tools[selTool];
                    if (lPress && onMap) { dragging = true; dragX0 = lx; dragY0 = ly; }
                    if (lRel && dragging) {
                        dragging = false; int x0 = std::min(dragX0, lx), x1 = std::max(dragX0, lx), y0 = std::min(dragY0, ly), y1 = std::max(dragY0, ly);
                        for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) applyTool(td, x, y); }
                } else if (lPress && onMap) {
                    bool picked = false;
                    for (int pi : alive) if (C.pawns[pi].x == lx && C.pawns[pi].y == ly) { selectedPawnIdx = pi; selTileX = selTileY = -1; picked = true; break; }
                    if (!picked) {
                        Critter* cr = critterAt(lx, ly);
                        if (cr && cr->type == CType::TRADER) { tradeSettle = false; tradeCritterId = cr->id; openPanel = 5; }
                        else { selectedPawnIdx = -1; selTileX = lx; selTileY = ly; }}}
            }
            if (!lDown && !lRel) dragging = dragging && lDown;
            // ======================= SIMULATION =======================
            if (sdt > 0.0f) {
                C.windTimer += gameHoursElapsed;
                if (C.windTimer >= 2.5f) { C.windTimer = 0.0f; int r = GetRandomValue(1, 100); C.windOut = (r <= 4) ? 0.0f : (r <= 14 ? (float)GetRandomValue(20, 69) : (r <= 88 ? (float)GetRandomValue(70, 200) : (float)GetRandomValue(201, 250))); }
                float solarOut = 0.0f;
                if (gameHour >= 5.0f && gameHour < 6.5f) solarOut = 100.0f * ((gameHour - 5.0f) / 1.5f);
                else if (gameHour >= 6.5f && gameHour <= 18.0f) solarOut = 100.0f;
                else if (gameHour > 18.0f && gameHour <= 19.0f) solarOut = 100.0f * (1.0f - (gameHour - 18.0f));
                int gen = 0, use = 0, batteries = 0; bool isDaytime = (gameHour >= 6.0f && gameHour <= 19.0f);
                for (int y = 0; y < localRows; ++y) for (int x = 0; x < localCols; ++x) {
                    LocalCell& c = C.map[y][x];
                    if (c.crop != CropType::NONE && c.cropGrowth < 1.0f) {
                        float rate = (isDaytime ? 0.06f : 0.01f) * (isResearchDone(R_FARMING) ? 1.25f : 1.0f);
                        if (c.crop == CropType::RICE) rate *= 1.35f; else if (c.crop == CropType::CORN) rate *= 0.75f; else if (c.crop == CropType::HEALROOT) rate *= 0.65f;
                        c.cropGrowth = std::min(1.0f, c.cropGrowth + rate * gameHoursElapsed); }
                    if (c.obj == ObjectType::WINDMILL) gen += (int)std::round(C.windOut);
                    else if (c.obj == ObjectType::SOLAR_PANEL) gen += (int)std::round(solarOut);
                    else if (c.obj == ObjectType::STOVE) use += 200;
                    else if (c.obj == ObjectType::MACHINING) use += 150;
                    else if (c.obj == ObjectType::LAMP_STANDING) use += 30;
                    else if (c.obj == ObjectType::BATTERY) batteries++;
                    else if (c.obj == ObjectType::GENERATOR && C.wood > 0) { gen += 1000; C.fuelAccum += gameHoursElapsed * 1.5f; }
                    else if (c.obj == ObjectType::TURRET) {
                        c.aux -= sdt;
                        if (c.aux <= 0.0f) {
                            Critter* best = nullptr; float bd = 12.5f;
                            for (auto& cr : C.critters) if (cr.hostile && cr.hp > 0 && !cr.gone) { float d = Dist(x, y, cr.x, cr.y); if (d < bd && lineClear(x, y, cr.x, cr.y)) { bd = d; best = &cr; }}
                            if (best) { c.aux = 1.2f; shots.push_back(Shot{(float)x, (float)y, (float)best->x, (float)best->y, 0.12f, SKYBLUE}); if (GetRandomValue(1, 100) <= 75) damageCritter(*best, 12.0f); }}}}
                C.powerGen = gen; C.powerUse = use;
                while (C.fuelAccum >= 1.0f && C.wood > 0) { C.wood--; C.fuelAccum -= 1.0f; }
                if (C.wood <= 0) C.fuelAccum = 0.0f;
                C.batteryCharge = std::clamp(C.batteryCharge + (float)(gen - use) * gameHoursElapsed, 0.0f, batteries * 600.0f);
                C.onBattery = (gen < use && C.batteryCharge > 0.0f);
                // ---- auto job scan ----
                scanTimer += dt;
                if (scanTimer >= 0.5f) {
                    scanTimer = 0.0f;
                    std::set<GridCoord> farmJobs;
                    for (const auto& j : C.jobs) if (j.type == TaskType::SOW_CROP || j.type == TaskType::HARVEST_CROP) farmJobs.insert(GridCoord{j.tx, j.ty});
                    for (int y = 0; y < localRows; ++y) for (int x = 0; x < localCols; ++x) {
                        LocalCell& c = C.map[y][x];
                        if (c.zoneCrop > 0 && !farmJobs.count(GridCoord{x, y})) {
                            if (c.crop == CropType::NONE && c.obj == ObjectType::NONE && c.terrain != BaseTerrain::WATER) { Job& j = addJob(TaskType::SOW_CROP, x, y, 0.9f); j.targetCrop = (CropType)c.zoneCrop; }
                            else if (c.crop != CropType::NONE && c.cropGrowth >= 1.0f) addJob(TaskType::HARVEST_CROP, x, y, 1.0f); }
                        if ((c.obj == ObjectType::STOVE && hasPower()) || c.obj == ObjectType::CAMPFIRE) {
                            if (C.rawFood >= 5 && C.cookedFood < 30) { bool has = false; for (const auto& j : C.jobs) if (j.type == TaskType::COOK_MEAL && j.tx == x && j.ty == y) { has = true; break; } if (!has) addJob(TaskType::COOK_MEAL, x, y, 1.5f); }}
                        if (c.obj == ObjectType::RESEARCH_BENCH && !isResearchDone(activeResearchIdx)) {
                            bool has = false; for (const auto& j : C.jobs) if (j.type == TaskType::RESEARCH_WORK && j.tx == x && j.ty == y) { has = true; break; }
                            if (!has) addJob(TaskType::RESEARCH_WORK, x, y, 9999.0f); }}
                    for (int r = 0; r < RECIPE_COUNT; ++r) {
                        if (C.billTarget[r] <= 0 || !isResearchDone(RECIPES[r].research)) continue;
                        int queued = 0; for (const auto& j : C.jobs) if (j.type == TaskType::CRAFT && j.recipe == r) queued++;
                        if (C.weaponInv[RECIPES[r].weapon] + queued >= C.billTarget[r]) continue;
                        if (C.wood < RECIPES[r].wood || C.steel < RECIPES[r].steel) continue;
                        ObjectType want = RECIPES[r].bench == 0 ? ObjectType::SMITHY : ObjectType::MACHINING;
                        if (want == ObjectType::MACHINING && !hasPower()) continue;
                        bool made = false;
                        for (int y = 0; y < localRows && !made; ++y) for (int x = 0; x < localCols && !made; ++x) if (C.map[y][x].obj == want) {
                            bool busy = false; for (const auto& j : C.jobs) if (j.type == TaskType::CRAFT && j.tx == x && j.ty == y) { busy = true; break; }
                            if (busy) continue;
                            C.wood -= RECIPES[r].wood; C.steel -= RECIPES[r].steel;
                            Job& j = addJob(TaskType::CRAFT, x, y, RECIPES[r].work); j.recipe = r; j.cw = RECIPES[r].wood; j.cst = RECIPES[r].steel; made = true; }}
                    for (const auto& cr : C.critters) if (cr.hunted && cr.hp > 0 && !cr.gone) {
                        bool has = false; for (const auto& j : C.jobs) if (j.type == TaskType::HUNT && j.targetId == cr.id) { has = true; break; }
                        if (!has) { Job& j = addJob(TaskType::HUNT, cr.x, cr.y, 9999.0f); j.targetId = cr.id; }}}
                // ---- events ----
                if (nowH >= C.nextRaidTime) { spawnRaid(); C.nextRaidTime = nowH + 72.0 + GetRandomValue(0, 48); }
                if (nowH >= C.nextTraderTime) { spawnTrader(); C.nextTraderTime = nowH + 60.0 + GetRandomValue(0, 36); }
                if (nowH >= C.nextAnimalTime) {
                    int animals = 0; for (const auto& c : C.critters) if (c.type != CType::RAIDER && c.type != CType::TRADER) animals++;
                    if (animals < 12) spawnWildlife(GetRandomValue(1, 2));
                    if (GetRandomValue(1, 100) <= 8) { int sx, sy; edgeSpawn(sx, sy); int n = GetRandomValue(2, 3); for (int i = 0; i < n; ++i) spawnCritter(CType::WOLF, sx, sy); statusMessage = "A wolf pack has appeared at the edge of the map!"; }
                    C.nextAnimalTime = nowH + 4.0; }
                // ======================= COLONISTS =======================
                for (size_t pIdx = 0; pIdx < C.pawns.size(); ++pIdx) {
                    Pawn& pawn = C.pawns[pIdx]; if (pawn.dead) continue;
                    pawn.moveTimer -= sdt; pawn.atkTimer -= sdt; pawn.thinkTimer -= sdt;
                    pawn.hunger -= gameHoursElapsed * (100.0f / 24.0f);
                    if (pawn.hunger <= 80.0f) {
                        if (C.cookedFood > 0) { C.cookedFood -= 1; pawn.hunger = std::min(100.0f, pawn.hunger + 20.0f); statusMessage = pawn.name + " ate a cooked meal."; }
                        else if (C.rawFood > 0) { int bite = std::min(C.rawFood, 10); C.rawFood -= bite; pawn.hunger = std::min(100.0f, pawn.hunger + bite * 2.0f); statusMessage = pawn.name + TextFormat(" ate %d raw food.", bite); }}
                    pawn.hunger = std::max(0.0f, pawn.hunger);
                    if (pawn.hunger <= 0.0f) damagePawn(pawn, 4.0f * gameHoursElapsed);
                    if (pawn.dead) continue;
                    if (pawn.hp < pawn.maxHp && pawn.hunger > 0.0f) pawn.hp = std::min(pawn.maxHp, pawn.hp + (pawn.downed ? 5.0f : 1.5f) * gameHoursElapsed);
                    if (!pawn.doingRecreation && !pawn.isSleeping) pawn.recreation = std::max(0.0f, pawn.recreation - gameHoursElapsed * 3.0f);
                    pawn.mood = std::clamp(35.0f + pawn.hunger * 0.22f + pawn.energy * 0.22f + pawn.recreation * 0.21f, 0.0f, 100.0f);
                    if (pawn.downed) { pawn.activity = "Downed"; if (pawn.hp >= 25.0f) { pawn.downed = false; pawn.activity = "Idle"; } continue; }
                    int act = pawn.schedule[std::clamp((int)gameHour, 0, 23)];
                    Critter* threat = nullptr; float threatD = 1e9f;
                    for (auto& c : C.critters) if (c.hostile && c.hp > 0 && !c.gone) { float d = Dist(pawn.x, pawn.y, c.x, c.y); if (d < threatD) { threatD = d; threat = &c; }}
                    if (pawn.forcedAttackId != -1) {
                        Critter* ft = nullptr; for (auto& c : C.critters) if (c.id == pawn.forcedAttackId && c.hp > 0 && !c.gone) ft = &c;
                        if (ft) { threat = ft; threatD = Dist(pawn.x, pawn.y, ft->x, ft->y); } else pawn.forcedAttackId = -1; }
                    if (pawn.isSleeping) {
                        ObjectType bo = C.map[pawn.y][pawn.x].obj;
                        float rate = (bo == ObjectType::BED) ? 15.0f : (bo == ObjectType::SLEEP_SPOT ? 12.0f : 10.5f);
                        pawn.energy = std::min(100.0f, pawn.energy + gameHoursElapsed * rate);
                        pawn.activity = "Sleeping";
                        bool wake = (act != SA_SLEEP && pawn.energy >= 70.0f) || pawn.energy >= 100.0f || (threat && threatD < 12.0f);
                        if (wake) { pawn.isSleeping = false; pawn.targetBedX = -1; pawn.targetBedY = -1; pawn.activity = "Idle"; }
                        continue; }
                    pawn.energy = std::max(0.0f, pawn.energy - gameHoursElapsed * (100.0f / 16.0f));
                    const WeaponDef& wd = WEAPONS[pawn.weapon];
                    if (threat) {
                        float engage = pawn.drafted ? wd.range + 0.5f : 15.0f; if (pawn.forcedAttackId != -1) engage = 60.0f;
                        if (threatD <= engage) {
                            releaseJob(pawn); pawn.doingRecreation = false;
                            bool inRange = threatD <= wd.range && (!wd.ranged || lineClear(pawn.x, pawn.y, threat->x, threat->y));
                            if (inRange) { pawn.activity = wd.ranged ? "Shooting" : "Fighting"; pawnAttack(pawn, *threat); }
                            else if (!pawn.drafted || pawn.forcedAttackId != -1) {
                                pawn.activity = "Engaging";
                                if (pawn.moveTimer <= 0.0f) { pawn.moveTimer = 0.18f; GridCoord s = FindNextStep(C.map, pawn.x, pawn.y, threat->x, threat->y, false); if (s.first != -1) { pawn.x = s.first; pawn.y = s.second; }}}
                            else pawn.activity = "Drafted";
                            continue; }}
                    if (pawn.drafted || pawn.gotoX != -1) {
                        if (pawn.gotoX != -1) {
                            if (pawn.x == pawn.gotoX && pawn.y == pawn.gotoY) pawn.gotoX = pawn.gotoY = -1;
                            else { pawn.activity = "Moving"; if (pawn.moveTimer <= 0.0f) { pawn.moveTimer = 0.18f; GridCoord s = FindNextStep(C.map, pawn.x, pawn.y, pawn.gotoX, pawn.gotoY, true); if (s.first == -1) pawn.gotoX = pawn.gotoY = -1; else { pawn.x = s.first; pawn.y = s.second; }}}
                        } else pawn.activity = "Drafted";
                        continue; }
                    bool wantSleep = (act == SA_SLEEP && pawn.energy < 85.0f) || pawn.energy <= 15.0f;
                    if (wantSleep) {
                        pawn.doingRecreation = false; releaseJob(pawn);
                        if (pawn.targetBedX == -1) {
                            if (pawn.bedX >= 0 && isBedObj(C.map[pawn.bedY][pawn.bedX].obj) && C.map[pawn.bedY][pawn.bedX].bedOwner == (int)pIdx) { pawn.targetBedX = pawn.bedX; pawn.targetBedY = pawn.bedY; }
                            else {
                                pawn.bedX = pawn.bedY = -1; int bestD = 999999;
                                for (int y = 0; y < localRows; ++y) for (int x = 0; x < localCols; ++x) if (isBedObj(C.map[y][x].obj) && C.map[y][x].bedOwner == -1) {
                                    int d = std::abs(x - pawn.x) + std::abs(y - pawn.y); if (d < bestD) { bestD = d; pawn.targetBedX = x; pawn.targetBedY = y; }}
                                if (pawn.targetBedX != -1) { C.map[pawn.targetBedY][pawn.targetBedX].bedOwner = (int)pIdx; pawn.bedX = pawn.targetBedX; pawn.bedY = pawn.targetBedY; }}}
                        if (pawn.targetBedX == -1 || (pawn.x == pawn.targetBedX && pawn.y == pawn.targetBedY)) { pawn.isSleeping = true; pawn.activity = "Sleeping"; }
                        else {
                            pawn.activity = "Going to bed";
                            if (pawn.moveTimer <= 0.0f) { pawn.moveTimer = 0.18f; GridCoord s = FindNextStep(C.map, pawn.x, pawn.y, pawn.targetBedX, pawn.targetBedY, true); if (s.first == -1) { pawn.targetBedX = -1; pawn.isSleeping = true; } else { pawn.x = s.first; pawn.y = s.second; }}}
                        continue; }
                    pawn.targetBedX = -1; pawn.targetBedY = -1;
                    if (pawn.weapon == 0 && pawn.thinkTimer <= 0.0f) {
                        pawn.thinkTimer = 1.0f; const int order[7] = {6, 7, 5, 4, 3, 2, 1};
                        for (int k = 0; k < 7; ++k) if (C.weaponInv[order[k]] > 0) { C.weaponInv[order[k]]--; pawn.weapon = order[k]; break; }}
                    bool mayWork = (act == SA_WORK || act == SA_ANY), mayJoy = (act != SA_WORK);
                    if (!mayWork) releaseJob(pawn);
                    if (mayWork) {
                        int cj = findJob(pawn.jobId);
                        if (cj >= 0 && C.jobs[cj].type == TaskType::RESEARCH_WORK && pawn.thinkTimer <= 0.0f) {
                            pawn.thinkTimer = 0.5f; int alt = pickJob(pIdx, true);
                            if (alt >= 0) { releaseJob(pawn); C.jobs[alt].assignedPawn = (int)pIdx; pawn.jobId = C.jobs[alt].id; pawn.pathTimer = 0.0f; }}
                        if (findJob(pawn.jobId) < 0) pawn.jobId = -1;
                        if (pawn.jobId == -1) {
                            int ji = pickJob(pIdx, false);
                            if (ji >= 0) { C.jobs[ji].assignedPawn = (int)pIdx; pawn.jobId = C.jobs[ji].id; pawn.pathTimer = 0.0f; pawn.doingRecreation = false; }
                            else if (!pawn.failedJobs.empty()) pawn.failedJobs.clear(); }}
                    int ji = findJob(pawn.jobId);
                    if (ji >= 0) {
                        Job& job = C.jobs[ji];
                        TaskType jt = job.type;
                        if (jt == TaskType::HUNT) {
                            Critter* tg = nullptr; for (auto& c : C.critters) if (c.id == job.targetId && c.hp > 0 && !c.gone) tg = &c;
                            if (!tg) { eraseJobAt(ji); pawn.activity = "Idle"; continue; }
                            job.tx = tg->x; job.ty = tg->y;
                            float d = Dist(pawn.x, pawn.y, tg->x, tg->y);
                            if (d <= wd.range && (!wd.ranged || lineClear(pawn.x, pawn.y, tg->x, tg->y))) { pawn.activity = "Hunting"; pawn.pathTimer = 0.0f; pawnAttack(pawn, *tg); }
                            else {
                                pawn.activity = "Stalking"; pawn.pathTimer += sdt;
                                if (pawn.pathTimer >= 40.0f) { pawn.failedJobs.insert(job.id); job.assignedPawn = -1; pawn.jobId = -1; pawn.pathTimer = 0.0f; continue; }
                                if (pawn.moveTimer <= 0.0f) { pawn.moveTimer = 0.18f; GridCoord s = FindNextStep(C.map, pawn.x, pawn.y, tg->x, tg->y, false); if (s.first != -1) { pawn.x = s.first; pawn.y = s.second; }}}
                            continue; }
                        int dist = std::max(std::abs(job.tx - pawn.x), std::abs(job.ty - pawn.y));
                        bool needExact = (jt == TaskType::BUILD_FLOOR || jt == TaskType::BUILD_CONDUIT);
                        if ((needExact && dist == 0) || (!needExact && dist <= 1)) {
                            pawn.pathTimer = 0.0f;
                            if (jt == TaskType::RESEARCH_WORK) {
                                if (C.map[job.ty][job.tx].obj != ObjectType::RESEARCH_BENCH || isResearchDone(activeResearchIdx)) { eraseJobAt(ji); pawn.activity = "Idle"; continue; }
                                pawn.activity = "Researching";
                                pawn.researchAccum += gameMinutesElapsed * 0.35f * GetSkillWorkSpeed(pawn.skillLevel[(int)SkillId::INTELLECTUAL]) * (isResearchDone(R_ANALYZER) ? 1.5f : 1.0f);
                                GrantSkillXP(pawn, SkillId::INTELLECTUAL, sdt * 85.0f, statusMessage);
                                while (pawn.researchAccum >= 1.0f && !isResearchDone(activeResearchIdx)) { pawn.researchAccum -= 1.0f; researchTree[activeResearchIdx].progress += 1; }
                                if (isResearchDone(activeResearchIdx)) { statusMessage = "RESEARCH COMPLETE: " + researchTree[activeResearchIdx].title + "!"; eraseJobAt(ji); pawn.activity = "Idle"; }
                            } else {
                                pawn.activity = (jt == TaskType::COOK_MEAL) ? "Cooking" : (jt == TaskType::CRAFT ? "Crafting" : "Working");
                                SkillId usedSkill = SkillOfTask(jt);
                                job.progress += sdt * GetSkillWorkSpeed(pawn.skillLevel[(int)usedSkill]) * (isResearchDone(R_TOOLS) ? 1.15f : 1.0f);
                                GrantSkillXP(pawn, usedSkill, sdt * 110.0f, statusMessage);
                                if (job.progress >= job.workRequired) {
                                    LocalCell& tc = C.map[job.ty][job.tx]; int doneId = job.id; Job done = job;
                                    switch (jt) {
                                        case TaskType::CHOP: C.wood += 15; statusMessage = pawn.name + " chopped wood (+15)."; tc.obj = ObjectType::NONE; break;
                                        case TaskType::MINE:
                                            if (tc.obj == ObjectType::STEEL_VEIN) { C.steel += 18; statusMessage = pawn.name + " mined steel (+18)."; } else { C.stone += 15; statusMessage = pawn.name + " mined stone (+15)."; }
                                            tc.obj = ObjectType::NONE; break;
                                        case TaskType::PICK_BUSH: C.rawFood += 15; statusMessage = pawn.name + " picked berries (+15 Raw)."; tc.obj = ObjectType::NONE; break;
                                        case TaskType::HARVEST_CROP: { int y2 = (tc.crop == CropType::CORN) ? 35 : 22; C.rawFood += y2; tc.crop = CropType::NONE; tc.cropGrowth = 0.0f; statusMessage = pawn.name + TextFormat(" harvested crops (+%d Raw Food).", y2); break; }
                                        case TaskType::SOW_CROP: if (tc.crop == CropType::NONE && tc.obj == ObjectType::NONE) { tc.crop = done.targetCrop; tc.cropGrowth = 0.05f; } break;
                                        case TaskType::BUILD_FLOOR: tc.floor = done.targetFloor; break;
                                        case TaskType::BUILD_OBJECT: tc.obj = done.targetObj; tc.objMat = done.mat; tc.hp = StructHp(done.targetObj, done.mat); tc.bedOwner = -1; tc.aux = 0.0f; break;
                                        case TaskType::BUILD_ROOF: tc.hasRoof = true; break;
                                        case TaskType::REMOVE_ROOF: tc.hasRoof = false; break;
                                        case TaskType::BUILD_CONDUIT: tc.hasConduit = true; break;
                                        case TaskType::COOK_MEAL: { int batch = std::min(C.rawFood, 5); C.rawFood -= batch; C.cookedFood += batch; statusMessage = pawn.name + TextFormat(" cooked %d meals.", batch); break; }
                                        case TaskType::CRAFT: C.weaponInv[RECIPES[done.recipe].weapon]++; statusMessage = pawn.name + " crafted a " + RECIPES[done.recipe].name + "."; break;
                                        case TaskType::DECONSTRUCT:
                                            if (tc.obj != ObjectType::NONE) {
                                                if (isBedObj(tc.obj)) for (auto& pp : C.pawns) if (pp.bedX == done.tx && pp.bedY == done.ty) { pp.bedX = pp.bedY = -1; pp.targetBedX = pp.targetBedY = -1; }
                                                tc.obj = ObjectType::NONE; tc.bedOwner = -1; purgeAutoJobs(done.tx, done.ty); }
                                            else if (tc.hasConduit) tc.hasConduit = false; else if (tc.floor != FloorType::NONE) tc.floor = FloorType::NONE;
                                            break;
                                        default: break; }
                                    int jj = findJob(doneId); if (jj >= 0) eraseJobAt(jj);
                                    pawn.jobId = -1; pawn.activity = "Idle"; }}
                        } else {
                            pawn.pathTimer += sdt;
                            if (pawn.pathTimer >= 30.0f) { statusMessage = pawn.name + " timed out reaching target - switching tasks."; pawn.failedJobs.insert(job.id); job.assignedPawn = -1; pawn.jobId = -1; pawn.pathTimer = 0.0f; continue; }
                            if (pawn.moveTimer <= 0.0f) {
                                pawn.activity = "Moving"; pawn.moveTimer = 0.18f;
                                GridCoord ns = FindNextStep(C.map, pawn.x, pawn.y, job.tx, job.ty, needExact);
                                if (ns.first == -1) { statusMessage = pawn.name + " cannot reach target - switching tasks."; pawn.failedJobs.insert(job.id); job.assignedPawn = -1; pawn.jobId = -1; pawn.pathTimer = 0.0f; continue; }
                                pawn.x = ns.first; pawn.y = ns.second; }}
                        continue; }
                    // ---- no job: recreation / idle ----
                    if (!pawn.doingRecreation && mayJoy && ((act == SA_JOY && pawn.recreation < 100.0f) || (act != SA_JOY && pawn.recreation < 85.0f))) {
                        pawn.doingRecreation = true; pawn.targetRecX = pawn.targetRecY = -1; int bestD = 999999;
                        for (int y = 0; y < localRows; ++y) for (int x = 0; x < localCols; ++x) { ObjectType o = C.map[y][x].obj; if (o == ObjectType::HORSESHOES || o == ObjectType::CHESS_TABLE) { int d = std::abs(x - pawn.x) + std::abs(y - pawn.y); if (d < bestD) { bestD = d; pawn.targetRecX = x; pawn.targetRecY = y; }}}}
                    if (pawn.doingRecreation && !mayJoy) { pawn.doingRecreation = false; pawn.targetRecX = pawn.targetRecY = -1; }
                    if (pawn.doingRecreation) {
                        if (pawn.targetRecX != -1 && (C.map[pawn.targetRecY][pawn.targetRecX].obj == ObjectType::HORSESHOES || C.map[pawn.targetRecY][pawn.targetRecX].obj == ObjectType::CHESS_TABLE)) {
                            int dist = std::max(std::abs(pawn.targetRecX - pawn.x), std::abs(pawn.targetRecY - pawn.y));
                            if (dist <= 1) {
                                ObjectType rObj = C.map[pawn.targetRecY][pawn.targetRecX].obj;
                                pawn.activity = (rObj == ObjectType::CHESS_TABLE) ? "Playing Chess" : "Playing Horseshoes";
                                pawn.recreation = std::min(100.0f, pawn.recreation + gameHoursElapsed * ((rObj == ObjectType::CHESS_TABLE) ? 85.0f : 55.0f));
                                if (rObj == ObjectType::CHESS_TABLE) GrantSkillXP(pawn, SkillId::INTELLECTUAL, sdt * 45.0f, statusMessage);
                            } else if (pawn.moveTimer <= 0.0f) {
                                pawn.activity = "Going to play"; pawn.moveTimer = 0.18f;
                                GridCoord s = FindNextStep(C.map, pawn.x, pawn.y, pawn.targetRecX, pawn.targetRecY, false);
                                if (s.first == -1) pawn.targetRecX = -1; else { pawn.x = s.first; pawn.y = s.second; }}
                        } else {
                            pawn.activity = "Going for a walk"; pawn.recreation = std::min(100.0f, pawn.recreation + gameHoursElapsed * 40.0f);
                            if (pawn.moveTimer <= 0.0f) { pawn.moveTimer = 0.45f; int nx = pawn.x + GetRandomValue(-1, 1), ny = pawn.y + GetRandomValue(-1, 1); if (isWalkable(nx, ny)) { pawn.x = nx; pawn.y = ny; }}}
                        if (pawn.recreation >= 99.5f) { pawn.doingRecreation = false; pawn.activity = "Idle"; }
                    } else {
                        pawn.activity = "Idle";
                        if (pawn.moveTimer <= 0.0f) { pawn.moveTimer = GetRandomValue(12, 26) * 0.1f; int nx = pawn.x + GetRandomValue(-1, 1), ny = pawn.y + GetRandomValue(-1, 1); if (isWalkable(nx, ny) && Dist(nx, ny, localCols / 2, localRows / 2) < 22.0f) { pawn.x = nx; pawn.y = ny; }}}}
                // ======================= CRITTERS =======================
                for (auto& c : C.critters) {
                    if (c.gone || c.hp <= 0.0f) continue;
                    c.moveTimer -= sdt; c.atkTimer -= sdt;
                    bool isHuman = (c.type == CType::RAIDER || c.type == CType::TRADER);
                    Pawn* tp = nullptr; float tpd = 1e9f; bool tpDowned = true;
                    for (auto& p : C.pawns) if (!p.dead) { float d = Dist(c.x, c.y, p.x, p.y); bool better = (tpDowned && !p.downed) || (tpDowned == p.downed && d < tpd); if (better) { tp = &p; tpd = d; tpDowned = p.downed; }}
                    if (c.type == CType::WOLF) { c.hostile = (tp && tpd < 12.0f) || c.hp < c.maxHp; }
                    if (c.type == CType::RAIDER) {
                        if (c.hp < c.maxHp * 0.3f || !tp) c.leaving = true;
                        if (!c.leaving) {
                            const WeaponDef& w = WEAPONS[c.weapon];
                            bool inRange = tpd <= w.range && (!w.ranged || lineClear(c.x, c.y, tp->x, tp->y));
                            if (inRange) {
                                if (c.atkTimer <= 0.0f) {
                                    c.atkTimer = w.cd * 1.15f;
                                    if (w.ranged) shots.push_back(Shot{(float)c.x, (float)c.y, (float)tp->x, (float)tp->y, 0.12f, ORANGE});
                                    float chance = w.ranged ? rangedHit(4, tpd) : 0.7f;
                                    if (w.ranged && C.map[tp->y][tp->x].obj == ObjectType::SANDBAGS) chance *= 0.6f;
                                    if (GetRandomValue(1, 1000) <= (int)(chance * 1000)) damagePawn(*tp, w.dmg * (0.8f + GetRandomValue(0, 40) * 0.01f)); }
                            } else if (c.moveTimer <= 0.0f) {
                                c.moveTimer = 0.2f; GridCoord s = FindNextStep(C.map, c.x, c.y, tp->x, tp->y, false, true);
                                if (s.first != -1 && !(s.first == c.x && s.second == c.y)) {
                                    LocalCell& sc = C.map[s.second][s.first];
                                    if (sc.obj == ObjectType::WALL) { sc.hp -= 12.0f; if (sc.hp <= 0.0f) { sc.obj = ObjectType::NONE; statusMessage = "Raiders broke through a wall!"; }}
                                    else { c.x = s.first; c.y = s.second; if (sc.obj == ObjectType::TRAP) { sc.obj = ObjectType::NONE; damageCritter(c, 55.0f); statusMessage = "A raider triggered a trap!"; }}}}}
                    } else if (c.type == CType::TRADER) {
                        c.stayHours -= gameHoursElapsed;
                        if (c.stayHours <= 0.0f) c.leaving = true;
                        if (!c.leaving && c.moveTimer <= 0.0f && Dist(c.x, c.y, localCols / 2, localRows / 2) > 5.0f) {
                            c.moveTimer = 0.25f; GridCoord s = FindNextStep(C.map, c.x, c.y, localCols / 2, localRows / 2, false); if (s.first != -1) { c.x = s.first; c.y = s.second; }}
                    } else if (c.hostile && tp && tpd < 30.0f) {
                        if (tpd <= 1.5f) { if (c.atkTimer <= 0.0f) { c.atkTimer = 1.1f; if (GetRandomValue(1, 100) <= 75) damagePawn(*tp, c.meleeDmg * (0.8f + GetRandomValue(0, 40) * 0.01f)); }}
                        else if (c.moveTimer <= 0.0f) { c.moveTimer = 0.2f; GridCoord s = FindNextStep(C.map, c.x, c.y, tp->x, tp->y, false); if (s.first != -1) { c.x = s.first; c.y = s.second; if (C.map[c.y][c.x].obj == ObjectType::TRAP) { C.map[c.y][c.x].obj = ObjectType::NONE; damageCritter(c, 55.0f); }}}
                    } else if (!isHuman) {
                        bool scared = c.hp < c.maxHp || (tp && tpd < 6.0f && c.type != CType::BOAR);
                        if (scared && c.moveTimer <= 0.0f && tp) {
                            c.moveTimer = 0.22f; int bx = c.x, by = c.y; float bd = Dist(c.x, c.y, tp->x, tp->y);
                            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) { int nx = c.x + dx, ny = c.y + dy; if (isWalkable(nx, ny)) { float d = Dist(nx, ny, tp->x, tp->y); if (d > bd) { bd = d; bx = nx; by = ny; }}}
                            c.x = bx; c.y = by;
                        } else if (c.moveTimer <= 0.0f) {
                            c.moveTimer = 0.8f + GetRandomValue(0, 15) * 0.1f;
                            if (GetRandomValue(1, 100) <= 40) { int nx = c.x + GetRandomValue(-1, 1), ny = c.y + GetRandomValue(-1, 1); if (isWalkable(nx, ny)) { c.x = nx; c.y = ny; }}}}
                    if (c.leaving && isHuman && !c.gone) {
                        int tx2 = c.x, ty2 = c.y; int dl = c.x, dr = localCols - 1 - c.x, dtp = c.y, db = localRows - 1 - c.y, m = std::min(std::min(dl, dr), std::min(dtp, db));
                        if (m == dl) tx2 = 0; else if (m == dr) tx2 = localCols - 1; else if (m == dtp) ty2 = 0; else ty2 = localRows - 1;
                        if (m == 0) { c.gone = true; continue; }
                        if (c.moveTimer <= 0.0f) { c.moveTimer = 0.2f; GridCoord s = FindNextStep(C.map, c.x, c.y, tx2, ty2, true, true); if (s.first != -1 && !(s.first == c.x && s.second == c.y) && C.map[s.second][s.first].obj != ObjectType::WALL) { c.x = s.first; c.y = s.second; } else c.gone = true; }}}
                // ---- death / removal sweep ----
                for (int i = (int)C.critters.size() - 1; i >= 0; --i) {
                    Critter& c = C.critters[i];
                    if (!c.gone && c.hp > 0.0f) continue;
                    if (c.gone) { if (!tradeSettle && tradeCritterId == c.id) { openPanel = 0; tradeCritterId = -1; }}
                    else {
                        if (c.type == CType::RAIDER) { C.silver += GetRandomValue(5, 25); if (c.weapon > 0 && GetRandomValue(1, 100) <= 50) C.weaponInv[c.weapon]++; statusMessage = "A raider was killed."; }
                        else if (c.type == CType::TRADER) { statusMessage = "The trader was killed!"; }
                        else { int meat = (c.type == CType::DEER) ? 45 : (c.type == CType::MUFFALO ? 120 : (c.type == CType::BOAR ? 70 : 35)); C.rawFood += meat; statusMessage = c.name + TextFormat(" killed (+%d raw food).", meat); }}
                    for (int j = (int)C.jobs.size() - 1; j >= 0; --j) if (C.jobs[j].type == TaskType::HUNT && C.jobs[j].targetId == c.id) eraseJobAt(j);
                    for (auto& p : C.pawns) if (p.forcedAttackId == c.id) p.forcedAttackId = -1;
                    if (!tradeSettle && tradeCritterId == c.id) { openPanel = 0; tradeCritterId = -1; }
                    C.critters.erase(C.critters.begin() + i); }
                for (int i = (int)shots.size() - 1; i >= 0; --i) { shots[i].t -= sdt; if (shots[i].t <= 0.0f) shots.erase(shots.begin() + i); }
                if (aliveCount() == 0) statusMessage = "All colonists have perished. Press ESC to return to the World Map.";
            }
            }
        }
        // ======================= DRAWING =======================
        BeginDrawing();
        ClearBackground(Color{20, 24, 30, 255});
        if (mode == GameMode::WORLD_EXPLORE) {
            for (int vy = 0; vy < viewRows; ++vy) for (int vx = 0; vx < viewCols; ++vx) {
                int wx = wcamX + vx, wy = wcamY + vy;
                DrawWorldTileIcon(vx * tileSize, vy * tileSize + topBarHeight, tileSize, getWorldElevation(wx, wy), HashTile(wx, wy, seed), time);
                int st = settlementTypeAt(wx, wy);
                if (st >= 0) DrawSettlementIcon(vx * tileSize, vy * tileSize + topBarHeight, tileSize, st, destroyedSettlements.count(GridCoord{wx, wy}) > 0);
                if (colonies.count(GridCoord{wx, wy}) || (haveCur && curKey.first == wx && curKey.second == wy)) {
                    int px = vx * tileSize, py = vy * tileSize + topBarHeight;
                    DrawLine(px + 8, py + 4, px + 8, py + 28, WHITE); DrawTriangle(Vector2{(float)(px + 8), (float)(py + 4)}, Vector2{(float)(px + 8), (float)(py + 14)}, Vector2{(float)(px + 24), (float)(py + 9)}, RED); }}
            int spx = (playerX - wcamX) * tileSize + tileSize / 2, spy = (playerY - wcamY) * tileSize + topBarHeight + tileSize / 2;
            DrawCircle(spx, spy + 2, 10.0f, Color{210, 45, 45, 255}); DrawCircleLines(spx, spy + 2, 10.0f, WHITE);
            DrawCircle(spx, spy - 4, 6.0f, Color{255, 215, 175, 255}); DrawRectangle(spx - 8, spy - 8, 16, 3, Color{120, 75, 35, 255});
        } else {
            for (int vy = 0; vy < visibleMapRows; ++vy) for (int vx = 0; vx < viewCols; ++vx)
                if (inMap(camX + vx, camY + vy)) DrawLocalCell(vx * tileSize, vy * tileSize + topBarHeight, tileSize, C.map[camY + vy][camX + vx], time, showRoofOverlay);
            for (const auto& job : C.jobs) {
                if (job.type == TaskType::RESEARCH_WORK || job.type == TaskType::COOK_MEAL || job.type == TaskType::CRAFT || job.type == TaskType::HUNT) continue;
                int vx = job.tx - camX, vy = job.ty - camY;
                if (vx >= 0 && vx < viewCols && vy >= 0 && vy < visibleMapRows) {
                    int px = vx * tileSize, py = vy * tileSize + topBarHeight;
                    Color mc = (job.type == TaskType::CHOP || job.type == TaskType::MINE || job.type == TaskType::PICK_BUSH || job.type == TaskType::HARVEST_CROP) ? ORANGE : SKYBLUE;
                    if (job.type == TaskType::DECONSTRUCT || job.type == TaskType::REMOVE_ROOF) mc = RED;
                    if (job.type == TaskType::SOW_CROP) mc = LIME;
                    if (job.forcedPawn >= 0) mc = GOLD;
                    DrawRectangleLinesEx(Rectangle{(float)(px + 2), (float)(py + 2), (float)(tileSize - 4), (float)(tileSize - 4)}, 2.0f, mc);
                    if (job.progress > 0.0f) DrawRectangle(px + 4, py + tileSize - 7, (int)((tileSize - 8) * std::min(1.0f, job.progress / job.workRequired)), 4, LIME); }}
            for (const auto& c : C.critters) { int vx = c.x - camX, vy = c.y - camY; if (!c.gone && vx >= 0 && vx < viewCols && vy >= 0 && vy < visibleMapRows) { DrawCritterIcon(vx * tileSize + tileSize / 2, vy * tileSize + topBarHeight + tileSize / 2, c); if (c.hunted) DrawCircleLines(vx * tileSize + tileSize / 2, vy * tileSize + topBarHeight + tileSize / 2, 14.0f, ORANGE); }}
            for (size_t i = 0; i < C.pawns.size(); ++i) {
                const Pawn& p = C.pawns[i]; if (p.dead) continue; int vx = p.x - camX, vy = p.y - camY;
                if (vx >= 0 && vx < viewCols && vy >= 0 && vy < visibleMapRows)
                    DrawPawnIcon(vx * tileSize + tileSize / 2, vy * tileSize + topBarHeight + tileSize / 2, p.shirtColor, p.name, p.activity == "Working" || p.activity == "Researching" || p.activity == "Cooking" || p.activity == "Crafting", p.isSleeping, selectedPawnIdx == (int)i, p.drafted, p.downed, p.weapon, p.hp / p.maxHp); }
            for (const auto& s : shots) DrawLineEx(Vector2{(s.x1 - camX) * tileSize + 16.0f, (s.y1 - camY) * tileSize + topBarHeight + 16.0f}, Vector2{(s.x2 - camX) * tileSize + 16.0f, (s.y2 - camY) * tileSize + topBarHeight + 16.0f}, 2.0f, s.col);
            float darkness = 0.0f;
            if (gameHour < 5.0f || gameHour > 21.0f) darkness = 0.48f; else if (gameHour < 7.0f) darkness = 0.48f * (1.0f - (gameHour - 5.0f) / 2.0f); else if (gameHour > 19.0f) darkness = 0.48f * ((gameHour - 19.0f) / 2.0f);
            if (darkness > 0.01f) {
                std::vector<GridCoord> lamps;
                for (int y = std::max(0, camY - 8); y < std::min(localRows, camY + visibleMapRows + 8); ++y) for (int x = std::max(0, camX - 8); x < std::min(localCols, camX + viewCols + 8); ++x) {
                    ObjectType o = C.map[y][x].obj;
                    if (o == ObjectType::LAMP_TORCH || o == ObjectType::CAMPFIRE || (o == ObjectType::LAMP_STANDING && hasPower())) lamps.push_back(GridCoord{x, y}); }
                if (lamps.empty()) DrawRectangle(0, topBarHeight, screenWidth, mapBottomY - topBarHeight, Color{10, 15, 35, (unsigned char)(darkness * 255)});
                else for (int vy = 0; vy < visibleMapRows; ++vy) for (int vx = 0; vx < viewCols; ++vx) {
                    float md = 99.0f; for (const auto& l : lamps) md = std::min(md, Dist(camX + vx, camY + vy, l.first, l.second));
                    float a = darkness * (1.0f - std::clamp(md / 7.0f, 0.0f, 1.0f));
                    if (a > 0.01f) DrawRectangle(vx * tileSize, vy * tileSize + topBarHeight, tileSize, tileSize, Color{10, 15, 35, (unsigned char)(a * 255)}); }}
            // hover / drag preview
            if (openPanel == 0 && mouse.y > topBarHeight && mouse.y < mapBottomY) {
                int hx = (int)mouse.x / tileSize, hy = (int)(mouse.y - topBarHeight) / tileSize;
                if (hy < visibleMapRows) {
                    if (dragging && selTool >= 0) {
                        int lx = camX + hx, ly = camY + hy, x0 = std::min(dragX0, lx), x1 = std::max(dragX0, lx), y0 = std::min(dragY0, ly), y1 = std::max(dragY0, ly);
                        DrawRectangle((x0 - camX) * tileSize, (y0 - camY) * tileSize + topBarHeight, (x1 - x0 + 1) * tileSize, (y1 - y0 + 1) * tileSize, Color{255, 220, 80, 50});
                        DrawRectangleLines((x0 - camX) * tileSize, (y0 - camY) * tileSize + topBarHeight, (x1 - x0 + 1) * tileSize, (y1 - y0 + 1) * tileSize, GOLD);
                    } else DrawRectangleLines(hx * tileSize, hy * tileSize + topBarHeight, tileSize, tileSize, selTool >= 0 ? GOLD : Color{200, 200, 200, 120}); }}
            if (selTileX >= 0) { int vx = selTileX - camX, vy = selTileY - camY; if (vx >= 0 && vx < viewCols && vy >= 0 && vy < visibleMapRows) DrawRectangleLines(vx * tileSize, vy * tileSize + topBarHeight, tileSize, tileSize, SKYBLUE); }
            // colonist bar
            { int k = 0; for (size_t i = 0; i < C.pawns.size(); ++i) { const Pawn& p = C.pawns[i]; if (p.dead) continue; int bx = 8 + k * 126, by = topBarHeight + 4;
                DrawRectangle(bx, by, 120, 30, Color{20, 24, 32, 215}); DrawRectangleLines(bx, by, 120, 30, selectedPawnIdx == (int)i ? GOLD : GRAY);
                DrawCircle(bx + 14, by + 15, 8.0f, p.shirtColor); DrawText(p.name.c_str(), bx + 28, by + 3, 12, WHITE);
                DrawRectangle(bx + 28, by + 19, 86, 6, BLACK); DrawRectangle(bx + 29, by + 20, (int)(84 * std::clamp(p.mood / 100.0f, 0.0f, 1.0f)), 4, p.mood > 60 ? LIME : (p.mood > 30 ? ORANGE : RED));
                if (p.drafted) DrawText("D", bx + 105, by + 3, 12, ORANGE); if (p.downed) DrawText("!", bx + 105, by + 3, 12, RED); k++; }}
            // bottom bar
            DrawRectangle(0, mapBottomY, screenWidth, bottomBarHeight, Color{22, 26, 34, 255});
            DrawLine(0, mapBottomY, screenWidth, mapBottomY, Color{70, 80, 95, 255});
            int itemsY = mapBottomY + 4, catY = mapBottomY + 52, tabY = mapBottomY + 96;
            if (openPanel == 0) {
                for (int ci = 0; ci < (int)catNames.size(); ++ci) DrawButton(10 + ci * 150, catY, 144, 38, catNames[ci], selCat == ci, LIME);
                if (selCat >= 0) { int k = 0; for (int ti = 0; ti < (int)tools.size(); ++ti) if (tools[ti].cat == selCat) {
                    const ToolDef& td = tools[ti]; int bx = 10 + k * 176; bool locked = (td.research >= 0 && !isResearchDone(td.research)); bool act = (selTool == ti);
                    DrawRectangle(bx, itemsY, 170, 44, act ? Color{55, 80, 115, 255} : Color{32, 38, 48, 255}); DrawRectangleLines(bx, itemsY, 170, 44, act ? GOLD : GRAY);
                    DrawText(td.name, bx + 8, itemsY + 5, 14, locked ? DARKGRAY : (act ? GOLD : WHITE));
                    if (locked) DrawText(TextFormat("Locked: %s", researchTree[td.research].title.c_str()), bx + 8, itemsY + 26, 10, Color{180, 90, 90, 255});
                    else if (td.matChoice) { int cw, cs, cst; ToolCost(td, selMat, cw, cs, cst); DrawText(TextFormat("%d %s (click again: material)", td.matAmt, selMat == MaterialType::WOOD ? "Wood" : (selMat == MaterialType::STONE ? "Stone" : "Steel")), bx + 8, itemsY + 26, 10, LIGHTGRAY); }
                    else DrawText(td.sub, bx + 8, itemsY + 26, 10, LIGHTGRAY);
                    k++; }}}
            const StrList tabNames = {"ARCHITECT", "WORK", "SCHEDULE", "ASSIGN", "RESEARCH", "WORLD MAP"};
            for (int m = 0; m < 6; ++m) { bool active = (m == 0) ? (openPanel == 0) : (m <= 4 ? openPanel == m : false); DrawButton(m * (screenWidth / 6) + 2, tabY, screenWidth / 6 - 4, 38, tabNames[m], active, GOLD, 16); }
            // selected pawn panel
            if (selectedPawnIdx >= 0 && selectedPawnIdx < (int)C.pawns.size() && openPanel == 0) {
                const Pawn& sp = C.pawns[selectedPawnIdx]; int px = 12, py = mapBottomY - 300, pw = 350, ph = 290;
                DrawRectangle(px, py, pw, ph, Color{22, 27, 36, 242}); DrawRectangleLinesEx(Rectangle{(float)px, (float)py, (float)pw, (float)ph}, 2.0f, GOLD);
                DrawText(TextFormat("%s  (%s)", sp.name.c_str(), sp.activity.c_str()), px + 12, py + 10, 15, GOLD);
                DrawButton(px + 230, py + 8, 84, 22, sp.drafted ? "DRAFTED [R]" : "Draft [R]", sp.drafted, ORANGE, 12);
                DrawRectangle(px + pw - 28, py + 8, 20, 20, MAROON); DrawText("X", px + pw - 22, py + 10, 14, WHITE);
                DrawLine(px + 10, py + 34, px + pw - 10, py + 34, Color{60, 70, 85, 255});
                auto bar = [&](int by, const char* label, float val, float maxv, Color col) {
                    DrawText(label, px + 12, by, 12, LIGHTGRAY); DrawRectangle(px + 96, by + 1, 195, 13, Color{14, 17, 22, 255});
                    DrawRectangle(px + 97, by + 2, (int)(193.0f * std::clamp(val / maxv, 0.0f, 1.0f)), 11, col); DrawRectangleLines(px + 96, by + 1, 195, 13, GRAY);
                    DrawText(TextFormat("%d", (int)val), px + 298, by, 12, WHITE); };
                bar(py + 42, "Health:", std::max(0.0f, sp.hp), sp.maxHp, sp.hp > 50 ? LIME : (sp.hp > 25 ? ORANGE : RED));
                bar(py + 62, "Mood:", sp.mood, 100.0f, sp.mood > 60.0f ? LIME : (sp.mood > 30.0f ? ORANGE : RED));
                bar(py + 82, "Food:", sp.hunger, 100.0f, SKYBLUE); bar(py + 102, "Recreation:", sp.recreation, 100.0f, GOLD); bar(py + 122, "Sleep:", sp.energy, 100.0f, PURPLE);
                DrawText(TextFormat("Weapon: %s", WEAPONS[sp.weapon].name), px + 12, py + 144, 12, WHITE);
                DrawText(sp.bedX >= 0 ? TextFormat("Bed: (%d,%d)", sp.bedX, sp.bedY) : "Bed: unassigned", px + 190, py + 144, 12, WHITE);
                DrawLine(px + 10, py + 164, px + pw - 10, py + 164, Color{60, 70, 85, 255});
                DrawText("SKILLS  (* Minor 100%  |  ** Major 150%)", px + 12, py + 170, 11, GOLD);
                for (int s = 0; s < (int)SkillId::COUNT; ++s) {
                    int col = s % 2, row = s / 2, sx = px + 12 + col * 168, sy = py + 189 + row * 24;
                    const char* pm = (sp.skillPassion[s] == 2) ? "**" : (sp.skillPassion[s] == 1 ? "*" : "");
                    DrawText(TextFormat("%s: %d%s", SKILL_NAMES[s], sp.skillLevel[s], pm), sx, sy, 12, sp.skillPassion[s] == 2 ? GOLD : (sp.skillPassion[s] == 1 ? SKYBLUE : RAYWHITE)); }}
            // bills panel
            if (selTileX >= 0 && openPanel == 0 && inMap(selTileX, selTileY) && (C.map[selTileY][selTileX].obj == ObjectType::SMITHY || C.map[selTileY][selTileX].obj == ObjectType::MACHINING)) {
                int bench = (C.map[selTileY][selTileX].obj == ObjectType::SMITHY) ? 0 : 1; int px = 12, py = mapBottomY - 230;
                DrawRectangle(px, py, 400, 220, Color{22, 27, 36, 242}); DrawRectangleLinesEx(Rectangle{(float)px, (float)py, 400.0f, 220.0f}, 2.0f, GOLD);
                DrawText(bench == 0 ? "SMITHY - make until (stockpile count)" : "MACHINING TABLE - make until", px + 10, py + 10, 14, GOLD);
                DrawRectangle(px + 372, py + 6, 20, 20, MAROON); DrawText("X", px + 378, py + 8, 14, WHITE);
                if (bench == 1 && !hasPower()) DrawText("NO POWER", px + 300, py + 12, 11, RED);
                int row = 0; for (int r = 0; r < RECIPE_COUNT; ++r) if (RECIPES[r].bench == bench) {
                    int ry = py + 40 + row * 36;
                    DrawText(RECIPES[r].name, px + 10, ry + 2, 13, isResearchDone(RECIPES[r].research) ? WHITE : DARKGRAY);
                    if (!isResearchDone(RECIPES[r].research)) DrawText(TextFormat("Locked: %s", researchTree[RECIPES[r].research].title.c_str()), px + 150, ry + 2, 10, Color{180, 90, 90, 255});
                    DrawText(TextFormat("W%d S%d  have %d", RECIPES[r].wood, RECIPES[r].steel, C.weaponInv[RECIPES[r].weapon]), px + 10, ry + 17, 10, LIGHTGRAY);
                    DrawButton(px + 240, ry, 40, 28, "-", false, GOLD); DrawText(TextFormat("%d", C.billTarget[r]), px + 292, ry + 6, 16, GOLD); DrawButton(px + 330, ry, 40, 28, "+", false, GOLD); row++; }}
            // ---- full panels ----
            if (openPanel >= 1 && openPanel <= 4) {
                DrawRectangle(PX, PY, PW, PH, Color{20, 24, 32, 248}); DrawRectangleLinesEx(Rectangle{(float)PX, (float)PY, (float)PW, (float)PH}, 2.0f, GOLD);
                std::vector<int> alive; for (int i = 0; i < (int)C.pawns.size(); ++i) if (!C.pawns[i].dead) alive.push_back(i);
                if (openPanel == 1) {
                    DrawText("WORK  (left-click: raise number / right-click: lower;  1 = top priority, blank = disabled)", PX + 20, PY + 15, 18, GOLD);
                    for (int w = 0; w < 8; ++w) DrawText(WORK_NAMES[w], PX + 230 + w * 130 + 6, PY + 62, 13, WHITE);
                    for (size_t r = 0; r < alive.size(); ++r) { const Pawn& p = C.pawns[alive[r]]; int by = PY + 90 + (int)r * 44;
                        DrawCircle(PX + 40, by + 18, 10.0f, p.shirtColor); DrawText(p.name.c_str(), PX + 60, by + 10, 16, WHITE);
                        for (int w = 0; w < 8; ++w) { int bx = PX + 230 + w * 130, v = p.workPrio[w]; Color bg = v == 0 ? Color{30, 32, 38, 255} : (v == 1 ? Color{70, 140, 70, 255} : (v == 2 ? Color{90, 130, 90, 255} : (v == 3 ? Color{120, 130, 90, 255} : Color{130, 110, 80, 255})));
                            DrawRectangle(bx, by, 120, 36, bg); DrawRectangleLines(bx, by, 120, 36, GRAY); if (v > 0) DrawText(TextFormat("%d", v), bx + 54, by + 8, 20, WHITE); }}
                } else if (openPanel == 2) {
                    DrawText("SCHEDULE  (pick a brush, then click or drag over the hours)", PX + 20, PY + 15, 18, GOLD);
                    const StrList bn = {"Sleep", "Work", "Joy", "Anything"}; const Color bc[4] = {Color{70, 90, 190, 255}, Color{200, 150, 50, 255}, Color{80, 180, 90, 255}, Color{130, 135, 145, 255}};
                    for (int b = 0; b < 4; ++b) { DrawRectangle(PX + 170 + b * 130, PY + 50, 120, 30, bc[b]); DrawRectangleLines(PX + 170 + b * 130, PY + 50, 120, 30, schedBrush == b ? WHITE : BLACK); DrawText(bn[b], PX + 170 + b * 130 + 10, PY + 57, 16, WHITE); }
                    for (int h = 0; h < 24; ++h) DrawText(TextFormat("%d", h), PX + 170 + h * 50 + 14, PY + 108, 14, (int)gameHour == h ? GOLD : LIGHTGRAY);
                    for (size_t r = 0; r < alive.size(); ++r) { const Pawn& p = C.pawns[alive[r]]; int by = PY + 130 + (int)r * 46;
                        DrawCircle(PX + 30, by + 20, 10.0f, p.shirtColor); DrawText(p.name.c_str(), PX + 50, by + 12, 16, WHITE);
                        for (int h = 0; h < 24; ++h) { DrawRectangle(PX + 170 + h * 50, by, 46, 40, bc[p.schedule[h]]); DrawRectangleLines(PX + 170 + h * 50, by, 46, 40, (int)gameHour == h ? WHITE : Color{20, 24, 32, 255}); }}
                } else if (openPanel == 3) {
                    DrawText("ASSIGN  (click a bed or weapon cell to cycle through the available options)", PX + 20, PY + 15, 18, GOLD);
                    DrawText("Bed", PX + 210, PY + 62, 14, LIGHTGRAY); DrawText("Weapon", PX + 650, PY + 62, 14, LIGHTGRAY);
                    for (size_t r = 0; r < alive.size(); ++r) { const Pawn& p = C.pawns[alive[r]]; int by = PY + 90 + (int)r * 50;
                        DrawCircle(PX + 40, by + 20, 10.0f, p.shirtColor); DrawText(p.name.c_str(), PX + 60, by + 12, 16, WHITE);
                        DrawButton(PX + 200, by, 420, 40, p.bedX >= 0 ? TextFormat("%s at (%d,%d)", C.map[p.bedY][p.bedX].obj == ObjectType::BED ? "Bed" : "Sleeping spot", p.bedX, p.bedY) : "Unassigned", false, GOLD);
                        DrawButton(PX + 640, by, 420, 40, WEAPONS[p.weapon].name, false, GOLD);
                        }
                    int ty = PY + 90 + (int)alive.size() * 50 + 30; DrawText("Weapons in stockpile:", PX + 20, ty, 14, GOLD);
                    for (int w = 1; w < WEAPON_COUNT; ++w) DrawText(TextFormat("%s x%d", WEAPONS[w].name, C.weaponInv[w]), PX + 20 + ((w - 1) % 4) * 250, ty + 24 + ((w - 1) / 4) * 22, 13, WHITE);
                } else if (openPanel == 4) {
                    DrawText("COLONY RESEARCH TREE (research is slow: costs are in the hundreds; click a project to set it active)", PX + 20, PY + 18, 20, GOLD);
                    DrawRectangle(PX + PW - 110, PY + 10, 90, 35, MAROON); DrawText("CLOSE", PX + PW - 98, PY + 20, 15, WHITE);
                    for (int i = 0; i < (int)researchTree.size(); ++i) { int pre = researchTree[i].prereq;
                        if (pre != -1) DrawLineEx(Vector2{(float)(PX + 40 + researchTree[pre].uiX + 125), (float)(PY + 70 + researchTree[pre].uiY + 47)}, Vector2{(float)(PX + 40 + researchTree[i].uiX + 125), (float)(PY + 70 + researchTree[i].uiY + 47)}, 3.0f, isResearchDone(pre) ? LIME : GRAY); }
                    for (int i = 0; i < (int)researchTree.size(); ++i) {
                        const ResearchNode& node = researchTree[i]; int bx = PX + 40 + node.uiX, by = PY + 70 + node.uiY;
                        bool done = isResearchDone(i), selected = (activeResearchIdx == i && !done), locked = (node.prereq != -1 && !isResearchDone(node.prereq));
                        Color bg = done ? Color{35, 85, 50, 255} : (selected ? Color{55, 80, 125, 255} : (locked ? Color{30, 32, 38, 255} : Color{42, 48, 60, 255}));
                        Color border = done ? LIME : (selected ? GOLD : (locked ? DARKGRAY : LIGHTGRAY));
                        DrawRectangle(bx, by, 250, 95, bg); DrawRectangleLinesEx(Rectangle{(float)bx, (float)by, 250.0f, 95.0f}, 2.0f, border);
                        DrawText(node.title.c_str(), bx + 10, by + 10, 16, done ? LIME : WHITE); DrawText(node.desc.c_str(), bx + 10, by + 32, 11, LIGHTGRAY);
                        const char* statusTxt = done ? "COMPLETED" : (locked ? "LOCKED" : TextFormat("Progress: %d / %d", node.progress, node.cost));
                        DrawText(statusTxt, bx + 10, by + 68, 13, done ? LIME : (selected ? GOLD : RAYWHITE)); }}}
            if (ctxOpen) { for (size_t i = 0; i < ctx.size(); ++i) { bool hov = MouseIn(mouse, ctxX, ctxY + (int)i * 26, 300, 26); DrawRectangle(ctxX, ctxY + (int)i * 26, 300, 26, hov ? Color{70, 100, 150, 255} : Color{30, 36, 46, 245}); DrawRectangleLines(ctxX, ctxY + (int)i * 26, 300, 26, GRAY); DrawText(ctx[i].label.c_str(), ctxX + 8, ctxY + (int)i * 26 + 6, 14, WHITE); }}
        }
        // ---- trade panel (both modes) ----
        if (openPanel == 5) {
            TradeStock* ts = getTradeStock();
            if (!ts || !haveCur) openPanel = 0;
            else {
                DrawRectangle(PX, PY, PW, PH, Color{20, 24, 32, 250}); DrawRectangleLinesEx(Rectangle{(float)PX, (float)PY, (float)PW, (float)PH}, 2.0f, GOLD);
                DrawText(TextFormat("TRADE with %s", ts->who.c_str()), PX + 20, PY + 15, 20, GOLD);
                DrawText(TextFormat("Your silver: %d    Their silver: %d", C.silver, ts->silver), PX + 20, PY + 50, 16, WHITE);
                DrawRectangle(PX + PW - 110, PY + 10, 90, 35, MAROON); DrawText("CLOSE", PX + PW - 98, PY + 20, 15, WHITE);
                DrawText("Item", PX + 20, PY + 85, 14, LIGHTGRAY); DrawText("You have", PX + 220, PY + 85, 14, LIGHTGRAY); DrawText("They have", PX + 340, PY + 85, 14, LIGHTGRAY);
                DrawText("Buy price", PX + 450, PY + 85, 14, LIGHTGRAY); DrawText("Sell price", PX + 1010, PY + 85, 14, LIGHTGRAY);
                for (int i = 0; i < TRADE_COUNT; ++i) {
                    int ry = PY + 110 + i * 40; int buyP = std::max(1, (int)std::ceil(TRADE_BASE[i] * 1.4)), sellP = std::max(1, (int)(TRADE_BASE[i] * 0.5));
                    DrawText(TradeName(i), PX + 20, ry + 8, 16, WHITE); DrawText(TextFormat("%d", TradeQtyRef(C, i)), PX + 230, ry + 8, 16, SKYBLUE); DrawText(TextFormat("%d", ts->qty[i]), PX + 350, ry + 8, 16, ORANGE);
                    DrawText(TextFormat("%d", buyP), PX + 470, ry + 8, 16, GOLD); DrawText(TextFormat("%d", sellP), PX + 1030, ry + 8, 16, GOLD);
                    DrawButton(PX + 560, ry, 82, 32, "Buy 1", false, LIME, 14); DrawButton(PX + 650, ry, 82, 32, "Buy 10", false, LIME, 14);
                    DrawButton(PX + 740, ry, 82, 32, "Sell 1", false, ORANGE, 14); DrawButton(PX + 830, ry, 82, 32, "Sell 10", false, ORANGE, 14); }}}
        // ---- top bar ----
        DrawRectangle(0, 0, screenWidth, topBarHeight, Color{25, 30, 38, 255});
        int hourInt = (int)gameHour, minInt = (int)((gameHour - hourInt) * 60.0f);
        const char* timeStr = allSleeping ? TextFormat("[SLEEP 10X]  DAY %d  |  %02d:%02d", gameDay, hourInt, minInt) : TextFormat("DAY %d  |  %02d:%02d", gameDay, hourInt, minInt);
        DrawText(timeStr, screenWidth - MeasureText(timeStr, 18) - 20, 10, 18, allSleeping ? GOLD : SKYBLUE);
        const char* spd = speed == 0 ? "|| PAUSED  (Space)" : (speed == 1 ? "> x1  (1/2/3, Space)" : (speed == 2 ? ">> x3  (1/2/3, Space)" : ">>> x6  (1/2/3, Space)"));
        DrawText(spd, screenWidth - MeasureText(spd, 14) - 20, 36, 14, speed == 0 ? RED : LIGHTGRAY);
        if (mode == GameMode::WORLD_EXPLORE) {
            DrawText(TextFormat("WORLD MAP  |  COORDS: (%d, %d)  |  SEED: %u  |  [ENTER] Settle/Return  [T] Trade  [X] Raid", playerX, playerY, seed), 18, 10, 17, GOLD);
            DrawText(statusMessage.c_str(), 18, 42, 15, RAYWHITE);
        } else {
            DrawText(TextFormat("SECTOR (%d,%d) [%s] | WOOD %d | STONE %d | STEEL %d | SILVER %d | RAW %d | MEALS %d | POWER %dW/%dW%s", C.worldX, C.worldY, C.biome.c_str(), C.wood, C.stone, C.steel, C.silver, C.rawFood, C.cookedFood, C.powerGen, C.powerUse, C.onBattery ? " (BATTERY)" : ""), 18, 10, 14, LIME);
            DrawText(statusMessage.c_str(), 18, 42, 15, RAYWHITE); }
        EndDrawing(); }
    UnloadAllTex();
    CloseWindow();
    return 0; }

// title: Bug_Invasion_Compatibility
// author: Daniel Hanrahan Tools and Games
// license: GNU GPL-3.0
// version: 1
// script: C/C++

#include "raylib.h"
#include <cstdio>

extern "C" {
#include "lua542Linux/include/lua.h"
#include "lua542Linux/include/lauxlib.h"
#include "lua542Linux/include/lualib.h"
}

//==============================================================
// CONSTANTS
//==============================================================
static const int GAME_W = 240;
static const int GAME_H = 136;
static const int TILE  = 8;
static const int MAP_W = 30;
static const int MAP_H = 17;
static const int MAX_BULLETS = 128;
static const int MAX_ENEMIES = 256;

//==============================================================
// DATA (Must be above LuaCall so player exists in scope)
//==============================================================
struct Player { float x, y, speed; };
struct Bullet { float x, y; int type; bool active; };
struct Enemy  { float x, y, ix, iy; float speed; int type; bool alive; bool active; };

Player player;
Bullet bullets[MAX_BULLETS] = {};
Enemy enemies[MAX_ENEMIES] = {};

//==============================================================
// LUA (OPTIONAL MOD SYSTEM - SAFE HOOKS)
//==============================================================
lua_State* L = nullptr;
bool luaEnabled = false;

// Redirect Lua print to terminal
void HookLuaPrint(lua_State* L)
{
    lua_pushcfunction(L, [](lua_State* L) -> int {
        int n = lua_gettop(L);
        for (int i = 1; i <= n; i++) {
            if (i > 1) printf("\t");
            printf("%s", lua_tostring(L, i));
        }
        printf("\n");
        return 0;
    });
    lua_setglobal(L, "print");
}

void InitLua()
{
    L = luaL_newstate();
    if (!L) return;

    luaL_openlibs(L);
    HookLuaPrint(L);  // <-- redirect Lua print to terminal
    luaEnabled = true;

    // Load optional mod file (safe fail)
    if (luaL_dofile(L, "Bug_Invasion_Compatibility_Mod/Bug_Invasion_Compatibility_Mod.lua") != LUA_OK)
    {
        printf("WARNING: No mods loaded or error in Lua mod file: %s\n", lua_tostring(L,-1));
        lua_pop(L,1);
        lua_close(L);
        L = nullptr;
        luaEnabled = false;
    }
}

void LuaCall(const char* fn)
{
    if (!luaEnabled) return;

    lua_getglobal(L, fn);

    if (lua_isfunction(L, -1))
    {
        // push player state safely
        lua_pushnumber(L, player.x);
        lua_setglobal(L, "player_x");

        lua_pushnumber(L, player.y);
        lua_setglobal(L, "player_y");

        if (lua_pcall(L, 0, 0, 0) != LUA_OK)
        {
            printf("Lua error calling '%s': %s\n", fn, lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    else
    {
        printf("Lua function '%s' not found\n", fn);
        lua_pop(L, 1);
    }
}

//==============================================================
// STATE
//==============================================================
enum GameState { STATE_TITLE, STATE_GAME, STATE_GAMEOVER };
GameState state = STATE_TITLE;
int room = 1;

//==============================================================
// TEXTURES
//==============================================================
Texture2D spriteSheet = {0};
Texture2D tileSheet   = {0};

//==============================================================
Camera2D cam = {0};

//==============================================================
int mapData[MAP_H][MAP_W];

//==============================================================
// SAFE TILE CHECK
//==============================================================
bool IsSafeTile(int tx, int ty)
{
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H)
        return true;
    return (mapData[ty][tx] == 1);
}

//==============================================================
Rectangle GetSpriteRect(int id, Texture2D tex)
{
    int cols = tex.width / TILE;
    return {
        (float)((id % cols) * TILE),
        (float)((id / cols) * TILE),
        (float)TILE,
        (float)TILE
    };
}

//==============================================================
// POOLS
//==============================================================
void ClearBullets() { for(int i=0;i<MAX_BULLETS;i++) bullets[i].active=false; }
void ClearEnemies() { for(int i=0;i<MAX_ENEMIES;i++) enemies[i].active=false; }

void AddBullet(float x, float y, int type)
{
    for(int i=0;i<MAX_BULLETS;i++)
    {
        if(!bullets[i].active)
        {
            bullets[i].x=x; bullets[i].y=y; bullets[i].type=type; bullets[i].active=true;
            return;
        }
    }
}

void AddEnemy(int type, int tx, int ty, float speed)
{
    for(int i=0;i<MAX_ENEMIES;i++)
    {
        if(!enemies[i].active)
        {
            float px = tx * TILE; float py = ty * TILE;
            enemies[i].x=px; enemies[i].y=py; enemies[i].ix=px; enemies[i].iy=py;
            enemies[i].speed=speed; enemies[i].type=type; enemies[i].alive=true; enemies[i].active=true;
            return;
        }
    }
}

//==============================================================
// MAPS
//==============================================================
void LoadRoomMap1()
{
    const char* rows[MAP_H] = {
        "000000000000000000000000000000","000000000000000000000000000000",
        "000000000000000000000000000000","111111111111111111111111111111",
        "111111111111111111111111111111","111111111111111111111111111111",
        "111111111111111111111111111111","111111111111111111111111111111",
        "111111111111111111111111111111","111111111111111111111111111111",
        "111111111111111111111111111111","111111111111111111111111111111",
        "111111111111111111111111111111","111111111111111111111111111111",
        "111111111111111111111111111111","000000000000000000000000000000",
        "000000000000000000000000000000"
    };
    for(int y=0;y<MAP_H;y++) for(int x=0;x<MAP_W;x++) mapData[y][x] = rows[y][x]-'0';
}

void LoadRoomMap2()
{
    const char* rows[MAP_H] = {
        "222222222222222222222222222222","222222222222222222222222222222",
        "222222222211111111111111111111","222222222211111111111111111111",
        "222222222211111111111111111111","222222222211111111111111111111",
        "222222222211111111111111111111","222222222211111111222222222222",
        "222222222211111111222222222222","222222222211111111222222222222",
        "111111111111111111222222222222","111111111111111111222222222222",
        "111111111111111111222222222222","111111111111111111222222222222",
        "111111111111111111222222222222","111111111111111111222222222222",
        "222222222222222222222222222222"
    };
    for(int y=0;y<MAP_H;y++) for(int x=0;x<MAP_W;x++) mapData[y][x] = rows[y][x]-'0';
}

void LoadRoomMap3()
{
    const char* rows[MAP_H] = {
        "333333333333333333333333333333","333333333333333333333333333333",
        "333333333333333333333333333333","333333331111111111111113333333",
        "333333331111111111111113333333","333333331111111111111113333333",
        "333333331111111111111113333333","333333331111111111111113333333",
        "333333331111333333311113333333","333333331111333333311113333333",
        "333333331111333333311113333333","111111111111333333311111111111",
        "111111111111333333311111111111","111111111111333333311111111111",
        "111111111111333333311111111111","111111111111333333311111111111",
        "333333333333333333333333333333"
    };
    for(int y=0;y<MAP_H;y++) for(int x=0;x<MAP_W;x++) mapData[y][x] = rows[y][x]-'0';
}

void LoadRooms()
{
    if(room==1||room==4||room==7) LoadRoomMap1();
    if(room==2||room==5||room==8) LoadRoomMap2();
    if(room==3||room==6||room==9) LoadRoomMap3();
}

//==============================================================
// ROOM LOAD
//==============================================================
void LoadRoom()
{
    room = GetRandomValue(1,9);
    ClearBullets(); ClearEnemies(); LoadRooms();

    float speed=0.0f;
    if(room>=4 && room<=6) speed=0.3f;
    if(room>=7) speed=0.8f;

    player.x=1*TILE; player.y=(room<=6?12:13)*TILE; player.speed=1.2f;

    for(int i=0;i<16;i++) AddEnemy(i%4,24+(i%5),3+(i%10),speed);
}

//==============================================================
// GAME UPDATE
//==============================================================
void UpdateGame()
{
    LuaCall("on_update"); // Lua now has player_x/player_y

    if(IsKeyDown(KEY_UP)) player.y-=player.speed;
    if(IsKeyDown(KEY_DOWN)) player.y+=player.speed;
    if(IsKeyDown(KEY_LEFT)) player.x-=player.speed;
    if(IsKeyDown(KEY_RIGHT)) player.x+=player.speed;

    if(player.x<0) player.x=0;
    if(player.x>=GAME_W-TILE){ LoadRoom(); return; }

    if(IsKeyPressed(KEY_Z)) AddBullet(player.x+8, player.y+2,2);
    if(IsKeyPressed(KEY_X)) AddBullet(player.x+8, player.y+2,0);
    if(IsKeyPressed(KEY_A)) AddBullet(player.x+8, player.y+2,3);
    if(IsKeyPressed(KEY_S)) AddBullet(player.x+8, player.y+2,1);

    for(int i=0;i<MAX_BULLETS;i++)
        if(bullets[i].active){ bullets[i].x+=3; if(bullets[i].x>GAME_W) bullets[i].active=false; }

    for(int i=0;i<MAX_ENEMIES;i++)
    {
        if(!enemies[i].active || !enemies[i].alive) continue;
        enemies[i].x-=enemies[i].speed;
        if(enemies[i].x<-8){ enemies[i].x=enemies[i].ix; enemies[i].y=enemies[i].iy; }
    }

    for(int b=0;b<MAX_BULLETS;b++)
    {
        if(!bullets[b].active) continue;
        Rectangle br={bullets[b].x,bullets[b].y,8,8};
        for(int e=0;e<MAX_ENEMIES;e++)
        {
            if(!enemies[e].active||!enemies[e].alive) continue;
            Rectangle er={enemies[e].x,enemies[e].y,8,8};
            if(CheckCollisionRecs(br,er))
            {
                if(bullets[b].type==enemies[e].type) enemies[e].alive=false;
                bullets[b].active=false;
                break;
            }
        }
    }

    Rectangle pr={player.x,player.y,8,8};
    for(int e=0;e<MAX_ENEMIES;e++)
    {
        if(!enemies[e].active||!enemies[e].alive) continue;
        Rectangle er={enemies[e].x,enemies[e].y,8,8};
        if(CheckCollisionRecs(pr,er)){ state=STATE_GAMEOVER; return; }
    }

    int tx=(int)(player.x/TILE);
    int ty=(int)(player.y/TILE);
    if(tx>=0 && tx<MAP_W && ty>=0 && ty<MAP_H)
        if(!IsSafeTile(tx,ty)) state=STATE_GAMEOVER;
}

//==============================================================
// DRAW
//==============================================================
void DrawAll()
{
    BeginDrawing();
    ClearBackground(BLACK);

    LuaCall("on_draw");

    if(state==STATE_TITLE)
    {
        DrawText("BUG_INVASION_COMPATIBILITY",70,50,24,WHITE);
        DrawText("PRESS Z",95,90,20,WHITE);
        DrawText("Copyright (C) 2026 Daniel",70,140,12,WHITE);
        DrawText("Hanrahan Tools and Games",70,155,12,WHITE);
        DrawText("SPDX-License-Identifier: GPL-3.0-or-later",70,170,12,WHITE);
        DrawText("CC BY-SA 4.0 components included",70,240,12,WHITE);
        EndDrawing();
        return;
    }

    if(state==STATE_GAMEOVER)
    {
        DrawText("GAME OVER",80,50,24,RED);
        DrawText("PRESS Z",95,90,20,WHITE);
        EndDrawing();
        return;
    }

    BeginMode2D(cam);
    for(int y=0;y<MAP_H;y++)
        for(int x=0;x<MAP_W;x++)
            DrawTextureRec(tileSheet,GetSpriteRect(mapData[y][x],tileSheet),{x*TILE,y*TILE},WHITE);

    DrawTextureRec(spriteSheet,GetSpriteRect(0,spriteSheet),{player.x,player.y},WHITE);

    for(int i=0;i<MAX_BULLETS;i++)
        if(bullets[i].active)
            DrawTextureRec(spriteSheet,GetSpriteRect(1+bullets[i].type,spriteSheet),{bullets[i].x,bullets[i].y},WHITE);

    for(int i=0;i<MAX_ENEMIES;i++)
        if(enemies[i].active&&enemies[i].alive)
            DrawTextureRec(spriteSheet,GetSpriteRect(5+enemies[i].type,spriteSheet),{enemies[i].x,enemies[i].y},WHITE);

    EndMode2D();
    EndDrawing();
}

//==============================================================
void UpdateAll()
{
    if(state==STATE_TITLE)
    {
        if(IsKeyPressed(KEY_Z)){ state=STATE_GAME; LoadRoom(); }
        return;
    }

    if(state==STATE_GAMEOVER)
    {
        if(IsKeyPressed(KEY_Z)){ state=STATE_GAME; LoadRoom(); }
        return;
    }

    UpdateGame();
}

//==============================================================
int main()
{
    InitWindow(960,540,"Bug_Invasion_Compatibility");

    spriteSheet=LoadTexture("assets/sprites.png");
    tileSheet=LoadTexture("assets/tiles.png");

    cam.zoom=4.0f;

    InitLua(); // safe optional mods

    SetTargetFPS(60);

    while(!WindowShouldClose())
    {
        UpdateAll();
        DrawAll();
    }

    CloseWindow();
    return 0;
}

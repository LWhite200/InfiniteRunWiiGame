//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//   -=-=-=-=-= Things To Add -=-=-=-=-=
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// 1. Collision, Jumping
// 2. World Reset, winning, dying, and pausing
//
// 
// 3. Different World Blocks (Holes) (spikes) (enemy placement)
//
// 
// 4. Enemys, shooting, and Boss
// 5. debug menu, gui, menus, and ammo


// fps settings don't work, so fix if possible to increase performance
// as probably working at 'unlimited' fps


#include <grrlib.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <ogc/lwp_watchdog.h> // For gettime() and ticks_to_millisecs()
#include <wiiuse/wpad.h>
#include <stdio.h>
#include <time.h> // Include this for random number generation
#include <string.h>
#include <unistd.h> // for readlink()
#include <limits.h>

#define TARGET_FPS 60
#define FRAME_TIME (1000 / TARGET_FPS) // Time per frame in milliseconds

#define UP 0
#define DOWN 1
#define RIGHT 2
#define LEFT 3

#define GRRLIB_BLACK   0x00000080
#define GRRLIB_WHITE   0xFFFFFFFF
#define GRRLIB_GREEN   0x008000FF
#define GRRLIB_LIME    0x00FF00FF
#define GRRLIB_SILVER  0xC0C0C0FF
#define GRRLIB_RED     0xFF0000FF

#define GRID_WIDTH 1
#define GRID_HEIGHT 5
#define BLOCK_W 30
#define BLOCK_H 18
#define TILE_SIZE 1.0f

const float GRAVITY = 0.5f;            // Gravity acceleration
const float JUMP_STRENGTH = 0.1f;     // Initial jump strength
const float MAX_JUMP_DURATION = 0.6f;  // Maximum duration the jump button can be held
const float JUMP_INCREASE_RATE = 0.00125f; // Rate at which jump increases per frame
int isJumping = 0;

float playerSize = 0.25;

float bulletLifeTime = 0.8f;
float bulletSpeed = 0.475f;

#define PLAYER_SPEED 0.1f
#define ENEMY_SPEED 0.01f
#define MAX_ENEMIES 10

int numEnemies = 0;
int playColor = 5;

typedef struct {
    float x;
    float y;
    float z;
    float width;
    float height;
    float depth;
} BoundingBox;


#define MAX_CUBES 100

BoundingBox cubes[MAX_CUBES];
int numCubes = 0; // Number of cubes currently in the game



int distanceMoved = 0;
int world[7]; //-=-=-=-=-=-=-=-=-==--=-=-
int TopBlocks[7]; //-=-=-=-=-=-=-=-=-==--=-=-
int worldDistance[7]; //-=-=-=-=-=-=-=-=-==--=-=-

typedef struct {
    int matrix[BLOCK_H][BLOCK_W];
} Blocks;

Blocks topLayer[7]; //-=-=-=-=-=-=-=-=-==--=-=-

typedef struct {
    int matrix[BLOCK_H][BLOCK_W];
} Block;

Block groundLayer[7]; //-=-=-=-=-=-=-=-=-==--=-=-

u32 colors[] = {
    GRRLIB_BLACK, GRRLIB_WHITE, GRRLIB_GREEN, GRRLIB_LIME,  GRRLIB_SILVER, GRRLIB_RED
};

// Update the updateWorldGrid function to handle the new world size
// Update the updateWorldGrid function to handle the new world size
void updateWorldGrid() {
    float baseX = 0.0f; // Adjust base position as needed

    numCubes = 0;
    for (int i = 0; i < MAX_CUBES; i++) {
        cubes[i] = (BoundingBox){ 0 }; // Set all fields to zero or defaults
    }

    for (int i = 0; i < 7; i++) {
        for (int col = 0; col < BLOCK_W; col++) {
            for (int row = 0; row < BLOCK_H; row++) {
                // Render ground layer block
                int groundTileType = groundLayer[i].matrix[row][col];
                u32 groundColor = colors[groundTileType];
                float groundWorldX = baseX + worldDistance[i] + row * TILE_SIZE;
                float groundWorldZ = -col * TILE_SIZE;
                GRRLIB_ObjectViewBegin();
                GRRLIB_ObjectViewRotate(0, 0, 0);
                GRRLIB_ObjectViewTrans(groundWorldX, 0.5 * (1- playerSize), groundWorldZ);
                GRRLIB_ObjectViewEnd();
                GRRLIB_DrawCube(TILE_SIZE, TILE_SIZE, groundColor);

                // Render top layer block (1 unit above ground layer)
                int topTileType = topLayer[i].matrix[row][col];
                if (topTileType != 0) {
                    u32 topColor = colors[topTileType];
                    float topWorldX = baseX + worldDistance[i] + row * TILE_SIZE;
                    float topWorldZ = -col * TILE_SIZE; // Keep it at the same Z level as ground layer
                    float topWorldY = TILE_SIZE; // 1 unit above ground layer

                    // This holds coordinates for collision
                    cubes[numCubes++] = (BoundingBox){
                   .x = topWorldX,
                   .y = topWorldY,
                   .z = topWorldZ,
                   .width = TILE_SIZE,
                   .height = TILE_SIZE,
                   .depth = TILE_SIZE,
                    };

                    GRRLIB_ObjectViewBegin();
                    GRRLIB_ObjectViewRotate(0, 0, 0);
                    GRRLIB_ObjectViewTrans(topWorldX, topWorldY + 0.5 * (1 - playerSize), topWorldZ);
                    GRRLIB_ObjectViewEnd();
                    GRRLIB_DrawCube(TILE_SIZE, 1, topColor);

                    GRRLIB_ObjectViewBegin();
                    GRRLIB_ObjectViewRotate(0, 0, 0);
                    GRRLIB_ObjectViewTrans(topWorldX, topWorldY + 0.5 * (1 - playerSize), topWorldZ);
                    GRRLIB_ObjectViewEnd();
                    GRRLIB_DrawCube(TILE_SIZE + 0.01, 0, 0x00000080);
                }
            }
        }
    }
}






void fillWorldDistance()
{
    worldDistance[0] = -BLOCK_H;
    worldDistance[1] = 0;
    worldDistance[2] = BLOCK_H;
    worldDistance[3] = BLOCK_H * 2;
    worldDistance[4] = BLOCK_H * 3;
    worldDistance[5] = BLOCK_H * 4;
    worldDistance[6] = BLOCK_H * 5;
}

// (1) = flat
// (2) = few bumps
// (3) = few holes
// (4) = bumps and holes

void GenerateNewBlock(int index, int newBlockType) {

    // Clear previous top layer data
    for (int row = 0; row < BLOCK_H; row++) {
        for (int col = 0; col < BLOCK_W; col++) {
            topLayer[index].matrix[row][col] = 0;
        }
    }

    switch (newBlockType) {
    case 1:
        // Generate block type 1
        for (int row = 0; row < BLOCK_H; row++) {
            for (int col = 0; col < BLOCK_W; col++) {
                // Example: Assigning values to the new block (adjust as per your game logic)
                groundLayer[index].matrix[row][col] = ((row + col) % 2 == 0) ? 3 : 2;
            }
        }
        break;
    case 2:
        // Generate block type 2
        for (int row = 0; row < BLOCK_H; row++) {
            for (int col = 0; col < BLOCK_W; col++) {
                // Example: Assigning different values for block type 2
                groundLayer[index].matrix[row][col] = ((row + col) % 3 == 0) ? 1 : 4;
            }
        }
        break;
        // Add more cases as needed for different block types
    default:
        // Handle default case or error condition
        break;
    }

    //-=-===-=-=-=-=-=-==-=-==--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
    //-=-===-=-=-=-=-=-==-=-==--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
    //-=-===-=-=-=-=-=-==-=-==--=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

    int numCubesToGenerate = 5;

    // Clear previous top layer data
    for (int row = 0; row < BLOCK_H; row++) {
        for (int col = 0; col < BLOCK_W; col++) {
            topLayer[index].matrix[row][col] = 0;
        }
    }

    // Create and store BoundingBox
    if (numCubes < MAX_CUBES) {
        switch (newBlockType) {
        case 1:
            for (int i = 0; i < numCubesToGenerate; i++) {
                int randRow = rand() % BLOCK_H;
                int randCol = rand() % BLOCK_W;
                topLayer[index].matrix[randRow][randCol] = 1; // Place a cube
            }
            break;
        case 2:
            for (int i = 0; i < numCubesToGenerate + 3; i++) {
                int randRow = rand() % BLOCK_H;
                int randCol = rand() % BLOCK_W;
                topLayer[index].matrix[randRow][randCol] = 4; // Place a cube
            }
            break;
        default:
            break;
        }
    }
}

void GenerateWorld() {

    for (int i = 0; i < 7; i++) {
        int newGroundBlock = rand() % 3 + 1; // Random number between 1 and 2
        GenerateNewBlock(i, newGroundBlock);
    }
}


void shiftWorldArray() {
    int newGroundBlock = rand() % 2 + 1;

    // Shift the world array to the left
    for (int i = 0; i < 6; i++) {
        world[i] = world[i + 1];
        memcpy(&groundLayer[i], &groundLayer[i + 1], sizeof(Block));
        memcpy(&topLayer[i], &topLayer[i + 1], sizeof(Blocks));
        worldDistance[i] = worldDistance[i + 1];
    }

    // Set the last element to the new block type
    world[6] = newGroundBlock;
    GenerateNewBlock(6, newGroundBlock);
    worldDistance[6] += BLOCK_H;
}



//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//-------Objects

#define MAX_BULLETS 10
typedef struct {
    float x, y, z;
    float vx, vy, vz;
    int active;
    float lifetime; // New field for bullet lifetime
} Bullet;
Bullet bullets[MAX_BULLETS];

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    int facing; // 00=up, 11=down, 01=right, 10=left
    int width, height, maxHP, curHP;
    int invincible; // New field to indicate invincibility
    float invincibleTime; // New field to keep track of invincibility duration
    float jumpTime; // Field to keep track of jump button held time
} Player;

typedef struct {
    float x, y;
    float vx, vy;
    int direction; // 00=up, 11=down, 01=right, 10=left
    int width, height, maxHP, curHP;
    int invincible; // New field to indicate invincibility
    float invincibleTime; // New field to keep track of invincibility duration
} Enemy;

Enemy enemies[MAX_ENEMIES]; // array of enemys

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//-------intitiats none-world objects

void checkPlayerPosition(Player* player) {
    int playerGridX = floor(player->x); // Assuming each block is 8 units wide

    // Check if player has moved to the right by 8 units
    if (playerGridX > distanceMoved + BLOCK_H) {
        shiftWorldArray();
        distanceMoved += BLOCK_H; // Update the world shift distance
    }
    // Handle shifting to the left if necessary (not implemented in original request)
}





void initBullets() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        bullets[i].active = 0;
    }
}

void initEnemies() {
    // Example enemy initialization
    enemies[0] = (Enemy){ (GRID_WIDTH / 2), (GRID_HEIGHT / 2) + 3, 0, 0, TILE_SIZE, TILE_SIZE, 3, 3 };
}

void renderEnemies() {
    // Render enemies
    for (int i = 0; i < numEnemies; i++) {
        GRRLIB_ObjectViewBegin();
        GRRLIB_ObjectViewRotate(0, 0, 0);
        GRRLIB_ObjectViewTrans(enemies[i].x, -enemies[i].y, 0);
        GRRLIB_ObjectViewEnd();
        GRRLIB_DrawCube(TILE_SIZE, 1, 0x00FF00FF); // Draw enemy as a green cube
    }
}

void renderBullets() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            GRRLIB_ObjectViewBegin();
            GRRLIB_ObjectViewRotate(0, 0, 0);
            // Adjusting the z-coordinate for bullet rendering in 3D
            GRRLIB_ObjectViewTrans(bullets[i].x, bullets[i].y, bullets[i].z);
            GRRLIB_ObjectViewEnd();
            GRRLIB_DrawCube(playerSize, 1, 0xFF0000FF); // Render bullet as a small purple cube
        }
    }
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//-------collision and object updates

int checkBulletEnemyCollision(Bullet* bullet, Enemy* enemy) {
    return fabs(bullet->x - enemy->x) < TILE_SIZE / 2 && fabs(bullet->y - enemy->y) < TILE_SIZE / 2;
}

void handleBulletEnemyCollisions() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            for (int j = 0; j < numEnemies; j++) {
                if (checkBulletEnemyCollision(&bullets[i], &enemies[j])) {
                    bullets[i].active = 0; // Deactivate bullet
                    enemies[j].curHP -= 1; // Decrease enemy health
                    if (enemies[j].curHP <= 0) {
                        // Remove the enemy
                        for (int k = j; k < numEnemies - 1; k++) {
                            enemies[k] = enemies[k + 1];
                        }
                        numEnemies--;
                        j--; // Recheck the current index as it now has a new enemy
                    }
                    break; // No need to check other enemies for this bullet
                }
            }
        }
    }
}

int checkPlayerEnemyCollision(Player* player, Enemy* enemy) {
    return fabs(player->x - enemy->x) < TILE_SIZE && fabs(player->y - enemy->y) < TILE_SIZE;
}

void launchBullet(Player* player) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].active = 1;
            bullets[i].x = player->x;
            bullets[i].y = player->y;
            bullets[i].z = player->z; // Set the z-coordinate
            bullets[i].lifetime = bulletLifeTime; // Set initial lifetime to 0.25 seconds

            switch (player->facing) {
            case UP:
                bullets[i].vx = player->vx * 0.75f;
                bullets[i].vz = bulletSpeed; // Move in the z direction
                break;
            case DOWN:
                bullets[i].vx = player->vx * 0.75f;
                bullets[i].vz = -bulletSpeed;
                break;
            case RIGHT:
                bullets[i].vx = -bulletSpeed;
                bullets[i].vz = player->vz * 0.75f;
                break;
            case LEFT:
                bullets[i].vx = bulletSpeed;
                bullets[i].vz = player->vz * 0.75f;
                break;
            }
            break;
        }
    }
}

void updateBullets(float deltaTime) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            bullets[i].x += bullets[i].vx * deltaTime * 60;
            bullets[i].y += bullets[i].vy * deltaTime * 60;
            bullets[i].z += bullets[i].vz * deltaTime * 60; // Update z-coordinate

            bullets[i].lifetime -= deltaTime; // Decrement lifetime

            // Deactivate bullet if it goes out of bounds or its lifetime expires
            if (bullets[i].lifetime <= 0) {
                bullets[i].active = 0;
            }
        }
    }
    handleBulletEnemyCollisions();
}


//=-==-=-==-=-

void resetGame(Player* player) {

    // Player player = { 0, 1, -(BLOCK_W / 2), 0, 0, 0, RIGHT, TILE_SIZE, TILE_SIZE, 3, 3, 0, 0.0f, 0.0f };

    // Reset player position and health
    player->x = 0;
    player->y = 1;
    player->z = -(BLOCK_W / 2);
    player->vx = 0;
    player->vy = 0;
    player->vz = 0;
    player->curHP = player->maxHP;
    player->invincible = 0;
    player->invincibleTime = 0.0f;

    distanceMoved = 0;

    // Clear previous top layer data
    for (int i = 0; i < 7; i++)
    {
        world[i] = 0;
        worldDistance[i] = 0;

        for (int row = 0; row < BLOCK_H; row++) {
            for (int col = 0; col < BLOCK_W; col++) {
                topLayer[i].matrix[row][col] = 0;
                groundLayer[i].matrix[row][col] = 0;

                
            }
        }
    }

    fillWorldDistance();
    GenerateWorld();
}

//=-==-=-==-=-

// Check collision with ground and top layers, considering z-coordinate
int checkCollision(Player* player, float new_x, float new_y, float new_z) {
    for (int i = 0; i < numCubes; i++) {
        BoundingBox box = cubes[i];

        // Check if the player's new position intersects with the bounding box
        if (new_x < box.x + box.width && new_x + player->width > box.x &&
            new_y < box.y + box.height && new_y + player->height > box.y &&
            new_z < box.z + box.depth && new_z + player->width > box.z) { // maybe error 
            return 1; // Collision detected
        }
    }
    return 0; // No collision
}

int checkGroundCollision(Player* player, float new_x, float new_y, float new_z) {
    if (!isJumping && new_z >= -BLOCK_W + 0.25 && new_z <= 0.25 && new_x > worldDistance[0] - 1 && new_y >= 0.0f && new_y <= 1.025f) {
        return 1;
    }
    else {
        return 0;
    }
}


void updatePlayer(Player* player, int faceDirection, float deltaTime, u32 held) {
    // Apply gravity if not jumping or falling
    if (!(held & WPAD_BUTTON_B) || player->jumpTime >= MAX_JUMP_DURATION) {
        player->vy -= GRAVITY * deltaTime;
        isJumping = 0; // Not jumping anymore
    }

    

    // Calculate potential new position
    float new_x = player->x + player->vx;
    float new_y = player->y + player->vy;
    float new_z = player->z + player->vz;

    

    // Check for ground collision
    if (checkGroundCollision(player, new_x, new_y, new_z)) {
        player->y = 1;  // Ground level
        player->vy = 0; // Stop vertical velocity
        playColor = 5;
        isJumping = 0; // Not jumping anymore
    }
    else {
        player->y = new_y; // Update vertical position
        playColor = 0;
    }

    // Handle jumping mechanics
    if (held & WPAD_BUTTON_B && player->vy == 0) {
        player->vy = JUMP_STRENGTH; // Initial jump strength
        player->jumpTime = 0.0f; // Reset jump time
        isJumping = 1; // Set jumping flag
    }
    if (held & WPAD_BUTTON_B && player->jumpTime < MAX_JUMP_DURATION) {
        player->jumpTime += deltaTime;
        player->vy += JUMP_INCREASE_RATE; // Increase jump height
        isJumping = 1; // Set jumping flag
    }

    




    // Check for horizontal collision
    if (!checkCollision(player, new_x, player->y, player->z)) {
        player->x = new_x;
    }
    else {
        player->vx = 0;
        player->x = round(player->x); // Round to nearest whole number
    }

    // Check for z-axis collision
    if (!checkCollision(player, player->x, player->y, new_z)) {
        player->z = new_z;
    }
    else {
        player->vz = 0;
        player->z = round(player->z); // Round to nearest whole number
    }

    player->facing = faceDirection;

    if (player->y < -10)
    {
        resetGame(player);
    }
}






void updatePlayerFace(Player* player) {
    float faceX = player->x;
    float faceZ = player->z;

    // UP=0, DOWN=1, RIGHT=2, LEFT=3
    if (player->facing == UP) {
        faceZ += 0.55 * playerSize;
    }
    else if (player->facing == DOWN) {
        faceZ -= 0.55 * playerSize;
    }
    else if (player->facing == RIGHT) {
        faceX -= 0.55 * playerSize;
    }
    else if (player->facing == LEFT) {
        faceX += 0.55 * playerSize;
    }

    GRRLIB_ObjectViewBegin();
    GRRLIB_ObjectViewRotate(0, 0, 0);
    GRRLIB_ObjectViewTrans(faceX, player->y, faceZ);
    GRRLIB_ObjectViewEnd();
    GRRLIB_DrawCube(playerSize * 0.45f, 1, 0xFFB6C1FF); // 0x800080FF  FFB6C1

    GRRLIB_ObjectViewBegin();
    GRRLIB_ObjectViewRotate(0, 0, 0);
    GRRLIB_ObjectViewTrans(faceX, player->y, faceZ);
    GRRLIB_ObjectViewEnd();
    GRRLIB_DrawCube(playerSize * 0.45f, 0, 0x00000080); // 0x800080FF  FFB6C1
}




void updateEnemies(Player* player, float deltaTime) {
   
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//-------instantiate the scene and game loop

int main(void) {
    srand(time(NULL)); // Initialize random number generator
    u64 startTick = gettime();

    float camZ = 0.0f;
    GRRLIB_Init();
    WPAD_Init();
    GRRLIB_Settings.antialias = true;

    Player player = { 0, 1, -(BLOCK_W / 2), 0, 0, 0, RIGHT, TILE_SIZE, TILE_SIZE, 3, 3, 0, 0.0f, 0.0f };
    initEnemies();
    initBullets(); // Initialize bullets

    int faceDirection = UP;

    fillWorldDistance();
    GenerateWorld();

    while (1) {
        WPAD_ScanPads();
        GRRLIB_2dMode();
        u32 pressed = WPAD_ButtonsDown(0);
        u32 held = WPAD_ButtonsHeld(0);

        if (pressed & WPAD_BUTTON_HOME) exit(0);
        if (held & WPAD_BUTTON_PLUS) camZ += 1.0f;
        if (held & WPAD_BUTTON_MINUS) camZ -= 1.0f;

        if (held & WPAD_BUTTON_RIGHT) {
            player.vz = PLAYER_SPEED;
            faceDirection = UP;
        }
        else if (held & WPAD_BUTTON_LEFT) {
            player.vz = -PLAYER_SPEED;
            faceDirection = DOWN;
        }
        else {
            player.vz = 0;
        }

        if (held & WPAD_BUTTON_UP) {
            player.vx = PLAYER_SPEED;
            faceDirection = LEFT;
        }
        else if (held & WPAD_BUTTON_DOWN) {
            player.vx = -PLAYER_SPEED;
            faceDirection = RIGHT;
        }
        else {
            player.vx = 0;
        }

        if (pressed & WPAD_BUTTON_A) {
            launchBullet(&player); // Launch bullet when A is pressed
        }

        float deltaTime = 1.0f / TARGET_FPS;

        // Handle jumping
        if (pressed & WPAD_BUTTON_B && player.vy == 0) {
            player.vy = JUMP_STRENGTH; // Initial jump strength
            player.jumpTime = 0.0f; // Reset jump time
        }
        if (held & WPAD_BUTTON_B && player.jumpTime < MAX_JUMP_DURATION) {
            player.jumpTime += deltaTime;
            player.vy += JUMP_INCREASE_RATE; // Increase jump height
        }

        GRRLIB_SetBackgroundColour(0x00, 0x00, 0xFF, 0xFF);  // 0x0000FFFF
        GRRLIB_3dMode(0.1, 1000, 45, 0, 1);
        //GRRLIB_SetLightAmbient(0x111111FF);

        // Example of setting a bright ambient light color
        GRRLIB_SetLightAmbient(0xFFFFFFFF);  // Full white ambient light

        GRRLIB_SetLightDiff(0, (guVector) { -3.0f, 0.0f, 0.0f }, 30.0f, 1.0f, 0xFF0000FF);

        checkPlayerPosition(&player); // Check player position and shift world array if needed
        updateWorldGrid();

        updateBullets(deltaTime); // Update bullets
        updatePlayer(&player, faceDirection, deltaTime, held);
        updatePlayerFace(&player);
        updateEnemies(&player, deltaTime);
        

        GRRLIB_Camera3dSettings(
            player.x - 8, player.y + camZ, player.z,  // Adjusted for player's position and some offset
            0, 1, 0,
            player.x, player.y, player.z
        );


        GRRLIB_ObjectViewBegin();
        GRRLIB_ObjectViewRotate(0, 0, 0);
        GRRLIB_ObjectViewTrans(player.x, player.y, player.z);
        GRRLIB_ObjectViewEnd();
        GRRLIB_DrawCube(playerSize, 1, colors[playColor]);

        GRRLIB_ObjectViewBegin();
        GRRLIB_ObjectViewRotate(0, 0, 0);
        GRRLIB_ObjectViewTrans(player.x, player.y, player.z);
        GRRLIB_ObjectViewEnd();
        GRRLIB_DrawCube(playerSize + 0.01, 0, 0x00000080);

        renderEnemies();
        renderBullets(); // Render bullets
        GRRLIB_Render();

        u64 endTick = gettime();
        u32 frameTime = ticks_to_millisecs(endTick - startTick);
        if (frameTime < FRAME_TIME) {
            usleep((FRAME_TIME - frameTime) * 1000);
        }
    }

    GRRLIB_Exit();
    return 0;
}

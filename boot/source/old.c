//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//   -=-=-=-=-= Things To Add -=-=-=-=-=
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// 1. Collision, Jumping
// 2. World Reset, winning, dying, and pausing
// 3. Different World Blocks (Holes) (spikes) (enemy placement)
// 4. Enemys, shooting, and Boss
// 5. debug menu, gui, menus, and ammo

// [RIGHT NOW]
// Improve World Generation


// fix fps 


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

#define GRRLIB_BLACK   0x000000FF
#define GRRLIB_WHITE   0xFFFFFFFF
#define GRRLIB_GREEN   0x008000FF
#define GRRLIB_LIME    0x00FF00FF
#define GRRLIB_SILVER  0xC0C0C0FF
#define GRRLIB_RED     0xFF0000FF

#define numBlocks 5
#define numObs 28
#define GRID_WIDTH 1
#define GRID_HEIGHT 5
#define BLOCK_W 38 // the z position
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

#define PLAYER_SPEED 0.075f
#define ENEMY_SPEED 0.01f
#define MAX_ENEMIES 10

int numEnemies = 0;
int playColor = 5;

// cubes
typedef struct {
    float x;
    float y;
    float z;
    int index;
} BoundingBox;

typedef struct {
    BoundingBox matrix[BLOCK_H][BLOCK_W];
} TopBlock;
TopBlock topBlock[numBlocks];


// holes
typedef struct {
    float x;
    float y;
    float z;
    int index;
} holeObj;

typedef struct {
    holeObj matrix[BLOCK_H][BLOCK_W];
} HoleBlock;
HoleBlock holeBlock[numBlocks];





int distanceMoved = 0;
int worldDistance[numBlocks]; //-=-=-=-=-=-=-=-=-==--=-=-

typedef struct {
    int matrix[BLOCK_H][BLOCK_W];
} Block;
Block groundLayer[numBlocks]; //-=-=-=-=-=-=-=-=-==--=-=-

u32 colors[] = {
    GRRLIB_BLACK, GRRLIB_WHITE, GRRLIB_GREEN, GRRLIB_LIME,  GRRLIB_SILVER, GRRLIB_RED
};

// Update the updateWorldGrid function to handle the new world size
// Update the updateWorldGrid function to handle the new world size
void updateWorldGrid() {

    // Render ground layers
    for (int i = 0; i < numBlocks; i++) {
        for (int col = 0; col < BLOCK_H; col++) {
            for (int row = 0; row < BLOCK_W; row++) {
                int groundTileType = groundLayer[i].matrix[col][row];
                u32 groundColor = colors[groundTileType];
                float groundWorldX = worldDistance[i] + col;
                float groundWorldZ = -row * TILE_SIZE;

                if (groundTileType != 0)
                {
                    // Render ground cubes
                    GRRLIB_ObjectViewBegin();
                    GRRLIB_ObjectViewRotate(0, 0, 0);
                    GRRLIB_ObjectViewTrans(groundWorldX, 0.5 * (1 - playerSize), groundWorldZ);
                    GRRLIB_ObjectViewEnd();
                    GRRLIB_DrawCube(TILE_SIZE, TILE_SIZE, groundColor);
                }
            }
        }
    }

    // Render top layers
    for (int i = 0; i < numBlocks; i++) {
        for (int row = 0; row < BLOCK_H; row++) {
            for (int col = 0; col < BLOCK_W; col++) {
                if (topBlock[i].matrix[row][col].x != 0.0f) { // Check if the top block exists at this position
                    BoundingBox box = topBlock[i].matrix[row][col];

                    // Render top block (adjust position and size as needed)
                    GRRLIB_ObjectViewBegin();
                    GRRLIB_ObjectViewRotate(0, 0, 0);
                    GRRLIB_ObjectViewTrans(box.x, 1 + 0.5f * (1 - playerSize), box.z);
                    GRRLIB_ObjectViewEnd();
                    GRRLIB_DrawCube(TILE_SIZE, 1, GRRLIB_RED); // Adjust size and color as necessary
                    GRRLIB_DrawCube(TILE_SIZE + 0.01, 0, GRRLIB_BLACK); // Adjust size and color as necessary
                }
            }
        }
    }
}




// loop until numBlocks - 1
void fillWorldDistance()
{
    worldDistance[0] = -BLOCK_H;
    worldDistance[1] = 0;

    for (int i = 2; i < numBlocks; ++i) {
        worldDistance[i] = BLOCK_H * (i - 1);
    }
}

void addNewTop(int index, int x, int z) {
    topBlock[index].matrix[x][z] = (BoundingBox){
        .x = worldDistance[index] + x, // Corrected position calculation
        .y = 1,
        .z = -z,
        .index = index,
    };
}

void addNewHole(int index, int x, int z) {
    holeBlock[index].matrix[x][z] = (holeObj){
        .x = worldDistance[index] + x, // Corrected position calculation
        .y = 0,
        .z = -z,
        .index = index,
    };
}

// (1) = flat
// (2) = 2x2 and 3x3 Top
// (3) = randomTop
// (4) = holes
// (5) = 2x2 + holes
// (6) = holes + randomTop
// (7) = water level
// (8) = boss
void genFlatFloor(int index)
{
    for (int col = 0; col < BLOCK_H; col++) {
        for (int row = 0; row < BLOCK_W; row++) {
            groundLayer[index].matrix[col][row] = ((col + row) % 3 == 0) ? 1 : 4;
        }
    }
}

void gen2x2(int index) {
    int placedObstacles = 0;
    bool occupied[BLOCK_H][BLOCK_W] = { false };

    while (placedObstacles < numObs)
    {
        int threeOrTwo = rand() % 3 + 2; 
        int randRow = rand() % BLOCK_W;
        int randCol = rand() % BLOCK_H;

        if (randRow >= 1 && randRow < BLOCK_W - 1 && randCol >= 1 && randCol < BLOCK_H - 1 && !occupied[randCol][randRow])
        {
            int clumpSize = (threeOrTwo == 2) ? 2 : 3;

            if (clumpSize == 3) {
                occupied[randCol][randRow] = true;
            }

            for (int i = -1; i <= clumpSize - 2; ++i)
            {
                for (int j = -1; j <= clumpSize - 2; ++j)
                {
                    if (!occupied[randCol + i][randRow + j] && placedObstacles < numObs)
                    {
                        addNewTop(index, randCol + i, randRow + j);
                        occupied[randCol + i][randRow + j] = true;
                        placedObstacles++;
                    }
                }
            }
        }
    }
}

void genRandTop(int index)
{
    bool occupied[BLOCK_H][BLOCK_W] = { false };
    int placedObstacles = 0;
    while (placedObstacles < numObs) {

        // why this crash???
        int randRow = rand() % BLOCK_W;
        int randCol = rand() % BLOCK_H;
        // why this crash???

        if (!occupied[randCol][randRow]) {
            occupied[randCol][randRow] = true;
            addNewTop(index, randCol, randRow); // Swap randRow and randCol for correct orientation
            placedObstacles++;
        }
    }
}

void genHoleFloor(int index)
{
    // generates the ground layer
    for (int row = 0; row < BLOCK_H; row++) {
        for (int col = 0; col < BLOCK_W; col++) {
            groundLayer[index].matrix[row][col] = ((row + col) % 2 == 0) ? 3 : 2;
        }
    }

    //replaces parts with holes
    int placedObstacles = 0;
    bool occupied[BLOCK_H][BLOCK_W] = { false };

    while (placedObstacles < numObs)
    {
        int threeOrTwo = rand() % 3 + 2;
        int randRow = rand() % BLOCK_W;
        int randCol = rand() % BLOCK_H;

        if (randRow >= 1 && randRow < BLOCK_W - 1 && randCol >= 1 && randCol < BLOCK_H - 1 && !occupied[randCol][randRow])
        {
            int clumpSize = (threeOrTwo == 2) ? 2 : 3;

            // Place the hole and ensure surrounding blocks are the same color (3)
            for (int i = -1; i <= clumpSize - 2; ++i)
            {
                for (int j = -1; j <= clumpSize - 2; ++j)
                {
                    int targetRow = randCol + i;
                    int targetCol = randRow + j;

                    if (!occupied[targetRow][targetCol] && placedObstacles < numObs)
                    {
                        // Place the hole
                        addNewHole(index, targetRow, targetCol);
                        occupied[targetRow][targetCol] = true;
                        groundLayer[index].matrix[targetRow][targetCol] = 0;
                        placedObstacles++;

                        // Set surrounding blocks to color 3
                        for (int ni = -1; ni <= 1; ++ni)
                        {
                            for (int nj = -1; nj <= 1; ++nj)
                            {
                                if (targetRow + ni >= 0 && targetRow + ni < BLOCK_H &&
                                    targetCol + nj >= 0 && targetCol + nj < BLOCK_W &&
                                    !occupied[targetRow + ni][targetCol + nj])
                                {
                                    groundLayer[index].matrix[targetRow + ni][targetCol + nj] = 2;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void genLinedRoom(int index)
{
    // Determine orientation: 0 for horizontal (2/3 probability), 1 for vertical (1/3 probability)
    int orientation;
    if (rand() % 3 < 2) {
        orientation = 1; // Horizontal orientation chosen 2/3 of the time
    }
    else {
        orientation = 0; // Vertical orientation chosen 1/3 of the time
    }

    // Determine gap size between stripes (1 to 3 spaces)
    int gapSize = rand() % (int)(0.33 * BLOCK_W) + 3;

    // Generate stripes
    for (int i = 0; i < (orientation == 0 ? BLOCK_H : BLOCK_W); i += gapSize + 1)
    {
        for (int j = 0; j < (orientation == 0 ? BLOCK_W : BLOCK_H); ++j)
        {
            // Randomly decide whether to add a block
            if (rand() % 5 != 0) {
                if (orientation == 0) {
                    // Horizontal stripes
                    addNewTop(index, i, j);
                }
                else {
                    // Vertical stripes
                    addNewTop(index, j, i);
                }
            }
        }
    }
}




// (1) = flat
// (2) = 2x2 and 3x3 Top
// (3) = randomTop
// (4) = holes
// (5) = 2x2 + holes
// (6) = holes + randomTop
// (7) = lined room (flat)
// (8) = lined room (hole)
//
// The following are for a later date
// (9) = water boat stage
// (10) = boss
void GenerateNewBlock(int index) {

    int newGroundBlock = rand() % 7 + 2; // 2 through 8 somehow

    if (newGroundBlock < 2 && newGroundBlock > 8)
    {
        newGroundBlock = 1;
    }

    switch (newGroundBlock) {
    case 1:
        genFlatFloor(index);
        break;
    case 2:
        genFlatFloor(index);
        gen2x2(index);
        break;
    case 3:
        genFlatFloor(index);
        genRandTop(index);
        break;
    case 4:
        genHoleFloor(index);
        break;
    case 5:
        genHoleFloor(index);
        gen2x2(index);
        break;
    case 6:
        genHoleFloor(index);
        genRandTop(index);
        break;
    case 7:
        genFlatFloor(index);
        genLinedRoom(index);
        break;
    case 8:
        genHoleFloor(index);
        genLinedRoom(index);
        break;
    default:
        // Handle default case if needed
        break;
    }
}



void GenerateWorld() {

    for (int i = 0; i < numBlocks; i++) {
        GenerateNewBlock(i);
    }
}

void shiftWorldArray() {
    for (int i = 0; i < (numBlocks - 1); i++) {
        memcpy(&groundLayer[i], &groundLayer[i + 1], sizeof(Block));
        memcpy(&topBlock[i], &topBlock[i + 1], sizeof(TopBlock));
        memcpy(&holeBlock[i], &holeBlock[i + 1], sizeof(HoleBlock));
        worldDistance[i] = worldDistance[i + 1];
    }

    memset(&groundLayer[numBlocks - 1], 0, sizeof(Block));
    memset(&topBlock[numBlocks - 1], 0, sizeof(TopBlock));
    memset(&holeBlock[numBlocks - 1], 0, sizeof(HoleBlock));
    worldDistance[numBlocks - 1] += BLOCK_H;

    GenerateNewBlock(numBlocks - 1);
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

    // Clear world data
    for (int i = 0; i < numBlocks; i++) {
        worldDistance[i] = 0;

        for (int row = 0; row < BLOCK_H; row++) {
            for (int col = 0; col < BLOCK_W; col++) {
                groundLayer[i].matrix[row][col] = 0;
                // Clear topBlock data
                topBlock[i].matrix[row][col].x = 0.0f;
                topBlock[i].matrix[row][col].y = 0.0f;
                topBlock[i].matrix[row][col].z = 0.0f;

                holeBlock[i].matrix[row][col].x = 0.0f;
                holeBlock[i].matrix[row][col].y = 0.0f;
                holeBlock[i].matrix[row][col].z = 0.0f;
            }
        }
    }

    fillWorldDistance();
    GenerateWorld();
}


//=-==-=-==-=-

// Check collision with ground and top layers, considering z-coordinate
int checkCollision(Player* player, float new_x, float new_y, float new_z) {
    for (int i = 0; i < numBlocks; i++) {
        for (int row = 0; row < BLOCK_H; row++) {
            for (int col = 0; col < BLOCK_W; col++) {
                BoundingBox box = topBlock[i].matrix[row][col];

                // Check if the player's new position intersects with the top block's bounding box
                if (box.x != 0.0f &&
                    new_x < box.x + TILE_SIZE && new_x + player->width > box.x &&
                    new_y < box.y + TILE_SIZE && new_y + player->height > box.y &&
                    new_z < box.z + TILE_SIZE && new_z + player->width > box.z) {
                    return 1; // Collision detected
                }
            }
        }
    }
    return 0; // No collision
}

int checkGroundCollision(Player* player, float new_x, float new_y, float new_z) {
    if (!isJumping && new_z >= -BLOCK_W + 0.25 && new_z <= 0.25 && new_x > worldDistance[0] - 1 && new_y >= 0.0f && new_y <= 1.025f) {
        for (int i = 0; i < numBlocks; i++) {
            for (int row = 0; row < BLOCK_H; row++) {
                for (int col = 0; col < BLOCK_W; col++) {
                    holeObj hole = holeBlock[i].matrix[row][col];

                    // Check if the player's new position intersects with the hole object
                    if (round(new_x) == round(hole.x) && round(new_z) == round(hole.z)) {
                        return 0; // The player is over a hole, so no collision
                    }
                }
            }
        }
        return 1; // The player is on solid ground, so collision with the ground
    }
    else {
        return 0; // The player is either jumping or out of ground bounds
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

    if (player->y < -30)
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
            player.x - 8, player.y + camZ + 3.75, player.z,  // Adjusted for player's position and some offset
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

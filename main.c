#include <raylib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>


#define BOARD_SIZE 8
#define TILE_SIZE 42
#define TILE_TYPES 5
#define SCORE_FONT_SIZE 32
#define MAX_SCORE_POPUPS 32
#define MAX_PARTICLES 256

typedef struct {
    Vector2 position;
    Vector2 velocity;
    float lifetime;
    float alpha;
    Color color;
    bool active;
} Particle;

extern Particle particles[MAX_PARTICLES];
void spawn_particles(int x, int y, Vector2 grid_origin);
void update_particles(float dt);
void draw_particles(void);

bool find_hint(Vector2 *);

const char tile_chars[TILE_TYPES] = {'#', '@', '$', '%', '&'};

char board[BOARD_SIZE][BOARD_SIZE];
bool matched[BOARD_SIZE][BOARD_SIZE] = { 0 }; // To track matched tiles
float fall_offset[BOARD_SIZE][BOARD_SIZE] = { 0 }; // To track falling tiles


typedef enum {
    STATE_INTRO, // Added intro state
	STATE_IDLE,
	STATE_ANIMATING,
	STATE_MATCH_DELAY,
	STATE_SWAPPING
} TileState;


TileState tile_state;
TileState post_intro_state; // Store the state after init_board

Vector2 swap_from = {-1, -1};
Vector2 swap_to = {-1, -1};
float swap_progress = 0.0f;
const float SWAP_DURATION = 0.15f; // Duration in seconds

typedef struct {
	Vector2 position;
	int amount;
	float lifetime;
	float alpha;
	bool active;
} ScorePopup;

ScorePopup score_popups[MAX_SCORE_POPUPS] = { 0 };

int score = 0;
int high_score = 0;
Vector2 grid_origin;
Texture2D background;
Font score_font;
Vector2 selected_tile = { -1, -1 }; // To track the selected tile position
float fall_speed = 8.0f; // Speed of falling tiles
float match_delay_timer = 0.0f; // Timer for match delay
const float MATCH_DELAY_DURATION = 0.2f; // Delay before resolving matches

float score_scale = 1.0f;
float score_scale_velocity = 0.0f;
bool score_animating = false;



// Hint system variables
float idle_timer = 0.0f;
const float HINT_IDLE_DURATION = 15.0f; 
bool hint_active = false;
Vector2 hint_tiles[3] = { {-1, -1}, {-1, -1}, {-1, -1} };


// Wrong move warning variables
bool wrong_move = false;
float wrong_move_timer = 0.0f;
const float WRONG_MOVE_DURATION = 0.3f;
Vector2 wrong_move_from = {-1, -1};
Vector2 wrong_move_to = {-1, -1};

bool music_on = true;


Music background_music;
Sound match_sound;


char random_tile() {
    return tile_chars[rand() % TILE_TYPES];
}


void swap_tiles(int x1, int y1, int x2, int y2){
	char temp = board[y1][x1];
	board[y1][x1] = board[y2][x2];
	board[y2][x2] = temp;
}


bool are_tiles_adjacent(Vector2 a, Vector2 b){
	return(abs((int)a.x - (int)b.x) + abs((int)a.y - (int)b.y)) == 1;
}


void add_score_popup(int x, int y, int amount, Vector2 grid_origin){
	for(int i = 0; i < MAX_SCORE_POPUPS; i++){
		if (!score_popups[i].active){
			score_popups[i].position = (Vector2){
				grid_origin.x + x * TILE_SIZE + TILE_SIZE / 2,
				grid_origin.y + y * TILE_SIZE + TILE_SIZE / 2
			};
			score_popups[i].amount = amount;
			score_popups[i].lifetime = 1.0f;
			score_popups[i].alpha = 1.0f;
			score_popups[i].active = true;
			break;
		}
	}
}

bool find_matches(){
	bool found = false;
	for (int y = 0; y < BOARD_SIZE; y++) {
		for (int x = 0; x < BOARD_SIZE; x++){
			matched[y][x] = false; // Reset matched status
		}
	}

	for (int y = 0; y < BOARD_SIZE; y++){
		for(int x = 0; x < BOARD_SIZE - 2; x++){
			char t = board[y][x];
			if( t == board[y][x + 1] &&
				 t == board[y][x + 2]){
				matched[y][x] = matched[y][x + 1] = matched[y][x + 2] = true;
				// update score
				score += 10;
				found = true;
				PlaySound(match_sound);
				score_animating = true;
				score_scale = 2.0f;
				score_scale_velocity = -2.5f;
				add_score_popup(x, y, 10, grid_origin);
				spawn_particles(x, y, grid_origin); // spawn particles for match
			}
		}
	}

	for (int x = 0; x < BOARD_SIZE; x++){
		for(int y = 0; y < BOARD_SIZE - 2; y++){
			char t = board[y][x];
			if( t == board[y + 1][x] &&
				 t == board[y + 2][x]){
				matched[y][x] = matched[y + 1][x] = matched[y + 2][x] = true;
				// update score
				score += 10;
				found = true;
				PlaySound(match_sound);
				score_animating = true;
				score_scale = 2.0f;
				score_scale_velocity = -2.5f;
				add_score_popup(x, y, 10, grid_origin);
				spawn_particles(x, y, grid_origin); // spawn particles for match
			}
		}
	}

	return found;
	
}


void resolve_matches(){
	for (int x = 0; x < BOARD_SIZE; x++){
		int write_y = BOARD_SIZE - 1; // Start from the bottom
		for(int y = BOARD_SIZE - 1; y >= 0; y--){
			if(!matched[y][x]){
				if(y != write_y){
					board[write_y][x] = board[y][x]; // Move non-matched tiles down
					fall_offset[write_y][x] = (write_y - y) * TILE_SIZE; // Set fall offset
					board[y][x] = ' '; // Clear the original position
				}
				write_y--;
			}
		}

		// Fill the remaining tiles with random tiles
		while(write_y >= 0){
			board[write_y][x] = random_tile();
			fall_offset[write_y][x] = (write_y + 1) * TILE_SIZE; // Set fall offset for new tiles
			write_y--;
		}
	}


	 tile_state = STATE_ANIMATING; // Set state to animating

}



void init_board(){
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            board[y][x] = random_tile();
        }
    }

    int grid_width = BOARD_SIZE * TILE_SIZE;
    int grid_height = BOARD_SIZE * TILE_SIZE;

    grid_origin = (Vector2){
        (GetScreenWidth() - grid_width) / 2,
        (GetScreenHeight() - grid_height) / 2
    };


	if (find_matches()){
		resolve_matches();
	} else {
		tile_state = STATE_IDLE;
	}
}

// High score file path
#define HIGH_SCORE_FILE "highscore.txt"

// load high score from file
void load_high_score() {
    FILE *file = fopen(HIGH_SCORE_FILE, "r");
    if (file) {
        fscanf(file, "%d", &high_score);
        fclose(file);
    } else {
        high_score = 0;
    }
}

// save high score to file
void save_high_score() {
    FILE *file = fopen(HIGH_SCORE_FILE, "w");
    if (file) {
        fprintf(file, "%d", high_score);
        fclose(file);
    }
}

float intro_timer = 0.0f; // Timer for intro screen
#define INTRO_DURATION 5.0f // 5 seconds

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "MAtch-3");
    SetTargetFPS(60);
    srand(time(NULL));

	InitAudioDevice();

    background = LoadTexture("assets/background.png");
    score_font = LoadFontEx("assets/04b03.ttf", SCORE_FONT_SIZE, NULL, 0);
	background_music = LoadMusicStream("assets/bgm.mp3");
	match_sound = LoadSound("assets/match.mp3");

	PlayMusicStream(background_music);

    init_board();
    post_intro_state = tile_state; // Save the state set by init_board
    Vector2 mouse = {0, 0};
    load_high_score();

    tile_state = STATE_INTRO; // Start with intro screen
    intro_timer = 0.0f;
    Rectangle musicButton = { 20, 70, 120, 36 };

    while(!WindowShouldClose()){

		UpdateMusicStream(background_music); // Update music stream

        // Intro screen
        if (tile_state == STATE_INTRO) {
            intro_timer += GetFrameTime();
            BeginDrawing();
            ClearBackground(BLACK);
            const char* intro_text = "Mechres";
            int font_size = 48;
            Vector2 text_size = MeasureTextEx(score_font, intro_text, font_size, 1.0f);
            DrawTextEx(score_font, intro_text,
                (Vector2){(screenWidth - text_size.x) / 2, (screenHeight - text_size.y) / 2},
                font_size, 1.0f, YELLOW);
            EndDrawing();
            if (intro_timer >= INTRO_DURATION) {
                tile_state = post_intro_state; // Restore the state set by init_board
            }
            continue; // Skip rest of loop while in intro
        }

        // update game logic
        mouse = GetMousePosition();
        if (tile_state == STATE_IDLE && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            int x = (mouse.x - grid_origin.x) / TILE_SIZE;
            int y = (mouse.y - grid_origin.y) / TILE_SIZE;

            if (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE) {
                Vector2 current_tile = (Vector2){ x, y };
				if (selected_tile.x < 0) {
					selected_tile = current_tile;
				}
				else {
					if (are_tiles_adjacent(selected_tile, current_tile)) {
						swap_from = selected_tile;
						swap_to = current_tile;
						swap_progress = 0.0f;
						tile_state = STATE_SWAPPING;
					}
					selected_tile = (Vector2){-1, -1};
					
				}
            }
        }
		
		// Handle swap animation
		if (tile_state == STATE_SWAPPING) {
			swap_progress += GetFrameTime() / SWAP_DURATION;
			if (swap_progress >= 1.0f) {
				swap_tiles(swap_from.x, swap_from.y, swap_to.x, swap_to.y);
				if (find_matches()) {
					resolve_matches();
				} else {
					swap_tiles(swap_from.x, swap_from.y, swap_to.x, swap_to.y); // Swap back if no match
					// Set wrong move warning
					wrong_move = true;
					wrong_move_timer = WRONG_MOVE_DURATION;
					wrong_move_from = swap_from;
					wrong_move_to = swap_to;
					tile_state = STATE_IDLE;
				}
				swap_from = swap_to = (Vector2){-1, -1};
			}
		}

        // Update particles
        update_particles(GetFrameTime());

        // Wrong move
        if (wrong_move) {
            wrong_move_timer -= GetFrameTime();
            if (wrong_move_timer <= 0.0f) {
                wrong_move = false;
            }
        }

		if (tile_state == STATE_ANIMATING) {
			bool still_animating = false;
			for (int y = 0; y < BOARD_SIZE; y++){
				for (int x = 0; x < BOARD_SIZE; x++){
					if (fall_offset[y][x] > 0){
						fall_offset[y][x] -= fall_speed; // Decrease fall offset
						if (fall_offset[y][x] < 0) {
							fall_offset[y][x] = 0; // Reset to zero if it falls below
						} else {
							still_animating = true; // Still animating
						}
					}
				}
			}

			if (!still_animating) {
				tile_state = STATE_MATCH_DELAY; // Move to match delay state
				match_delay_timer = MATCH_DELAY_DURATION;
				}
		}
		  if (tile_state == STATE_MATCH_DELAY) {
			match_delay_timer -= GetFrameTime();
			if( match_delay_timer <= 0) {
				if(find_matches()){
					resolve_matches();
				} else {
					tile_state = STATE_IDLE; // Go back to idle state if no matches found
				}
			}
		  }
		
		  // update score popups array
		  for (int i = 0; i < MAX_SCORE_POPUPS; i++){
			if (score_popups[i].active){
				score_popups[i].lifetime -= GetFrameTime();
				score_popups[i].position.y -= 30 * GetFrameTime();
				score_popups[i].alpha -= 1.0f * GetFrameTime();
				if (score_popups[i].lifetime <= 0.0f) {
					score_popups[i].active = false;
				}
			}
		  }


		  // update score animation
		  if(score_animating){
			score_scale += score_scale_velocity * GetFrameTime();
			if (score_scale <= 1.0f){
				score_scale = 1.0f;
				score_animating = false;
			}
		  }

        BeginDrawing();
        ClearBackground(BLACK);

        // Draw background
        DrawTexturePro(background, (Rectangle){0,0, background.width, background.height},
                       (Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()}, 
                       (Vector2){0, 0}, 0.0f, WHITE);

        DrawRectangle(
            grid_origin.x,
            grid_origin.y,
            BOARD_SIZE* TILE_SIZE,
            BOARD_SIZE* TILE_SIZE,
            Fade(DARKGRAY, 0.60f)
        );

        draw_particles(); // Draw particles before tiles
        // --- HINT SYSTEM: Update hint logic (should only be here, outside the tile drawing loop) ---
        if (tile_state == STATE_IDLE) {
            idle_timer += GetFrameTime();
            if (idle_timer >= HINT_IDLE_DURATION && !hint_active) {
                if (find_hint(hint_tiles)) {
                    hint_active = true;
                } else {
                    hint_active = false;
                }
            }
            // Reset hint if player interacts
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                idle_timer = 0.0f;
                hint_active = false;
            }
        } else {
            idle_timer = 0.0f;
            hint_active = false;
        }

        for (int y = 0; y < BOARD_SIZE; y++) {
            for (int x = 0; x < BOARD_SIZE; x++) {
                Vector2 draw_pos = {
                    grid_origin.x + (x * TILE_SIZE),
                    grid_origin.y + (y * TILE_SIZE) - fall_offset[y][x]
                };

                // If swapping, interpolate positions
                if (tile_state == STATE_SWAPPING) {
                    if (swap_from.x == x && swap_from.y == y) {
                        draw_pos.x = grid_origin.x + (swap_from.x + (swap_to.x - swap_from.x) * swap_progress) * TILE_SIZE;
                        draw_pos.y = grid_origin.y + (swap_from.y + (swap_to.y - swap_from.y) * swap_progress) * TILE_SIZE;
                    } else if (swap_to.x == x && swap_to.y == y) {
                        draw_pos.x = grid_origin.x + (swap_to.x + (swap_from.x - swap_to.x) * (1 - swap_progress)) * TILE_SIZE;
                        draw_pos.y = grid_origin.y + (swap_to.y + (swap_from.y - swap_to.y) * (1 - swap_progress)) * TILE_SIZE;
                    }
                }

                Rectangle rect = { draw_pos.x, draw_pos.y, TILE_SIZE, TILE_SIZE };
                DrawRectangleLinesEx(rect, 1, DARKGRAY);

                // Hint system
                if (hint_active) {
                    for (int h = 0; h < 2; h++) {
                        if ((int)hint_tiles[h].x == x && (int)hint_tiles[h].y == y) {
                            DrawRectangleRec(rect, Fade(YELLOW, 0.25f));
                        }
                    }
                }

                // Wrong move red overlay
                if (wrong_move && 
                    ((x == (int)wrong_move_from.x && y == (int)wrong_move_from.y) ||
                     (x == (int)wrong_move_to.x && y == (int)wrong_move_to.y))) {
                    DrawRectangleRec(rect, Fade(RED, 0.6f));
                }

                DrawTextEx(GetFontDefault(),
                    TextFormat("%c", board[y][x]),
                    (Vector2){ rect.x + 12, rect.y + 8 },
                    20,
                    1,
                    matched[y][x] ? GREEN : PINK
                );
            }
        }

        // Draw selected tile
        if (selected_tile.x >= 0) {
            DrawRectangleLinesEx((Rectangle){
                grid_origin.x + (selected_tile.x * TILE_SIZE),
                grid_origin.y + (selected_tile.y * TILE_SIZE),
                TILE_SIZE, TILE_SIZE
            }, 2, YELLOW);
        }

        DrawTextEx(score_font, 
                   TextFormat("Score: %d", score), 
                   (Vector2){20, 20}, 
                   SCORE_FONT_SIZE * score_scale, 1.0f, SKYBLUE);
        DrawTextEx(score_font, 
                   TextFormat("High Score: %d", high_score), 
                   (Vector2){20, 120}, 
                   SCORE_FONT_SIZE * 0.7f, 1.0f, DARKBLUE);

			// draw score popups
			for (int i = 0; i < MAX_SCORE_POPUPS; i++){
				if (score_popups[i].active){
					Color c = Fade(PURPLE, score_popups[i].alpha);
					DrawText(
						TextFormat("+%d", score_popups[i].amount),
						score_popups[i].position.x,
						score_popups[i].position.y,
						20, c);
				}
			}


		DrawRectangleRec(musicButton, music_on ? BLUE : DARKGRAY);
        DrawRectangleLinesEx(musicButton, 2, PURPLE);
        DrawText(music_on ? "Music: ON" : "Music: OFF", musicButton.x + 7, musicButton.y + 8, 20, WHITE);
        
		// Music toggle button
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointRec(mouse, musicButton)) {
            music_on = !music_on;
            if (music_on) {
                PlayMusicStream(background_music);
            } else {
                PauseMusicStream(background_music);
            }
        }
		

		if (score > high_score) {
		high_score = score;
		save_high_score();
		}
	
        // Draw score
        //DrawText(TextFormat("Score: %d", score), 20, 20, 24, YELLOW);
        EndDrawing();
    }

	StopMusicStream(background_music); // Stop music stream
	UnloadMusicStream(background_music); // Unload music stream
	UnloadSound(match_sound); // Unload match sound
    UnloadTexture(background); // Unload background texture
    UnloadFont(score_font); // Unload score font

	CloseAudioDevice();

    save_high_score(); // Ensure high score is saved on exit

    CloseWindow(); // Close window and OpenGL context
    return 0;
}

Particle particles[MAX_PARTICLES] = {0};

void spawn_particles(int x, int y, Vector2 grid_origin) {
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < MAX_PARTICLES; j++) {
            if (!particles[j].active) {
                float angle = (float)(i * 30) * (PI / 180.0f);
                float speed = 60 + rand() % 40;
                particles[j].position = (Vector2){
                    grid_origin.x + x * TILE_SIZE + TILE_SIZE / 2,
                    grid_origin.y + y * TILE_SIZE + TILE_SIZE / 2
                };
                particles[j].velocity = (Vector2){
                    cosf(angle) * speed,
                    sinf(angle) * speed
                };
                particles[j].lifetime = 0.5f + (rand() % 10) * 0.02f;
                particles[j].alpha = 1.0f;
                particles[j].color = (Color){255, 255, 100 + rand()%156, 255};
                particles[j].active = true;
                break;
            }
        }
    }
}

void update_particles(float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            particles[i].position.x += particles[i].velocity.x * dt;
            particles[i].position.y += particles[i].velocity.y * dt;
            particles[i].velocity.x *= 0.95f;
            particles[i].velocity.y *= 0.95f;
            particles[i].lifetime -= dt;
            particles[i].alpha -= dt * 2.0f;
            if (particles[i].lifetime <= 0.0f || particles[i].alpha <= 0.0f) {
                particles[i].active = false;
            }
        }
    }
}

void draw_particles(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            Color c = particles[i].color;
            c.a = (unsigned char)(particles[i].alpha * 255);
            DrawCircleV(particles[i].position, 4, c);
        }
    }
}



bool find_hint(Vector2 out_tiles[2]) {
    // Check for possible horizontal swaps
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE - 1; x++) {
            // Swap horizontally
            char temp = board[y][x];
            board[y][x] = board[y][x+1];
            board[y][x+1] = temp;
            // Check for match
            for (int i = 0; i < BOARD_SIZE; i++) {
                for (int j = 0; j < BOARD_SIZE - 2; j++) {
                    char t = board[i][j];
                    if (t == board[i][j+1] && t == board[i][j+2]) {
                        // Found a match, so the swap (x,y) <-> (x+1,y) is a valid move
                        out_tiles[0] = (Vector2){x, y};
                        out_tiles[1] = (Vector2){x+1, y};
                        // Undo swap
                        temp = board[y][x];
                        board[y][x] = board[y][x+1];
                        board[y][x+1] = temp;
                        return true;
                    }
                }
            }
            // Undo swap
            temp = board[y][x];
            board[y][x] = board[y][x+1];
            board[y][x+1] = temp;
        }
    }
    // Check for possible vertical swaps
    for (int x = 0; x < BOARD_SIZE; x++) {
        for (int y = 0; y < BOARD_SIZE - 1; y++) {
            // Swap vertically
            char temp = board[y][x];
            board[y][x] = board[y+1][x];
            board[y+1][x] = temp;
            // Check for match
            for (int i = 0; i < BOARD_SIZE - 2; i++) {
                char t = board[i][x];
                if (t == board[i+1][x] && t == board[i+2][x]) {
                    // Found a match, so the swap (x,y) <-> (x,y+1) is a valid move
                    out_tiles[0] = (Vector2){x, y};
                    out_tiles[1] = (Vector2){x, y+1};
                    // Undo swap
                    temp = board[y][x];
                    board[y][x] = board[y+1][x];
                    board[y+1][x] = temp;
                    return true;
                }
            }
            // Undo swap
            temp = board[y][x];
            board[y][x] = board[y+1][x];
            board[y+1][x] = temp;
        }
    }
    return false;
}
// 
// Simple vectorscope to play .wav and .mp3 files
// 
// 2023, Jonathan Tainer
//

#include <raylib.h>
#include <stdio.h>
#include <semaphore.h>

// Various drawing parameters
#define WINDOW_SIZE	1024
#define WINDOW_COLOR	BLACK
#define LINE_COLOR	WHITE
#define LINE_WIDTH	2.f

// Number of samples drawn to the screen each frame.
// This number can be adjusted for a particular use case.
// e.g. low-end hardware might have to draw fewer samples,
// otoh very low freq. requires many samples to draw a whole cycle.
#define VERTEX_BUFFER_SIZE 2048

// Circular buffer for storing most recently processed samples.
// This allows us to draw the same number of samples each frame,
// regardless of how many times the callback is called.
//
// Because drawing and audio processing occur in separate threads,
// a mutex is used to keep them from walking over each other and
// causing undefined behavior.
Vector2 vertexBuffer[VERTEX_BUFFER_SIZE];
unsigned int cursor = 0;
sem_t mutex;

// Convert audio stream (array of float) into vertex information.
// This gets called by miniaudio approximately every 256-512 frames.
void callback(void* buffer, unsigned int frames) {
	// printf("%d\n", frames);

	float* frameptr = (float*) buffer;

	sem_wait(&mutex);
	for (int i = 0; i < frames; i++) {
		Vector2 pos = { WINDOW_SIZE / 2, WINDOW_SIZE / 2 };
		pos.x += WINDOW_SIZE / 2 * frameptr[(2 * i) + 0];
		pos.y += WINDOW_SIZE / 2 * frameptr[(2 * i) + 1];
		vertexBuffer[cursor] = pos;
		cursor = (cursor + 1) % VERTEX_BUFFER_SIZE;
	}
	sem_post(&mutex);
}

void DrawVertexBuffer() {
	sem_wait(&mutex);
	Vector2 begin = vertexBuffer[cursor];
	for (int i = 1; i < VERTEX_BUFFER_SIZE; i++) {
		Vector2 end = vertexBuffer[(cursor + i) % VERTEX_BUFFER_SIZE];
		DrawLineEx(begin, end, LINE_WIDTH, LINE_COLOR);
		begin = end;
	}
	sem_post(&mutex);
}

int main(int argc, char** argv) {

	if (argc < 2)
		return 0;

	sem_init(&mutex, 0, 1);

	SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN | FLAG_VSYNC_HINT);
	InitWindow(WINDOW_SIZE, WINDOW_SIZE, "");
	SetTargetFPS(120);
	InitAudioDevice();

	Music music = LoadMusicStream(argv[1]);
	music.looping = false;
	AttachAudioStreamProcessor(music.stream, callback);
	
	PlayMusicStream(music);

	while (!WindowShouldClose() && IsMusicStreamPlaying(music)) {

		UpdateMusicStream(music);

		BeginDrawing();
		ClearBackground(WINDOW_COLOR);
		DrawVertexBuffer();
		EndDrawing();
	}

	UnloadMusicStream(music);
	CloseAudioDevice();
	CloseWindow();
	sem_destroy(&mutex);
	return 0;
}

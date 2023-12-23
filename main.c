// 
// Simple vectorscope to play .wav and .mp3 files
// 
// Copyright (c) 2022-2023, Jonathan Tainer. Subject to the BSD 2-Clause License.
//

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <stdio.h>
#include <semaphore.h>

// Various drawing parameters
#define WINDOW_WIDTH	1024
#define WINDOW_HEIGHT	1024
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
Vector2 vertexBuffer[VERTEX_BUFFER_SIZE] = { 0 };
unsigned int cursor = 0;
sem_t mutex;

// Convert audio stream (array of float) into vertex information.
// This gets called by miniaudio approximately every 256-512 frames.
void callback(void* buffer, unsigned int frames) {
	Vector2* frameptr = (Vector2*) buffer;
	sem_wait(&mutex);
	for (int i = 0; i < frames; i++) {
		vertexBuffer[cursor] = frameptr[i];
		cursor = (cursor + 1) % VERTEX_BUFFER_SIZE;
	}
	sem_post(&mutex);
}

void DrawVertexBuffer(Vector2 drawOffset, float drawScale, float lineWidth, Color color) {
	sem_wait(&mutex);
	Vector2 begin = vertexBuffer[cursor];
	begin = Vector2Scale(begin, drawScale);
	begin = Vector2Add(begin, drawOffset);

	rlSetLineWidth(lineWidth);
	rlBegin(RL_LINES);
	rlColor4ub(color.r, color.g, color.b, color.a);
	for (int i = 1; i < VERTEX_BUFFER_SIZE; i++) {
		Vector2 end = vertexBuffer[(cursor + i) % VERTEX_BUFFER_SIZE];
		end = Vector2Scale(end, drawScale);
		end = Vector2Add(end, drawOffset);
		rlVertex2f(begin.x, begin.y);
		rlVertex2f(end.x, end.y);
		begin = end;
	}
	rlEnd();

	sem_post(&mutex);
}

int main(int argc, char** argv) {

	if (argc < 2)
		return 0;

	InitAudioDevice();
	Music music = LoadMusicStream(argv[1]);
	if (music.stream.buffer == NULL) {
		CloseAudioDevice();
		return 0;
	}
	music.looping = false;
	AttachAudioStreamProcessor(music.stream, callback);
	sem_init(&mutex, 0, 1);

	int windowWidth = WINDOW_WIDTH, windowHeight = WINDOW_HEIGHT;
	SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN | FLAG_VSYNC_HINT);
	InitWindow(windowWidth, windowHeight, "");
	SetTargetFPS(120);
	int monitor = GetCurrentMonitor();
	const int monitorWidth = GetMonitorWidth(monitor);
	const int monitorHeight = GetMonitorHeight(monitor);

	PlayMusicStream(music);

	while (!WindowShouldClose() && IsMusicStreamPlaying(music)) {

		// Toggle fullscreen
		if (IsKeyPressed(KEY_F)) {
			if (IsWindowFullscreen()) {
				windowWidth = WINDOW_WIDTH;
				windowHeight = WINDOW_HEIGHT;
				ToggleFullscreen();
				SetWindowSize(windowWidth, windowHeight);
				ShowCursor();
			}
			else {
				windowWidth = monitorWidth;
				windowHeight = monitorHeight;
				ToggleFullscreen();
				SetWindowSize(windowWidth, windowHeight);
				HideCursor();
			}
		}

		// Allow seeking with arrow keys
		const float skipSize = 5.f;
		float duration = GetMusicTimeLength(music);
		float elapsed = GetMusicTimePlayed(music);
		if (IsKeyPressed(KEY_RIGHT)) {
			float seek = fmin(elapsed + skipSize, duration);
			SeekMusicStream(music, seek);
		}
		if (IsKeyPressed(KEY_LEFT)) {
			float seek = fmax(elapsed - skipSize, 0.f);
			SeekMusicStream(music, seek);
		}
		UpdateMusicStream(music);

		Vector2 drawOffset = { windowWidth/2, windowHeight/2 };
		float drawScale = windowHeight/2;

		BeginDrawing();
		ClearBackground(WINDOW_COLOR);
		DrawVertexBuffer(drawOffset, drawScale, LINE_WIDTH, LINE_COLOR);
		EndDrawing();
	}

	UnloadMusicStream(music);
	CloseWindow();	
	sem_destroy(&mutex);

	return 0;
}

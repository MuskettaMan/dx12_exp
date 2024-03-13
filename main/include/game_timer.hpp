#pragma once
#include <cstdint>

class GameTimer 
{
public:
	GameTimer();

	float GameTime();
	float DeltaTime() const { return static_cast<float>(_deltaTime); }

	void Reset();
	void Start();
	void Stop();
	void Tick();

private:
	double _secondsPerCount;
	double _deltaTime;

	int64_t _baseTime;
	int64_t _pausedTime;
	int64_t _stopTime;
	int64_t _prevTime;
	int64_t _currentTime;

	bool _stopped;
};
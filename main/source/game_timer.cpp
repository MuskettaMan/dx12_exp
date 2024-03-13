#include "game_timer.hpp"
#include <windows.h>

GameTimer::GameTimer() :
	_secondsPerCount(0.0),
	_deltaTime(-1.0),
	_baseTime(0),
	_pausedTime(0),
	_prevTime(0),
	_currentTime(0),
	_stopped(false)
{
	int64_t countsPerSec;

	QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&countsPerSec));

	_secondsPerCount = 1.0 / countsPerSec;
}

float GameTimer::GameTime()
{
	if (_stopped)
	{
		return static_cast<float>(((_stopTime - _pausedTime) - _baseTime) * _secondsPerCount);
	}
	else
	{
		return static_cast<float>(((_currentTime - _pausedTime) - _baseTime) * _secondsPerCount);
	}
}

void GameTimer::Reset()
{
	int64_t currentTime;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentTime));

	_baseTime = currentTime;
	_prevTime = currentTime;
	_stopTime = 0;
	_stopped = false;
}

void GameTimer::Start()
{
	if (_stopped)
	{
		int64_t startTime;
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&startTime));

		_pausedTime += (startTime - _stopTime);
		_prevTime = startTime;
		_stopTime = 0;
		_stopped = false;
	}
}

void GameTimer::Stop()
{
	if (!_stopped)
	{
		int64_t currentTime;
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentTime));

		_stopTime = currentTime;
		_stopped = true;
	}
}

void GameTimer::Tick()
{
	if (_stopped) 
	{
		_deltaTime = 0.0;
		return;
	}

	int64_t currentTime;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentTime));
	_currentTime = currentTime;

	_deltaTime = (_currentTime - _prevTime) * _secondsPerCount;

	_prevTime = _currentTime;

	if (_deltaTime < 0.0)
	{
		_deltaTime = 0.0;
	}
}

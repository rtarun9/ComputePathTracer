#include "Pch.hpp"

#include "Application.hpp"
#include "Engine.hpp"

int WINAPI wWinMain(HINSTANCE instance, [[maybe_unused]] HINSTANCE prevInstance, [[maybe_unused]] LPWSTR commandLine, [[maybe_unused]] INT commandShow)
{
	cpt::Config config
	{
		.title = L"Compute Path Tracer",
		.width = 1280,
		.height = 640,
	};

	cpt::Engine engine{ config };

	return cpt::Application::Run(&engine, instance);
}
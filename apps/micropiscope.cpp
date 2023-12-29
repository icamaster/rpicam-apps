/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * rpicam_jpeg.cpp - minimal libcamera jpeg capture app.
 */

#include <chrono>
#include <pigpio.h>

#include "core/rpicam_app.hpp"
#include "core/still_options.hpp"

#include "image/image.hpp"

#include <unistd.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <sys/mount.h>
#include <errno.h>

using namespace std::placeholders;
using libcamera::Stream;

enum KeyType : unsigned int {
	KEY_UP      = 15,
	KEY_DOWN    = 4,
	KEY_LEFT    = 17,
	KEY_RIGHT   = 2,
	KEY_MENU    = 14,
	KEY_POWER   = 18
};

constexpr auto KEY_POWER_DOWN_MS = std::chrono::milliseconds(2000);
constexpr auto KEY_MENU_DOWN_MS = std::chrono::milliseconds(100);
constexpr auto UMOUNT_TIMEOUT_S = std::chrono::seconds(5);

static std::string get_mount_location(){
	std::ifstream mounts;
	
	mounts.open("/proc/mounts");

	if (!mounts.is_open()) {
		// Return empty string
		return std::string();
	}

	std::string line;
	std::string mountLocation;
	while (std::getline(mounts, line))
	{
		if (line.find("/media/") != std::string::npos) {
			size_t start = line.find(' ');
			size_t end = line.find(' ', start + 1);

			mountLocation = line.substr(start + 1, end - start - 1);
		}
	}

	mounts.close();

	return mountLocation;
}

static void unmount_drive()
{
	std::string mountLocation = get_mount_location();

	if (mountLocation.empty()) {
		LOG_ERROR("ERROR Unmounting drive, cannot find mount location");
	}

	sync();

	auto start_time = std::chrono::high_resolution_clock::now();

	while(umount(mountLocation.c_str()) < 0) {
		std::string errorText = "Re-attempting to unmount device. Error: " + std::string(strerror(errno));
		LOG(1, errorText);

		// Time between retries
		std::this_thread::sleep_for(1000ms);

		auto now = std::chrono::high_resolution_clock::now();

		if (now - start_time > UMOUNT_TIMEOUT_S) {
			errorText = "Attempting to force unmount device. Error: " + std::string(strerror(errno));
			LOG(1, errorText);
			
			if (umount2(mountLocation.c_str(), MNT_FORCE) < 0) {
				LOG_ERROR("ERROR Failed to unmount device");
			}
			break;
		}
	}
}

static std::string generate_filename()
{
	std::string mountLocation = get_mount_location();

	if (mountLocation.empty()) {
		// Return empty string
		return std::string();
	}

	mountLocation.append("/micropiscope");

	std::filesystem::path mountLocationPath = std::filesystem::path(mountLocation);

	if (!std::filesystem::is_directory(mountLocationPath)){
		if (!std::filesystem::create_directory(mountLocationPath)) {
			// Return empty string
			return std::string();
		}
	}

	char filename[128];

	std::time_t raw_time;
	std::time(&raw_time);
	char time_string[32];
	std::tm *time_info = std::localtime(&raw_time);
	std::strftime(time_string, sizeof(time_string), "%Y-%m-%d-%H-%M-%S", time_info);

	snprintf(filename, sizeof(filename), "%s.jpg", time_string);

	filename[sizeof(filename) - 1] = 0;

	mountLocation.append("/");
	mountLocation.append(filename);

	std::cout << "Saving image at " << mountLocation << std::endl;

	return std::string(mountLocation);
}

class RPiCamJpegApp : public RPiCamApp
{
public:
	RPiCamJpegApp()
		: RPiCamApp(std::make_unique<StillOptions>())
		, doShutdown(false), takeImage(false)
	{		
		// Setup GPIOs with pull-ups and debounce
		for (const auto key : { KeyType::KEY_UP, KeyType::KEY_DOWN, KeyType::KEY_LEFT, KeyType::KEY_RIGHT, KeyType::KEY_MENU, KeyType::KEY_POWER} ) {
			gpioSetMode(key, PI_INPUT);
			gpioSetPullUpDown(key, PI_PUD_UP);
			gpioGlitchFilter(key, 100 * 1000);
			gpioSetAlertFuncEx(key, _callbackExt, (void *)this);
		}
	}

	StillOptions *GetOptions() const
	{
		return static_cast<StillOptions *>(options_.get());
	}

	void _callback(int gpio, int level, uint32_t tick) {
		if (gpio == KeyType::KEY_POWER) {
			static uint32_t keyPowerTimeStart = tick;

			if (level == PI_LOW) {
				keyPowerTimeStart = tick;
			} else if (level == PI_HIGH && ((uint32_t)(tick - keyPowerTimeStart) > 2000 * 1000)) {
				LOG(1, "KEY POWER");
				this->doShutdown = true;
			}

			return;
		}
		
		if (gpio == KeyType::KEY_MENU && level == PI_LOW) {
			LOG(1, "KEY MENU");
			this->takeImage = true;
			return;
		}

		if (gpio == KeyType::KEY_DOWN && level == PI_LOW) {
			LOG(1, "KEY DOWN");
			unmount_drive();
			return;
		}
	}

	/* Need a static callback to link with C. */
	static void _callbackExt(int gpio, int level, uint32_t tick, void *user) {
		RPiCamJpegApp *mySelf = (RPiCamJpegApp *) user;

   		mySelf->_callback(gpio, level, tick); /* Call the instance callback. */
	}

	bool DoShutdown() 
	{
		// This is a clear on read
		if (this->doShutdown) {
			this->doShutdown = false;
			return true;
		}

		return false;
	}

	bool TakeImage() 
	{
		// This is a clear on read
		if (this->takeImage) {
			this->takeImage = false;
			return true;
		}

		return false;
	}

private:
	bool doShutdown;
	bool takeImage;
};

// The main even loop for the application.

static void event_loop(RPiCamJpegApp &app)
{
	StillOptions const *options = app.GetOptions();
	app.OpenCamera();
	app.ConfigureViewfinder();
	app.StartCamera();

	for (;;)
	{
		RPiCamApp::Msg msg = app.Wait();
		if (msg.type == RPiCamApp::MsgType::Timeout)
		{
			LOG_ERROR("ERROR: Device timeout detected, attempting a restart!!!");
			app.StopCamera();
			app.StartCamera();
			continue;
		}
		if (msg.type == RPiCamApp::MsgType::Quit)
			return;
		else if (msg.type != RPiCamApp::MsgType::RequestComplete)
			throw std::runtime_error("unrecognised message!");

		// In viewfinder mode, simply run until the timeout. When that happens, switch to
		// capture mode.
		if (app.ViewfinderStream())
		{
			if (app.TakeImage())
			{
				app.StopCamera();
				app.Teardown();
				app.ConfigureStill();
				app.StartCamera();
			}
			else if (app.DoShutdown())
			{
				app.StopCamera();
				app.Teardown();
				LOG(1, "Shutting down!");
				sync();
				reboot(LINUX_REBOOT_CMD_POWER_OFF);
				return;
			}
			else
			{
				CompletedRequestPtr &completed_request = std::get<CompletedRequestPtr>(msg.payload);
				app.ShowPreview(completed_request, app.ViewfinderStream());
			}
		}
		// In still capture mode, save a jpeg and quit.
		else if (app.StillStream())
		{
			app.StopCamera();
			LOG(1, "Still capture image received");

			Stream *stream = app.StillStream();
			StreamInfo info = app.GetStreamInfo(stream);
			CompletedRequestPtr &payload = std::get<CompletedRequestPtr>(msg.payload);
			BufferReadSync r(&app, payload->buffers[stream]);
			const std::vector<libcamera::Span<uint8_t>> mem = r.Get();
			std::string filename = generate_filename();
			if (filename.empty()) {
				LOG_ERROR("Cannot save image, no mounted drive!");
			} else {
				jpeg_save(mem, info, payload->metadata, filename, app.CameraModel(), options);
			}
			
			// Return to preview mode
			app.Teardown();
			app.ConfigureViewfinder();
			app.StartCamera();
		}
	}
}

int main(int argc, char *argv[])
{
	try
	{
		if (gpioInitialise() < 0)
			throw std::runtime_error("GPIO init failed");
		RPiCamJpegApp app;
		StillOptions *options = app.GetOptions();
		if (options->Parse(argc, argv))
		{
			if (options->verbose >= 2)
				options->Print();

			event_loop(app);
		}
		gpioTerminate();
	}
	catch (std::exception const &e)
	{
		LOG_ERROR("ERROR: *** " << e.what() << " ***");
		return -1;
	}
	return 0;
}

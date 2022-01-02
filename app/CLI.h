#include <cstdlib>
#if defined(USE_EXPERIMENTAL_FS)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#if defined(__APPLE__)
#include <unistd.h>
#endif
#endif
#include <cstdint>
#include <iomanip>
#include <thread>
#include <chrono>
#include "CRSDK/CameraRemote_SDK.h"
#include "CameraDevice.h"
#include "Text.h"
#include "clipp.h"

enum class mode {
    capture,
    get,
    set,
    sdk,
    help
};

//#define LIVEVIEW_ENB

namespace SDK = SCRSDK;
using namespace std::chrono_literals;
using namespace clipp; using std::string;

void releaseExitSuccess() {
    SDK::Release();
    std::exit(EXIT_SUCCESS);
}

void releaseExitFailure() {
    SDK::Release();
    std::exit(EXIT_FAILURE);
}

void capture(string outpath)
{
    // Change global locale to native locale
    std::locale::global(std::locale(""));

    // Make the stream's locale the same as the current global locale
    cli::tin.imbue(std::locale());
    cli::tout.imbue(std::locale());

    auto init_success = SDK::Init();
    if (!init_success) {
        cli::tout << "Error: Failed to initialize Remote SDK\n";
        releaseExitFailure();
    }
    cli::tout << "Remote SDK successfully initialized.\n";

    SDK::ICrEnumCameraObjectInfo* camera_list = nullptr;

    auto enum_status = SDK::EnumCameraObjects(&camera_list, 0);
    if (CR_FAILED(enum_status) || camera_list == nullptr) {
        cli::tout << "Error: No cameras detected\n";
        releaseExitFailure();
    }
    auto ncams = camera_list->GetCount();

    cli::tout << "Camera enumeration successful. " << ncams << " detected\n";

    typedef std::shared_ptr<cli::CameraDevice> CameraDevicePtr;
    std::int32_t cameraNumUniq = 1;
    std::int32_t selectCamera = 1;
    std::int8_t no = 1;

    // cli::tout << "Connect to selected camera...\n";
    auto* camera_info = camera_list->GetCameraObjectInfo(no - 1);

    // cli::tout << "Create camera SDK camera callback object.\n";
    CameraDevicePtr camera = CameraDevicePtr(new cli::CameraDevice(cameraNumUniq, nullptr, camera_info));
    camera->releaseExitSuccess = releaseExitSuccess;
    camera->set_release_on_completedownload(true);

    // cli::tout << "Release enumerated camera list.\n";
    camera_list->Release();

    if (!camera->connect(SDK::CrSdkControlMode_Remote)) {
        cli::tout << "Error: Unable to connect to camera\n";
        releaseExitFailure();
    }

    std::this_thread::sleep_for(1000ms);

    camera->set_save_path(outpath, "", -1);

    if (SDK::CrSdkControlMode_Remote == camera->get_sdkmode()) {
        cli::tout << "Capturing\n";
        camera->half_ael_full_release();
    }
    else {
        cli::tout << "Error: Unable to start remote control\n";
        releaseExitFailure();
    }

    std::this_thread::sleep_for(5000ms);

    cli::tout << "Error: Unable to download image\n";
    releaseExitFailure();
}

mode CommandLine(int argc, char* argv[])
{
    mode selected = mode::help;
    string outpath = "";

    auto cli = (
        (command("--capture").set(selected, mode::capture).doc("Capture image") & (option("--dir").doc("Output dir") & value("output dir", outpath))) |
        command("--get").set(selected, mode::get).doc("Gets the value of a camera property") |
        command("--set").set(selected, mode::set).doc("Sets the value of camera property") |
        command("--remote-app").set(selected, mode::sdk).doc("Load the sample app from Camera SDK") |
        command("--help").set(selected, mode::help).doc("This printed message")
    );

    if(parse(argc, argv, cli)) {
        switch(selected) {
            case mode::capture:
                capture(outpath);
                break;
            case mode::sdk:
                return mode::sdk;
                break;
            case mode::help:
                cli::tout << make_man_page(cli, argv[0]);
                break;
        }
    } else {
        cli::tout << "Try --help to see usage\n";
    }

    std::exit(EXIT_FAILURE);
}
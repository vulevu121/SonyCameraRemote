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
#include "DevicePropertyDefines.h"

enum class mode {
    capture,
    get,
    set,
    sdk,
    help
};

//#define LIVEVIEW_ENB

namespace SDK = SCRSDK;
using namespace SCRSDK;
using namespace cli;
using namespace std::chrono_literals;
using namespace clipp;
using std::string;

typedef std::shared_ptr<CameraDevice> CameraDevicePtr;

void releaseExitSuccess() {
    SDK::Release();
    std::exit(EXIT_SUCCESS);
}

void releaseExitFailure() {
    SDK::Release();
    std::exit(EXIT_FAILURE);
}

CameraDevicePtr getCamera(bool verbose) 
{
    // Change global locale to native locale
    std::locale::global(std::locale(""));

    // Make the stream's locale the same as the current global locale
    tin.imbue(std::locale());
    tout.imbue(std::locale());

    auto init_success = SDK::Init();
    if (!init_success) {
        tout << "Error: Failed to initialize Remote SDK\n";
        releaseExitFailure();
    }
    // tout << "Remote SDK successfully initialized.\n";

    SDK::ICrEnumCameraObjectInfo* camera_list = nullptr;

    auto enum_status = SDK::EnumCameraObjects(&camera_list, 0);
    if (CR_FAILED(enum_status) || camera_list == nullptr) {
        tout << "Error: No cameras detected\n";
        releaseExitFailure();
    }
    auto ncams = camera_list->GetCount();

    // tout << "Camera enumeration successful. " << ncams << " detected\n";

    std::int32_t cameraNumUniq = 1;
    std::int32_t selectCamera = 1;
    std::int8_t no = 1;

    // tout << "Connect to selected camera...\n";
    auto* camera_info = camera_list->GetCameraObjectInfo(no - 1);

    // tout << "Create camera SDK camera callback object.\n";
    CameraDevicePtr camera = CameraDevicePtr(new CameraDevice(cameraNumUniq, nullptr, camera_info));
    camera->releaseExitSuccess = releaseExitSuccess;
    
    camera->set_verbose(verbose);

    // tout << "Release enumerated camera list.\n";
    camera_list->Release();

    if (!camera->connect(SDK::CrSdkControlMode_Remote)) {
        tout << "Error: Unable to connect to camera\n";
        return nullptr;
    }
    std::this_thread::sleep_for(1000ms);
    if (verbose) tout << "Camera connected\n";
    return camera;
}

void capture(string dir, bool verbose)
{
    CameraDevicePtr camera = getCamera(verbose);
    if (camera == nullptr) releaseExitFailure();

    camera->set_release_after_download(true);

    if (dir.length() > 0) {
        camera->set_save_path(dir, "", -1);
    }
    camera->half_full_release();
    std::this_thread::sleep_for(5000ms);
    tout << "Error: Unable to download image\n";
    releaseExitFailure();
}

void getProperty(const string prop, bool verbose) {
    if (map_device_property.count(prop) == 0) {
        tout << "Error: Property not found\n";
        releaseExitFailure();
    }

    CameraDevicePtr camera = getCamera(verbose);

    if (camera == nullptr) releaseExitFailure();
    
    CrInt32u code = map_device_property.at(prop);
    CrInt64 value = 0;
    if (!camera->get_property_value(code, value)) releaseExitFailure();
    tout << prop << ": " << value << "\n";
    releaseExitSuccess();
}

void setProperty(const string &prop, const string &value, bool verbose) 
{
    if (map_device_property.count(prop) == 0) {
        tout << "Error: Property not found\n";
        releaseExitFailure();
    }

    CameraDevicePtr camera = getCamera(verbose);
    if (camera == nullptr) releaseExitFailure();

    CrInt32u code = map_device_property.at(prop);

    if (!camera->set_property_value(code, std::stoi(value))) {
        tout << "Error: Unable to set property\n";
        releaseExitFailure();
    }
    releaseExitSuccess();
}

mode ArgParser(int argc, char* argv[])
{
    mode selected = mode::help;
    string dir = "";
    bool verbose = false;
    string prop;
    string val;

    auto captureCommand = (
        command("capture").set(selected, mode::capture).doc("Capture an image"),
        option("--dir").doc("Output dir") & value("output dir", dir)
    );

    auto getCommand = (
        command("get").set(selected, mode::get).doc("Gets the value of a camera property"),
        required("--prop") & value("prop", prop)
    );

    auto setCommand = (
        command("set").set(selected, mode::set).doc("Sets the value of camera property"),
        required("--prop").doc("Property name") & value("prop", prop),
        required("--value").doc("Property value") & value("value", val)
    );

    auto cli = (
        captureCommand |
        getCommand |
        setCommand |
        command("sdk").set(selected, mode::sdk).doc("Load the sample app from Sony Camera SDK") |
        command("--help").set(selected, mode::help).doc("This printed message"),
        option("--verbose").set(verbose, true).doc("Prints debugging messages")
    );

    if(parse(argc, argv, cli)) {
        switch(selected) {
            case mode::capture:
                capture(dir, verbose);
                break;
            case mode::get:
                getProperty(prop, verbose);
                break;
            case mode::set:
                setProperty(prop, val, verbose);
                break;
            case mode::sdk:
                return mode::sdk;
                break;
            case mode::help:
                tout << make_man_page(cli, argv[0]);
                break;
        }
    } else {
        tout << "Try --help to see usage\n";
    }
    std::exit(EXIT_FAILURE);
}
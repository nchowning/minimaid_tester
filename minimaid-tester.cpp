#include <iostream>
#include <unistd.h>

extern "C" {
#include <usb.h>
}

// USB Globals
static const int USB_DIR_OUT = 0x00;
static const int USB_DIR_IN = 0x80;
static const int TYPE_STANDARD = 0x00;
static const int TYPE_CLASS = (0x01 << 5);
static const int TYPE_VENDOR = (0x02 << 5);
static const int TYPE_RESERVED = (0x03 << 5);
static const int RECIP_DEVICE = 0x00;
static const int RECIP_INTERFACE = 0x01;
static const int RECIP_ENDPOINT = 0x02;
static const int RECIP_OTHER = 0x03;

// HID Globals
static const int HID_REPORT_TYPE_INPUT = 0x01;
static const int HID_REPORT_TYPE_OUTPUT = 0x02;
static const int HID_REPORT_TYPE_FEATURE = 0x03;
static const int HID_GET_REPORT = 0x01;
static const int HID_SET_REPORT = 0x09;

// App Specific Globals
static const int TIMEOUT = 10000;
static const int MINIMAID_LIGHTS_INTERFACE = 1;

// Function Declarations
struct usb_device *FindDevice(int, int);
usb_dev_handle *open_device(unsigned short vendorID, unsigned short productID);
void close_device(usb_dev_handle *dev);
bool write_data(usb_dev_handle *pHandle, unsigned long long);

int main() {
    int on_delay = 2;
    int off_delay = 1;

    // Open the MiniMaid
    usb_dev_handle *pHandle = open_device(0xbeef, 0x5730);
    if (pHandle == NULL) {
        return 1;
    }
    std::cout << "SUCCESS: It must have worked" << std::endl;

    // Data holder
    unsigned long long all_on = 0x100FFFF00;
    unsigned long long all_off = 0x0;
    unsigned long long data = 0x0;

    for (int f = 1; f < 5; f++) {
        for (int i = 0; i < 8; i++) {
            // Reset our data
            data = 0x0;
            data |= (1 << i);
            data = data << (f * 8);

            if (!write_data(pHandle, data))
                std::cout << "ERROR: Unable to write data...";
            usleep(1000000);

            if (i == 7)
                i = 0;
        }
        if (f == 4)
            f = 1;
    }

    write_data(pHandle, all_off);
}

struct usb_device *FindDevice(int vendorID, int productID) {
    for (usb_bus *bus = usb_get_busses(); bus; bus = bus->next)
        for (struct usb_device *dev = bus->devices; dev; dev = dev->next)
            if (vendorID == dev->descriptor.idVendor && productID == dev->descriptor.idProduct)
                return dev;

    return NULL;
}

usb_dev_handle *open_device(unsigned short vendorID, unsigned short productID) {
    // Initialize usb
    usb_init();
    usb_find_busses();
    usb_find_devices();

    // See if we can find our MiniMaid
    struct usb_device *dev = FindDevice(vendorID, productID);
    if (dev == NULL) {
        std::cout << "ERROR: MiniMaid not found... " << std::endl;
        return NULL;
    }

    // Attempt to open the MiniMaid
    usb_dev_handle *pHandle = usb_open(dev);
    if (pHandle == NULL) {
        std::cout << "ERROR: Unable to open MiniMaid..." << std::endl;
        return NULL;
    }

    // MiniMaid is probably claimed by kernel. Try to reclaim it
    for (unsigned iface = 0; iface < dev->config->bNumInterfaces; iface++)
         usb_detach_kernel_driver_np(pHandle, iface);

    // Attempt to set the MiniMaid configuration
    if (!usb_set_configuration(pHandle, dev->config->bConfigurationValue) == 0) {
        std::cout << "ERROR: Unable to set configuration for MiniMaid..." << std::endl;
        close_device(pHandle);
        return NULL;
    }

    // Attempt to claim all interfaces for device
    for (unsigned i = 0; i < dev->config->bNumInterfaces; i++)
    {
        if (!usb_claim_interface(pHandle, i) == 0)
        {
            std::cout << "ERROR: Unable to claim interface..." << std::endl;
            close_device(pHandle);
            return NULL;
        }
    }

    return pHandle;
}

void close_device(usb_dev_handle *pHandle) {
    usb_set_altinterface(pHandle, 0);
    usb_reset(pHandle);
    usb_close(pHandle);
    pHandle = NULL;
}

bool write_data(usb_dev_handle *pHandle, unsigned long long data) {
    int type = (USB_DIR_OUT | TYPE_CLASS | RECIP_INTERFACE);
    int request = HID_SET_REPORT;
    int value = (HID_REPORT_TYPE_OUTPUT << 8) | 0x00;

    int result = usb_control_msg(
        pHandle,
        type,
        request,
        value,
        MINIMAID_LIGHTS_INTERFACE,
        (char *)&data,
        sizeof(data),
        TIMEOUT);

    return result == sizeof(data);
}

Command to build usb_test

gcc -Wall -O3 `pkg-config --cflags libusb-1.0 libudev` `pkg-config --libs libusb-1.0 libudev` usb.c -o usb-test


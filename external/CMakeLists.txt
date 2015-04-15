ADD_SUBDIRECTORY(libusb)

# NOTE: the Quantis component must be build after libusb as it depends on it.
ADD_SUBDIRECTORY(quantis)

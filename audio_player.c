#include <stdio.h>
#include <stdlib.h>
#include <mpg123.h>
#include <ao/ao.h>

int main(int argc, char *argv[]) {
    // Initialize variables
    mpg123_handle *mh; // Handle for mpg123
    unsigned char *buffer; // Buffer to hold audio data
    size_t buffer_size; // Size of the buffer
    size_t done; // Variable to hold the number of bytes read
    int err; // Variable to hold errors

    ao_device *dev; // libao device
    ao_sample_format format; // Audio format
    int driver; // Driver for audio playback

    // Check for correct command-line arguments
    if(argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 0;
    }

    // Initialize libao
    ao_initialize();

    // Initialize mpg123
    mpg123_init();
    // Create a new mpg123 handle
    mh = mpg123_new(NULL, &err);
    // Get the buffer size for one decoded frame
    buffer_size = mpg123_outblock(mh);
    // Allocate memory for the buffer
    buffer = (unsigned char*) malloc(buffer_size * sizeof(unsigned char));

    // Open the audio file
    mpg123_open(mh, argv[1]);

    // Retrieve audio format information
    long rate;
    int channels, encoding;
    mpg123_getformat(mh, &rate, &channels, &encoding);

    // Set up the audio output format
    format.rate = rate;
    format.channels = channels;
    format.bits = mpg123_encsize(encoding) * 8;
    format.byte_format = AO_FMT_NATIVE;
    format.matrix = 0;

    // Open the default libao driver
    dev = ao_open_live(ao_default_driver_id(), &format, NULL);
    if (dev == NULL) {
        fprintf(stderr, "Error opening device.\n");
        return 1;
    }

    // Read and play audio frames until end of file
    while (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK)
        ao_play(dev, buffer, done);

    // Clean up resources
    free(buffer);
    ao_close(dev);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
    ao_shutdown();

    return 0;
}

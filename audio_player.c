#include <stdio.h>
#include <stdlib.h>
#include <mpg123.h>
#include <ao/ao.h>
#include <gtk/gtk.h>

mpg123_handle *mh;
unsigned char *buffer;
size_t buffer_size;
size_t done;
ao_device *dev;
ao_sample_format format;
int driver;

void play_audio(char *filename) {
    printf("Playing audio file: %s\n", filename); // Print filename to console

    mpg123_init();
    mh = mpg123_new(NULL, NULL);
    buffer_size = mpg123_outblock(mh);
    buffer = (unsigned char*) malloc(buffer_size * sizeof(unsigned char));

    mpg123_open(mh, filename);

    long rate;
    int channels, encoding;
    mpg123_getformat(mh, &rate, &channels, &encoding);

    format.rate = rate;
    format.channels = channels;
    format.bits = mpg123_encsize(encoding) * 8;
    format.byte_format = AO_FMT_NATIVE;
    format.matrix = 0;

    /* Get the default driver ID */
    driver = ao_default_driver_id();
    if (driver < 0) {
        fprintf(stderr, "Error getting default driver ID.\n");
        exit(1);
    }

    /* Open the audio device */
    dev = ao_open_live(driver, &format, NULL);
    if (dev == NULL) {
        fprintf(stderr, "Error opening audio device.\n");
        fprintf(stderr, "Driver ID: %d\n", driver);
        fprintf(stderr, "Sample Rate: %d\n", format.rate);
        fprintf(stderr, "Channels: %d\n", format.channels);
        fprintf(stderr, "Bits per Sample: %d\n", format.bits);
        fprintf(stderr, "Byte Format: %d\n", format.byte_format);
        exit(1);
    }

    printf("Audio device opened successfully.\n");

    while (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK)
        ao_play(dev, buffer, done);
}

void on_file_open(GtkWidget *widget, gpointer window) {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new("Open File",
                                         GTK_WINDOW(window),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         "_Cancel",
                                         GTK_RESPONSE_CANCEL,
                                         "_Open",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);
    gint res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        filename = gtk_file_chooser_get_filename(chooser);
        play_audio(filename);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *button;

    ao_initialize();

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    gtk_window_set_title(GTK_WINDOW(window), "Audio Player");
    gtk_window_set_default_size(GTK_WINDOW(window), 200, 100);

    button = gtk_button_new_with_label("Open File");
    g_signal_connect(button, "clicked", G_CALLBACK(on_file_open), (gpointer) window);
    gtk_container_add(GTK_CONTAINER(window), button);

    gtk_widget_show_all(window);

    gtk_main();

    free(buffer);
    ao_close(dev);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
    ao_shutdown();

    return 0;
}

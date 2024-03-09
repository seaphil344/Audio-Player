#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mpg123.h>
#include <ao/ao.h>
#include <dirent.h>
#include <string.h>

// Global variables to manage audio playback and UI elements
mpg123_handle *mh;
unsigned char *buffer;
size_t buffer_size;
size_t done;
ao_device *dev;
ao_sample_format format;
int driver;
bool is_playing = false;
GtkWidget *label_track_info;
GtkWidget *progress_bar;
GtkWidget *label_time;
GtkWidget *button_next;
GtkWidget *button_previous;
GtkWidget *button_select_folder;
GtkListStore *music_list_store;
GtkWidget *window;
int current_track_index = -1;
int total_length_seconds;
char *current_music_directory = NULL;

// Function to update the track information displayed on the GUI
void update_track_info(const char *info) {
    gtk_label_set_text(GTK_LABEL(label_track_info), info);
}

// Function called repeatedly to play chunks of audio data; it also updates the progress bar and time labels
gboolean play_chunk(gpointer data) {
    if (!is_playing || mpg123_read(mh, buffer, buffer_size, &done) != MPG123_OK) {
        is_playing = false;
        update_track_info("Playback finished or stopped.");
        ao_close(dev);
        mpg123_close(mh);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
        gtk_label_set_text(GTK_LABEL(label_time), "00:00 / 00:00");
        return FALSE;
    }

    ao_play(dev, buffer, done);

    // Update progress bar and time labels based on the current playback position
    if (total_length_seconds > 0) {
        off_t current_position = mpg123_tell(mh);
        double current_seconds = current_position / (double)format.rate;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), current_seconds / total_length_seconds);

        int current_minutes = (int)current_seconds / 60;
        int current_seconds_int = (int)current_seconds % 60;
        int total_minutes = (int)total_length_seconds / 60;
        int total_seconds = (int)total_length_seconds % 60;
        char time_str[100];
        snprintf(time_str, sizeof(time_str), "%02d:%02d / %02d:%02d", current_minutes, current_seconds_int, total_minutes, total_seconds);
        gtk_label_set_text(GTK_LABEL(label_time), time_str);
    }

    return TRUE;
}

// Function to handle audio playback; it initializes the necessary libraries and opens the audio file
void play_audio(const char *filename) {
    if (is_playing) {
        ao_close(dev);
        mpg123_close(mh);
        is_playing = false;
    }

    printf("Playing audio file: %s\n", filename);
    update_track_info(filename);

    mpg123_init();
    mh = mpg123_new(NULL, NULL);
    mpg123_open(mh, filename);
    buffer_size = mpg123_outblock(mh);
    buffer = (unsigned char*) malloc(buffer_size * sizeof(unsigned char));

    long rate;
    int channels, encoding;
    mpg123_getformat(mh, &rate, &channels, &encoding);

    off_t total_length = mpg123_length(mh);
    if (total_length != MPG123_ERR) {
        total_length_seconds = total_length / (double)rate;
        char length_str[100];
        snprintf(length_str, sizeof(length_str), "00:00 / %02d:%02d", total_length_seconds / 60, total_length_seconds % 60);
        gtk_label_set_text(GTK_LABEL(label_time), length_str);
    } else {
        gtk_label_set_text(GTK_LABEL(label_time), "00:00 / 00:00");
    }

    format.rate = rate;
    format.channels = channels;
    format.bits = mpg123_encsize(encoding) * 8;
    format.byte_format = AO_FMT_NATIVE;
    format.matrix = 0;

    driver = ao_default_driver_id();
    dev = ao_open_live(driver, &format, NULL);

    if (!dev) {
        fprintf(stderr, "Error opening audio device.\n");
        exit(1);
    }

    is_playing = true;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
    g_idle_add(play_chunk, NULL);
}

// Function to stop audio playback and clean up resources
void stop_audio(GtkWidget *widget, gpointer data) {
    if (is_playing) {
        is_playing = false;
        ao_close(dev);
        dev = NULL;
        mpg123_close(mh);
        mpg123_delete(mh);
        mh = NULL;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
        gtk_label_set_text(GTK_LABEL(label_time), "00:00 / 00:00");
    }
}

// Function to play the next track in the playlist
void play_next(GtkWidget *widget, gpointer data) {
    // Ensure the current track index is within bounds
    if (current_track_index < 0 || current_track_index >= gtk_tree_model_iter_n_children(GTK_TREE_MODEL(music_list_store), NULL) - 1) return;

    GtkTreeIter iter;
    GtkTreePath *path = gtk_tree_path_new_from_indices(current_track_index + 1, -1);

    // Retrieve the filename of the next track and play it
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(music_list_store), &iter, path)) {
        gchar *filename;
        gtk_tree_model_get(GTK_TREE_MODEL(music_list_store), &iter, 0, &filename, -1);
        if (current_music_directory) {
            char *fullpath = g_build_filename(current_music_directory, filename, NULL);
            current_track_index++;
            play_audio(fullpath);
            g_free(fullpath);
        } else {
            play_audio(filename);
        }
        g_free(filename);
    }
    gtk_tree_path_free(path);
}

// Function to play the previous track in the playlist
void play_previous(GtkWidget *widget, gpointer data) {
    // Ensure the current track index is within bounds
    if (current_track_index <= 0) return;

    GtkTreeIter iter;
    GtkTreePath *path = gtk_tree_path_new_from_indices(current_track_index - 1, -1);

    // Retrieve the filename of the previous track and play it
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(music_list_store), &iter, path)) {
        gchar *filename;
        gtk_tree_model_get(GTK_TREE_MODEL(music_list_store), &iter, 0, &filename, -1);
        if (current_music_directory) {
            char *fullpath = g_build_filename(current_music_directory, filename, NULL);
            current_track_index--;
            play_audio(fullpath);
            g_free(fullpath);
        } else {
            play_audio(filename);
        }
        g_free(filename);
    }
    gtk_tree_path_free(path);
}

// Callback function for when a music file is selected from the list
void on_music_file_selected(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;

    // Retrieve the filename of the selected track and play it
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gchar *filename;
        gtk_tree_model_get(model, &iter, 0, &filename, -1);
        if (current_music_directory) {
            char *fullpath = g_build_filename(current_music_directory, filename, NULL);
            current_track_index = gtk_tree_path_get_indices(path)[0];
            play_audio(fullpath);
            g_free(fullpath);
        } else {
            play_audio(filename);
        }
        g_free(filename);
    }
}

// Function to populate the music list with files from the specified directory
void populate_music_list(GtkListStore *store, const char *directory) {
    gtk_list_store_clear(store);  // Clear existing entries before populating new ones

    DIR *dir;
    struct dirent *ent;
    // Iterate over each entry in the directory and add MP3 files to the list
    if ((dir = opendir(directory)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_REG && strstr(ent->d_name, ".mp3") != NULL) {
                GtkTreeIter iter;
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter, 0, ent->d_name, -1);
            }
        }
        closedir(dir);
    } else {
        perror("Could not open directory");
    }
}

// Function to create and display a folder selection dialog, then update the music list based on the selected folder
void select_folder(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
    gint res;

    dialog = gtk_file_chooser_dialog_new("Select Folder",
                                         GTK_WINDOW(window),
                                         action,
                                         "_Cancel",
                                         GTK_RESPONSE_CANCEL,
                                         "_Open",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        // Free the previous directory path and store the new one
        if (current_music_directory) {
            g_free(current_music_directory);
        }
        current_music_directory = gtk_file_chooser_get_filename(chooser);
        // Update the music list with files from the selected directory
        populate_music_list(music_list_store, current_music_directory);
    }

    gtk_widget_destroy(dialog);
}

// Main function that initializes the GTK application and sets up the UI elements
int main(int argc, char *argv[]) {
    GtkWidget *paned;
    GtkWidget *left_pane;
    GtkWidget *right_pane;
    GtkWidget *tree_view;
    GtkWidget *button_stop;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    ao_initialize();
    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_window_set_title(GTK_WINDOW(window), "Audio Player");
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_container_add(GTK_CONTAINER(window), paned);

    left_pane = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(left_pane, 400, -1);
    gtk_paned_add1(GTK_PANED(paned), left_pane);

    music_list_store = gtk_list_store_new(1, G_TYPE_STRING);
    populate_music_list(music_list_store, ".");
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(music_list_store));
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Music Files", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    gtk_container_add(GTK_CONTAINER(left_pane), tree_view);
    g_signal_connect(tree_view, "row-activated", G_CALLBACK(on_music_file_selected), NULL);

    right_pane = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_paned_add2(GTK_PANED(paned), right_pane);

    label_track_info = gtk_label_new("No file selected");
    gtk_box_pack_start(GTK_BOX(right_pane), label_track_info, FALSE, FALSE, 0);

    progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(right_pane), progress_bar, FALSE, FALSE, 0);

    label_time = gtk_label_new("00:00 / 00:00");
    gtk_box_pack_start(GTK_BOX(right_pane), label_time, FALSE, FALSE, 0);

    button_previous = gtk_button_new_with_label("Previous");
    gtk_box_pack_start(GTK_BOX(right_pane), button_previous, FALSE, FALSE, 0);
    g_signal_connect(button_previous, "clicked", G_CALLBACK(play_previous), NULL);

    button_stop = gtk_button_new_with_label("Stop");
    gtk_box_pack_start(GTK_BOX(right_pane), button_stop, FALSE, FALSE, 0);
    g_signal_connect(button_stop, "clicked", G_CALLBACK(stop_audio), NULL);

    button_next = gtk_button_new_with_label("Next");
    gtk_box_pack_start(GTK_BOX(right_pane), button_next, FALSE, FALSE, 0);
    g_signal_connect(button_next, "clicked", G_CALLBACK(play_next), NULL);

    button_select_folder = gtk_button_new_with_label("Select Folder");
    gtk_box_pack_start(GTK_BOX(right_pane), button_select_folder, FALSE, FALSE, 0);
    g_signal_connect(button_select_folder, "clicked", G_CALLBACK(select_folder), NULL);

    gtk_widget_show_all(window);
    gtk_main();

    free(buffer);
    if (is_playing) {
        ao_close(dev);
        mpg123_close(mh);
    }
    mpg123_delete(mh);
    mpg123_exit();
    ao_shutdown();

    return 0;
}

/*
 * calc_gui.c — a GTK3 front end for the calculator in calc.c.
 *
 * Build:
 *   sudo apt install -y libgtk-3-dev
 *   cc -std=c99 -O2 -Wall -Wextra -o calc-gui calc_gui.c \
 *      $(pkg-config --cflags --libs gtk+-3.0) -lm
 *   ./calc-gui
 *
 * How the parser is shared: calc.c is included as source with its main()
 * renamed out of the way, which gives this file access to evaluate(), g_ans
 * and g_error without editing calc.c. For a bigger project you would split the
 * engine into engine.c plus engine.h and link two objects instead — see the
 * note at the bottom of this file.
 */

#define main calc_cli_main
#include "calc.c"
#undef main

#include <gtk/gtk.h>

#define DISPLAY_SIZE 256
#define STANDARD_W 330
#define STANDARD_H 520
#define SCIENTIFIC_W 520
#define SCIENTIFIC_H 600

typedef enum {
    A_INS, A_WRAP, A_CLEAR, A_CLEAR_ALL, A_BACK, A_NEGATE, A_EQUALS
} KeyAction;

typedef enum {
    K_NUM, K_FUNC, K_OP, K_EQUALS
} KeyKind;

typedef struct {
    const char *label;
    KeyAction action;
    const char *text;     /* text to insert, or the prefix for A_WRAP */
    const char *suffix;   /* closing text for A_WRAP */
    KeyKind kind;
} KeyDef;

static KeyDef STANDARD_KEYS[] = {
    {"mod", A_INS, "%", "", K_FUNC},
    {"CE", A_CLEAR, "", "", K_FUNC},
    {"C", A_CLEAR_ALL, "", "", K_FUNC},
    {"⌫", A_BACK, "", "", K_FUNC},

    {"1/x", A_WRAP, "1/(", ")", K_FUNC},
    {"x²", A_WRAP, "(", ")^2", K_FUNC},
    {"√", A_WRAP, "sqrt(", ")", K_FUNC},
    {"÷", A_INS, "/", "", K_OP},

    {"7", A_INS, "7", "", K_NUM},
    {"8", A_INS, "8", "", K_NUM},
    {"9", A_INS, "9", "", K_NUM},
    {"×", A_INS, "*", "", K_OP},

    {"4", A_INS, "4", "", K_NUM},
    {"5", A_INS, "5", "", K_NUM},
    {"6", A_INS, "6", "", K_NUM},
    {"−", A_INS, "-", "", K_OP},

    {"1", A_INS, "1", "", K_NUM},
    {"2", A_INS, "2", "", K_NUM},
    {"3", A_INS, "3", "", K_NUM},
    {"+", A_INS, "+", "", K_OP},

    {"±", A_NEGATE, "", "", K_FUNC},
    {"0", A_INS, "0", "", K_NUM},
    {".", A_INS, ".", "", K_NUM},
    {"=", A_EQUALS, "", "", K_EQUALS},
};

static KeyDef SCIENTIFIC_KEYS[] = {
    {"π", A_INS, "pi", "", K_FUNC},
    {"e", A_INS, "e", "", K_FUNC},
    {"C", A_CLEAR_ALL, "", "", K_FUNC},
    {"⌫", A_BACK, "", "", K_FUNC},
    {"÷", A_INS, "/", "", K_OP},
    {"mod", A_INS, "%", "", K_OP},

    {"x²", A_WRAP, "(", ")^2", K_FUNC},
    {"1/x", A_WRAP, "1/(", ")", K_FUNC},
    {"|x|", A_WRAP, "abs(", ")", K_FUNC},
    {"7", A_INS, "7", "", K_NUM},
    {"8", A_INS, "8", "", K_NUM},
    {"9", A_INS, "9", "", K_NUM},

    {"√", A_WRAP, "sqrt(", ")", K_FUNC},
    {"x^y", A_INS, "^", "", K_FUNC},
    {"n!", A_WRAP, "fact(", ")", K_FUNC},
    {"4", A_INS, "4", "", K_NUM},
    {"5", A_INS, "5", "", K_NUM},
    {"6", A_INS, "6", "", K_NUM},

    {"ln", A_WRAP, "log(", ")", K_FUNC},
    {"log", A_WRAP, "log10(", ")", K_FUNC},
    {"exp", A_WRAP, "exp(", ")", K_FUNC},
    {"1", A_INS, "1", "", K_NUM},
    {"2", A_INS, "2", "", K_NUM},
    {"3", A_INS, "3", "", K_NUM},

    {"sin", A_WRAP, "sin(", ")", K_FUNC},
    {"cos", A_WRAP, "cos(", ")", K_FUNC},
    {"tan", A_WRAP, "tan(", ")", K_FUNC},
    {"0", A_INS, "0", "", K_NUM},
    {".", A_INS, ".", "", K_NUM},
    {"=", A_EQUALS, "", "", K_EQUALS},

    {"(", A_INS, "(", "", K_FUNC},
    {")", A_INS, ")", "", K_FUNC},
    {"±", A_NEGATE, "", "", K_FUNC},
    {"×", A_INS, "*", "", K_OP},
    {"−", A_INS, "-", "", K_OP},
    {"+", A_INS, "+", "", K_OP},
};

static const char *CSS =
    "window, box, grid { background-color: #f3f3f3; }\n"
    ".display { font-size: 30px; color: #191919; }\n"
    ".history { font-size: 11px; color: #6b6b6b; }\n"
    ".mode { font-size: 12px; font-weight: bold; color: #191919; }\n"
    "button { background-image: none; background-color: #ffffff;\n"
    "         border: 1px solid #e0e0e0; border-radius: 4px;\n"
    "         font-size: 15px; color: #191919; padding: 4px 8px; min-height: 36px; }\n"
    "button:hover { background-color: #f7f7f7; }\n"
    "button.func { background-color: #ededed; }\n"
    "button.func:hover { background-color: #e3e3e3; }\n"
    "button.flat { background-color: transparent; border: none; min-height: 0; padding: 2px 8px; }\n"
    "button.equals { background-color: #0067c0; color: #ffffff; border: 1px solid #0067c0; }\n"
    "button.equals:hover { background-color: #1975c5; }\n";

static GtkWidget *g_window;
static GtkWidget *g_grid;
static GtkWidget *g_display_label;
static GtkWidget *g_history_label;
static GtkWidget *g_mode_label;

static char g_text[DISPLAY_SIZE];
static char g_history[DISPLAY_SIZE];
static int g_scientific;

static void update_labels(void)
{
    gtk_label_set_text(GTK_LABEL(g_display_label), g_text[0] != '\0' ? g_text : "0");
    gtk_label_set_text(GTK_LABEL(g_history_label), g_history);
}

static void append_text(const char *text)
{
    size_t used = strlen(g_text);
    size_t added = strlen(text);

    if (used + added + 1 >= sizeof g_text) {
        return;
    }
    memcpy(g_text + used, text, added + 1);
}

static void wrap_text(const char *prefix, const char *suffix)
{
    char buffer[sizeof g_text];

    snprintf(buffer, sizeof buffer, "%s%s%s", prefix, g_text, suffix);
    memcpy(g_text, buffer, sizeof g_text);
}

static void negate_text(void)
{
    char buffer[sizeof g_text];
    size_t length = strlen(g_text);

    if (length == 0) {
        return;
    }
    if (strncmp(g_text, "-(", 2) == 0 && g_text[length - 1] == ')') {
        snprintf(buffer, sizeof buffer, "%.*s", (int)(length - 3), g_text + 2);
    } else {
        snprintf(buffer, sizeof buffer, "-(%s)", g_text);
    }
    memcpy(g_text, buffer, sizeof g_text);
}

static void do_equals(void)
{
    char source[DISPLAY_SIZE];
    double value;
    int depth = 0;
    size_t index;
    size_t length;

    snprintf(source, sizeof source, "%s", g_text);

    for (index = 0; source[index] != '\0'; index++) {
        if (source[index] == '(') {
            depth++;
        } else if (source[index] == ')') {
            depth--;
        }
    }

    length = strlen(source);
    while (depth > 0 && length + 1 < sizeof source) {
        source[length++] = ')';
        depth--;
    }
    source[length] = '\0';

    if (length == 0) {
        return;
    }

    if (!evaluate(source, &value)) {
        snprintf(g_history, sizeof g_history, "error: %s", g_error);
        update_labels();
        return;
    }
    if (isnan(value) || isinf(value)) {
        snprintf(g_history, sizeof g_history, "error: %s",
                 isnan(value) ? "result is not a number" : "result is too large to represent");
        update_labels();
        return;
    }

    g_ans = value;
    snprintf(g_history, sizeof g_history, "%s =", source);
    snprintf(g_text, sizeof g_text, "%.12g", value);
    update_labels();
}

static void apply_key(KeyDef *key)
{
    switch (key->action) {
    case A_INS:
        append_text(key->text);
        break;
    case A_WRAP:
        wrap_text(key->text, key->suffix);
        break;
    case A_CLEAR:
        g_text[0] = '\0';
        break;
    case A_CLEAR_ALL:
        g_text[0] = '\0';
        g_history[0] = '\0';
        break;
    case A_BACK: {
        size_t length = strlen(g_text);

        if (length > 0) {
            g_text[length - 1] = '\0';
        }
        break;
    }
    case A_NEGATE:
        negate_text();
        break;
    case A_EQUALS:
        do_equals();
        return;
    }
    update_labels();
}

static void on_key_clicked(GtkButton *button, gpointer data)
{
    (void)button;
    apply_key((KeyDef *)data);
}

static void build_keypad(void)
{
    KeyDef *keys = g_scientific ? SCIENTIFIC_KEYS : STANDARD_KEYS;
    int count = g_scientific
        ? (int)(sizeof SCIENTIFIC_KEYS / sizeof SCIENTIFIC_KEYS[0])
        : (int)(sizeof STANDARD_KEYS / sizeof STANDARD_KEYS[0]);
    int columns = g_scientific ? 6 : 4;
    GList *children;
    GList *item;
    int index;

    children = gtk_container_get_children(GTK_CONTAINER(g_grid));
    for (item = children; item != NULL; item = item->next) {
        gtk_widget_destroy(GTK_WIDGET(item->data));
    }
    g_list_free(children);

    for (index = 0; index < count; index++) {
        KeyDef *key = &keys[index];
        GtkWidget *button = gtk_button_new_with_label(key->label);
        GtkStyleContext *context = gtk_widget_get_style_context(button);
        int row = index / columns;
        int column = index % columns;

        if (key->kind == K_FUNC) {
            gtk_style_context_add_class(context, "func");
        } else if (key->kind == K_EQUALS) {
            gtk_style_context_add_class(context, "equals");
        }

        g_signal_connect(button, "clicked", G_CALLBACK(on_key_clicked), key);
        gtk_widget_set_hexpand(button, TRUE);
        gtk_widget_set_vexpand(button, TRUE);
        gtk_grid_attach(GTK_GRID(g_grid), button, column, row, 1, 1);
    }

    gtk_widget_show_all(g_grid);
}

static void toggle_mode(GtkButton *button, gpointer data)
{
    (void)button;
    (void)data;

    g_scientific = !g_scientific;
    gtk_label_set_text(GTK_LABEL(g_mode_label), g_scientific ? "Scientific" : "Standard");
    build_keypad();
    gtk_window_resize(GTK_WINDOW(g_window),
                      g_scientific ? SCIENTIFIC_W : STANDARD_W,
                      g_scientific ? SCIENTIFIC_H : STANDARD_H);
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    gunichar character = gdk_keyval_to_unicode(event->keyval);

    (void)widget;
    (void)data;

    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        do_equals();
        return TRUE;
    }
    if (event->keyval == GDK_KEY_BackSpace) {
        size_t length = strlen(g_text);

        if (length > 0) {
            g_text[length - 1] = '\0';
        }
        update_labels();
        return TRUE;
    }
    if (event->keyval == GDK_KEY_Escape) {
        g_text[0] = '\0';
        g_history[0] = '\0';
        update_labels();
        return TRUE;
    }

    if (character < 128 && strchr("0123456789.+-*/%^()", (int)character) != NULL) {
        char buffer[2];

        buffer[0] = (char)character;
        buffer[1] = '\0';
        append_text(buffer);
        update_labels();
        return TRUE;
    }

    return FALSE;
}

int main(int argc, char **argv)
{
    GtkWidget *box;
    GtkWidget *header;
    GtkWidget *mode_button;
    GtkCssProvider *provider;

    gtk_init(&argc, &argv);

    g_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_window), "Calculator");
    gtk_window_set_default_size(GTK_WINDOW(g_window), STANDARD_W, STANDARD_H);
    g_signal_connect(g_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(g_window, "key-press-event", G_CALLBACK(on_key_press), NULL);

    provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(g_window), box);

    header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(header, 10);
    gtk_widget_set_margin_end(header, 10);
    gtk_widget_set_margin_top(header, 8);
    gtk_box_pack_start(GTK_BOX(box), header, FALSE, FALSE, 0);

    mode_button = gtk_button_new_with_label("☰");
    gtk_style_context_add_class(gtk_widget_get_style_context(mode_button), "flat");
    g_signal_connect(mode_button, "clicked", G_CALLBACK(toggle_mode), NULL);
    gtk_box_pack_start(GTK_BOX(header), mode_button, FALSE, FALSE, 0);

    g_mode_label = gtk_label_new("Standard");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_mode_label), "mode");
    gtk_widget_set_halign(g_mode_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(header), g_mode_label, TRUE, TRUE, 0);

    g_history_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(g_history_label), 1.0);
    gtk_label_set_ellipsize(GTK_LABEL(g_history_label), PANGO_ELLIPSIZE_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(g_history_label), "history");
    gtk_widget_set_margin_start(g_history_label, 16);
    gtk_widget_set_margin_end(g_history_label, 16);
    gtk_box_pack_start(GTK_BOX(box), g_history_label, FALSE, FALSE, 2);

    g_display_label = gtk_label_new("0");
    gtk_label_set_xalign(GTK_LABEL(g_display_label), 1.0);
    gtk_label_set_ellipsize(GTK_LABEL(g_display_label), PANGO_ELLIPSIZE_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(g_display_label), "display");
    gtk_widget_set_margin_start(g_display_label, 16);
    gtk_widget_set_margin_end(g_display_label, 16);
    gtk_widget_set_margin_bottom(g_display_label, 8);
    gtk_box_pack_start(GTK_BOX(box), g_display_label, FALSE, FALSE, 2);

    g_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(g_grid), 3);
    gtk_grid_set_column_spacing(GTK_GRID(g_grid), 3);
    gtk_grid_set_row_homogeneous(GTK_GRID(g_grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(g_grid), TRUE);
    gtk_widget_set_margin_start(g_grid, 10);
    gtk_widget_set_margin_end(g_grid, 10);
    gtk_widget_set_margin_bottom(g_grid, 10);
    gtk_box_pack_start(GTK_BOX(box), g_grid, TRUE, TRUE, 0);

    build_keypad();

    gtk_widget_show_all(g_window);
    gtk_main();
    return 0;
}

/*
 * Note on sharing the engine
 * -------------------------
 * Including calc.c works and costs nothing at runtime, but it is a hack: the
 * parser is now compiled into two programs and cannot be unit tested on its
 * own. The clean version is:
 *
 *   engine.h   declarations: evaluate(), format_number(), g_ans, g_error
 *   engine.c   the tokenizer, parser and libm calls (no main)
 *   calc.c     CLI: #include "engine.h"
 *   calc_gui.c GUI: #include "engine.h"
 *
 *   cc calc.c engine.c -o calc -lm
 *   cc calc_gui.c engine.c -o calc-gui $(pkg-config --cflags --libs gtk+-3.0) -lm
 */

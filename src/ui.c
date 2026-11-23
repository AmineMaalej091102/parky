#include "ui.h"

#include <gtk/gtk.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

#include "admin.h"
#include "agent.h"
#include "citoyen.h"
#include "storage.h"

typedef struct {
    ParkyData *data;
    int activeUserId;
    GtkWidget *notebook;
    GtkWidget *adminIdentity;
    GtkWidget *agentIdentity;
    GtkWidget *citizenIdentity;
    GtkWidget *adminUsers;
    GtkWidget *adminParkings;
    GtkWidget *adminComplaints;
    GtkWidget *adminOccupancy;
    GtkWidget *adminTotalUsers;
    GtkWidget *adminTotalParkings;
    GtkWidget *adminParkingAgents;
    GtkWidget *adminPendingComplaints;
    GtkWidget *adminGlobalOccupancy;
    GtkWidget *adminUserName;
    GtkWidget *adminUserEmail;
    GtkWidget *adminUserPassword;
    GtkWidget *adminUserRole;
    GtkWidget *adminUserParking;
    GtkWidget *adminParkingName;
    GtkWidget *adminParkingAddress;
    GtkWidget *adminParkingTotal;
    GtkWidget *adminParkingAvailable;
    GtkWidget *adminParkingAgent;
    GtkWidget *adminComplaintDetail;
    GtkWidget *agentReservations;
    GtkWidget *agentServices;
    GtkWidget *citizenParkings;
    GtkWidget *citizenReservations;
    GtkWidget *citizenReviews;
    GtkWidget *loginEmail;
    GtkWidget *loginPassword;
    GtkWidget *signupUsername;
    GtkWidget *signupEmail;
    GtkWidget *signupPassword;
    GtkWidget *signupAvatarPreview;
    char signupAvatarPath[PARKY_TEXT_LENGTH];
    GtkWidget *status;
    GtkWidget *citizenName;
    GtkWidget *citizenAddress;
    GtkWidget *citizenAvailable;
    GtkWidget *citizenParking;
    GtkWidget *citizenSpot;
    GtkWidget *citizenDate;
    GtkWidget *citizenTime;
    GtkWidget *citizenService;
    GtkWidget *citizenHours;
    GtkWidget *tariff;
    GtkWidget *reviewRating;
    GtkWidget *reviewComment;
    GtkWidget *complaintSubject;
    GtkWidget *complaintText;
    GtkWidget *agentServiceName;
    GtkWidget *agentServiceDescription;
    GtkWidget *agentServicePrice;
    GtkWidget *root;
    GtkWidget *profileImage;
} UiState;

static GtkWidget *main_window = NULL;

static void set_status(UiState *state, const char *message);
static void set_page(UiState *state, int page);
static void refresh_dashboard_identity(UiState *state);
static GdkPixbuf *make_parky_logo(void);

static gint show_dialog(GtkWindow *parent, GtkMessageType type,
                        const char *title, const char *msg)
{
    GtkWidget *dialog;
    GtkButtonsType buttons =
        title != NULL && strstr(title, "Confirm") != NULL ?
        GTK_BUTTONS_YES_NO : GTK_BUTTONS_OK;
    GtkWindow *dialogParent =
        parent != NULL ? parent : GTK_WINDOW(main_window);

    dialog = gtk_message_dialog_new(
        dialogParent,
        GTK_DIALOG_MODAL,
        type,
        buttons,
        "%s",
        msg);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_window_set_transient_for(GTK_WINDOW(dialog),
                                 GTK_WINDOW(main_window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(dialog),
                            GTK_WIN_POS_CENTER_ON_PARENT);
    {
        gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return response;
    }
}

static void show_message(UiState *state, GtkMessageType type,
                         const char *title, const char *message)
{
    (void)state;
    show_dialog(NULL, type, title, message);
}

static int contains_auth_delimiter(const char *text)
{
    for (; *text != '\0'; ++text) {
        if (isspace((unsigned char)*text) || *text == '|' ||
            *text == '\n' || *text == '\r' || *text == '\t') {
            return 1;
        }
    }
    return 0;
}

static int valid_auth_field(const char *text)
{
    const char *cursor;
    int length = 0;
    int hasContent = 0;

    if (text == NULL) {
        return 0;
    }
    for (cursor = text; *cursor != '\0'; ++cursor) {
        if (!isspace((unsigned char)*cursor)) {
            ++hasContent;
        }
        ++length;
    }
    return length >= 3 && hasContent > 0 &&
           !contains_auth_delimiter(text);
}

static int valid_email(const char *email)
{
    return valid_auth_field(email) &&
           strchr(email, '@') != NULL &&
           strchr(email, '.') != NULL;
}

static void sign_out(UiState *state)
{
    state->activeUserId = 0;
    refresh_dashboard_identity(state);
    gtk_entry_set_text(GTK_ENTRY(state->loginEmail), "");
    gtk_entry_set_text(GTK_ENTRY(state->loginPassword), "");
    set_page(state, 0);
    set_status(state, "Signed out");
}

static void sign_out_clicked(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    sign_out((UiState *)user_data);
}

static GtkWidget *make_labeled_entry(const char *label, GtkWidget **entry)
{
    GtkWidget *box = gtk_hbox_new(FALSE, 8);
    GtkWidget *text = gtk_entry_new();
    gtk_widget_set_size_request(text, -1, 32);
    gtk_box_pack_start(GTK_BOX(box), gtk_label_new(label),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), text, TRUE, TRUE, 0);
    *entry = text;
    return box;
}

static GtkWidget *make_auth_row(const char *iconStock,
                                GtkWidget **entry,
                                GtkSizeGroup *sizeGroup)
{
    GtkWidget *row = gtk_hbox_new(FALSE, 10);
    GtkWidget *icon = gtk_image_new_from_stock(
        iconStock, GTK_ICON_SIZE_BUTTON);
    GtkWidget *text = gtk_entry_new();
    gtk_widget_set_size_request(icon, 20, 20);
    gtk_widget_set_size_request(text, 320, 38);
    if (sizeGroup != NULL)
        gtk_size_group_add_widget(sizeGroup, text);
    gtk_box_pack_start(GTK_BOX(row), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), text, FALSE, FALSE, 0);
    *entry = text;
    return row;
}

static GtkWidget *make_stock_button(const char *label, const char *stock)
{
    GtkWidget *button = gtk_button_new();
    GtkWidget *content = gtk_hbox_new(FALSE, 6);
    GtkWidget *icon = gtk_image_new_from_stock(
        stock, GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(content), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), gtk_label_new(label),
                       FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(button), content);
    return button;
}

static GtkWidget *make_icon_button(const char *stock,
                                   const char *tooltip)
{
    GtkWidget *button = gtk_button_new();
    GtkWidget *icon = gtk_image_new_from_stock(
        stock, GTK_ICON_SIZE_BUTTON);
    gtk_container_add(GTK_CONTAINER(button), icon);
    gtk_widget_set_size_request(button, 34, 34);
    gtk_widget_set_tooltip_text(button, tooltip);
    return button;
}

static GdkPixbuf *load_parky_logo(void)
{
    GdkPixbuf *logo = gdk_pixbuf_new_from_file_at_scale(
        "assets/logo.png", 72, 72, TRUE, NULL);
    if (logo == NULL)
        logo = gdk_pixbuf_new_from_file_at_scale(
            "assets/logo.jpg", 72, 72, TRUE, NULL);
    return logo;
}

static void entry_placeholder_focus(GtkWidget *entry,
                                    GdkEventFocus *event,
                                    gpointer user_data)
{
    const char *placeholder = (const char *)user_data;
    GdkColor normal;
    (void)event;
    if (gtk_entry_get_text(GTK_ENTRY(entry)) &&
        strcmp(gtk_entry_get_text(GTK_ENTRY(entry)), placeholder) == 0)
        gtk_entry_set_text(GTK_ENTRY(entry), "");
    gdk_color_parse("#1B2A4A", &normal);
    gtk_widget_modify_text(entry, GTK_STATE_NORMAL, &normal);
    if (strcmp(placeholder, "Password") == 0)
        gtk_entry_set_visibility(GTK_ENTRY(entry), TRUE);
}

static void entry_placeholder_blur(GtkWidget *entry,
                                   GdkEventFocus *event,
                                   gpointer user_data)
{
    const char *placeholder = (const char *)user_data;
    GdkColor gray;
    gdk_color_parse("#8A94A6", &gray);
    (void)event;
    if (gtk_entry_get_text(GTK_ENTRY(entry))[0] == '\0') {
        gtk_entry_set_text(GTK_ENTRY(entry), placeholder);
        gtk_widget_modify_text(entry, GTK_STATE_NORMAL, &gray);
        if (strcmp(placeholder, "Password") == 0)
            gtk_entry_set_visibility(GTK_ENTRY(entry), TRUE);
    } else {
        GdkColor normal;
        gdk_color_parse("#1B2A4A", &normal);
        gtk_widget_modify_text(entry, GTK_STATE_NORMAL, &normal);
    }
}

static void placeholder_changed(GtkWidget *entry, gpointer user_data)
{
    const char *placeholder = (const char *)user_data;
    const char *value = gtk_entry_get_text(GTK_ENTRY(entry));
    GdkColor normal;

    if (strcmp(value, placeholder) == 0)
        return;
    gdk_color_parse("#1B2A4A", &normal);
    gtk_widget_modify_text(entry, GTK_STATE_NORMAL, &normal);
    if (strcmp(placeholder, "Password") == 0)
        gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
}

static void set_entry_placeholder(GtkWidget *entry, const char *text)
{
    gtk_entry_set_text(GTK_ENTRY(entry), text);
    {
        GdkColor gray;
        gdk_color_parse("#8A94A6", &gray);
        gtk_widget_modify_text(entry, GTK_STATE_NORMAL, &gray);
    }
    g_object_set_data_full(G_OBJECT(entry), "parky-placeholder",
                           g_strdup(text), g_free);
    g_signal_connect(entry, "focus-in-event",
                     G_CALLBACK(entry_placeholder_focus),
                     g_object_get_data(G_OBJECT(entry),
                                       "parky-placeholder"));
    g_signal_connect(entry, "focus-out-event",
                     G_CALLBACK(entry_placeholder_blur),
                     g_object_get_data(G_OBJECT(entry),
                                       "parky-placeholder"));
    g_signal_connect(entry, "changed",
                     G_CALLBACK(placeholder_changed),
                     g_object_get_data(G_OBJECT(entry),
                                       "parky-placeholder"));
}

static const char *entry_placeholder_value(GtkWidget *entry)
{
    const char *value = gtk_entry_get_text(GTK_ENTRY(entry));
    const char *placeholder =
        (const char *)g_object_get_data(G_OBJECT(entry),
                                        "parky-placeholder");
    if (placeholder != NULL && strcmp(value, placeholder) == 0)
        return "";
    return value;
}

static GtkWidget *rounded_card(int width)
{
    GtkWidget *card = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(card), GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_size_request(card, width, -1);
    gtk_container_set_border_width(GTK_CONTAINER(card), 24);
    return card;
}

static GtkWidget *compact_auth_button(const char *label,
                                      const char *stock,
                                      const char *accentHex)
{
    GtkWidget *button = make_stock_button(label, stock);
    GdkColor accent;
    GdkColor white;
    gdk_color_parse(accentHex, &accent);
    gdk_color_parse("#FFFFFF", &white);
    gtk_widget_modify_bg(button, GTK_STATE_NORMAL, &accent);
    gtk_widget_modify_bg(button, GTK_STATE_PRELIGHT, &accent);
    gtk_widget_modify_bg(button, GTK_STATE_ACTIVE, &accent);
    gtk_widget_modify_fg(button, GTK_STATE_NORMAL, &white);
    gtk_widget_modify_fg(button, GTK_STATE_PRELIGHT, &white);
    gtk_widget_modify_fg(button, GTK_STATE_ACTIVE, &white);
    {
        GtkWidget *content = gtk_bin_get_child(GTK_BIN(button));
        if (GTK_IS_CONTAINER(content)) {
            GList *children =
                gtk_container_get_children(GTK_CONTAINER(content));
            GList *child;
            for (child = children; child != NULL; child = child->next) {
                if (GTK_IS_LABEL(child->data)) {
                    gtk_widget_modify_fg(GTK_WIDGET(child->data),
                                         GTK_STATE_NORMAL, &white);
                    gtk_widget_modify_fg(GTK_WIDGET(child->data),
                                         GTK_STATE_PRELIGHT, &white);
                    gtk_widget_modify_fg(GTK_WIDGET(child->data),
                                         GTK_STATE_ACTIVE, &white);
                }
            }
            g_list_free(children);
        }
    }
    gtk_widget_set_size_request(button, 180, 38);
    return button;
}

static GtkWidget *make_auth_hero(void)
{
    GtkWidget *hero = gtk_vbox_new(FALSE, 6);
    GdkPixbuf *logo = load_parky_logo();
    GtkWidget *logoImage;
    GtkWidget *line1 = gtk_label_new(
        "Find, reserve, and manage urban parking spaces in seconds.");
    GtkWidget *line2 = gtk_label_new(
        "Real-time spot availability and on-demand car services.");
    if (logo == NULL)
        logo = make_parky_logo();
    logo = gdk_pixbuf_scale_simple(logo, 64, 64,
                                   GDK_INTERP_BILINEAR);
    logoImage = gtk_image_new_from_pixbuf(logo);
    gtk_misc_set_alignment(GTK_MISC(line1), 0.5, 0.5);
    gtk_misc_set_alignment(GTK_MISC(line2), 0.5, 0.5);
    gtk_label_set_line_wrap(GTK_LABEL(line1), TRUE);
    gtk_label_set_line_wrap(GTK_LABEL(line2), TRUE);
    gtk_label_set_justify(GTK_LABEL(line1), GTK_JUSTIFY_CENTER);
    gtk_label_set_justify(GTK_LABEL(line2), GTK_JUSTIFY_CENTER);
    gtk_widget_set_size_request(line1, 480, -1);
    gtk_widget_set_size_request(line2, 480, -1);
    gtk_box_pack_start(GTK_BOX(hero), logoImage, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hero), line1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hero), line2, FALSE, FALSE, 0);
    g_object_unref(logo);
    return hero;
}

static GtkWidget *make_stock_tab(const char *label, const char *stock)
{
    GtkWidget *tab = gtk_hbox_new(FALSE, 4);
    GtkWidget *icon = gtk_image_new_from_stock(
        stock, GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(tab), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tab), gtk_label_new(label),
                       FALSE, FALSE, 0);
    return tab;
}

static GdkPixbuf *make_parky_logo(void)
{
    GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                       TRUE, 8, 96, 96);
    cairo_surface_t *surface;
    cairo_t *cr;
    gdk_pixbuf_fill(pixbuf, 0x00000000);
    surface = cairo_image_surface_create_for_data(
        gdk_pixbuf_get_pixels(pixbuf),
        CAIRO_FORMAT_ARGB32,
        96, 96,
        gdk_pixbuf_get_rowstride(pixbuf));
    cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 0.106, 0.165, 0.290);
    cairo_rectangle(cr, 4, 4, 88, 88);
    cairo_fill(cr);
    cairo_set_line_width(cr, 4);
    cairo_set_source_rgb(cr, 0.949, 0.663, 0.000);
    cairo_rectangle(cr, 8, 8, 80, 80);
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_font_size(cr, 54);
    cairo_move_to(cr, 29, 67);
    cairo_show_text(cr, "P");
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return pixbuf;
}

static GdkPixbuf *make_default_avatar(const char *initials)
{
    GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                       TRUE, 8, 36, 36);
    cairo_surface_t *surface;
    cairo_t *cr;
    char letter[2] = {'U', '\0'};
    gdk_pixbuf_fill(pixbuf, 0x00000000);
    surface = cairo_image_surface_create_for_data(
        gdk_pixbuf_get_pixels(pixbuf),
        CAIRO_FORMAT_ARGB32,
        36, 36,
        gdk_pixbuf_get_rowstride(pixbuf));
    cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 0.122, 0.373, 0.302);
    cairo_arc(cr, 18, 18, 16, 0.0, 6.28318);
    cairo_fill(cr);
    if (initials != NULL && initials[0] != '\0')
        letter[0] = initials[0];
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_font_size(cr, 15);
    cairo_move_to(cr, 12, 23);
    cairo_show_text(cr, letter);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return pixbuf;
}

static void apply_light_theme(GtkWidget *widget)
{
    GList *children;
    GtkWidget *child;
    GdkColor background;
    GdkColor white;

    if (!GTK_IS_WIDGET(widget))
        return;
    if (GTK_IS_EVENT_BOX(widget) || GTK_IS_BUTTON(widget))
        return;
    gdk_color_parse("#EEF2F7", &background);
    gdk_color_parse("#FFFFFF", &white);
    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &background);
    if (GTK_IS_ENTRY(widget) || GTK_IS_TEXT_VIEW(widget) ||
        GTK_IS_TREE_VIEW(widget)) {
        gtk_widget_modify_base(widget, GTK_STATE_NORMAL, &white);
    }
    if (GTK_IS_CONTAINER(widget)) {
        children = gtk_container_get_children(GTK_CONTAINER(widget));
        while (children != NULL) {
            child = GTK_WIDGET(children->data);
            apply_light_theme(child);
            children = g_list_delete_link(children, children);
        }
    }
}

static void load_profile_image(UiState *state)
{
    int i;
    const char *path = NULL;
    GdkPixbuf *pixbuf;

    if (state->profileImage == NULL)
        return;
    for (i = 0; i < state->data->userCount; ++i) {
        if (state->data->users[i].id == state->activeUserId) {
            path = state->data->users[i].avatarPath;
            break;
        }
    }
    if (path != NULL && path[0] != '\0') {
        pixbuf = gdk_pixbuf_new_from_file_at_scale(
            path, 96, 96, TRUE, NULL);
        if (pixbuf != NULL) {
            gtk_image_set_from_pixbuf(GTK_IMAGE(state->profileImage),
                                      pixbuf);
            g_object_unref(pixbuf);
            return;
        }
    }
    {
        GdkPixbuf *defaultAvatar = make_default_avatar("U");
        gtk_image_set_from_pixbuf(GTK_IMAGE(state->profileImage),
                                  defaultAvatar);
        g_object_unref(defaultAvatar);
    }
}

static void choose_profile_photo_clicked(GtkWidget *widget,
                                         gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    GtkWidget *dialog;
    GtkFileFilter *filter;
    gchar *selected;
    FILE *source;
    FILE *destination;
    char target[256];
    char buffer[4096];
    size_t count;
    int i;
    (void)widget;

    dialog = gtk_file_chooser_dialog_new(
        "Choose Profile Photo",
        NULL,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
        NULL);
    gtk_window_set_transient_for(GTK_WINDOW(dialog),
                                 GTK_WINDOW(main_window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(dialog),
                            GTK_WIN_POS_CENTER_ON_PARENT);
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "PNG and JPEG images");
    gtk_file_filter_add_pattern(filter, "*.png");
    gtk_file_filter_add_pattern(filter, "*.jpg");
    gtk_file_filter_add_pattern(filter, "*.jpeg");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        selected = gtk_file_chooser_get_filename(
            GTK_FILE_CHOOSER(dialog));
        mkdir("data/avatars", 0755);
        snprintf(target, sizeof(target),
                 "data/avatars/user-%d.jpg", state->activeUserId);
        source = fopen(selected, "rb");
        destination = source != NULL ? fopen(target, "wb") : NULL;
        if (source != NULL && destination != NULL) {
            while ((count = fread(buffer, 1, sizeof(buffer), source)) > 0)
                fwrite(buffer, 1, count, destination);
            fclose(source);
            fclose(destination);
            for (i = 0; i < state->data->userCount; ++i) {
                if (state->data->users[i].id == state->activeUserId) {
                    snprintf(state->data->users[i].avatarPath,
                             sizeof(state->data->users[i].avatarPath),
                             "%s", target);
                    break;
                }
            }
            if (storage_save(state->data))
                show_message(state, GTK_MESSAGE_INFO,
                             "Profile photo saved",
                             "The profile photo is saved and will remain across sessions.");
            load_profile_image(state);
        } else {
            show_message(state, GTK_MESSAGE_ERROR,
                         "Profile photo error",
                         "PARKY could not copy the selected image.");
            if (source != NULL) fclose(source);
        }
        g_free(selected);
    }
    gtk_widget_destroy(dialog);
}

static void style_button(GtkWidget *button, const char *background,
                         const char *foreground)
{
    GdkColor bg;
    GdkColor fg;
    if (gdk_color_parse(background, &bg))
        gtk_widget_modify_bg(button, GTK_STATE_NORMAL, &bg);
    if (gdk_color_parse(foreground, &fg))
        gtk_widget_modify_fg(button, GTK_STATE_NORMAL, &fg);
}

static GtkWidget *make_dashboard_header(UiState *state, const char *role)
{
    GtkWidget *event = gtk_event_box_new();
    GtkWidget *header = gtk_hbox_new(FALSE, 8);
    GtkWidget *brand = gtk_hbox_new(FALSE, 8);
    GtkWidget *spacer = gtk_label_new("");
    GtkWidget *profile = gtk_hbox_new(FALSE, 6);
    GdkPixbuf *logoPixbuf = load_parky_logo();
    GtkWidget *brandTitle = gtk_label_new(NULL);
    GtkWidget *brandText = gtk_vbox_new(FALSE, 0);
    GtkWidget *avatar;
    GtkWidget *identity = gtk_label_new("User");
    GtkWidget *badge = gtk_label_new(role);
    GtkWidget *signOut = make_icon_button(GTK_STOCK_QUIT, "Sign Out");
    const char *name = "Guest";
    char identityText[128];
    char badgeMarkup[128];
    char identityMarkup[128];
    const char *badgeColor;
    int i;
    int loadedAvatar = 0;
    GdkColor navy;
    if (logoPixbuf == NULL)
        logoPixbuf = make_parky_logo();
    avatar = gtk_image_new_from_pixbuf(logoPixbuf);
    gtk_widget_set_size_request(avatar, 36, 36);
    gdk_color_parse("#1B2A4A", &navy);
    gtk_widget_modify_bg(event, GTK_STATE_NORMAL, &navy);
    gtk_container_set_border_width(GTK_CONTAINER(header), 8);
    gtk_widget_set_size_request(avatar, 36, 36);

    for (i = 0; i < state->data->userCount; ++i) {
        if (state->data->users[i].id == state->activeUserId) {
            name = state->data->users[i].name;
            if (state->data->users[i].avatarPath[0] != '\0') {
                GdkPixbuf *sessionPixbuf =
                    gdk_pixbuf_new_from_file_at_scale(
                        state->data->users[i].avatarPath,
                        36, 36, TRUE, NULL);
                if (sessionPixbuf != NULL) {
                    gtk_image_set_from_pixbuf(GTK_IMAGE(avatar),
                                              sessionPixbuf);
                    loadedAvatar = 1;
                    g_object_unref(sessionPixbuf);
                }
            }
            break;
        }
    }
    if (!loadedAvatar) {
        GdkPixbuf *defaultAvatar = make_default_avatar(name);
        gtk_image_set_from_pixbuf(GTK_IMAGE(avatar), defaultAvatar);
        g_object_unref(defaultAvatar);
    }
    snprintf(identityText, sizeof(identityText),
             "Signed in: %s", name);
    snprintf(identityMarkup, sizeof(identityMarkup),
             "<span foreground='white' weight='bold'>%s</span>",
             identityText);
    gtk_label_set_markup(GTK_LABEL(identity), identityMarkup);
    badgeColor = strcmp(role, "ADMIN") == 0 ? "#1F5FBF" :
                 strcmp(role, "AGENT") == 0 ? "#287A4D" : "#8A5A00";
    snprintf(badgeMarkup, sizeof(badgeMarkup),
             "<span background='%s' foreground='white' weight='bold'>  %s  </span>",
             badgeColor, role);
    gtk_label_set_markup(GTK_LABEL(badge), badgeMarkup);
    if (strcmp(role, "ADMIN") == 0) state->adminIdentity = identity;
    else if (strcmp(role, "AGENT") == 0) state->agentIdentity = identity;
    else state->citizenIdentity = identity;
    {
        GdkPixbuf *brandLogoPixbuf = gdk_pixbuf_scale_simple(
            logoPixbuf, 44, 44, GDK_INTERP_BILINEAR);
        GtkWidget *brandLogo =
            gtk_image_new_from_pixbuf(brandLogoPixbuf);
        gtk_label_set_markup(GTK_LABEL(brandTitle),
            "<span foreground='white' size='large' weight='bold'>PARKY</span>\n"
            "<span foreground='#E5EAF3' size='small'>"
            "Smart Parking and Urban Mobility Management</span>");
        gtk_box_pack_start(GTK_BOX(brandText), brandTitle,
                           FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(brand), brandLogo,
                           FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(brand), brandText,
                           FALSE, FALSE, 4);
        g_object_unref(brandLogoPixbuf);
    }
    gtk_box_pack_start(GTK_BOX(header), brand, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(header), spacer, TRUE, TRUE, 0);
    if (strcmp(role, "ADMIN") != 0) {
        gtk_box_pack_start(GTK_BOX(profile), avatar, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(profile), identity, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(profile), badge, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(profile), signOut, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(header), profile, FALSE, FALSE, 8);
    style_button(signOut, "#F4F7FB", "#1B2A4A");
    gtk_container_add(GTK_CONTAINER(event), header);
    g_object_unref(logoPixbuf);
    g_signal_connect(signOut, "clicked",
                     G_CALLBACK(sign_out_clicked), state);
    return event;
}

static int confirm_destructive_action(UiState *state,
                                      const char *action,
                                      const char *details)
{
    gint response;
    char message[256];
    snprintf(message, sizeof(message), "%s\n%s", action, details);
    (void)state;
    response = show_dialog(NULL, GTK_MESSAGE_WARNING,
                           "Confirm destructive action", message);
    return response == GTK_RESPONSE_YES;
}

static void set_status(UiState *state, const char *message)
{
    gtk_statusbar_push(GTK_STATUSBAR(state->status), 1, message);
}

static void refresh_dashboard_identity(UiState *state)
{
    char identityText[128];
    char identityMarkup[128];
    const char *name = "Guest";
    int i;
    for (i = 0; i < state->data->userCount; ++i) {
        if (state->data->users[i].id == state->activeUserId) {
            name = state->data->users[i].name;
            break;
        }
    }
    snprintf(identityText, sizeof(identityText), "Signed in: %s", name);
    snprintf(identityMarkup, sizeof(identityMarkup),
             "<span foreground='white' weight='bold'>%s</span>",
             identityText);
    if (state->adminIdentity != NULL)
        gtk_label_set_markup(GTK_LABEL(state->adminIdentity), identityMarkup);
    if (state->agentIdentity != NULL)
        gtk_label_set_markup(GTK_LABEL(state->agentIdentity), identityMarkup);
    if (state->citizenIdentity != NULL)
        gtk_label_set_markup(GTK_LABEL(state->citizenIdentity), identityMarkup);
}

static void set_page(UiState *state, int page)
{
    int i;
    int role = 0;

    for (i = 0; i < state->data->userCount; ++i) {
        if (state->data->users[i].id == state->activeUserId) {
            role = (int)state->data->users[i].role;
            break;
        }
    }
    if (page == 0) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(state->notebook), 0);
    } else if (page == 4) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(state->notebook), 4);
    } else if (page == 1 && role == USER_ADMIN) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(state->notebook), 1);
    } else if (page == 2 && role == USER_AGENT) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(state->notebook), 2);
    } else if (page == 3 && role == USER_CITIZEN) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(state->notebook), 3);
    }
}

static GtkWidget *make_tree_view(const char *frameTitle,
                                 GtkListStore **store,
                                 const char **titles, int columnCount)
{
    GtkWidget *tree = gtk_tree_view_new();
    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *frame = gtk_frame_new(frameTitle);
    GType types[8];
    int i;
    if (columnCount > 8) {
        columnCount = 8;
    }
    for (i = 0; i < columnCount; ++i) {
        types[i] = G_TYPE_STRING;
    }
    gtk_container_set_border_width(GTK_CONTAINER(frame), 6);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
    *store = gtk_list_store_newv(columnCount, types);
    gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(*store));
    g_object_unref(*store);
    for (i = 0; i < columnCount; ++i) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
            titles[i], renderer, "text", i, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
        if (strcmp(titles[i], "ID") == 0)
            gtk_tree_view_column_set_visible(column, FALSE);
    }
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroller), tree);
    gtk_container_add(GTK_CONTAINER(frame), scroller);
    gtk_widget_set_size_request(scroller, -1, 90);
    return frame;
}

static GtkListStore *tree_store(UiState *state, GtkWidget *treeWidget)
{
    GtkWidget *tree;
    (void)state;
    tree = gtk_bin_get_child(GTK_BIN(
        gtk_bin_get_child(GTK_BIN(treeWidget))));
    return GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(tree)));
}

static int tree_selected_first_id(UiState *state, GtkWidget *treeWidget)
{
    GtkWidget *frameChild;
    GtkWidget *scroller;
    GtkWidget *tree;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *text = NULL;
    int id = 0;

    frameChild = gtk_bin_get_child(GTK_BIN(treeWidget));
    scroller = GTK_IS_SCROLLED_WINDOW(frameChild) ?
               frameChild : frameChild;
    tree = gtk_bin_get_child(GTK_BIN(scroller));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree));
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, 0, &text, -1);
        if (text != NULL) {
            id = atoi(text);
            g_free(text);
        }
    }
    (void)state;
    return id;
}

static void refresh_admin_tables(UiState *state)
{
    GtkListStore *store;
    GtkTreeIter iter;
    char value[128];
    int i;
    int agentCount = 0;
    int pending = 0;

    store = tree_store(state, state->adminUsers);
    gtk_list_store_clear(store);
    for (i = 0; i < state->data->userCount; ++i) {
        const char *role = state->data->users[i].role == USER_ADMIN ? "Admin" :
                           state->data->users[i].role == USER_AGENT ? "Agent" :
                           "Citizen";
        char assigned[PARKY_NAME_LENGTH];
        if (state->data->users[i].role == USER_AGENT)
            ++agentCount;
        assigned[0] = '\0';
        if (state->data->users[i].assignedParkingId > 0) {
            int p;
            for (p = 0; p < state->data->parkingCount; ++p) {
                if (state->data->parkings[p].id ==
                    state->data->users[i].assignedParkingId) {
                    snprintf(assigned, sizeof(assigned), "%s",
                             state->data->parkings[p].name);
                    break;
                }
            }
        }
        if (assigned[0] == '\0')
            snprintf(assigned, sizeof(assigned), "Unassigned");
        gtk_list_store_append(store, &iter);
        snprintf(value, sizeof(value), "%d", state->data->users[i].id);
        gtk_list_store_set(store, &iter, 0, value, 1,
                           state->data->users[i].name, 2,
                           state->data->users[i].email, 3, role, 4,
                           assigned, -1);
    }

    store = tree_store(state, state->adminParkings);
    gtk_list_store_clear(store);
    for (i = 0; i < state->data->parkingCount; ++i) {
        char total[32], available[32], price[32];
        const char *agentName = "Unassigned";
        int j;
        snprintf(total, sizeof(total), "%d",
                 state->data->parkings[i].totalSpots);
        snprintf(available, sizeof(available), "%d",
                 state->data->parkings[i].availableSpots);
        snprintf(price, sizeof(price), "2.00 TND/hr");
        for (j = 0; j < state->data->userCount; ++j) {
            if (state->data->users[j].role == USER_AGENT &&
                state->data->users[j].assignedParkingId ==
                state->data->parkings[i].id) {
                agentName = state->data->users[j].name;
                break;
            }
        }
        gtk_list_store_append(store, &iter);
        snprintf(value, sizeof(value), "%d", state->data->parkings[i].id);
        gtk_list_store_set(store, &iter, 0, value, 1,
                           state->data->parkings[i].name, 2,
                           state->data->parkings[i].address, 3, total,
                           4, available, 5, price, 6, agentName, -1);
    }

    store = tree_store(state, state->adminComplaints);
    gtk_list_store_clear(store);
    for (i = 0; i < state->data->complaintCount; ++i) {
        const char *from = "Unknown";
        int j;
        if (state->data->complaints[i].status != 2)
            ++pending;
        for (j = 0; j < state->data->userCount; ++j) {
            if (state->data->users[j].id ==
                state->data->complaints[i].citizenId) {
                from = state->data->users[j].name;
                break;
            }
        }
        gtk_list_store_append(store, &iter);
        snprintf(value, sizeof(value), "%d", state->data->complaints[i].id);
        gtk_list_store_set(store, &iter, 0, value, 1,
                           from, 2, state->data->complaints[i].subject, 3,
                           state->data->complaints[i].status == 2 ?
                           "Resolved" : "Open", -1);
    }

    {
        char markup[128];
        snprintf(markup, sizeof(markup),
                 "<span size='xx-large' weight='bold'>%d</span>",
                 state->data->userCount);
        gtk_label_set_markup(GTK_LABEL(state->adminTotalUsers), markup);
        snprintf(markup, sizeof(markup),
                 "<span size='xx-large' weight='bold'>%d</span>",
                 state->data->parkingCount);
        gtk_label_set_markup(GTK_LABEL(state->adminTotalParkings), markup);
        snprintf(markup, sizeof(markup),
                 "<span size='xx-large' weight='bold'>%d</span>",
                 agentCount);
        gtk_label_set_markup(GTK_LABEL(state->adminParkingAgents), markup);
        snprintf(markup, sizeof(markup),
                 "<span size='xx-large' weight='bold'>%d</span>",
                 pending);
        gtk_label_set_markup(GTK_LABEL(state->adminPendingComplaints), markup);
        if (state->adminGlobalOccupancy != NULL) {
            int totalSpots = 0;
            int occupiedSpots = 0;
            double globalRate;
            for (i = 0; i < state->data->parkingCount; ++i) {
                totalSpots += state->data->parkings[i].totalSpots;
                occupiedSpots += state->data->parkings[i].totalSpots -
                                 state->data->parkings[i].availableSpots;
            }
            globalRate = totalSpots > 0 ?
                         ((double)occupiedSpots / (double)totalSpots) * 100.0 :
                         0.0;
            snprintf(markup, sizeof(markup),
                     "<span size='xx-large' weight='bold'>%.1f%%</span>",
                     globalRate);
            gtk_label_set_markup(GTK_LABEL(state->adminGlobalOccupancy),
                                 markup);
        }
    }

    if (state->adminOccupancy != NULL) {
        GtkListStore *occupancyStore =
            tree_store(state, state->adminOccupancy);
        gtk_list_store_clear(occupancyStore);
        for (i = 0; i < state->data->parkingCount; ++i) {
            int occupied =
                state->data->parkings[i].totalSpots -
                state->data->parkings[i].availableSpots;
            const char *agentName = "Unassigned";
            char capacityText[32], occupiedText[32], revenueText[32];
            int j;
            for (j = 0; j < state->data->userCount; ++j) {
                if (state->data->users[j].role == USER_AGENT &&
                    state->data->users[j].assignedParkingId ==
                    state->data->parkings[i].id) {
                    agentName = state->data->users[j].name;
                    break;
                }
            }
            snprintf(occupiedText, sizeof(occupiedText), "%d", occupied);
            snprintf(capacityText, sizeof(capacityText), "%d",
                     state->data->parkings[i].totalSpots);
            snprintf(revenueText, sizeof(revenueText), "%.2f TND",
                     occupied * 2.0);
            gtk_list_store_append(occupancyStore, &iter);
            gtk_list_store_set(occupancyStore, &iter, 0,
                               state->data->parkings[i].name, 1,
                               capacityText, 2, occupiedText, 3, revenueText,
                               4, agentName, -1);
        }
    }
}

static void refresh_agent_tables(UiState *state)
{
    GtkListStore *store;
    GtkTreeIter iter;
    char value[128];
    int i;

    store = tree_store(state, state->agentReservations);
    gtk_list_store_clear(store);
    for (i = 0; i < state->data->reservationCount; ++i) {
        const char *status;
        if (state->data->reservations[i].status == RESERVATION_APPROVED)
            status = "Approved";
        else if (state->data->reservations[i].status ==
                 RESERVATION_REJECTED)
            status = "Rejected";
        else if (state->data->reservations[i].status == 3)
            status = "Completed";
        else
            status = "Pending";
        gtk_list_store_append(store, &iter);
        snprintf(value, sizeof(value), "%d", state->data->reservations[i].id);
        gtk_list_store_set(store, &iter, 0, value, 1,
                           state->data->reservations[i].date, 2,
                           state->data->reservations[i].time, 3, status,
                           -1);
    }

    store = tree_store(state, state->agentServices);
    gtk_list_store_clear(store);
    for (i = 0; i < state->data->serviceCount; ++i) {
        gtk_list_store_append(store, &iter);
        snprintf(value, sizeof(value), "%d", state->data->services[i].id);
        gtk_list_store_set(store, &iter, 0, value, 1,
                           state->data->services[i].name, 2,
                           state->data->services[i].description, 3,
                           state->data->services[i].status ? "Active" :
                           "Inactive", -1);
    }
}

static void refresh_citizen_tables(UiState *state)
{
    GtkListStore *store;
    GtkTreeIter iter;
    char value[128];
    int i;

    store = tree_store(state, state->citizenParkings);
    gtk_list_store_clear(store);
    for (i = 0; i < state->data->parkingCount; ++i) {
        gtk_list_store_append(store, &iter);
        snprintf(value, sizeof(value), "%d", state->data->parkings[i].id);
        {
            char available[32];
            snprintf(available, sizeof(available), "%d",
                     state->data->parkings[i].availableSpots);
            gtk_list_store_set(store, &iter, 0, value, 1,
                               state->data->parkings[i].name, 2,
                               state->data->parkings[i].address, 3,
                               available, -1);
        }
    }

    if (state->citizenReservations != NULL) {
        store = tree_store(state, state->citizenReservations);
        gtk_list_store_clear(store);
        for (i = 0; i < state->data->reservationCount; ++i) {
            if (state->data->reservations[i].citizenId ==
                state->activeUserId) {
                const char *status =
                    state->data->reservations[i].status ==
                    RESERVATION_APPROVED ? "Approved" :
                    state->data->reservations[i].status ==
                    RESERVATION_REJECTED ? "Rejected" :
                    state->data->reservations[i].status == 3 ?
                    "Completed" : "Pending";
                gtk_list_store_append(store, &iter);
                snprintf(value, sizeof(value), "%d",
                         state->data->reservations[i].id);
                gtk_list_store_set(store, &iter, 0, value, 1,
                                   state->data->reservations[i].date,
                                   2, state->data->reservations[i].time,
                                   3, status, -1);
            }
        }
    }

    store = tree_store(state, state->citizenReviews);
    gtk_list_store_clear(store);
    for (i = 0; i < state->data->reviewCount; ++i) {
        gtk_list_store_append(store, &iter);
        snprintf(value, sizeof(value), "%d", state->data->reviews[i].rating);
        gtk_list_store_set(store, &iter, 0, value, 1,
                           state->data->reviews[i].comment, -1);
    }
}

static void open_signup_clicked(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    set_page((UiState *)user_data, 4);
}

static void back_to_login_clicked(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    set_page((UiState *)user_data, 0);
}

static void choose_signup_avatar_clicked(GtkWidget *widget,
                                         gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    GtkWidget *dialog;
    GtkFileFilter *filter;
    gchar *selected;
    char target[256];
    (void)widget;
    dialog = gtk_file_chooser_dialog_new(
        "Choose Profile Picture",
        NULL,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
        NULL);
    gtk_window_set_transient_for(GTK_WINDOW(dialog),
                                 GTK_WINDOW(main_window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(dialog),
                            GTK_WIN_POS_CENTER_ON_PARENT);
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "PNG and JPEG images");
    gtk_file_filter_add_pattern(filter, "*.png");
    gtk_file_filter_add_pattern(filter, "*.jpg");
    gtk_file_filter_add_pattern(filter, "*.jpeg");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        selected = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        mkdir("data/avatars", 0755);
        snprintf(target, sizeof(target),
                 "data/avatars/signup-%d.jpg",
                 state->data->userCount + 1);
        {
            FILE *source = fopen(selected, "rb");
            FILE *destination =
                source != NULL ? fopen(target, "wb") : NULL;
            char buffer[4096];
            size_t count;
            if (source != NULL && destination != NULL) {
                while ((count = fread(buffer, 1, sizeof(buffer), source)) > 0)
                    fwrite(buffer, 1, count, destination);
                snprintf(state->signupAvatarPath,
                         sizeof(state->signupAvatarPath), "%s", target);
                {
                    GdkPixbuf *pixbuf =
                        gdk_pixbuf_new_from_file_at_scale(
                            target, 96, 96, TRUE, NULL);
                    if (pixbuf != NULL) {
                        gtk_image_set_from_pixbuf(
                            GTK_IMAGE(state->signupAvatarPreview),
                            pixbuf);
                        g_object_unref(pixbuf);
                    }
                }
            } else {
                show_message(state, GTK_MESSAGE_ERROR,
                             "Avatar error",
                             "PARKY could not copy the selected image.");
            }
            if (source != NULL) fclose(source);
            if (destination != NULL) fclose(destination);
        }
        g_free(selected);
    }
    gtk_widget_destroy(dialog);
}

static void create_account_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    const char *username =
        gtk_entry_get_text(GTK_ENTRY(state->signupUsername));
    const char *email =
        gtk_entry_get_text(GTK_ENTRY(state->signupEmail));
    const char *password =
        gtk_entry_get_text(GTK_ENTRY(state->signupPassword));
    User user;
    int i;
    (void)widget;

    if (username[0] == '\0' || strcmp(username, "Username") == 0 ||
        email[0] == '\0' || strcmp(email, "Email") == 0 ||
        password[0] == '\0' || strcmp(password, "Password") == 0) {
        show_message(state, GTK_MESSAGE_ERROR, "Missing sign-up fields",
                     "Please enter a username, email and password.");
        return;
    }
    if (!valid_auth_field(username) || !valid_email(email) ||
        !valid_auth_field(password)) {
        show_message(state, GTK_MESSAGE_ERROR, "Invalid sign-up details",
                     "Use at least 3 characters and no spaces or '|' separators.");
        return;
    }
    for (i = 0; i < state->data->userCount; ++i) {
        if (strcmp(state->data->users[i].name, username) == 0 ||
            strcmp(state->data->users[i].email, email) == 0) {
            show_message(state, GTK_MESSAGE_ERROR, "Duplicate account",
                         "This username or email is already registered.");
            return;
        }
    }
    memset(&user, 0, sizeof(user));
    user.id = state->data->userCount + 1;
    snprintf(user.name, sizeof(user.name), "%s", username);
    snprintf(user.email, sizeof(user.email), "%s", email);
    snprintf(user.password, sizeof(user.password), "%s", password);
    snprintf(user.avatarPath, sizeof(user.avatarPath), "%s",
             state->signupAvatarPath);
    user.role = USER_CITIZEN;
    if (citoyen_register(state->data, &user)) {
        show_message(state, GTK_MESSAGE_INFO, "Account created",
                     "Your PARKY citizen account was created. You can sign in now.");
        set_page(state, 0);
    } else {
        show_message(state, GTK_MESSAGE_ERROR, "Account creation failed",
                     "PARKY could not create the account. Please try again.");
    }
}

static void login_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    const char *email = gtk_entry_get_text(GTK_ENTRY(state->loginEmail));
    const char *password = gtk_entry_get_text(GTK_ENTRY(state->loginPassword));
    int i;
    int emailFound = 0;
    (void)widget;

    if (!valid_email(email) || !valid_auth_field(password)) {
        show_message(state, GTK_MESSAGE_ERROR, "Missing or invalid credentials",
                     email[0] == '\0' || password[0] == '\0'
                     ? "Enter both your email and password before signing in."
                     : "Credentials must be at least 3 characters and cannot contain spaces or '|' separators.");
        set_status(state, "Login validation failed");
        return;
    }

    for (i = 0; i < state->data->userCount; ++i) {
        User *user = &state->data->users[i];
        if (strcmp(user->email, email) == 0) {
            emailFound = 1;
            if (strcmp(user->password, password) != 0)
                continue;
            state->activeUserId = user->id;
            refresh_dashboard_identity(state);
            load_profile_image(state);
            if (user->role == USER_ADMIN) {
                refresh_admin_tables(state);
                set_page(state, 1);
            } else if (user->role == USER_AGENT) {
                refresh_agent_tables(state);
                set_page(state, 2);
            } else {
                refresh_citizen_tables(state);
                set_page(state, 3);
            }
            set_status(state, "Login successful");
            return;
        }
    }
    gtk_entry_set_text(GTK_ENTRY(state->loginPassword), "");
    if (!emailFound) {
        show_message(state, GTK_MESSAGE_ERROR, "Unknown account",
                     "No PARKY account uses this email. Check the address or register as a citizen.");
        set_status(state, "Unknown account");
    } else {
        show_message(state, GTK_MESSAGE_ERROR, "Incorrect password",
                     "The email is valid, but the password is not. Please try again.");
        set_status(state, "Incorrect password");
    }
}

static void resolve_complaint_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int complaintId =
        tree_selected_first_id(state, state->adminComplaints);
    (void)widget;
    if (complaintId == 0) {
        show_message(state, GTK_MESSAGE_WARNING, "No complaint selected",
                     "Select a complaint from the list first.");
        return;
    }
    if (admin_resolve_complaint(state->data, complaintId)) {
        show_message(state, GTK_MESSAGE_INFO, "Complaint resolved",
                     "The selected complaint was resolved.");
        refresh_admin_tables(state);
    } else {
        show_message(state, GTK_MESSAGE_ERROR, "Resolution failed",
                     "The selected complaint could not be resolved.");
    }
}

static void delete_user_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int userId =
        tree_selected_first_id(state, state->adminUsers);
    int i;
    int isAgent = 0;
    (void)widget;
    if (userId == 0) {
        show_message(state, GTK_MESSAGE_WARNING, "No user selected",
                     "Select a user or agent from the list first.");
        return;
    }
    for (i = 0; i < state->data->userCount; ++i) {
        if (state->data->users[i].id == userId) {
            if (state->data->users[i].role == USER_ADMIN) {
                show_message(state, GTK_MESSAGE_ERROR, "Protected account",
                             "The administrator account cannot be deleted.");
                return;
            }
            isAgent = state->data->users[i].role == USER_AGENT;
            break;
        }
    }
    if (confirm_destructive_action(state, "Delete selected user?",
                                   "This action cannot be undone.") &&
        (isAgent ? admin_delete_agent(state->data, userId) :
                   admin_delete_user(state->data, userId))) {
        show_message(state, GTK_MESSAGE_INFO, "User deleted",
                     "The selected user was deleted.");
        refresh_admin_tables(state);
    } else if (userId > 0) {
        show_message(state, GTK_MESSAGE_ERROR, "Deletion failed",
                     "The selected user could not be deleted.");
    }
}

static void delete_parking_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int parkingId =
        tree_selected_first_id(state, state->adminParkings);
    (void)widget;
    if (parkingId == 0) {
        show_message(state, GTK_MESSAGE_WARNING, "No parking selected",
                     "Select a parking from the list first.");
        return;
    }
    if (confirm_destructive_action(state, "Delete selected parking?",
                                   "This action cannot be undone.") &&
        admin_delete_parking(state->data, parkingId)) {
        show_message(state, GTK_MESSAGE_INFO, "Parking deleted",
                     "The selected parking was deleted.");
        refresh_admin_tables(state);
    } else {
        show_message(state, GTK_MESSAGE_ERROR, "Deletion failed",
                     "The selected parking could not be deleted.");
    }
}

static void mark_available_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int i;
    int parkingId = 0;
    (void)widget;
    for (i = 0; i < state->data->userCount; ++i) {
        if (state->data->users[i].id == state->activeUserId)
            parkingId = state->data->users[i].assignedParkingId;
    }
    for (i = 0; i < state->data->parkingCount; ++i) {
        if (state->data->parkings[i].id == parkingId) {
            if (agent_set_spot_status(state->data, parkingId,
                                      PARKING_AVAILABLE))
                show_message(state, GTK_MESSAGE_INFO, "Spot available",
                             "The parking is marked available.");
            refresh_agent_tables(state);
            refresh_citizen_tables(state);
            return;
        }
    }
    set_status(state, "Assigned parking not found");
}

static void mark_occupied_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int i;
    int parkingId = 0;
    (void)widget;
    for (i = 0; i < state->data->userCount; ++i) {
        if (state->data->users[i].id == state->activeUserId)
            parkingId = state->data->users[i].assignedParkingId;
    }
    for (i = 0; i < state->data->parkingCount; ++i) {
        if (state->data->parkings[i].id == parkingId) {
            if (agent_set_spot_status(state->data, parkingId,
                                      PARKING_OCCUPIED))
                show_message(state, GTK_MESSAGE_INFO, "Spot occupied",
                             "The parking is marked occupied.");
            refresh_agent_tables(state);
            refresh_citizen_tables(state);
            return;
        }
    }
    set_status(state, "Assigned parking not found");
}

static void add_service_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    Service service;
    const char *name;
    const char *description;
    const char *priceText;
    (void)widget;
    name = gtk_entry_get_text(GTK_ENTRY(state->agentServiceName));
    description = gtk_entry_get_text(GTK_ENTRY(state->agentServiceDescription));
    priceText = gtk_entry_get_text(GTK_ENTRY(state->agentServicePrice));
    if (name[0] == '\0' || description[0] == '\0' ||
        atoi(priceText) <= 0) {
        show_message(state, GTK_MESSAGE_ERROR, "Service validation",
                     "Enter a name, description and positive price.");
        return;
    }
    memset(&service, 0, sizeof(service));
    service.id = state->data->serviceCount + 1;
    snprintf(service.name, sizeof(service.name), "%s", name);
    snprintf(service.description, sizeof(service.description), "%s",
             description);
    service.price = (float)atof(priceText);
    service.status = 1;
    if (agent_add_service(state->data, &service)) {
        gtk_entry_set_text(GTK_ENTRY(state->agentServiceName), "");
        gtk_entry_set_text(GTK_ENTRY(state->agentServiceDescription), "");
        gtk_entry_set_text(GTK_ENTRY(state->agentServicePrice), "");
        show_message(state, GTK_MESSAGE_INFO, "Service added",
                     "The parking service was added.");
        refresh_agent_tables(state);
    }
}

static void send_agent_complaint_clicked(GtkWidget *widget,
                                         gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    const char *subject =
        gtk_entry_get_text(GTK_ENTRY(state->complaintSubject));
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(state->complaintText));
    GtkTextIter start, end;
    char *text;
    (void)widget;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    if (subject[0] == '\0') {
        show_message(state, GTK_MESSAGE_ERROR, "Empty complaint subject",
                     "Add a short subject describing the complaint.");
        g_free(text);
        return;
    }
    if (text == NULL || text[0] == '\0') {
        show_message(state, GTK_MESSAGE_ERROR, "Empty complaint",
                     "Describe the problem before sending the complaint.");
        g_free(text);
        return;
    }
    if (subject[0] == '\0' || text[0] == '\0') {
        show_message(state, GTK_MESSAGE_ERROR, "Complaint validation",
                     "Enter a subject and description.");
    } else if (agent_submit_complaint(state->data, state->activeUserId,
                                      subject, text)) {
        gtk_entry_set_text(GTK_ENTRY(state->complaintSubject), "");
        gtk_text_buffer_set_text(buffer, "", 0);
        show_message(state, GTK_MESSAGE_INFO, "Complaint sent",
                     "Your complaint was sent to the administrator.");
        refresh_admin_tables(state);
    }
    g_free(text);
}

static void approve_reservation_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int reservationId =
        tree_selected_first_id(state, state->agentReservations);
    (void)widget;
    if (reservationId == 0 && state->data->reservationCount > 0)
        reservationId = state->data->reservations[0].id;
    if (reservationId > 0 &&
        agent_resolve_reservation(state->data,
                                  reservationId, 1))
        set_status(state, "Selected reservation approved");
    else {
        show_message(state, GTK_MESSAGE_ERROR, "No reservation selected",
                     "Select a reservation in the table before approving it.");
        set_status(state, "No reservation selected");
    }
    refresh_agent_tables(state);
}

static void reject_reservation_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int reservationId;
    (void)widget;
    reservationId = tree_selected_first_id(state,
                                           state->agentReservations);
    if (reservationId == 0 && state->data->reservationCount > 0)
        reservationId = state->data->reservations[0].id;
    if (reservationId > 0) {
        if (confirm_destructive_action(state, "Reject the selected reservation?",
                                       "The citizen will be notified by status.") &&
            agent_resolve_reservation(state->data,
                                      reservationId, 0)) {
            show_message(state, GTK_MESSAGE_INFO,
                         "Reservation rejected",
                         "The reservation was rejected.");
            set_status(state, "Selected reservation rejected");
        }
    } else {
        show_message(state, GTK_MESSAGE_ERROR, "No reservation selected",
                     "Select a reservation in the table before rejecting it.");
        set_status(state, "No reservation selected");
    }
    refresh_agent_tables(state);
}

static void update_tariff(UiState *state)
{
    int hours = atoi(gtk_entry_get_text(GTK_ENTRY(state->citizenHours)));
    int serviceId = gtk_combo_box_get_active(GTK_COMBO_BOX(state->citizenService));
    char message[128];
    double total;
    if (serviceId >= 0) ++serviceId;
    else serviceId = 0;
    total = citoyen_compute_billing(state->data, 1, serviceId, hours);
    snprintf(message, sizeof(message), "Estimated tariff: %.2f TND", total);
    gtk_label_set_text(GTK_LABEL(state->tariff), message);
}

static void tariff_changed(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    update_tariff((UiState *)user_data);
}

static void search_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    Parking results[PARKY_MAX_PARKINGS];
    char message[128];
    int count;
    (void)widget;
    count = citoyen_filter_parkings(
        state->data,
        gtk_entry_get_text(GTK_ENTRY(state->citizenName)),
        gtk_entry_get_text(GTK_ENTRY(state->citizenAddress)),
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(state->citizenAvailable)),
        results, PARKY_MAX_PARKINGS);
    snprintf(message, sizeof(message), "%d parking(s) matched", count);
    set_status(state, message);
    refresh_citizen_tables(state);
}

static void reserve_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int parkingId = atoi(gtk_entry_get_text(GTK_ENTRY(state->citizenParking)));
    int spot = atoi(gtk_entry_get_text(GTK_ENTRY(state->citizenSpot)));
    int i;
    int found = 0;
    int full = 0;
    (void)widget;
    if (parkingId <= 0) {
        show_message(state, GTK_MESSAGE_ERROR, "No parking selected",
                     "Enter a valid parking ID or select a parking from the table.");
        set_status(state, "No parking selected");
        return;
    }
    for (i = 0; i < state->data->parkingCount; ++i) {
        if (state->data->parkings[i].id == parkingId) {
            found = 1;
            full = state->data->parkings[i].availableSpots <= 0;
            break;
        }
    }
    if (!found) {
        show_message(state, GTK_MESSAGE_ERROR, "Parking not found",
                     "The entered parking ID does not exist.");
        return;
    }
    if (full) {
        show_message(state, GTK_MESSAGE_ERROR, "Parking is full",
                     "This parking has no available spots. Choose another parking.");
        set_status(state, "Parking is full");
        return;
    }
    if (citoyen_reserve_spot(state->data, state->activeUserId, parkingId,
                             gtk_entry_get_text(GTK_ENTRY(state->citizenDate)),
                             gtk_entry_get_text(GTK_ENTRY(state->citizenTime)),
                             spot)) {
        show_message(state, GTK_MESSAGE_INFO, "Reservation submitted",
                     "Your parking spot reservation was submitted.");
        gtk_entry_set_text(GTK_ENTRY(state->citizenSpot), "");
        gtk_entry_set_text(GTK_ENTRY(state->citizenDate), "");
        gtk_entry_set_text(GTK_ENTRY(state->citizenTime), "");
        set_status(state, "Spot reservation submitted");
    } else {
        set_status(state, "Unable to reserve spot");
    }
    refresh_citizen_tables(state);
    refresh_agent_tables(state);
}

static void book_service_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int parkingId = atoi(gtk_entry_get_text(GTK_ENTRY(state->citizenParking)));
    int serviceId = gtk_combo_box_get_active(GTK_COMBO_BOX(state->citizenService));
    (void)widget;
    if (serviceId >= 0) ++serviceId;
    else serviceId = 0;
    if (citoyen_book_service(state->data, state->activeUserId,
                             parkingId, serviceId)) {
        show_message(state, GTK_MESSAGE_INFO, "Service booked",
                     "Your parking service was submitted.");
        set_status(state, "Service booking submitted");
    } else {
        set_status(state, "Unable to book service");
    }
    refresh_citizen_tables(state);
    refresh_agent_tables(state);
}

static void review_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int parkingId = atoi(gtk_entry_get_text(GTK_ENTRY(state->citizenParking)));
    int rating = gtk_spin_button_get_value_as_int(
        GTK_SPIN_BUTTON(state->reviewRating));
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(state->reviewComment));
    GtkTextIter start, end;
    char *comment;
    (void)widget;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    comment = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    if (parkingId <= 0) {
        show_message(state, GTK_MESSAGE_ERROR, "No parking selected",
                     "Enter the parking ID you want to review.");
        g_free(comment);
        return;
    }
    if (comment == NULL || comment[0] == '\0') {
        show_message(state, GTK_MESSAGE_ERROR, "Empty review",
                     "Write a short review before submitting it.");
        g_free(comment);
        return;
    }
    if (citoyen_submit_review(state->data, state->activeUserId,
                              parkingId, rating, comment)) {
        gtk_text_buffer_set_text(buffer, "", 0);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(state->reviewRating), 5);
        show_message(state, GTK_MESSAGE_INFO, "Review submitted",
                     "Thank you. Your review was saved.");
        set_status(state, "Review submitted");
    } else {
        set_status(state, "Unable to submit review");
    }
    refresh_citizen_tables(state);
    g_free(comment);
}

static void complaint_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    const char *subject =
        gtk_entry_get_text(GTK_ENTRY(state->complaintSubject));
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(state->complaintText));
    GtkTextIter start, end;
    char *text;
    (void)widget;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    if (citoyen_submit_complaint(state->data, state->activeUserId,
                                 subject, text)) {
        gtk_entry_set_text(GTK_ENTRY(state->complaintSubject), "");
        gtk_text_buffer_set_text(buffer, "", 0);
        show_message(state, GTK_MESSAGE_INFO, "Complaint submitted",
                     "Your complaint was saved.");
        set_status(state, "Complaint submitted");
    } else {
        set_status(state, "Unable to submit complaint");
    }
    refresh_admin_tables(state);
    g_free(text);
}

static GtkWidget *signup_view(UiState *state)
{
    GtkWidget *root = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    GtkWidget *card = rounded_card(520);
    GtkWidget *box = gtk_vbox_new(FALSE, 12);
    GtkWidget *title = gtk_label_new(NULL);
    GdkPixbuf *signupFallback =
        make_default_avatar("U");
    GtkWidget *avatar =
        gtk_image_new_from_pixbuf(signupFallback);
    g_object_unref(signupFallback);
    GtkWidget *avatarButton = make_stock_button(
        "Choose Profile Picture", GTK_STOCK_OPEN);
    GtkWidget *avatarRow = gtk_hbox_new(FALSE, 10);
    GtkWidget *avatarAlign = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    GtkWidget *createButton = compact_auth_button(
        "Create Account", GTK_STOCK_APPLY, "#10B981");
    GtkWidget *backLink = gtk_link_button_new_with_label(
        "", "Already have an account? Sign In");
    GtkSizeGroup *authFields = gtk_size_group_new(
        GTK_SIZE_GROUP_HORIZONTAL);

    state->signupAvatarPreview = avatar;
    state->signupAvatarPath[0] = '\0';
    GtkWidget *heroAlignment =
        gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    gtk_container_set_border_width(GTK_CONTAINER(box), 32);
    gtk_box_set_spacing(GTK_BOX(box), 14);
    gtk_widget_set_size_request(card, 520, -1);
    gtk_widget_set_size_request(createButton, 180, 40);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='x-large' weight='bold'>Create your PARKY account</span>");
    gtk_box_pack_start(GTK_BOX(box),
                       make_auth_row(GTK_STOCK_DIALOG_AUTHENTICATION,
                                     &state->signupUsername,
                                     authFields),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box),
                       make_auth_row(GTK_STOCK_EDIT,
                                     &state->signupEmail,
                                     authFields),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box),
                       make_auth_row(GTK_STOCK_DIALOG_AUTHENTICATION,
                                     &state->signupPassword,
                                     authFields),
                       FALSE, FALSE, 0);
    gtk_widget_set_size_request(state->signupUsername, 320, 38);
    gtk_widget_set_size_request(state->signupEmail, 320, 38);
    gtk_widget_set_size_request(state->signupPassword, 320, 38);
    gtk_entry_set_visibility(GTK_ENTRY(state->signupPassword), TRUE);
    set_entry_placeholder(state->signupUsername, "Username");
    set_entry_placeholder(state->signupEmail, "Email");
    set_entry_placeholder(state->signupPassword, "Password");
    gtk_widget_set_size_request(avatar, 48, 48);
    gtk_widget_set_size_request(avatarButton, 200, 34);
    style_button(avatarButton, "#1F5FBF", "#FFFFFF");

    gtk_container_add(GTK_CONTAINER(heroAlignment),
                      make_auth_hero());
    gtk_box_pack_start(GTK_BOX(box), heroAlignment,
                       FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(avatarRow), avatar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(avatarRow), avatarButton, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(avatarAlign), avatarRow);
    gtk_box_pack_start(GTK_BOX(box), avatarAlign, FALSE, FALSE, 8);
    style_button(createButton, "#287A4D", "#FFFFFF");
    {
        GtkWidget *createAlignment =
            gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
        gtk_container_add(GTK_CONTAINER(createAlignment), createButton);
        gtk_box_pack_start(GTK_BOX(box), createAlignment,
                           FALSE, FALSE, 8);
    }
    gtk_box_pack_start(GTK_BOX(box), backLink, FALSE, FALSE, 4);
    gtk_container_add(GTK_CONTAINER(card), box);
    gtk_container_add(GTK_CONTAINER(root), card);

    g_signal_connect(avatarButton, "clicked",
                     G_CALLBACK(choose_signup_avatar_clicked), state);
    g_signal_connect(createButton, "clicked",
                     G_CALLBACK(create_account_clicked), state);
    g_signal_connect(backLink, "clicked",
                     G_CALLBACK(back_to_login_clicked), state);
    return root;
}

static GtkWidget *login_view(UiState *state)
{
    GtkWidget *root = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    GtkWidget *card = rounded_card(520);
    GtkWidget *cardBox = gtk_vbox_new(FALSE, 12);
    GtkWidget *title = gtk_label_new(NULL);
    GtkWidget *loginBtn = gtk_button_new_with_label("Sign In");
    GdkColor normalBg;
    GdkColor hoverBg;
    GdkColor whiteFg;
    GtkWidget *signupLink = gtk_link_button_new_with_label(
        "", "Don't have an account? Sign Up");
    GtkSizeGroup *authFields = gtk_size_group_new(
        GTK_SIZE_GROUP_HORIZONTAL);
    gtk_container_set_border_width(GTK_CONTAINER(cardBox), 32);
    gtk_box_set_spacing(GTK_BOX(cardBox), 14);
    {
        GtkWidget *heroAlignment =
            gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
        gtk_container_add(GTK_CONTAINER(heroAlignment),
                          make_auth_hero());
        gtk_box_pack_start(GTK_BOX(cardBox), heroAlignment,
                           FALSE, FALSE, 4);
    }
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='x-large' weight='bold'>Welcome to PARKY</span>");
    gtk_misc_set_alignment(GTK_MISC(title), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(cardBox), title, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(cardBox),
                       make_auth_row(GTK_STOCK_EDIT,
                                     &state->loginEmail,
                                     authFields),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cardBox),
                       make_auth_row(GTK_STOCK_DIALOG_AUTHENTICATION,
                                     &state->loginPassword,
                                     authFields),
                       FALSE, FALSE, 0);
    gtk_widget_set_size_request(card, 520, -1);
    gtk_widget_set_size_request(state->loginEmail, 330, 36);
    gtk_widget_set_size_request(state->loginPassword, 330, 36);
    set_entry_placeholder(state->loginEmail, "Email");
    set_entry_placeholder(state->loginPassword, "Password");
    gtk_entry_set_visibility(GTK_ENTRY(state->loginPassword), TRUE);
    gtk_widget_set_size_request(loginBtn, 180, 40);
    gdk_color_parse("#1B2A4A", &normalBg);
    gdk_color_parse("#2B4478", &hoverBg);
    gdk_color_parse("#FFFFFF", &whiteFg);
    gtk_widget_modify_bg(loginBtn, GTK_STATE_NORMAL, &normalBg);
    gtk_widget_modify_bg(loginBtn, GTK_STATE_PRELIGHT, &hoverBg);
    gtk_widget_modify_fg(loginBtn, GTK_STATE_NORMAL, &whiteFg);
    gtk_widget_modify_fg(loginBtn, GTK_STATE_PRELIGHT, &whiteFg);
    if (GTK_IS_BIN(loginBtn)) {
        GtkWidget *loginLabel = gtk_bin_get_child(GTK_BIN(loginBtn));
        if (GTK_IS_LABEL(loginLabel)) {
            gtk_widget_modify_fg(loginLabel, GTK_STATE_NORMAL, &whiteFg);
            gtk_widget_modify_fg(loginLabel, GTK_STATE_PRELIGHT, &whiteFg);
        }
    }
    {
        GtkWidget *loginAlignment =
            gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
        gtk_container_add(GTK_CONTAINER(loginAlignment), loginBtn);
        gtk_box_pack_start(GTK_BOX(cardBox), loginAlignment,
                           FALSE, FALSE, 8);
    }
    gtk_box_pack_start(GTK_BOX(cardBox), signupLink, FALSE, FALSE, 4);
    gtk_container_add(GTK_CONTAINER(card), cardBox);
    gtk_container_add(GTK_CONTAINER(root), card);
    g_signal_connect(loginBtn, "clicked", G_CALLBACK(login_clicked), state);
    g_signal_connect(signupLink, "clicked",
                     G_CALLBACK(open_signup_clicked), state);
    gtk_widget_grab_focus(loginBtn);
    return root;
}

static int next_id_from_users(const ParkyData *data)
{
    int i, next = 1;
    for (i = 0; i < data->userCount; ++i)
        if (data->users[i].id >= next)
            next = data->users[i].id + 1;
    return next;
}

static int next_id_from_parkings(const ParkyData *data)
{
    int i, next = 1;
    for (i = 0; i < data->parkingCount; ++i)
        if (data->parkings[i].id >= next)
            next = data->parkings[i].id + 1;
    return next;
}

static GtkWidget *admin_entry_row(const char *iconStock,
                                  const char *placeholder,
                                  GtkWidget **entry)
{
    GtkWidget *box = gtk_hbox_new(FALSE, 8);
    GtkWidget *icon = gtk_image_new_from_stock(
        iconStock, GTK_ICON_SIZE_BUTTON);
    GtkWidget *text = gtk_entry_new();
    gtk_widget_set_size_request(icon, 20, 20);
    gtk_widget_set_size_request(text, 240, 34);
    gtk_entry_set_alignment(GTK_ENTRY(text), 0.5);
    set_entry_placeholder(text, placeholder);
    gtk_entry_set_visibility(GTK_ENTRY(text), TRUE);
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), text, FALSE, FALSE, 0);
    *entry = text;
    return box;
}

static void admin_refresh_clicked(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    refresh_admin_tables((UiState *)user_data);
    set_status((UiState *)user_data, "Dashboard statistics refreshed");
}

static void admin_add_user_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    const char *name =
        entry_placeholder_value(state->adminUserName);
    const char *email =
        entry_placeholder_value(state->adminUserEmail);
    const char *password =
        entry_placeholder_value(state->adminUserPassword);
    User user;
    UserRole role = gtk_combo_box_get_active(GTK_COMBO_BOX(state->adminUserRole)) == 1 ?
                   USER_AGENT : USER_CITIZEN;
    int active = gtk_combo_box_get_active(GTK_COMBO_BOX(state->adminUserParking));
    (void)widget;
    if (!valid_auth_field(name) || !valid_email(email) ||
        !valid_auth_field(password)) {
        show_message(state, GTK_MESSAGE_ERROR, "Invalid user details",
                     "Enter a valid name, email, and password.");
        return;
    }
    memset(&user, 0, sizeof(user));
    user.id = next_id_from_users(state->data);
    snprintf(user.name, sizeof(user.name), "%s", name);
    snprintf(user.email, sizeof(user.email), "%s", email);
    snprintf(user.password, sizeof(user.password), "%s", password);
    user.role = role;
    user.assignedParkingId = active > 0 && active - 1 < state->data->parkingCount ?
                             state->data->parkings[active - 1].id : 0;
    if ((role == USER_AGENT ? admin_add_agent(state->data, &user) :
                              admin_add_user(state->data, &user))) {
        show_message(state, GTK_MESSAGE_INFO, "User added",
                     "The selected account was added.");
        refresh_admin_tables(state);
    } else {
        show_message(state, GTK_MESSAGE_ERROR, "User add failed",
                     "PARKY could not add the account.");
    }
}

static void admin_edit_user_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int id = tree_selected_first_id(state, state->adminUsers);
    User user;
    int i, active;
    int targetAgent;
    int originalAgent;
    (void)widget;
    if (id == 0 || !admin_get_user(state->data, id, &user)) {
        show_message(state, GTK_MESSAGE_WARNING, "No user selected",
                     "Select a user or agent from the list first.");
        return;
    }
    targetAgent =
        gtk_combo_box_get_active(GTK_COMBO_BOX(state->adminUserRole)) == 1;
    originalAgent = user.role == USER_AGENT;
    snprintf(user.name, sizeof(user.name), "%s",
             entry_placeholder_value(state->adminUserName));
    snprintf(user.email, sizeof(user.email), "%s",
             entry_placeholder_value(state->adminUserEmail));
    snprintf(user.password, sizeof(user.password), "%s",
             entry_placeholder_value(state->adminUserPassword));
    active = gtk_combo_box_get_active(GTK_COMBO_BOX(state->adminUserParking));
    user.assignedParkingId = active > 0 && active - 1 < state->data->parkingCount ?
                             state->data->parkings[active - 1].id : 0;
    if (targetAgent)
        user.role = USER_AGENT;
    else
        user.role = USER_CITIZEN;
    i = targetAgent && originalAgent ?
        admin_update_agent(state->data, &user) :
        admin_update_user(state->data, &user);
    if (i) {
        show_message(state, GTK_MESSAGE_INFO, "User updated",
                     "The selected account was updated.");
        refresh_admin_tables(state);
    } else {
        show_message(state, GTK_MESSAGE_ERROR, "User update failed",
                     "PARKY could not update the account.");
    }
}

static void admin_assign_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int agentId = tree_selected_first_id(state, state->adminUsers);
    int active = gtk_combo_box_get_active(GTK_COMBO_BOX(state->adminUserParking));
    (void)widget;
    if (agentId == 0 || active <= 0) {
        show_message(state, GTK_MESSAGE_WARNING, "Assignment incomplete",
                     "Select an agent and a destination parking.");
        return;
    }
    if (admin_assign_agent(state->data, agentId,
                           state->data->parkings[active - 1].id)) {
        show_message(state, GTK_MESSAGE_INFO, "Agent assigned",
                     "The selected agent was assigned to the parking.");
        refresh_admin_tables(state);
    } else {
        show_message(state, GTK_MESSAGE_ERROR, "Assignment failed",
                     "Select an agent and assign a valid parking.");
    }
}

static void admin_add_parking_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    Parking parking;
    (void)widget;
    memset(&parking, 0, sizeof(parking));
    parking.id = next_id_from_parkings(state->data);
    snprintf(parking.name, sizeof(parking.name), "%s",
             entry_placeholder_value(state->adminParkingName));
    snprintf(parking.address, sizeof(parking.address), "%s",
             entry_placeholder_value(state->adminParkingAddress));
    parking.totalSpots =
        atoi(entry_placeholder_value(state->adminParkingTotal));
    parking.availableSpots =
        atoi(entry_placeholder_value(state->adminParkingAvailable));
    if (parking.name[0] == '\0' || parking.address[0] == '\0' ||
        parking.totalSpots <= 0 ||
        parking.availableSpots < 0 ||
        parking.availableSpots > parking.totalSpots) {
        show_message(state, GTK_MESSAGE_ERROR, "Invalid parking details",
                     "Enter a name, address, valid total spots, and available spots.");
        return;
    }
    if (admin_add_parking(state->data, &parking)) {
        show_message(state, GTK_MESSAGE_INFO, "Parking added",
                     "The parking was added successfully.");
        refresh_admin_tables(state);
    } else {
        show_message(state, GTK_MESSAGE_ERROR, "Parking add failed",
                     "PARKY could not add the parking.");
    }
}

static void admin_update_parking_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int id = tree_selected_first_id(state, state->adminParkings);
    Parking parking;
    int i;
    (void)widget;
    if (id == 0) {
        show_message(state, GTK_MESSAGE_WARNING, "No parking selected",
                     "Select a parking to update.");
        return;
    }
    memset(&parking, 0, sizeof(parking));
    parking.id = id;
    snprintf(parking.name, sizeof(parking.name), "%s",
             entry_placeholder_value(state->adminParkingName));
    snprintf(parking.address, sizeof(parking.address), "%s",
             entry_placeholder_value(state->adminParkingAddress));
    parking.totalSpots =
        atoi(entry_placeholder_value(state->adminParkingTotal));
    parking.availableSpots =
        atoi(entry_placeholder_value(state->adminParkingAvailable));
    if (parking.name[0] == '\0' || parking.address[0] == '\0' ||
        parking.totalSpots <= 0 ||
        parking.availableSpots < 0 ||
        parking.availableSpots > parking.totalSpots) {
        show_message(state, GTK_MESSAGE_ERROR, "Invalid parking details",
                     "Enter valid parking values.");
        return;
    }
    for (i = 0; i < state->data->parkingCount; ++i) {
        if (state->data->parkings[i].id == id)
            parking.status = state->data->parkings[i].status;
    }
    if (admin_update_parking(state->data, &parking)) {
        show_message(state, GTK_MESSAGE_INFO, "Parking updated",
                     "The selected parking was updated.");
        refresh_admin_tables(state);
    } else {
        show_message(state, GTK_MESSAGE_ERROR, "Parking update failed",
                     "PARKY could not update the parking.");
    }
}

static void admin_parking_row_selected(GtkTreeSelection *selection,
                                       gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *textId = NULL;
    int id;
    int i;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;
    gtk_tree_model_get(model, &iter, 0, &textId, -1);
    id = textId != NULL ? atoi(textId) : 0;
    g_free(textId);
    for (i = 0; i < state->data->parkingCount; ++i) {
        if (state->data->parkings[i].id == id) {
            char number[32];
            gtk_entry_set_text(GTK_ENTRY(state->adminParkingName),
                               state->data->parkings[i].name);
            gtk_entry_set_text(GTK_ENTRY(state->adminParkingAddress),
                               state->data->parkings[i].address);
            snprintf(number, sizeof(number), "%d",
                     state->data->parkings[i].totalSpots);
            gtk_entry_set_text(GTK_ENTRY(state->adminParkingTotal), number);
            snprintf(number, sizeof(number), "%d",
                     state->data->parkings[i].availableSpots);
            gtk_entry_set_text(GTK_ENTRY(state->adminParkingAvailable), number);
            return;
        }
    }
}

static void admin_complaint_row_selected(GtkTreeSelection *selection,
                                         gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *textId = NULL;
    int id;
    int i;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;
    gtk_tree_model_get(model, &iter, 0, &textId, -1);
    id = textId != NULL ? atoi(textId) : 0;
    g_free(textId);
    for (i = 0; i < state->data->complaintCount; ++i) {
        if (state->data->complaints[i].id == id) {
            GtkTextBuffer *buffer =
                gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->adminComplaintDetail));
            char detail[PARKY_TEXT_LENGTH + PARKY_NAME_LENGTH + 64];
            snprintf(detail, sizeof(detail), "Subject: %s\n\n%s",
                     state->data->complaints[i].subject,
                     state->data->complaints[i].description);
            gtk_text_buffer_set_text(buffer, detail, -1);
            return;
        }
    }
}

static void delete_complaint_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int id = tree_selected_first_id(state, state->adminComplaints);
    int i, index = -1;
    (void)widget;
    if (id == 0) {
        show_message(state, GTK_MESSAGE_WARNING, "No complaint selected",
                     "Select a complaint from the list first.");
        return;
    }
    if (!confirm_destructive_action(state, "Delete selected complaint?",
                                    "This action cannot be undone."))
        return;
    for (i = 0; i < state->data->complaintCount; ++i)
        if (state->data->complaints[i].id == id)
            index = i;
    if (index >= 0) {
        for (i = index; i < state->data->complaintCount - 1; ++i)
            state->data->complaints[i] = state->data->complaints[i + 1];
        --state->data->complaintCount;
        if (storage_save(state->data)) {
            show_message(state, GTK_MESSAGE_INFO, "Complaint deleted",
                         "The selected complaint was deleted.");
            refresh_admin_tables(state);
        }
    }
}

static GtkWidget *admin_view(UiState *state)
{
    GtkWidget *root = gtk_vbox_new(FALSE, 8);
    GtkWidget *tabs = gtk_notebook_new();
    GtkWidget *overview = gtk_vbox_new(FALSE, 8);
    GtkWidget *kpis = gtk_hbox_new(TRUE, 8);
    GtkWidget *refresh = make_icon_button(GTK_STOCK_REFRESH,
                                          "Refresh Stats");
    GtkWidget *occupancyHeader = gtk_hbox_new(FALSE, 8);
    GtkWidget *occupancyTitle = gtk_label_new(NULL);
    GtkListStore *occupancyStore;
    const char *occupancyTitles[] = {
        "Parking", "Capacity", "Occupied", "Revenue", "Agent"
    };
    GtkWidget *occupancyCard;
    GtkWidget *usersTab = gtk_hpaned_new();
    GtkWidget *usersPanel = gtk_vbox_new(FALSE, 8);
    GtkWidget *parkingsTab = gtk_hpaned_new();
    GtkWidget *parkingPanel = gtk_vbox_new(FALSE, 8);
    GtkWidget *complaintsTab = gtk_hpaned_new();
    GtkWidget *complaintPanel = gtk_vbox_new(FALSE, 8);
    GtkListStore *store;
    GtkWidget *tree;
    GtkTreeSelection *selection;
    GtkWidget *roleCombo = gtk_combo_box_new_text();
    GtkWidget *parkingCombo = gtk_combo_box_new_text();
    GtkWidget *addUser, *editUser, *deleteUser, *assignAgent;
    GtkWidget *addParking, *updateParking, *deleteParking;
    GtkWidget *resolveComplaint, *deleteComplaint;
    GtkWidget *detailScroller;
    const char *userTitles[] = {
        "ID", "Username", "Email", "Role", "Assigned Parking"
    };
    const char *parkingTitles[] = {
        "ID", "Name", "Address/Zone", "Total Spots",
        "Available", "Price/Hr", "Agent"
    };
    const char *complaintTitles[] = {"ID", "From", "Subject", "Status"};
    int i;

    gtk_container_set_border_width(GTK_CONTAINER(root), 8);
    gtk_box_pack_start(GTK_BOX(root), make_dashboard_header(state, "ADMIN"),
                       FALSE, FALSE, 0);
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(tabs), TRUE);

    state->adminTotalUsers = gtk_label_new(NULL);
    state->adminTotalParkings = gtk_label_new(NULL);
    state->adminParkingAgents = gtk_label_new(NULL);
    state->adminPendingComplaints = gtk_label_new(NULL);
    state->adminGlobalOccupancy = gtk_label_new(NULL);
    {
        GtkWidget *cards[5];
        const char *titles[5] = {
            "Total Users", "Total Parkings", "Parking Agents",
            "Pending Complaints", "Occupancy Rate"
        };
        GtkWidget *values[5];
        values[0] = state->adminTotalUsers;
        values[1] = state->adminTotalParkings;
        values[2] = state->adminParkingAgents;
        values[3] = state->adminPendingComplaints;
        values[4] = state->adminGlobalOccupancy;
        for (i = 0; i < 5; ++i) {
            GtkWidget *card = gtk_event_box_new();
            GtkWidget *cardBox = gtk_vbox_new(FALSE, 4);
            gtk_container_set_border_width(GTK_CONTAINER(card), 10);
            gtk_box_pack_start(GTK_BOX(cardBox),
                               gtk_label_new(titles[i]), FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(cardBox), values[i], FALSE, FALSE, 0);
            gtk_container_add(GTK_CONTAINER(card), cardBox);
            cards[i] = card;
        }
        for (i = 0; i < 5; ++i)
            gtk_box_pack_start(GTK_BOX(kpis), cards[i], TRUE, TRUE, 0);
    }

    style_button(refresh, "#1F5FBF", "#FFFFFF");
    gtk_box_pack_start(GTK_BOX(overview), kpis, FALSE, FALSE, 4);
    gtk_label_set_markup(GTK_LABEL(occupancyTitle),
                         "<span size='large' weight='bold'>"
                         "Parking Occupancy &amp; Revenue Summary"
                         "</span>");
    gtk_box_pack_start(GTK_BOX(occupancyHeader), occupancyTitle,
                       TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(occupancyHeader), refresh, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(overview), occupancyHeader,
                       FALSE, FALSE, 4);
    occupancyCard = make_tree_view("Occupancy Summary", &occupancyStore,
                                   occupancyTitles, 5);
    state->adminOccupancy = occupancyCard;
    gtk_widget_set_size_request(occupancyCard, -1, 220);
    gtk_box_pack_start(GTK_BOX(overview), occupancyCard, TRUE, TRUE, 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), overview,
                             gtk_label_new("Overview & Stats"));
    g_signal_connect(refresh, "clicked",
                     G_CALLBACK(admin_refresh_clicked), state);

    state->adminUsers = make_tree_view("Users & Agents", &store,
                                       userTitles, 5);
    gtk_widget_set_size_request(state->adminUsers, 600, 420);
    gtk_paned_pack1(GTK_PANED(usersTab), state->adminUsers, TRUE, FALSE);
    gtk_combo_box_append_text(GTK_COMBO_BOX(roleCombo), "Citizen");
    gtk_combo_box_append_text(GTK_COMBO_BOX(roleCombo), "Agent");
    gtk_combo_box_set_active(GTK_COMBO_BOX(roleCombo), 0);
    gtk_combo_box_append_text(GTK_COMBO_BOX(parkingCombo), "Unassigned");
    for (i = 0; i < state->data->parkingCount; ++i)
        gtk_combo_box_append_text(GTK_COMBO_BOX(parkingCombo),
                                  state->data->parkings[i].name);
    gtk_combo_box_set_active(GTK_COMBO_BOX(parkingCombo), 0);
    state->adminUserRole = roleCombo;
    state->adminUserParking = parkingCombo;
    usersPanel = gtk_vbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(usersPanel), 8);
    gtk_widget_set_size_request(usersPanel, 320, -1);
    gtk_box_pack_start(GTK_BOX(usersPanel),
                       gtk_label_new("Account Management"), FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(usersPanel),
                       admin_entry_row(GTK_STOCK_ORIENTATION_PORTRAIT,
                                       "Username", &state->adminUserName),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(usersPanel),
                       admin_entry_row(GTK_STOCK_NETWORK,
                                       "Email", &state->adminUserEmail),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(usersPanel),
                       admin_entry_row(GTK_STOCK_DIALOG_AUTHENTICATION,
                                       "Password", &state->adminUserPassword),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(usersPanel), roleCombo, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(usersPanel), parkingCombo, FALSE, FALSE, 2);

    addUser = make_icon_button(GTK_STOCK_ADD, "Add user or agent");
    editUser = make_icon_button(GTK_STOCK_EDIT, "Edit selected account");
    deleteUser = make_icon_button(GTK_STOCK_DELETE, "Delete selected account");
    assignAgent = make_icon_button(GTK_STOCK_APPLY,
                                   "Assign selected agent to parking");
    style_button(addUser, "#287A4D", "#FFFFFF");
    style_button(editUser, "#1F5FBF", "#FFFFFF");
    style_button(deleteUser, "#B33A3A", "#FFFFFF");
    style_button(assignAgent, "#F2A900", "#1B2A4A");
    gtk_box_pack_start(GTK_BOX(usersPanel), addUser, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(usersPanel), editUser, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(usersPanel), deleteUser, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(usersPanel), assignAgent, FALSE, FALSE, 2);
    gtk_paned_pack2(GTK_PANED(usersTab), usersPanel, FALSE, FALSE);
    gtk_paned_set_position(GTK_PANED(usersTab), 760);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), usersTab,
                             gtk_label_new("Users & Agents"));
    g_signal_connect(addUser, "clicked",
                     G_CALLBACK(admin_add_user_clicked), state);
    g_signal_connect(editUser, "clicked",
                     G_CALLBACK(admin_edit_user_clicked), state);
    g_signal_connect(deleteUser, "clicked",
                     G_CALLBACK(delete_user_clicked), state);
    g_signal_connect(assignAgent, "clicked",
                     G_CALLBACK(admin_assign_clicked), state);

    state->adminParkings = make_tree_view("Parkings", &store,
                                          parkingTitles, 7);
    gtk_widget_set_size_request(state->adminParkings, 700, 420);
    gtk_paned_pack1(GTK_PANED(parkingsTab), state->adminParkings,
                    TRUE, FALSE);
    parkingPanel = gtk_vbox_new(FALSE, 6);
    gtk_container_set_border_width(GTK_CONTAINER(parkingPanel), 8);
    gtk_widget_set_size_request(parkingPanel, 320, -1);
    gtk_box_pack_start(GTK_BOX(parkingPanel),
                       gtk_label_new("Parking Management"),
                       FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(parkingPanel),
                       admin_entry_row(GTK_STOCK_DND,
                                       "Name", &state->adminParkingName),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(parkingPanel),
                       admin_entry_row(GTK_STOCK_JUMP_TO,
                                       "Address/Zone",
                                       &state->adminParkingAddress),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(parkingPanel),
                       admin_entry_row(GTK_STOCK_ADD,
                                       "Total Spots",
                                       &state->adminParkingTotal),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(parkingPanel),
                       admin_entry_row(GTK_STOCK_APPLY,
                                       "Available",
                                       &state->adminParkingAvailable),
                       FALSE, FALSE, 2);
    addParking = make_icon_button(GTK_STOCK_ADD, "Add parking");
    updateParking = make_icon_button(GTK_STOCK_EDIT,
                                     "Update selected parking");
    deleteParking = make_icon_button(GTK_STOCK_DELETE,
                                     "Delete selected parking");
    style_button(addParking, "#287A4D", "#FFFFFF");
    style_button(updateParking, "#1F5FBF", "#FFFFFF");
    style_button(deleteParking, "#B33A3A", "#FFFFFF");
    gtk_box_pack_start(GTK_BOX(parkingPanel), addParking,
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(parkingPanel), updateParking,
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(parkingPanel), deleteParking,
                       FALSE, FALSE, 2);
    gtk_paned_pack2(GTK_PANED(parkingsTab), parkingPanel, FALSE, FALSE);
    gtk_paned_set_position(GTK_PANED(parkingsTab), 760);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), parkingsTab,
                             gtk_label_new("Parkings"));
    g_signal_connect(addParking, "clicked",
                     G_CALLBACK(admin_add_parking_clicked), state);
    g_signal_connect(updateParking, "clicked",
                     G_CALLBACK(admin_update_parking_clicked), state);
    g_signal_connect(deleteParking, "clicked",
                     G_CALLBACK(delete_parking_clicked), state);
    tree = gtk_bin_get_child(GTK_BIN(
        gtk_bin_get_child(GTK_BIN(state->adminParkings))));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(admin_parking_row_selected), state);

    state->adminComplaints = make_tree_view("Complaints", &store,
                                            complaintTitles, 4);
    gtk_widget_set_size_request(state->adminComplaints, 700, 420);
    gtk_paned_pack1(GTK_PANED(complaintsTab), state->adminComplaints,
                    TRUE, FALSE);
    complaintPanel = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(complaintPanel), 8);
    gtk_box_pack_start(GTK_BOX(complaintPanel),
                       gtk_label_new("Complaint Details"),
                       FALSE, FALSE, 4);
    detailScroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(detailScroller),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    state->adminComplaintDetail = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(state->adminComplaintDetail),
                                GTK_WRAP_WORD);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(state->adminComplaintDetail),
                               FALSE);
    gtk_widget_set_size_request(detailScroller, 280, 260);
    gtk_container_add(GTK_CONTAINER(detailScroller),
                      state->adminComplaintDetail);
    gtk_box_pack_start(GTK_BOX(complaintPanel), detailScroller,
                       TRUE, TRUE, 4);
    resolveComplaint = make_icon_button(GTK_STOCK_APPLY,
                                        "Resolve selected complaint");
    deleteComplaint = make_icon_button(GTK_STOCK_DELETE,
                                       "Delete selected complaint");
    style_button(resolveComplaint, "#287A4D", "#FFFFFF");
    style_button(deleteComplaint, "#B33A3A", "#FFFFFF");
    gtk_box_pack_start(GTK_BOX(complaintPanel), resolveComplaint,
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(complaintPanel), deleteComplaint,
                       FALSE, FALSE, 2);
    gtk_paned_pack2(GTK_PANED(complaintsTab), complaintPanel,
                    FALSE, FALSE);
    gtk_paned_set_position(GTK_PANED(complaintsTab), 760);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), complaintsTab,
                             gtk_label_new("Complaints"));
    g_signal_connect(resolveComplaint, "clicked",
                     G_CALLBACK(resolve_complaint_clicked), state);
    g_signal_connect(deleteComplaint, "clicked",
                     G_CALLBACK(delete_complaint_clicked), state);
    tree = gtk_bin_get_child(GTK_BIN(
        gtk_bin_get_child(GTK_BIN(state->adminComplaints))));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(admin_complaint_row_selected), state);

    gtk_box_pack_start(GTK_BOX(root), tabs, TRUE, TRUE, 0);
    refresh_admin_tables(state);
    return root;
}

static GtkWidget *agent_view(UiState *state)
{
    GtkWidget *box = gtk_vbox_new(FALSE, 10);
    GtkWidget *header = make_dashboard_header(state, "AGENT");
    GtkWidget *assigned = gtk_frame_new("Assigned Parking");
    GtkWidget *assignedBox = gtk_hbox_new(FALSE, 12);
    GtkWidget *parkingText = gtk_label_new("");
    GtkWidget *actions = gtk_hbox_new(TRUE, 8);
    GtkWidget *available = make_stock_button(
        "Mark Spot Available (+1)", GTK_STOCK_ADD);
    GtkWidget *occupied = make_stock_button(
        "Mark Spot Occupied (-1)", GTK_STOCK_DELETE);
    GtkWidget *approve = make_stock_button(
        "Approve Selected Reservation", GTK_STOCK_APPLY);
    GtkWidget *reject = make_stock_button(
        "Reject Selected Reservation", GTK_STOCK_CANCEL);
    GtkWidget *addService = make_stock_button(
        "Add Service", GTK_STOCK_ADD);
    GtkWidget *sendComplaint = make_stock_button(
        "Send Complaint", GTK_STOCK_DIALOG_AUTHENTICATION);
    GtkWidget *tables = gtk_hbox_new(TRUE, 8);
    GtkWidget *serviceForm = gtk_vbox_new(FALSE, 6);
    GtkWidget *complaintForm = gtk_vbox_new(FALSE, 6);
    GtkWidget *scroller;
    GtkListStore *store;
    const char *reservationTitles[] = {"ID", "Date", "Time", "Status"};
    const char *serviceTitles[] = {"ID", "Name", "Description", "Status"};
    char summary[512];
    int i;
    int parkingId = 0;

    gtk_container_set_border_width(GTK_CONTAINER(box), 12);
    gtk_box_pack_start(GTK_BOX(box), header, FALSE, FALSE, 4);

    for (i = 0; i < state->data->userCount; ++i) {
        if (state->data->users[i].id == state->activeUserId) {
            parkingId = state->data->users[i].assignedParkingId;
            break;
        }
    }
    snprintf(summary, sizeof(summary),
             "<b>No assigned parking</b>");
    for (i = 0; i < state->data->parkingCount; ++i) {
        if (state->data->parkings[i].id == parkingId) {
            snprintf(summary, sizeof(summary),
                     "<span size='large' weight='bold'>%s</span>\n"
                     "%s\n"
                     "<b>Available:</b> %d    "
                     "<b>Occupied:</b> %d",
                     state->data->parkings[i].name,
                     state->data->parkings[i].address,
                     state->data->parkings[i].availableSpots,
                     state->data->parkings[i].totalSpots -
                     state->data->parkings[i].availableSpots);
            break;
        }
    }
    gtk_label_set_markup(GTK_LABEL(parkingText), summary);
    gtk_box_pack_start(GTK_BOX(assignedBox), parkingText,
                       TRUE, TRUE, 12);
    gtk_container_add(GTK_CONTAINER(assigned), assignedBox);
    gtk_container_set_border_width(GTK_CONTAINER(assigned), 8);
    gtk_box_pack_start(GTK_BOX(box), assigned, FALSE, FALSE, 4);

    gtk_widget_set_size_request(available, -1, 40);
    gtk_widget_set_size_request(occupied, -1, 40);
    gtk_widget_set_size_request(approve, -1, 40);
    gtk_widget_set_size_request(reject, -1, 40);
    gtk_widget_set_size_request(addService, -1, 40);
    gtk_widget_set_size_request(sendComplaint, -1, 40);
    style_button(available, "#287A4D", "#FFFFFF");
    style_button(occupied, "#F2A900", "#1B2A4A");
    style_button(approve, "#287A4D", "#FFFFFF");
    style_button(reject, "#B33A3A", "#FFFFFF");
    style_button(addService, "#1F5FBF", "#FFFFFF");
    style_button(sendComplaint, "#B33A3A", "#FFFFFF");
    gtk_box_pack_start(GTK_BOX(actions), available, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(actions), occupied, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(actions), approve, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(actions), reject, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), actions, FALSE, FALSE, 4);

    state->agentReservations =
        make_tree_view("Reservations", &store, reservationTitles, 4);
    state->agentServices =
        make_tree_view("Parking Services", &store, serviceTitles, 4);
    gtk_box_pack_start(GTK_BOX(tables), state->agentReservations,
                       TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(tables), state->agentServices,
                       TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(box), tables, TRUE, TRUE, 4);

    gtk_box_pack_start(GTK_BOX(serviceForm),
                       make_labeled_entry("Service name",
                                          &state->agentServiceName),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(serviceForm),
                       make_labeled_entry("Description",
                                          &state->agentServiceDescription),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(serviceForm),
                       make_labeled_entry("Price (TND)",
                                          &state->agentServicePrice),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(serviceForm), addService,
                       FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(complaintForm),
                       make_labeled_entry("Complaint subject",
                                          &state->complaintSubject),
                       FALSE, FALSE, 2);
    state->complaintText = gtk_text_view_new();
    scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroller), state->complaintText);
    gtk_box_pack_start(GTK_BOX(complaintForm), scroller,
                       TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(complaintForm), sendComplaint,
                       FALSE, FALSE, 4);

    {
        GtkWidget *forms = gtk_hbox_new(TRUE, 8);
        gtk_box_pack_start(GTK_BOX(forms), serviceForm, TRUE, TRUE, 4);
        gtk_box_pack_start(GTK_BOX(forms), complaintForm, TRUE, TRUE, 4);
        gtk_box_pack_start(GTK_BOX(box), forms, TRUE, TRUE, 4);
    }

    g_signal_connect(available, "clicked",
                     G_CALLBACK(mark_available_clicked), state);
    g_signal_connect(occupied, "clicked",
                     G_CALLBACK(mark_occupied_clicked), state);
    g_signal_connect(approve, "clicked",
                     G_CALLBACK(approve_reservation_clicked), state);
    g_signal_connect(reject, "clicked",
                     G_CALLBACK(reject_reservation_clicked), state);
    g_signal_connect(addService, "clicked",
                     G_CALLBACK(add_service_clicked), state);
    g_signal_connect(sendComplaint, "clicked",
                     G_CALLBACK(send_agent_complaint_clicked), state);
    refresh_agent_tables(state);
    return box;
}

static GtkWidget *citizen_view(UiState *state)
{
    GtkWidget *box = gtk_vbox_new(FALSE, 8);
    GtkWidget *inner = gtk_notebook_new();
    GtkWidget *findTab = gtk_vbox_new(FALSE, 8);
    GtkWidget *filters = gtk_hbox_new(FALSE, 8);
    GtkWidget *bookingSplit = gtk_hpaned_new();
    GtkWidget *parkingSide = gtk_vbox_new(FALSE, 6);
    GtkWidget *bookingSide = gtk_vbox_new(FALSE, 6);
    GtkWidget *reservationsTab = gtk_vbox_new(FALSE, 8);
    GtkWidget *feedbackTab = gtk_hbox_new(TRUE, 8);
    GtkWidget *profileTab = gtk_vbox_new(FALSE, 12);
    GtkWidget *reviewSide = gtk_vbox_new(FALSE, 8);
    GtkWidget *complaintSide = gtk_vbox_new(FALSE, 8);
    GtkWidget *search = make_stock_button("Search", GTK_STOCK_FIND);
    GtkWidget *reserve = make_stock_button(
        "Reserve spot", GTK_STOCK_ADD);
    GtkWidget *book = make_stock_button(
        "Book service", GTK_STOCK_APPLY);
    GtkWidget *review = make_stock_button(
        "Submit review", GTK_STOCK_EDIT);
    GtkWidget *complaint = make_stock_button(
        "Submit complaint", GTK_STOCK_DIALOG_AUTHENTICATION);
    GtkAdjustment *adjustment;
    GtkWidget *scroller;
    GtkListStore *store;
    const char *parkingTitles[] = {"ID", "Name", "Address", "Available"};
    const char *reservationTitles[] = {"ID", "Date", "Time", "Status"};
    const char *reviewTitles[] = {"Rating", "Comment"};

    gtk_container_set_border_width(GTK_CONTAINER(box), 12);
    style_button(search, "#F2A900", "#1B2A4A");
    style_button(reserve, "#287A4D", "#FFFFFF");
    style_button(book, "#287A4D", "#FFFFFF");
    style_button(review, "#1F5FBF", "#FFFFFF");
    style_button(complaint, "#B33A3A", "#FFFFFF");
    gtk_box_pack_start(GTK_BOX(box),
                       make_dashboard_header(state, "CITIZEN"),
                       FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(filters),
                       make_labeled_entry("Parking name",
                                          &state->citizenName),
                       TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(filters),
                       make_labeled_entry("Address",
                                          &state->citizenAddress),
                       TRUE, TRUE, 2);
    state->citizenAvailable = gtk_check_button_new_with_label(
        "Only show available parkings");
    gtk_box_pack_start(GTK_BOX(filters), state->citizenAvailable,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(filters), search, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(findTab), filters, FALSE, FALSE, 4);

    state->citizenParkings =
        make_tree_view("Available Parkings", &store, parkingTitles, 4);
    gtk_box_pack_start(GTK_BOX(parkingSide), state->citizenParkings,
                       TRUE, TRUE, 4);

    gtk_box_pack_start(GTK_BOX(bookingSide),
                       make_labeled_entry("Parking ID",
                                          &state->citizenParking),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(bookingSide),
                       make_labeled_entry("Spot number",
                                          &state->citizenSpot),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(bookingSide),
                       make_labeled_entry("Date",
                                          &state->citizenDate),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(bookingSide),
                       make_labeled_entry("Time",
                                          &state->citizenTime),
                       FALSE, FALSE, 2);

    state->citizenService = gtk_combo_box_new_text();
    gtk_widget_set_size_request(state->citizenService, -1, 32);
    gtk_combo_box_append_text(GTK_COMBO_BOX(state->citizenService),
                              "No extra service");
    gtk_combo_box_append_text(GTK_COMBO_BOX(state->citizenService),
                              "Car Wash");
    gtk_combo_box_set_active(GTK_COMBO_BOX(state->citizenService), 0);
    gtk_box_pack_start(GTK_BOX(bookingSide),
                       make_labeled_entry("Service",
                                          &state->citizenHours),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(bookingSide), state->citizenService,
                       FALSE, FALSE, 2);
    gtk_entry_set_text(GTK_ENTRY(state->citizenHours), "1");

    state->tariff = gtk_label_new("Estimated tariff: 2.00 TND");
    gtk_box_pack_start(GTK_BOX(bookingSide), state->tariff,
                       FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(bookingSide), reserve, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(bookingSide), book, FALSE, FALSE, 4);

    gtk_paned_pack1(GTK_PANED(bookingSplit), parkingSide,
                    TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(bookingSplit), bookingSide,
                    FALSE, FALSE);
    gtk_paned_set_position(GTK_PANED(bookingSplit), 620);
    gtk_box_pack_start(GTK_BOX(findTab), bookingSplit, TRUE, TRUE, 4);

    state->citizenReservations =
        make_tree_view("My Reservations", &store,
                       reservationTitles, 4);
    gtk_box_pack_start(GTK_BOX(reservationsTab),
                       state->citizenReservations, TRUE, TRUE, 8);

    adjustment = GTK_ADJUSTMENT(
        gtk_adjustment_new(5, 1, 5, 1, 1, 0));
    state->reviewRating = gtk_spin_button_new(
        GTK_ADJUSTMENT(adjustment), 1, 0);
    state->reviewComment = gtk_text_view_new();
    gtk_box_pack_start(GTK_BOX(reviewSide),
                       make_labeled_entry("Parking ID",
                                          &state->citizenParking),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(reviewSide),
                       gtk_label_new("Rating"), FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(reviewSide),
                       state->reviewRating, FALSE, FALSE, 2);
    scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroller), state->reviewComment);
    gtk_box_pack_start(GTK_BOX(reviewSide), scroller, TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(reviewSide), review, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(complaintSide),
                       make_labeled_entry("Complaint subject",
                                          &state->complaintSubject),
                       FALSE, FALSE, 2);
    state->complaintText = gtk_text_view_new();
    scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroller), state->complaintText);
    gtk_box_pack_start(GTK_BOX(complaintSide), scroller,
                       TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(complaintSide), complaint,
                       FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(feedbackTab), reviewSide, TRUE, TRUE, 8);
    gtk_box_pack_start(GTK_BOX(feedbackTab), complaintSide, TRUE, TRUE, 8);

    state->citizenReviews =
        make_tree_view("Reviews", &store, reviewTitles, 2);
    gtk_box_pack_start(GTK_BOX(feedbackTab), state->citizenReviews,
                       TRUE, TRUE, 8);

    {
        GtkWidget *profileCard = gtk_frame_new("Citizen Profile");
        GtkWidget *profileBox = gtk_vbox_new(FALSE, 8);
        GtkWidget *photoBox = gtk_hbox_new(FALSE, 12);
        GtkWidget *photoButton = make_stock_button(
            "Choose Profile Photo", GTK_STOCK_OPEN);
        GtkWidget *profileName = gtk_label_new(NULL);
        GtkWidget *profileEmail = gtk_label_new(NULL);
        GtkWidget *profileRole = gtk_label_new(NULL);
        char profileText[256];
        int i;
        const char *name = "Unknown";
        const char *email = "Unknown";

        for (i = 0; i < state->data->userCount; ++i) {
            if (state->data->users[i].id == state->activeUserId) {
                name = state->data->users[i].name;
                email = state->data->users[i].email;
                break;
            }
        }
        snprintf(profileText, sizeof(profileText),
                 "<span size='x-large' weight='bold'>%s</span>", name);
        gtk_label_set_markup(GTK_LABEL(profileName), profileText);
        snprintf(profileText, sizeof(profileText),
                 "<span>Email: %s</span>", email);
        gtk_label_set_markup(GTK_LABEL(profileEmail), profileText);
        gtk_label_set_markup(GTK_LABEL(profileRole),
            "<span background='#287A4D' foreground='white' "
            "weight='bold'>  CITIZEN  </span>");
        state->profileImage = gtk_image_new_from_stock(
            GTK_STOCK_DIALOG_AUTHENTICATION, GTK_ICON_SIZE_DIALOG);
        gtk_widget_set_size_request(state->profileImage, 96, 96);
        load_profile_image(state);
        gtk_box_pack_start(GTK_BOX(photoBox), state->profileImage,
                           FALSE, FALSE, 8);
        gtk_box_pack_start(GTK_BOX(profileBox), photoBox,
                           FALSE, FALSE, 8);
        gtk_box_pack_start(GTK_BOX(profileBox), profileName,
                           FALSE, FALSE, 8);
        gtk_box_pack_start(GTK_BOX(profileBox), profileEmail,
                           FALSE, FALSE, 4);
        gtk_box_pack_start(GTK_BOX(profileBox), profileRole,
                           FALSE, FALSE, 4);
        style_button(photoButton, "#1F5FBF", "#FFFFFF");
        gtk_box_pack_start(GTK_BOX(profileBox), photoButton,
                           FALSE, FALSE, 8);
        g_signal_connect(photoButton, "clicked",
                         G_CALLBACK(choose_profile_photo_clicked), state);
        gtk_container_set_border_width(GTK_CONTAINER(profileCard), 12);
        gtk_container_add(GTK_CONTAINER(profileCard), profileBox);
        gtk_box_pack_start(GTK_BOX(profileTab), profileCard,
                           FALSE, FALSE, 20);
    }

    gtk_notebook_append_page(GTK_NOTEBOOK(inner), findTab,
                             make_stock_tab("Find & Book Parking",
                                            GTK_STOCK_FIND));
    gtk_notebook_append_page(GTK_NOTEBOOK(inner), reservationsTab,
                             make_stock_tab("My Reservations & Bills",
                                            GTK_STOCK_APPLY));
    gtk_notebook_append_page(GTK_NOTEBOOK(inner), feedbackTab,
                             make_stock_tab("Reviews & Complaints",
                                            GTK_STOCK_EDIT));
    gtk_notebook_append_page(GTK_NOTEBOOK(inner), profileTab,
                             make_stock_tab("My Profile",
                                            GTK_STOCK_DIALOG_AUTHENTICATION));
    gtk_box_pack_start(GTK_BOX(box), inner, TRUE, TRUE, 4);

    g_signal_connect(search, "clicked", G_CALLBACK(search_clicked), state);
    g_signal_connect(reserve, "clicked", G_CALLBACK(reserve_clicked), state);
    g_signal_connect(book, "clicked",
                     G_CALLBACK(book_service_clicked), state);
    g_signal_connect(review, "clicked", G_CALLBACK(review_clicked), state);
    g_signal_connect(complaint, "clicked",
                     G_CALLBACK(complaint_clicked), state);
    g_signal_connect(state->citizenHours, "changed",
                     G_CALLBACK(tariff_changed), state);
    g_signal_connect(state->citizenService, "changed",
                     G_CALLBACK(tariff_changed), state);
    update_tariff(state);
    refresh_citizen_tables(state);
    return box;
}

int ui_run(ParkyData *data, int argc, char **argv)
{
    UiState state;
    GtkWidget *window;
    memset(&state, 0, sizeof(state));
    state.data = data;

    gtk_init(&argc, &argv);
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    main_window = window;
    gtk_window_set_title(GTK_WINDOW(window), "Parky");
    gtk_window_set_default_size(GTK_WINDOW(window), 1180, 740);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 8);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    state.notebook = gtk_notebook_new();
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(state.notebook), FALSE);
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(state.notebook), TRUE);
    gtk_notebook_append_page(GTK_NOTEBOOK(state.notebook),
                             login_view(&state),
                             make_stock_tab("Login",
                                            GTK_STOCK_DIALOG_AUTHENTICATION));
    gtk_notebook_append_page(GTK_NOTEBOOK(state.notebook),
                             admin_view(&state),
                             make_stock_tab("Admin",
                                            GTK_STOCK_DIALOG_AUTHENTICATION));
    gtk_notebook_append_page(GTK_NOTEBOOK(state.notebook),
                             agent_view(&state),
                             make_stock_tab("Agent", GTK_STOCK_EDIT));
    gtk_notebook_append_page(GTK_NOTEBOOK(state.notebook),
                             citizen_view(&state),
                             make_stock_tab("Citizen",
                                            GTK_STOCK_DIALOG_AUTHENTICATION));
    gtk_notebook_append_page(GTK_NOTEBOOK(state.notebook),
                             signup_view(&state),
                             make_stock_tab("Sign Up",
                                            GTK_STOCK_ADD));

    state.status = gtk_statusbar_new();
    {
        GtkWidget *root = gtk_vbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(root), state.notebook, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(root), state.status, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(window), root);
        state.root = root;
    }

    apply_light_theme(window);
    gtk_widget_show_all(window);
    set_status(&state, "Parky ready");
    gtk_main();
    return 0;
}

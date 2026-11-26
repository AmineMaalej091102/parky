#include "ui.h"

#include <gtk/gtk.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    GtkWidget *adminAgents;
    GtkWidget *adminParkings;
    GtkWidget *adminComplaints;
    GtkWidget *adminOccupancy;
    GtkWidget *adminTotalUsers;
    GtkWidget *adminTotalParkings;
    GtkWidget *adminParkingAgents;
    GtkWidget *adminPendingComplaints;
    GtkWidget *adminGlobalOccupancy;
    GtkWidget *adminCitizenName;
    GtkWidget *adminCitizenEmail;
    GtkWidget *adminCitizenPassword;
    GtkWidget *adminAgentName;
    GtkWidget *adminAgentEmail;
    GtkWidget *adminAgentPassword;
    GtkWidget *adminAgentParking;
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
    GtkWidget *status;
    GtkWidget *citizenName;
    GtkWidget *citizenAddress;
    GtkWidget *citizenAvailable;
    GtkWidget *citizenSearchStatus; /* "N parking(s) matched" label in filter bar */
    GtkWidget *citizenParking;       /* hidden GtkEntry carrying selected parking ID */
    GtkWidget *citizenSelectedLabel; /* "Selected: <name>" label in booking card */
    GtkWidget *citizenSpot;
    GtkWidget *citizenDate;          /* GtkLabel showing selected date YYYY-MM-DD */
    GtkWidget *citizenTime;          /* unused; time read from spin buttons below */
    GtkWidget *citizenHourSpin;      /* GtkSpinButton 0-23 */
    GtkWidget *citizenMinuteSpin;    /* GtkSpinButton 0-55 step 5 */
    GtkWidget *citizenService;
    GtkWidget *citizenHours;         /* hidden GtkEntry, always "1", drives tariff */
    GtkWidget *tariff;
    GtkWidget *reviewRating;
    GtkWidget *reviewComment;
    GtkWidget *reviewParkingCombo;   /* parking dropdown in review form */
    GtkWidget *complaintSubject;
    GtkWidget *complaintText;
    GtkWidget *citizenCancelBtn;     /* Cancel Reservation button in Tab 2 */
    guint      citizenRefreshTimerId;
    GtkWidget *agentServiceName;
    GtkWidget *agentServiceDescription;
    GtkWidget *agentServicePrice;
    GtkWidget *agentParkingHeroTitle;
    GtkWidget *agentParkingRate;
    GtkWidget *agentSpotsAvailable;
    GtkWidget *agentSpotsOccupied;
    GtkWidget *agentSpotsTotal;
    GtkWidget *agentSpotPlusBtn;
    GtkWidget *agentSpotMinusBtn;
    GtkWidget *agentComplaintSubject;
    GtkWidget *agentComplaintText;
    guint agentRefreshTimerId;
    guint adminRefreshTimerId;
    GtkWidget *root;
    GtkWidget *profileImage;
} UiState;

static GtkWidget *main_window = NULL;

static void set_status(UiState *state, const char *message);
static void set_page(UiState *state, int page);
static void refresh_dashboard_identity(UiState *state);
static void start_agent_realtime_timer(UiState *state);
static void stop_agent_realtime_timer(UiState *state);
static void start_admin_realtime_timer(UiState *state);
static void stop_admin_realtime_timer(UiState *state);
static void start_citizen_realtime_timer(UiState *state);
static void stop_citizen_realtime_timer(UiState *state);
static GdkPixbuf *make_parky_logo(void);
static void set_entry_placeholder(GtkWidget *entry, const char *text);

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
    stop_agent_realtime_timer(state);
    stop_admin_realtime_timer(state);
    stop_citizen_realtime_timer(state);
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

static GtkWidget *account_entry_row(const char *iconStock,
                                    GtkWidget **entry)
{
    GtkWidget *box = gtk_hbox_new(FALSE, 8);
    GtkWidget *icon = gtk_image_new_from_stock(
        iconStock, GTK_ICON_SIZE_BUTTON);
    GtkWidget *text = gtk_entry_new();
    gtk_widget_set_size_request(icon, 20, 20);
    gtk_widget_set_size_request(text, 240, 34);
    gtk_entry_set_alignment(GTK_ENTRY(text), 0.5);
    gtk_entry_set_visibility(GTK_ENTRY(text), TRUE);
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), text, FALSE, FALSE, 0);
    *entry = text;
    return box;
}

static GtkWidget *make_auth_row(const char *iconStock,
                                const char *placeholder,
                                GtkWidget **entry,
                                GtkSizeGroup *sizeGroup)
{
    GtkWidget *row = account_entry_row(iconStock, entry);
    GtkWidget *text = *entry;
    gtk_widget_set_size_request(text, 320, 38);
    if (sizeGroup != NULL)
        gtk_size_group_add_widget(sizeGroup, text);
    set_entry_placeholder(text, placeholder);
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
    gdk_color_parse("#C5D3E6", &background);
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
    __attribute__((unused));
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
    else if (strcmp(role, "AGENT") == 0) state->agentIdentity = NULL;
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
    /* For CITIZEN: show only badge + sign-out (no avatar or name text) */
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

static GtkWidget *make_tree_view_markup(const char *frameTitle,
                                        GtkListStore **store,
                                        const char **titles, int columnCount,
                                        int markupColumn)
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
            titles[i], renderer, i == markupColumn ? "markup" : "text", i, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
        gtk_tree_view_column_set_sizing(column,
                                        GTK_TREE_VIEW_COLUMN_AUTOSIZE);
        gtk_tree_view_column_set_expand(column, TRUE);
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

static GtkWidget *make_tree_view(const char *frameTitle,
                                 GtkListStore **store,
                                 const char **titles, int columnCount)
{
    return make_tree_view_markup(frameTitle, store, titles, columnCount, -1);
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

/* Re-select the row with the given ID (column 0) in a tree view, if present.
   Used by refresh_admin_tables() to keep the active selection across reloads. */
static void tree_select_id(GtkWidget *treeWidget, int id)
{
    GtkWidget *frameChild;
    GtkWidget *scroller;
    GtkWidget *tree;
    GtkListStore *store;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    gboolean valid;
    gchar *text = NULL;

    frameChild = gtk_bin_get_child(GTK_BIN(treeWidget));
    scroller = GTK_IS_SCROLLED_WINDOW(frameChild) ? frameChild : frameChild;
    tree = gtk_bin_get_child(GTK_BIN(scroller));
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(tree)));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    while (valid) {
        text = NULL;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 0, &text, -1);
        if (text != NULL && atoi(text) == id) {
            gtk_tree_selection_select_iter(selection, &iter);
            g_free(text);
            return;
        }
        g_free(text);
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }
}

static GtkWidget *admin_user_tree(UiState *state, UserRole role)
{
    return role == USER_AGENT ? state->adminAgents : state->adminUsers;
}

static GtkWidget *admin_user_name(UiState *state, UserRole role)
{
    return role == USER_AGENT ? state->adminAgentName :
                                state->adminCitizenName;
}

static GtkWidget *admin_user_email(UiState *state, UserRole role)
{
    return role == USER_AGENT ? state->adminAgentEmail :
                                state->adminCitizenEmail;
}

static GtkWidget *admin_user_password(UiState *state, UserRole role)
{
    return role == USER_AGENT ? state->adminAgentPassword :
                                state->adminCitizenPassword;
}

static UserRole admin_action_role(GtkWidget *widget)
{
    gpointer value = g_object_get_data(G_OBJECT(widget), "parky-role");
    return value != NULL ? (UserRole)GPOINTER_TO_INT(value) :
                           USER_CITIZEN;
}

static void set_admin_action_role(GtkWidget *widget, UserRole role)
{
    g_object_set_data(G_OBJECT(widget), "parky-role",
                      GINT_TO_POINTER((int)role));
}

static void refresh_admin_tables(UiState *state)
{
    GtkListStore *store;
    GtkListStore *agentStore;
    GtkTreeIter iter;
    char value[128];
    int i;
    int agentCount = 0;
    int pending = 0;
    int usersSelId = 0;
    int agentsSelId = 0;
    int parkingsSelId = 0;
    int complaintsSelId = 0;

    /* Remember the active row of each table so a timed reload keeps the
       selection instead of jumping back to the first row. */
    usersSelId = tree_selected_first_id(state, state->adminUsers);
    agentsSelId = tree_selected_first_id(state, state->adminAgents);
    parkingsSelId = tree_selected_first_id(state, state->adminParkings);
    complaintsSelId =
        tree_selected_first_id(state, state->adminComplaints);

    store = tree_store(state, state->adminUsers);
    agentStore = tree_store(state, state->adminAgents);
    gtk_list_store_clear(store);
    gtk_list_store_clear(agentStore);
    for (i = 0; i < state->data->userCount; ++i) {
        char assigned[PARKY_NAME_LENGTH];
        GtkListStore *targetStore;
        if (state->data->users[i].role == USER_ADMIN)
            continue;
        targetStore = state->data->users[i].role == USER_AGENT ?
                      agentStore : store;
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
        gtk_list_store_append(targetStore, &iter);
        snprintf(value, sizeof(value), "%d", state->data->users[i].id);
        if (state->data->users[i].role == USER_AGENT) {
            gtk_list_store_set(targetStore, &iter, 0, value, 1,
                               state->data->users[i].name, 2,
                               state->data->users[i].email, 3,
                               assigned, -1);
        } else {
            gtk_list_store_set(targetStore, &iter, 0, value, 1,
                               state->data->users[i].name, 2,
                               state->data->users[i].email, -1);
        }
    }
    tree_select_id(state->adminUsers, usersSelId);
    tree_select_id(state->adminAgents, agentsSelId);

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
    tree_select_id(state->adminParkings, parkingsSelId);

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
    tree_select_id(state->adminComplaints, complaintsSelId);

    {
        char markup[128];
        int totalSpots = 0;
        int occupiedSpots = 0;
        double globalRate;

        snprintf(markup, sizeof(markup),
                 "<span foreground='white' size='xx-large'"
                 " weight='bold'>%d</span>",
                 state->data->userCount);
        gtk_label_set_markup(GTK_LABEL(state->adminTotalUsers), markup);

        snprintf(markup, sizeof(markup),
                 "<span foreground='white' size='xx-large'"
                 " weight='bold'>%d</span>",
                 state->data->parkingCount);
        gtk_label_set_markup(GTK_LABEL(state->adminTotalParkings), markup);

        snprintf(markup, sizeof(markup),
                 "<span foreground='white' size='xx-large'"
                 " weight='bold'>%d</span>",
                 agentCount);
        gtk_label_set_markup(GTK_LABEL(state->adminParkingAgents), markup);

        snprintf(markup, sizeof(markup),
                 "<span foreground='white' size='xx-large'"
                 " weight='bold'>%d</span>",
                 pending);
        gtk_label_set_markup(GTK_LABEL(state->adminPendingComplaints), markup);

        if (state->adminGlobalOccupancy != NULL) {
            for (i = 0; i < state->data->parkingCount; ++i) {
                totalSpots += state->data->parkings[i].totalSpots;
                occupiedSpots += state->data->parkings[i].totalSpots -
                                 state->data->parkings[i].availableSpots;
            }
            globalRate = totalSpots > 0 ?
                         ((double)occupiedSpots / (double)totalSpots) * 100.0 :
                         0.0;
            snprintf(markup, sizeof(markup),
                     "<span foreground='white' size='xx-large'"
                     " weight='bold'>%.1f%%</span>",
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

static void format_display_text(char *dst, size_t size, const char *src)
{
    size_t i = 0;
    if (size == 0)
        return;
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1 < size) {
        dst[i] = (src[i] == '_') ? ' ' : src[i];
        ++i;
    }
    dst[i] = '\0';
}

static int sanitize_storage_text(char *dst, size_t size, const char *src)
{
    size_t len;
    size_t start = 0;
    size_t end;
    size_t out = 0;

    if (size == 0)
        return 0;
    dst[0] = '\0';
    if (src == NULL)
        return 0;
    len = strlen(src);
    while (start < len && isspace((unsigned char)src[start]))
        ++start;
    end = len;
    while (end > start && isspace((unsigned char)src[end - 1]))
        --end;
    if (start >= end)
        return 0;
    for (; start < end && out + 1 < size; ++start) {
        unsigned char ch = (unsigned char)src[start];
        if (isspace(ch) || ch == '|') {
            if (out == 0 || dst[out - 1] != '_')
                dst[out++] = '_';
        } else {
            dst[out++] = (char)ch;
        }
    }
    dst[out] = '\0';
    return out > 0;
}

static void clear_entry_to_placeholder(GtkWidget *entry)
{
    const char *placeholder = (const char *)g_object_get_data(
        G_OBJECT(entry), "parky-placeholder");
    gtk_entry_set_text(GTK_ENTRY(entry), "");
    if (placeholder != NULL)
        entry_placeholder_blur(entry, NULL, (gpointer)placeholder);
}

static void apply_fg_recursive(GtkWidget *widget, const GdkColor *fg)
{
    if (GTK_IS_LABEL(widget)) {
        gtk_widget_modify_fg(widget, GTK_STATE_NORMAL, fg);
        gtk_widget_modify_fg(widget, GTK_STATE_PRELIGHT, fg);
        gtk_widget_modify_fg(widget, GTK_STATE_ACTIVE, fg);
        return;
    }
    if (GTK_IS_CONTAINER(widget)) {
        GList *children = gtk_container_get_children(GTK_CONTAINER(widget));
        GList *item;
        for (item = children; item != NULL; item = item->next)
            apply_fg_recursive(GTK_WIDGET(item->data), fg);
        g_list_free(children);
    }
}

static void style_action_button(GtkWidget *button, const char *bgHex,
                                const char *prelightHex, const char *fgHex)
{
    GdkColor bg, prelight, fg;
    if (gdk_color_parse(bgHex, &bg)) {
        gtk_widget_modify_bg(button, GTK_STATE_NORMAL, &bg);
        gtk_widget_modify_bg(button, GTK_STATE_ACTIVE, &bg);
    }
    if (gdk_color_parse(prelightHex, &prelight))
        gtk_widget_modify_bg(button, GTK_STATE_PRELIGHT, &prelight);
    if (gdk_color_parse(fgHex, &fg)) {
        gtk_widget_modify_fg(button, GTK_STATE_NORMAL, &fg);
        gtk_widget_modify_fg(button, GTK_STATE_PRELIGHT, &fg);
        gtk_widget_modify_fg(button, GTK_STATE_ACTIVE, &fg);
        apply_fg_recursive(button, &fg);
    }
}

static GtkWidget *make_agent_button(const char *labelText, const char *stock,
                                    const char *bgHex, const char *prelightHex,
                                    const char *fgHex, int width, int height)
{
    GtkWidget *button = gtk_button_new();
    GtkWidget *align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    GtkWidget *content = gtk_hbox_new(FALSE, 8);
    GtkWidget *icon = gtk_image_new_from_stock(stock, GTK_ICON_SIZE_BUTTON);
    GtkWidget *label = gtk_label_new(NULL);
    gchar *escaped = g_markup_escape_text(labelText, -1);
    char markup[160];

    snprintf(markup, sizeof(markup), "<span weight='bold'>%s</span>", escaped);
    g_free(escaped);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_box_pack_start(GTK_BOX(content), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), label, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(align), content);
    gtk_container_add(GTK_CONTAINER(button), align);
    gtk_widget_set_size_request(button, width, height);
    style_action_button(button, bgHex, prelightHex, fgHex);
    return button;
}

static GtkWidget *agent_entry_row(const char *iconStock,
                                  const char *placeholder,
                                  GtkWidget **entry)
{
    GtkWidget *box = gtk_hbox_new(FALSE, 8);
    GtkWidget *icon = gtk_image_new_from_stock(
        iconStock, GTK_ICON_SIZE_BUTTON);
    GtkWidget *text = gtk_entry_new();
    GdkColor white;

    gdk_color_parse("#FFFFFF", &white);
    gtk_widget_set_size_request(icon, 20, 20);
    gtk_widget_set_size_request(text, -1, 34);
    gtk_widget_modify_base(text, GTK_STATE_NORMAL, &white);
    gtk_entry_set_alignment(GTK_ENTRY(text), 0.0);
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), text, TRUE, TRUE, 0);
    set_entry_placeholder(text, placeholder);
    *entry = text;
    return box;
}

static GtkWidget *make_white_card(int width, GtkWidget *child)
{
    GtkWidget *frame = gtk_frame_new(NULL);
    GtkWidget *event = gtk_event_box_new();
    GdkColor white;

    gdk_color_parse("#FFFFFF", &white);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    if (width > 0)
        gtk_widget_set_size_request(frame, width, -1);
    gtk_widget_modify_bg(event, GTK_STATE_NORMAL, &white);
    gtk_container_add(GTK_CONTAINER(event), child);
    gtk_container_add(GTK_CONTAINER(frame), event);
    return frame;
}

static GtkWidget *make_spot_counter_tile(const char *caption,
                                         GtkWidget **valueLabel)
{
    GtkWidget *box = gtk_vbox_new(FALSE, 2);
    GtkWidget *val = gtk_label_new(NULL);
    GtkWidget *cap = gtk_label_new(NULL);
    char capMarkup[128];

    gtk_misc_set_alignment(GTK_MISC(val), 0.5, 0.5);
    gtk_misc_set_alignment(GTK_MISC(cap), 0.5, 0.5);
    snprintf(capMarkup, sizeof(capMarkup),
             "<span foreground='#475569' size='small' weight='bold'>%s</span>",
             caption);
    gtk_label_set_markup(GTK_LABEL(cap), capMarkup);
    gtk_box_pack_start(GTK_BOX(box), val, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), cap, FALSE, FALSE, 0);
    *valueLabel = val;
    return box;
}

static Parking *find_agent_parking(UiState *state)
{
    int i;
    int parkingId = 0;

    for (i = 0; i < state->data->userCount; ++i) {
        if (state->data->users[i].id == state->activeUserId) {
            parkingId = state->data->users[i].assignedParkingId;
            break;
        }
    }
    if (parkingId <= 0)
        return NULL;
    for (i = 0; i < state->data->parkingCount; ++i) {
        if (state->data->parkings[i].id == parkingId)
            return &state->data->parkings[i];
    }
    return NULL;
}

static int next_id_from_services(const ParkyData *data)
{
    int i, next = 1;
    for (i = 0; i < data->serviceCount; ++i) {
        if (data->services[i].id >= next)
            next = data->services[i].id + 1;
    }
    return next;
}

static void refresh_agent_hero(UiState *state)
{
    Parking *parking = find_agent_parking(state);
    char markup[512];

    if (state->agentParkingHeroTitle == NULL)
        return;

    if (parking != NULL) {
        char displayName[PARKY_NAME_LENGTH];
        char displayZone[PARKY_TEXT_LENGTH];
        gchar *escapedName;
        gchar *escapedZone;
        int occupied = parking->totalSpots - parking->availableSpots;
        if (occupied < 0)
            occupied = 0;

        format_display_text(displayName, sizeof(displayName), parking->name);
        format_display_text(displayZone, sizeof(displayZone), parking->address);
        escapedName = g_markup_escape_text(displayName, -1);
        escapedZone = g_markup_escape_text(displayZone, -1);
        snprintf(markup, sizeof(markup),
                 "<span foreground='#1B2A4A' size='x-large' weight='bold'>%s</span>\n"
                 "<span foreground='#475569' size='medium'>Zone: %s</span>",
                 escapedName, escapedZone);
        g_free(escapedName);
        g_free(escapedZone);
        gtk_label_set_markup(GTK_LABEL(state->agentParkingHeroTitle), markup);

        gtk_label_set_markup(GTK_LABEL(state->agentParkingRate),
            "<span foreground='#1F5FBF' size='medium' weight='bold'>"
            "Hourly Rate: 2.00 TND/hr</span>");

        snprintf(markup, sizeof(markup),
                 "<span foreground='#10B981' size='xx-large' weight='bold'>%d</span>",
                 parking->availableSpots);
        gtk_label_set_markup(GTK_LABEL(state->agentSpotsAvailable), markup);

        snprintf(markup, sizeof(markup),
                 "<span foreground='#EF4444' size='xx-large' weight='bold'>%d</span>",
                 occupied);
        gtk_label_set_markup(GTK_LABEL(state->agentSpotsOccupied), markup);

        snprintf(markup, sizeof(markup),
                 "<span foreground='#1B2A4A' size='xx-large' weight='bold'>%d</span>",
                 parking->totalSpots);
        gtk_label_set_markup(GTK_LABEL(state->agentSpotsTotal), markup);

        gtk_widget_set_sensitive(state->agentSpotPlusBtn, TRUE);
        gtk_widget_set_sensitive(state->agentSpotMinusBtn, TRUE);
    } else {
        gtk_label_set_markup(GTK_LABEL(state->agentParkingHeroTitle),
            "<span foreground='#EF4444' size='x-large' weight='bold'>"
            "No Assigned Parking</span>\n"
            "<span foreground='#475569' size='medium'>"
            "Ask an administrator to assign a parking lot.</span>");
        gtk_label_set_markup(GTK_LABEL(state->agentParkingRate),
            "<span foreground='#475569' size='medium' weight='bold'>"
            "Hourly Rate: -- TND/hr</span>");
        gtk_label_set_markup(GTK_LABEL(state->agentSpotsAvailable),
            "<span foreground='#10B981' size='xx-large' weight='bold'>0</span>");
        gtk_label_set_markup(GTK_LABEL(state->agentSpotsOccupied),
            "<span foreground='#EF4444' size='xx-large' weight='bold'>0</span>");
        gtk_label_set_markup(GTK_LABEL(state->agentSpotsTotal),
            "<span foreground='#1B2A4A' size='xx-large' weight='bold'>0</span>");
        gtk_widget_set_sensitive(state->agentSpotPlusBtn, FALSE);
        gtk_widget_set_sensitive(state->agentSpotMinusBtn, FALSE);
    }
}

static void refresh_agent_reservations_and_hero(UiState *state)
{
    GtkListStore *store;
    GtkWidget *tree;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    Parking *parking = find_agent_parking(state);
    int assignedParkingId = parking != NULL ? parking->id : 0;
    int selectedId;
    char idText[32];
    int i;

    refresh_agent_hero(state);

    selectedId = tree_selected_first_id(state, state->agentReservations);
    tree = gtk_bin_get_child(GTK_BIN(
        gtk_bin_get_child(GTK_BIN(state->agentReservations))));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    store = tree_store(state, state->agentReservations);
    gtk_list_store_clear(store);
    for (i = 0; i < state->data->reservationCount; ++i) {
        const Reservation *res = &state->data->reservations[i];
        const char *citizenName = "Unknown";
        const char *dateText;
        const char *timeText;
        const char *statusMarkup;
        char serviceText[PARKY_NAME_LENGTH];
        char totalText[32];
        double total = 0.0;
        int j;

        if (assignedParkingId > 0 && res->parkingId != assignedParkingId)
            continue;

        for (j = 0; j < state->data->userCount; ++j) {
            if (state->data->users[j].id == res->citizenId) {
                citizenName = state->data->users[j].name;
                break;
            }
        }

        if (strncmp(res->time, "service-", 8) == 0) {
            int serviceId = atoi(res->time + 8);
            int foundService = 0;
            dateText = strcmp(res->date, "service") == 0 ?
                       "On-Demand" : res->date;
            timeText = "-";
            for (j = 0; j < state->data->serviceCount; ++j) {
                if (state->data->services[j].id == serviceId) {
                    format_display_text(serviceText, sizeof(serviceText),
                                        state->data->services[j].name);
                    total = (double)state->data->services[j].price;
                    foundService = 1;
                    break;
                }
            }
            if (!foundService)
                snprintf(serviceText, sizeof(serviceText),
                         "Service #%d", serviceId);
        } else {
            dateText = res->date;
            timeText = res->time;
            if (res->spotNumber > 0)
                snprintf(serviceText, sizeof(serviceText),
                         "Spot #%d", res->spotNumber);
            else
                snprintf(serviceText, sizeof(serviceText), "Standard Spot");
            total = citoyen_compute_billing(state->data, res->parkingId, 0, 1);
        }
        snprintf(totalText, sizeof(totalText), "%.2f TND", total);

        if (res->status == RESERVATION_APPROVED)
            statusMarkup = "<span foreground='#10B981' weight='bold'>Approved</span>";
        else if (res->status == RESERVATION_REJECTED)
            statusMarkup = "<span foreground='#EF4444' weight='bold'>Rejected</span>";
        else if (res->status == 3)
            statusMarkup = "<span foreground='#1F5FBF' weight='bold'>Completed</span>";
        else
            statusMarkup = "<span foreground='#D97706' weight='bold'>Pending</span>";

        gtk_list_store_append(store, &iter);
        snprintf(idText, sizeof(idText), "%d", res->id);
        gtk_list_store_set(store, &iter,
                           0, idText,
                           1, citizenName,
                           2, dateText,
                           3, timeText,
                           4, serviceText,
                           5, totalText,
                           6, statusMarkup,
                           -1);
        if (selectedId > 0 && res->id == selectedId)
            gtk_tree_selection_select_iter(selection, &iter);
    }
}

static void refresh_agent_tables(UiState *state)
{
    GtkListStore *store;
    GtkTreeIter iter;
    char idText[32];
    int i;

    refresh_agent_reservations_and_hero(state);

    store = tree_store(state, state->agentServices);
    gtk_list_store_clear(store);
    for (i = 0; i < state->data->serviceCount; ++i) {
        char displayName[PARKY_NAME_LENGTH];
        char displayDesc[PARKY_TEXT_LENGTH];
        char priceText[32];
        const char *statusMarkup;

        format_display_text(displayName, sizeof(displayName),
                            state->data->services[i].name);
        format_display_text(displayDesc, sizeof(displayDesc),
                            state->data->services[i].description);
        snprintf(priceText, sizeof(priceText), "%.2f TND",
                 state->data->services[i].price);
        statusMarkup = state->data->services[i].status ?
                       "<span foreground='#10B981' weight='bold'>Active</span>" :
                       "<span foreground='#EF4444' weight='bold'>Inactive</span>";

        gtk_list_store_append(store, &iter);
        snprintf(idText, sizeof(idText), "%d", state->data->services[i].id);
        gtk_list_store_set(store, &iter,
                           0, idText,
                           1, displayName,
                           2, displayDesc,
                           3, priceText,
                           4, statusMarkup,
                           -1);
    }
}

static gboolean on_agent_realtime_tick(gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int i;
    int isAgent = 0;

    if (state == NULL || state->activeUserId <= 0 || state->notebook == NULL) {
        if (state != NULL)
            state->agentRefreshTimerId = 0;
        return FALSE;
    }
    for (i = 0; i < state->data->userCount; ++i) {
        if (state->data->users[i].id == state->activeUserId) {
            isAgent = (state->data->users[i].role == USER_AGENT);
            break;
        }
    }
    if (!isAgent) {
        state->agentRefreshTimerId = 0;
        return FALSE;
    }
    if (gtk_notebook_get_current_page(GTK_NOTEBOOK(state->notebook)) == 2 &&
        storage_load(state->data)) {
        refresh_agent_reservations_and_hero(state);
    }
    return TRUE;
}

static void start_agent_realtime_timer(UiState *state)
{
    stop_agent_realtime_timer(state);
    if (state != NULL) {
        state->agentRefreshTimerId =
            g_timeout_add(2000, on_agent_realtime_tick, state);
    }
}

static void stop_agent_realtime_timer(UiState *state)
{
    if (state != NULL && state->agentRefreshTimerId != 0) {
        g_source_remove(state->agentRefreshTimerId);
        state->agentRefreshTimerId = 0;
    }
}

static gboolean on_admin_realtime_tick(gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int i;
    int isAdmin = 0;

    if (state == NULL || state->activeUserId <= 0 ||
        state->notebook == NULL) {
        if (state != NULL)
            state->adminRefreshTimerId = 0;
        return FALSE;
    }
    for (i = 0; i < state->data->userCount; ++i) {
        if (state->data->users[i].id == state->activeUserId) {
            isAdmin = (state->data->users[i].role == USER_ADMIN);
            break;
        }
    }
    if (!isAdmin) {
        state->adminRefreshTimerId = 0;
        return FALSE;
    }
    if (gtk_notebook_get_current_page(GTK_NOTEBOOK(state->notebook)) == 1 &&
        storage_load(state->data))
        refresh_admin_tables(state);
    return TRUE;
}

static void start_admin_realtime_timer(UiState *state)
{
    stop_admin_realtime_timer(state);
    if (state != NULL) {
        state->adminRefreshTimerId =
            g_timeout_add(2000, on_admin_realtime_tick, state);
    }
}

static void stop_admin_realtime_timer(UiState *state)
{
    if (state != NULL && state->adminRefreshTimerId != 0) {
        g_source_remove(state->adminRefreshTimerId);
        state->adminRefreshTimerId = 0;
    }
}

/* Repopulate the parking dropdown in the Review form with current parking names.
   Called from refresh_citizen_tables() so it stays in sync. */
static void refresh_citizen_review_combo(UiState *state)
{
    int i;
    int active;
    int activeParkingId = 0;
    GtkComboBox *combo;

    if (state->reviewParkingCombo == NULL)
        return;
    combo = GTK_COMBO_BOX(state->reviewParkingCombo);

    /* Remember which parking was selected so we can restore it */
    active = gtk_combo_box_get_active(combo);
    if (active > 0 && active - 1 < state->data->parkingCount)
        activeParkingId = state->data->parkings[active - 1].id;

    gtk_list_store_clear(GTK_LIST_STORE(
        gtk_combo_box_get_model(combo)));

    gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "Select a parking...");
    for (i = 0; i < state->data->parkingCount; ++i)
        gtk_combo_box_append_text(GTK_COMBO_BOX(combo),
                                  state->data->parkings[i].name);

    /* Restore previous selection */
    gtk_combo_box_set_active(combo, 0);
    if (activeParkingId > 0) {
        for (i = 0; i < state->data->parkingCount; ++i) {
            if (state->data->parkings[i].id == activeParkingId) {
                gtk_combo_box_set_active(combo, i + 1);
                break;
            }
        }
    }
}

/* Applies the current filter-bar values (name, address, only-available) to
   repopulate the Available Parkings tree.  Called from refresh_citizen_tables
   and from search_clicked so both paths always honour the active filter.
   The previously selected parking row is reselected if it still matches. */
static void refresh_citizen_parkings(UiState *state)
{
    GtkListStore *store;
    GtkTreeIter iter;
    GtkWidget *tree;
    GtkTreeSelection *sel;
    Parking results[PARKY_MAX_PARKINGS];
    int count, i;
    int selId = 0;
    const char *nameFilter;
    const char *addrFilter;
    int onlyAvail;

    if (state->citizenParkings == NULL)
        return;

    /* Read filter values — treat placeholder text as empty string */
    nameFilter = entry_placeholder_value(state->citizenName);
    addrFilter = entry_placeholder_value(state->citizenAddress);
    onlyAvail  = gtk_toggle_button_get_active(
                     GTK_TOGGLE_BUTTON(state->citizenAvailable));

    /* Remember selected parking ID so we can reselect it after reload */
    tree = gtk_bin_get_child(GTK_BIN(
               gtk_bin_get_child(GTK_BIN(state->citizenParkings))));
    sel  = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    {
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree));
        GtkTreeIter  si;
        if (gtk_tree_selection_get_selected(sel, &model, &si)) {
            gchar *txt = NULL;
            gtk_tree_model_get(model, &si, 0, &txt, -1);
            if (txt != NULL) { selId = atoi(txt); g_free(txt); }
        }
    }

    count = citoyen_filter_parkings(state->data,
                                    nameFilter, addrFilter,
                                    onlyAvail,
                                    results, PARKY_MAX_PARKINGS);

    store = tree_store(state, state->citizenParkings);
    gtk_list_store_clear(store);
    for (i = 0; i < count; ++i) {
        char idBuf[32], availBuf[32];
        snprintf(idBuf,    sizeof(idBuf),    "%d", results[i].id);
        snprintf(availBuf, sizeof(availBuf), "%d", results[i].availableSpots);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, idBuf,
                           1, results[i].name,
                           2, results[i].address,
                           3, availBuf,
                           4, "2.00 TND/hr",
                           -1);
        /* Restore selection if this row matches the previously selected ID */
        if (selId > 0 && results[i].id == selId)
            gtk_tree_selection_select_iter(sel, &iter);
    }

    /* Update the inline status badge above the parkings table */
    if (state->citizenSearchStatus != NULL) {
        char markup[128];
        snprintf(markup, sizeof(markup),
                 "<span background='#E2E8F0' foreground='#475569'"
                 " size='small'> %d matched </span>",
                 count);
        gtk_label_set_markup(GTK_LABEL(state->citizenSearchStatus),
                             markup);
    }
}

static void refresh_citizen_tables(UiState *state)
{
    GtkListStore *store;
    GtkTreeIter iter;
    int i;

    /* ---- Tab 1: Available Parkings (filtered, honours search bar) ---- */
    refresh_citizen_parkings(state);

    /* ---- Tab 2: My Reservations
            (ID hidden col 0, Parking Name, Spot, Date, Time,
             Total TND, Status markup) ---- */
    if (state->citizenReservations != NULL) {
        GtkWidget *resTree = gtk_bin_get_child(GTK_BIN(
            gtk_bin_get_child(GTK_BIN(state->citizenReservations))));
        GtkTreeSelection *resSel =
            gtk_tree_view_get_selection(GTK_TREE_VIEW(resTree));
        GtkTreeModel *resModel =
            gtk_tree_view_get_model(GTK_TREE_VIEW(resTree));
        GtkTreeIter selIter;
        int selId = 0;

        /* Remember selected ID to restore after reload */
        if (gtk_tree_selection_get_selected(resSel, &resModel, &selIter)) {
            gchar *txt = NULL;
            gtk_tree_model_get(resModel, &selIter, 0, &txt, -1);
            if (txt != NULL) { selId = atoi(txt); g_free(txt); }
        }

        store = tree_store(state, state->citizenReservations);
        gtk_list_store_clear(store);
        for (i = 0; i < state->data->reservationCount; ++i) {
            const Reservation *res = &state->data->reservations[i];
            const char *parkingName = "Unknown";
            const char *statusMarkup;
            char spotText[32], totalText[32], idText[32];
            int j;
            double total;

            if (res->citizenId != state->activeUserId)
                continue;

            /* Derive parking name */
            for (j = 0; j < state->data->parkingCount; ++j) {
                if (state->data->parkings[j].id == res->parkingId) {
                    parkingName = state->data->parkings[j].name;
                    break;
                }
            }

            if (res->spotNumber > 0)
                snprintf(spotText, sizeof(spotText), "%d", res->spotNumber);
            else
                snprintf(spotText, sizeof(spotText), "-");

            /* Compute total: service bookings store service id in time field */
            if (strncmp(res->time, "service-", 8) == 0) {
                int svcId = atoi(res->time + 8);
                total = 0.0;
                for (j = 0; j < state->data->serviceCount; ++j) {
                    if (state->data->services[j].id == svcId) {
                        total = (double)state->data->services[j].price;
                        break;
                    }
                }
            } else {
                total = citoyen_compute_billing(state->data,
                                               res->parkingId, 0, 1);
            }
            snprintf(totalText, sizeof(totalText), "%.2f TND", total);

            if (res->status == RESERVATION_APPROVED)
                statusMarkup =
                    "<span foreground='#10B981' weight='bold'>Approved</span>";
            else if (res->status == RESERVATION_REJECTED)
                statusMarkup =
                    "<span foreground='#EF4444' weight='bold'>Rejected</span>";
            else if (res->status == 3)
                statusMarkup =
                    "<span foreground='#1F5FBF' weight='bold'>Completed</span>";
            else
                statusMarkup =
                    "<span foreground='#D97706' weight='bold'>Pending</span>";

            snprintf(idText, sizeof(idText), "%d", res->id);
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                               0, idText,
                               1, parkingName,
                               2, spotText,
                               3, res->date,
                               4, res->time,
                               5, totalText,
                               6, statusMarkup,
                               -1);
            if (selId > 0 && res->id == selId)
                gtk_tree_selection_select_iter(resSel, &iter);
        }
    }

    /* ---- Tab 3: Community Reviews (Rating, Comment) ---- */
    store = tree_store(state, state->citizenReviews);
    gtk_list_store_clear(store);
    for (i = 0; i < state->data->reviewCount; ++i) {
        char ratingText[8];
        snprintf(ratingText, sizeof(ratingText), "%d★",
                 state->data->reviews[i].rating);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, ratingText,
                           1, state->data->reviews[i].comment,
                           -1);
    }

    refresh_citizen_review_combo(state);
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
                start_admin_realtime_timer(state);
            } else if (user->role == USER_AGENT) {
                refresh_agent_tables(state);
                set_page(state, 2);
                start_agent_realtime_timer(state);
            } else {
                refresh_citizen_tables(state);
                set_page(state, 3);
                start_citizen_realtime_timer(state);
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
    UserRole role = admin_action_role(widget);
    int userId = tree_selected_first_id(state, admin_user_tree(state, role));
    if (userId == 0) {
        show_message(state, GTK_MESSAGE_WARNING, "No user selected",
                     "Select an account from the list first.");
        return;
    }
    if (confirm_destructive_action(state, "Delete selected user?",
                                   "This action cannot be undone.") &&
        (role == USER_AGENT ? admin_delete_agent(state->data, userId) :
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

static void adjust_agent_spot(UiState *state, int delta)
{
    Parking *parking;

    storage_load(state->data);
    parking = find_agent_parking(state);
    if (parking == NULL) {
        show_dialog(NULL, GTK_MESSAGE_WARNING, "No Assigned Parking",
                    "No parking is currently assigned to your account.");
        set_status(state, "Assigned parking not found");
        return;
    }
    if (delta > 0 && parking->availableSpots >= parking->totalSpots) {
        show_dialog(NULL, GTK_MESSAGE_INFO, "Parking Already Empty",
                    "All spots in this parking are already free.");
        set_status(state, "All spots are already free");
        return;
    }
    if (delta < 0 && parking->availableSpots <= 0) {
        show_dialog(NULL, GTK_MESSAGE_WARNING, "Parking Full",
                    "All spots in this parking are currently occupied.");
        set_status(state, "Parking is already full");
        return;
    }
    parking->availableSpots += delta;
    parking->status = parking->availableSpots > 0 ?
                      PARKING_AVAILABLE : PARKING_OCCUPIED;
    if (storage_save(state->data)) {
        refresh_agent_reservations_and_hero(state);
        refresh_citizen_tables(state);
        refresh_admin_tables(state);
        set_status(state, delta > 0 ? "Added 1 free spot" :
                   "Marked 1 spot occupied");
    }
}

static void mark_available_clicked(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    adjust_agent_spot((UiState *)user_data, 1);
}

static void mark_occupied_clicked(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    adjust_agent_spot((UiState *)user_data, -1);
}

static void agent_service_row_selected(GtkTreeSelection *selection,
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
    for (i = 0; i < state->data->serviceCount; ++i) {
        if (state->data->services[i].id == id) {
            char displayName[PARKY_NAME_LENGTH];
            char displayDesc[PARKY_TEXT_LENGTH];
            char priceText[32];

            format_display_text(displayName, sizeof(displayName),
                                state->data->services[i].name);
            format_display_text(displayDesc, sizeof(displayDesc),
                                state->data->services[i].description);
            snprintf(priceText, sizeof(priceText), "%.2f",
                     state->data->services[i].price);
            gtk_entry_set_text(GTK_ENTRY(state->agentServiceName), displayName);
            gtk_entry_set_text(GTK_ENTRY(state->agentServiceDescription),
                               displayDesc);
            gtk_entry_set_text(GTK_ENTRY(state->agentServicePrice), priceText);
            return;
        }
    }
}

static void add_service_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    Service service;
    const char *rawName = entry_placeholder_value(state->agentServiceName);
    const char *rawDesc = entry_placeholder_value(state->agentServiceDescription);
    const char *priceText = entry_placeholder_value(state->agentServicePrice);
    double price = atof(priceText);
    (void)widget;

    memset(&service, 0, sizeof(service));
    if (!sanitize_storage_text(service.name, sizeof(service.name), rawName) ||
        !sanitize_storage_text(service.description, sizeof(service.description),
                               rawDesc) ||
        price <= 0.0) {
        show_dialog(NULL, GTK_MESSAGE_ERROR, "Invalid Service Details",
                    "Enter a service name, description, and positive price in TND.");
        return;
    }
    service.id = next_id_from_services(state->data);
    service.price = (float)price;
    service.status = 1;
    if (agent_add_service(state->data, &service)) {
        clear_entry_to_placeholder(state->agentServiceName);
        clear_entry_to_placeholder(state->agentServiceDescription);
        clear_entry_to_placeholder(state->agentServicePrice);
        show_dialog(NULL, GTK_MESSAGE_INFO, "Service Added",
                    "The parking service was added.");
        refresh_agent_tables(state);
        set_status(state, "Service added");
    } else {
        show_dialog(NULL, GTK_MESSAGE_ERROR, "Add Service Failed",
                    "PARKY could not add the parking service.");
    }
}

static void edit_service_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int serviceId = tree_selected_first_id(state, state->agentServices);
    Service service;
    const char *rawName = entry_placeholder_value(state->agentServiceName);
    const char *rawDesc = entry_placeholder_value(state->agentServiceDescription);
    const char *priceText = entry_placeholder_value(state->agentServicePrice);
    double price = atof(priceText);
    int i;
    (void)widget;

    if (serviceId == 0) {
        show_dialog(NULL, GTK_MESSAGE_WARNING, "No Service Selected",
                    "Select a service from the table first.");
        return;
    }
    memset(&service, 0, sizeof(service));
    service.id = serviceId;
    service.status = 1;
    for (i = 0; i < state->data->serviceCount; ++i) {
        if (state->data->services[i].id == serviceId) {
            service.status = state->data->services[i].status;
            break;
        }
    }
    if (!sanitize_storage_text(service.name, sizeof(service.name), rawName) ||
        !sanitize_storage_text(service.description, sizeof(service.description),
                               rawDesc) ||
        price <= 0.0) {
        show_dialog(NULL, GTK_MESSAGE_ERROR, "Invalid Service Details",
                    "Enter a valid service name, description, and positive price in TND.");
        return;
    }
    service.price = (float)price;
    if (agent_update_service(state->data, &service)) {
        show_dialog(NULL, GTK_MESSAGE_INFO, "Service Updated",
                    "The selected parking service was updated.");
        refresh_agent_tables(state);
        set_status(state, "Service updated");
    } else {
        show_dialog(NULL, GTK_MESSAGE_ERROR, "Update Service Failed",
                    "PARKY could not update the selected service.");
    }
}

static void toggle_service_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int serviceId = tree_selected_first_id(state, state->agentServices);
    Service service;
    int found = 0;
    int i;
    (void)widget;

    if (serviceId == 0) {
        show_dialog(NULL, GTK_MESSAGE_WARNING, "No Service Selected",
                    "Select a service from the table first.");
        return;
    }
    for (i = 0; i < state->data->serviceCount; ++i) {
        if (state->data->services[i].id == serviceId) {
            service = state->data->services[i];
            service.status = service.status ? 0 : 1;
            found = 1;
            break;
        }
    }
    if (found && agent_update_service(state->data, &service)) {
        refresh_agent_tables(state);
        set_status(state, service.status ? "Service activated" :
                   "Service deactivated");
    }
}

static void delete_service_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int serviceId = tree_selected_first_id(state, state->agentServices);
    (void)widget;

    if (serviceId == 0) {
        show_dialog(NULL, GTK_MESSAGE_WARNING, "No Service Selected",
                    "Select a service from the table first.");
        return;
    }
    if (!confirm_destructive_action(state, "Delete selected service?",
                                    "This action cannot be undone."))
        return;
    if (agent_delete_service(state->data, serviceId)) {
        clear_entry_to_placeholder(state->agentServiceName);
        clear_entry_to_placeholder(state->agentServiceDescription);
        clear_entry_to_placeholder(state->agentServicePrice);
        show_dialog(NULL, GTK_MESSAGE_INFO, "Service Deleted",
                    "The selected parking service was deleted.");
        refresh_agent_tables(state);
        set_status(state, "Service deleted");
    } else {
        show_dialog(NULL, GTK_MESSAGE_ERROR, "Deletion Failed",
                    "The selected service could not be deleted.");
    }
}

static void send_agent_complaint_clicked(GtkWidget *widget,
                                         gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    const char *rawSubject =
        entry_placeholder_value(state->agentComplaintSubject);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(state->agentComplaintText));
    GtkTextIter start, end;
    char *text;
    char cleanSubject[PARKY_NAME_LENGTH];
    char cleanDesc[PARKY_TEXT_LENGTH];
    (void)widget;

    gtk_text_buffer_get_bounds(buffer, &start, &end);
    text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    if (!sanitize_storage_text(cleanSubject, sizeof(cleanSubject), rawSubject)) {
        show_dialog(NULL, GTK_MESSAGE_ERROR, "Missing Issue Subject",
                    "Enter a short subject describing the issue.");
        g_free(text);
        return;
    }
    if (!sanitize_storage_text(cleanDesc, sizeof(cleanDesc), text)) {
        show_dialog(NULL, GTK_MESSAGE_ERROR, "Missing Issue Description",
                    "Describe the issue before sending the report.");
        g_free(text);
        return;
    }
    if (agent_submit_complaint(state->data, state->activeUserId,
                               cleanSubject, cleanDesc)) {
        clear_entry_to_placeholder(state->agentComplaintSubject);
        gtk_text_buffer_set_text(buffer, "", 0);
        show_dialog(NULL, GTK_MESSAGE_INFO, "Report Sent",
                    "Your issue report has been sent to the administrator.");
        refresh_admin_tables(state);
        set_status(state, "Issue report sent");
    } else {
        show_dialog(NULL, GTK_MESSAGE_ERROR, "Report Failed",
                    "PARKY could not save the issue report.");
    }
    g_free(text);
}

static void resolve_agent_reservation_action(UiState *state, int approved)
{
    int reservationId =
        tree_selected_first_id(state, state->agentReservations);

    if (reservationId <= 0) {
        show_dialog(NULL, GTK_MESSAGE_WARNING, "No Reservation Selected",
                    approved ?
                    "Select a reservation in the table before approving it." :
                    "Select a reservation in the table before rejecting it.");
        set_status(state, "No reservation selected");
        return;
    }
    if (!approved &&
        !confirm_destructive_action(state, "Reject the selected reservation?",
                                    "The citizen will see the status as Rejected.")) {
        return;
    }
    storage_load(state->data);
    if (agent_resolve_reservation(state->data, reservationId, approved)) {
        show_dialog(NULL, GTK_MESSAGE_INFO,
                    approved ? "Reservation Approved" : "Reservation Rejected",
                    approved ?
                    "The selected reservation has been approved." :
                    "The selected reservation has been rejected.");
        set_status(state, approved ?
                   "Selected reservation approved" :
                   "Selected reservation rejected");
    } else {
        show_dialog(NULL, GTK_MESSAGE_ERROR,
                    approved ? "Approval Failed" : "Rejection Failed",
                    approved ?
                    "Could not approve the selected reservation." :
                    "Could not reject the selected reservation.");
    }
    refresh_agent_reservations_and_hero(state);
    refresh_citizen_tables(state);
    refresh_admin_tables(state);
}

static void approve_reservation_clicked(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    resolve_agent_reservation_action((UiState *)user_data, 1);
}

static void reject_reservation_clicked(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    resolve_agent_reservation_action((UiState *)user_data, 0);
}

static void update_tariff(UiState *state)
{
    int hours = atoi(gtk_entry_get_text(GTK_ENTRY(state->citizenHours)));
    int comboIdx = gtk_combo_box_get_active(
        GTK_COMBO_BOX(state->citizenService));
    int serviceId = 0;
    char message[160];
    double total;
    int activePos, i;

    /* Map combo position (1-based, skipping index 0 = "No extra service")
       to the actual service ID in the active services list. */
    if (comboIdx > 0) {
        activePos = 0;
        for (i = 0; i < state->data->serviceCount; ++i) {
            if (state->data->services[i].status == 1) {
                ++activePos;
                if (activePos == comboIdx) {
                    serviceId = state->data->services[i].id;
                    break;
                }
            }
        }
    }

    if (hours <= 0) hours = 1;
    total = citoyen_compute_billing(state->data, 1, serviceId, hours);
    snprintf(message, sizeof(message),
             "Estimated Tariff: %.2f TND", total);
    if (state->tariff != NULL) {
        char markup[200];
        gchar *escaped = g_markup_escape_text(message, -1);
        snprintf(markup, sizeof(markup),
                 "<span foreground='#287A4D' weight='bold'>%s</span>",
                 escaped);
        g_free(escaped);
        gtk_label_set_markup(GTK_LABEL(state->tariff), markup);
    }
}

static void tariff_changed(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    update_tariff((UiState *)user_data);
}

static void clear_filters_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    (void)widget;
    clear_entry_to_placeholder(state->citizenName);
    clear_entry_to_placeholder(state->citizenAddress);
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(state->citizenAvailable), FALSE);
    /* refresh_citizen_parkings is triggered automatically via the
       "changed" / "toggled" signals connected to those widgets, but
       call it explicitly here in case signals are blocked or inactive. */
    refresh_citizen_parkings(state);
}

static void search_clicked(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    refresh_citizen_parkings((UiState *)user_data);
}

static void reserve_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int parkingId = atoi(gtk_entry_get_text(GTK_ENTRY(state->citizenParking)));
    int spot = atoi(entry_placeholder_value(state->citizenSpot));
    int serviceIdx;
    int serviceId = 0;
    int i;
    int found = 0;
    int full  = 0;
    const char *date;
    char timeBuf[8];
    int hour, minute;
    (void)widget;

    if (parkingId <= 0) {
        show_message(state, GTK_MESSAGE_ERROR, "No parking selected",
                     "Select a parking from the table first.");
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
                     "The selected parking no longer exists.");
        return;
    }
    if (full) {
        show_message(state, GTK_MESSAGE_ERROR, "Parking is full",
                     "This parking has no available spots. Choose another.");
        return;
    }

    /* Date is stored as "parky-date" on the label widget */
    date = (const char *)g_object_get_data(G_OBJECT(state->citizenDate),
                                           "parky-date");
    if (date == NULL || date[0] == '\0') {
        show_message(state, GTK_MESSAGE_ERROR, "Date required",
                     "Pick a date using the calendar button.");
        return;
    }

    /* Time is composed from the two spinbuttons */
    hour   = gtk_spin_button_get_value_as_int(
                 GTK_SPIN_BUTTON(state->citizenHourSpin));
    minute = gtk_spin_button_get_value_as_int(
                 GTK_SPIN_BUTTON(state->citizenMinuteSpin));
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", hour, minute);

    /* Extra service: combo index 0 = none, 1+ = active services in order */
    serviceIdx = gtk_combo_box_get_active(
        GTK_COMBO_BOX(state->citizenService));
    if (serviceIdx > 0) {
        int activePos = 0;
        for (i = 0; i < state->data->serviceCount; ++i) {
            if (state->data->services[i].status == 1) {
                ++activePos;
                if (activePos == serviceIdx) {
                    serviceId = state->data->services[i].id;
                    break;
                }
            }
        }
    }

    if (citoyen_reserve_spot(state->data, state->activeUserId, parkingId,
                             date, timeBuf, spot)) {
        /* Attach a service booking on top if one was selected */
        if (serviceId > 0)
            citoyen_book_service(state->data, state->activeUserId,
                                 parkingId, serviceId);
        show_message(state, GTK_MESSAGE_INFO, "Booking confirmed",
                     "Your parking reservation was submitted.");
        clear_entry_to_placeholder(state->citizenSpot);
        gtk_combo_box_set_active(GTK_COMBO_BOX(state->citizenService), 0);
        update_tariff(state);
    } else {
        show_message(state, GTK_MESSAGE_ERROR, "Booking failed",
                     "PARKY could not submit the reservation. Try again.");
    }
    refresh_citizen_tables(state);
    refresh_agent_tables(state);
    refresh_admin_tables(state);
}

static void book_service_clicked(GtkWidget *widget, gpointer user_data)
    __attribute__((unused));
static void book_service_clicked(GtkWidget *widget, gpointer user_data)
{
    /* Unified into reserve_clicked; kept to avoid unused-symbol warnings
       if any old signal connection references it. */
    (void)widget;
    (void)user_data;
}

static void review_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int parkingId = 0;
    int active;
    int rating = gtk_spin_button_get_value_as_int(
        GTK_SPIN_BUTTON(state->reviewRating));
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(state->reviewComment));
    GtkTextIter start, end;
    char *comment;
    (void)widget;

    /* Derive parkingId from the parking dropdown */
    if (state->reviewParkingCombo != NULL) {
        active = gtk_combo_box_get_active(
            GTK_COMBO_BOX(state->reviewParkingCombo));
        if (active > 0 && active - 1 < state->data->parkingCount)
            parkingId = state->data->parkings[active - 1].id;
    }

    gtk_text_buffer_get_bounds(buffer, &start, &end);
    comment = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    if (parkingId <= 0) {
        show_message(state, GTK_MESSAGE_ERROR, "No parking selected",
                     "Choose a parking from the dropdown before submitting.");
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
    } else {
        show_message(state, GTK_MESSAGE_ERROR, "Review failed",
                     "PARKY could not save the review.");
    }
    refresh_citizen_tables(state);
    g_free(comment);
}

static void complaint_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    const char *subject =
        entry_placeholder_value(state->complaintSubject);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(state->complaintText));
    GtkTextIter start, end;
    char *text;
    (void)widget;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    if (subject == NULL || subject[0] == '\0') {
        show_message(state, GTK_MESSAGE_ERROR, "Missing subject",
                     "Enter a short subject describing the complaint.");
        g_free(text);
        return;
    }
    if (text == NULL || text[0] == '\0') {
        show_message(state, GTK_MESSAGE_ERROR, "Missing description",
                     "Describe the issue before submitting.");
        g_free(text);
        return;
    }
    if (citoyen_submit_complaint(state->data, state->activeUserId,
                                 subject, text)) {
        clear_entry_to_placeholder(state->complaintSubject);
        gtk_text_buffer_set_text(buffer, "", 0);
        show_message(state, GTK_MESSAGE_INFO, "Complaint submitted",
                     "Your complaint was saved.");
    } else {
        show_message(state, GTK_MESSAGE_ERROR, "Complaint failed",
                     "PARKY could not save the complaint.");
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
    GtkWidget *createButton = compact_auth_button(
        "Create Account", GTK_STOCK_APPLY, "#10B981");
    GtkWidget *backLink = gtk_link_button_new_with_label(
        "", "Already have an account? Sign In");
    GtkSizeGroup *authFields = gtk_size_group_new(
        GTK_SIZE_GROUP_HORIZONTAL);

    GtkWidget *heroAlignment =
        gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    gtk_container_set_border_width(GTK_CONTAINER(box), 32);
    gtk_box_set_spacing(GTK_BOX(box), 14);
    gtk_widget_set_size_request(card, 520, -1);
    gtk_widget_set_size_request(createButton, 180, 40);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='x-large' weight='bold'>Create your PARKY account</span>");
    gtk_container_add(GTK_CONTAINER(heroAlignment),
                      make_auth_hero());
    gtk_box_pack_start(GTK_BOX(box), heroAlignment,
                       FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(box),
                       make_auth_row(GTK_STOCK_ORIENTATION_PORTRAIT,
                                     "Username",
                                     &state->signupUsername,
                                     authFields),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box),
                       make_auth_row(GTK_STOCK_NETWORK,
                                     "Email",
                                     &state->signupEmail,
                                     authFields),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box),
                       make_auth_row(GTK_STOCK_DIALOG_AUTHENTICATION,
                                     "Password",
                                     &state->signupPassword,
                                     authFields),
                       FALSE, FALSE, 0);
    gtk_widget_set_size_request(state->signupUsername, 320, 38);
    gtk_widget_set_size_request(state->signupEmail, 320, 38);
    gtk_widget_set_size_request(state->signupPassword, 320, 38);
    gtk_entry_set_visibility(GTK_ENTRY(state->signupPassword), TRUE);
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

    g_signal_connect(createButton, "clicked",
                     G_CALLBACK(create_account_clicked), state);
    g_signal_connect(backLink, "clicked",
                     G_CALLBACK(back_to_login_clicked), state);
    g_signal_connect(state->signupUsername, "activate",
                     G_CALLBACK(create_account_clicked), state);
    g_signal_connect(state->signupEmail,    "activate",
                     G_CALLBACK(create_account_clicked), state);
    g_signal_connect(state->signupPassword, "activate",
                     G_CALLBACK(create_account_clicked), state);
    gtk_widget_grab_focus(createButton);
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
                       make_auth_row(GTK_STOCK_NETWORK,
                                     "Email",
                                     &state->loginEmail,
                                     authFields),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cardBox),
                       make_auth_row(GTK_STOCK_DIALOG_AUTHENTICATION,
                                     "Password",
                                     &state->loginPassword,
                                     authFields),
                       FALSE, FALSE, 0);
    gtk_widget_set_size_request(card, 520, -1);
    gtk_widget_set_size_request(state->loginEmail, 330, 36);
    gtk_widget_set_size_request(state->loginPassword, 330, 36);
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
    g_signal_connect(state->loginEmail,    "activate",
                     G_CALLBACK(login_clicked), state);
    g_signal_connect(state->loginPassword, "activate",
                     G_CALLBACK(login_clicked), state);
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

static void admin_add_user_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    UserRole role = admin_action_role(widget);
    const char *name =
        entry_placeholder_value(admin_user_name(state, role));
    const char *email =
        entry_placeholder_value(admin_user_email(state, role));
    const char *password =
        entry_placeholder_value(admin_user_password(state, role));
    User user;
    int active = role == USER_AGENT ?
                 gtk_combo_box_get_active(GTK_COMBO_BOX(
                     state->adminAgentParking)) : 0;
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
    UserRole role = admin_action_role(widget);
    int id = tree_selected_first_id(state, admin_user_tree(state, role));
    User user;
    int active;
    if (id == 0 || !admin_get_user(state->data, id, &user)) {
        show_message(state, GTK_MESSAGE_WARNING, "No user selected",
                     "Select an account from the list first.");
        return;
    }
    snprintf(user.name, sizeof(user.name), "%s",
             entry_placeholder_value(admin_user_name(state, role)));
    snprintf(user.email, sizeof(user.email), "%s",
             entry_placeholder_value(admin_user_email(state, role)));
    snprintf(user.password, sizeof(user.password), "%s",
             entry_placeholder_value(admin_user_password(state, role)));
    active = role == USER_AGENT ?
             gtk_combo_box_get_active(GTK_COMBO_BOX(
                 state->adminAgentParking)) : 0;
    user.assignedParkingId = role == USER_AGENT &&
                             active > 0 &&
                             active - 1 < state->data->parkingCount ?
                             state->data->parkings[active - 1].id : 0;
    user.role = role;
    active = role == USER_AGENT ?
             admin_update_agent(state->data, &user) :
             admin_update_user(state->data, &user);
    if (active) {
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
    int agentId = tree_selected_first_id(state, state->adminAgents);
    int active = gtk_combo_box_get_active(GTK_COMBO_BOX(
        state->adminAgentParking));
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

static void admin_user_row_selected(GtkTreeSelection *selection,
                                    gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    UserRole role = (UserRole)GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(selection), "parky-role"));
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *textId = NULL;
    User user;
    int i;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;
    gtk_tree_model_get(model, &iter, 0, &textId, -1);
    if (textId == NULL || !admin_get_user(state->data, atoi(textId), &user)) {
        g_free(textId);
        return;
    }
    g_free(textId);
    gtk_entry_set_text(GTK_ENTRY(admin_user_name(state, role)), user.name);
    gtk_entry_set_text(GTK_ENTRY(admin_user_email(state, role)), user.email);
    gtk_entry_set_text(GTK_ENTRY(admin_user_password(state, role)),
                       user.password);
    if (role == USER_AGENT) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(state->adminAgentParking), 0);
        for (i = 0; i < state->data->parkingCount; ++i) {
            if (state->data->parkings[i].id == user.assignedParkingId) {
                gtk_combo_box_set_active(GTK_COMBO_BOX(
                    state->adminAgentParking), i + 1);
                break;
            }
        }
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

static GtkWidget *make_kpi_pill(const char *caption, GtkWidget **valueLabel,
                                const char *pillColorHex,
                                const char *valueColorHex)
{
    GtkWidget *event = gtk_event_box_new();
    GtkWidget *vbox  = gtk_vbox_new(FALSE, 3);
    GtkWidget *val   = gtk_label_new(NULL);
    GtkWidget *cap   = gtk_label_new(NULL);
    GdkColor pillBg;
    char capMarkup[160];

    gdk_color_parse(pillColorHex, &pillBg);
    gtk_widget_modify_bg(event, GTK_STATE_NORMAL, &pillBg);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    snprintf(capMarkup, sizeof(capMarkup),
             "<span foreground='white' size='small' weight='bold'>%s</span>",
             caption);
    gtk_label_set_markup(GTK_LABEL(cap), capMarkup);
    gtk_misc_set_alignment(GTK_MISC(cap), 0.5, 0.5);

    (void)valueColorHex; /* value markup set dynamically in refresh */
    gtk_misc_set_alignment(GTK_MISC(val), 0.5, 0.5);

    gtk_box_pack_start(GTK_BOX(vbox), val, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), cap, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(event), vbox);

    *valueLabel = val;
    return event;
}

static GtkWidget *make_admin_panel_card(GtkWidget *child, int width)
{
    GtkWidget *frame = gtk_frame_new(NULL);
    GtkWidget *event = gtk_event_box_new();
    GdkColor white;

    gdk_color_parse("#FFFFFF", &white);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_size_request(frame, width, -1);
    gtk_widget_modify_bg(event, GTK_STATE_NORMAL, &white);
    gtk_container_add(GTK_CONTAINER(event), child);
    gtk_container_add(GTK_CONTAINER(frame), event);
    return frame;
}

static GtkWidget *admin_panel_entry_row(const char *iconStock,
                                        const char *placeholder,
                                        GtkWidget **entry)
{
    GtkWidget *box  = gtk_hbox_new(FALSE, 8);
    GtkWidget *icon = gtk_image_new_from_stock(iconStock, GTK_ICON_SIZE_BUTTON);
    GtkWidget *text = gtk_entry_new();
    GdkColor white;

    gdk_color_parse("#FFFFFF", &white);
    gtk_widget_set_size_request(icon, 20, 20);
    gtk_widget_set_size_request(text, -1, 34);
    gtk_widget_modify_base(text, GTK_STATE_NORMAL, &white);
    gtk_entry_set_alignment(GTK_ENTRY(text), 0.0);
    set_entry_placeholder(text, placeholder);
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), text, TRUE, TRUE, 0);
    *entry = text;
    return box;
}

static GtkWidget *make_admin_action_button(const char *labelText,
                                           const char *stock,
                                           const char *bgHex,
                                           const char *prelightHex,
                                           const char *fgHex)
{
    GtkWidget *button  = gtk_button_new();
    GtkWidget *align   = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    GtkWidget *content = gtk_hbox_new(FALSE, 6);
    GtkWidget *icon    = gtk_image_new_from_stock(stock, GTK_ICON_SIZE_BUTTON);
    GtkWidget *label   = gtk_label_new(NULL);
    gchar *escaped     = g_markup_escape_text(labelText, -1);
    char markup[160];

    snprintf(markup, sizeof(markup),
             "<span weight='bold'>%s</span>", escaped);
    g_free(escaped);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_box_pack_start(GTK_BOX(content), icon,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), label, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(align),  content);
    gtk_container_add(GTK_CONTAINER(button), align);
    gtk_widget_set_size_request(button, -1, 36);
    style_action_button(button, bgHex, prelightHex, fgHex);
    return button;
}

static GtkWidget *admin_view(UiState *state)
{
    /* ---- outer shell ---- */
    GtkWidget *root = gtk_vbox_new(FALSE, 0);
    GtkWidget *heroBar;                     /* KPI hero bar */
    GtkWidget *heroBarInner;
    GtkWidget *tabs = gtk_notebook_new();
    GtkWidget *tabLabel;

    /* ---- KPI pill widgets ---- */
    GtkWidget *kpiRow;

    /* ---- Overview tab ---- */
    GtkWidget *overview = gtk_vbox_new(FALSE, 8);
    GtkWidget *overviewInner;
    GtkWidget *overviewEvent;
    GtkWidget *occupancyHeader = gtk_hbox_new(FALSE, 8);
    GtkWidget *occupancyTitle  = gtk_label_new(NULL);
    GtkListStore *occupancyStore;
    GtkWidget *occupancyCard;
    const char *occupancyTitles[] = {
        "Parking", "Capacity", "Occupied", "Revenue (TND)", "Agent"
    };

    /* ---- per-tab panes ---- */
    GtkWidget *usersTab      = gtk_hbox_new(FALSE, 8);
    GtkWidget *agentsTab     = gtk_hbox_new(FALSE, 8);
    GtkWidget *parkingsTab   = gtk_hbox_new(FALSE, 8);
    GtkWidget *complaintsTab = gtk_hbox_new(FALSE, 8);

    GtkWidget *usersPanelBox, *agentsPanelBox, *parkingPanelBox,
              *complaintPanelBox;
    GtkWidget *usersPanelCard, *agentsPanelCard, *parkingPanelCard,
              *complaintPanelCard;

    GtkListStore *store;
    GtkWidget *tree;
    GtkTreeSelection *selection;

    /* Shared parking combo for Agents tab */
    GtkWidget *parkingCombo = gtk_combo_box_new_text();

    /* Action buttons */
    GtkWidget *addUser, *editUser, *deleteUser;
    GtkWidget *addAgent, *editAgent, *deleteAgent, *assignAgent;
    GtkWidget *addParking, *updateParking, *deleteParking;
    GtkWidget *resolveComplaint, *deleteComplaint;
    GtkWidget *actionsTable;
    GtkWidget *detailScroller;

    const char *userTitles[]    = {"ID", "Username", "Email"};
    const char *agentTitles[]   = {"ID", "Username", "Email",
                                   "Assigned Parking"};
    const char *parkingTitles[] = {"ID", "Name", "Address/Zone",
                                   "Total Spots", "Available",
                                   "Price/Hr", "Agent"};
    const char *complaintTitles[] = {"ID", "From", "Subject", "Status"};

    int i;
    GdkColor navy, white, slate;
    gdk_color_parse("#1B2A4A", &navy);
    gdk_color_parse("#FFFFFF", &white);
    gdk_color_parse("#C5D3E6", &slate);

    gtk_container_set_border_width(GTK_CONTAINER(root), 0);

    /* ================================================================
       1. DASHBOARD HEADER  (#1B2A4A)
       ================================================================ */
    gtk_box_pack_start(GTK_BOX(root),
                       make_dashboard_header(state, "ADMIN"),
                       FALSE, FALSE, 0);

    /* ================================================================
       2. SYSTEM OVERVIEW HERO BAR — KPI pills on slate background
       ================================================================ */
    state->adminTotalUsers         = gtk_label_new(NULL);
    state->adminTotalParkings      = gtk_label_new(NULL);
    state->adminParkingAgents      = gtk_label_new(NULL);
    state->adminPendingComplaints  = gtk_label_new(NULL);
    state->adminGlobalOccupancy    = gtk_label_new(NULL);

    kpiRow = gtk_hbox_new(TRUE, 6);
    {
        /* pill colours: Total Users=blue, Parkings=indigo,
           Agents=teal, Complaints=amber-red, Occupancy=emerald */
        const char *pillColors[5] = {
            "#1F5FBF", "#4B3FA0", "#1A6B5C",
            "#B33A3A", "#287A4D"
        };
        const char *pillCaptions[5] = {
            "Total Users", "Total Parkings", "Parking Agents",
            "Pending Complaints", "Occupancy Rate"
        };
        GtkWidget *pillWidgets[5];
        pillWidgets[0] = make_kpi_pill(pillCaptions[0],
                             &state->adminTotalUsers,
                             pillColors[0], "#FFFFFF");
        pillWidgets[1] = make_kpi_pill(pillCaptions[1],
                             &state->adminTotalParkings,
                             pillColors[1], "#FFFFFF");
        pillWidgets[2] = make_kpi_pill(pillCaptions[2],
                             &state->adminParkingAgents,
                             pillColors[2], "#FFFFFF");
        pillWidgets[3] = make_kpi_pill(pillCaptions[3],
                             &state->adminPendingComplaints,
                             pillColors[3], "#FFFFFF");
        pillWidgets[4] = make_kpi_pill(pillCaptions[4],
                             &state->adminGlobalOccupancy,
                             pillColors[4], "#FFFFFF");
        for (i = 0; i < 5; ++i)
            gtk_box_pack_start(GTK_BOX(kpiRow), pillWidgets[i],
                               TRUE, TRUE, 0);
    }

    heroBarInner = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(heroBarInner), 8);
    gtk_box_pack_start(GTK_BOX(heroBarInner), kpiRow, FALSE, FALSE, 0);

    heroBar = gtk_event_box_new();
    gtk_widget_modify_bg(heroBar, GTK_STATE_NORMAL, &slate);
    gtk_container_add(GTK_CONTAINER(heroBar), heroBarInner);
    gtk_box_pack_start(GTK_BOX(root), heroBar, FALSE, FALSE, 0);

    /* ================================================================
       3. NOTEBOOK with icon+text tabs (gtk_widget_show_all on labels)
       ================================================================ */
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(tabs), TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(tabs), 4);

    /* -- Overview & Stats tab -- */
    overviewEvent = gtk_event_box_new();
    gtk_widget_modify_bg(overviewEvent, GTK_STATE_NORMAL, &slate);
    overviewInner = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(overviewInner), 10);

    gtk_label_set_markup(GTK_LABEL(occupancyTitle),
                         "<span foreground='#1B2A4A' size='large'"
                         " weight='bold'>"
                         "Parking Occupancy &amp; Revenue Summary"
                         "</span>");
    gtk_misc_set_alignment(GTK_MISC(occupancyTitle), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(occupancyHeader), occupancyTitle,
                       TRUE, TRUE, 0);

    occupancyCard = make_tree_view("Occupancy Summary", &occupancyStore,
                                   occupancyTitles, 5);
    state->adminOccupancy = occupancyCard;
    gtk_widget_set_size_request(occupancyCard, -1, 220);

    gtk_box_pack_start(GTK_BOX(overviewInner), occupancyHeader,
                       FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(overviewInner), occupancyCard,
                       TRUE, TRUE, 4);
    gtk_container_add(GTK_CONTAINER(overviewEvent), overviewInner);
    gtk_container_add(GTK_CONTAINER(overview), overviewEvent);

    tabLabel = make_stock_tab("Overview & Stats", GTK_STOCK_HOME);
    gtk_widget_show_all(tabLabel);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), overview, tabLabel);

    /* ================================================================
       4a. USERS TAB (Citizens only)
       ================================================================ */
    gtk_container_set_border_width(GTK_CONTAINER(usersTab), 8);

    state->adminUsers = make_tree_view("Citizens", &store, userTitles, 3);
    gtk_widget_set_size_request(state->adminUsers, -1, 400);
    gtk_box_pack_start(GTK_BOX(usersTab), state->adminUsers,
                       TRUE, TRUE, 0);

    /* right-hand management panel */
    usersPanelBox = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(usersPanelBox), 12);
    {
        GtkWidget *panelTitle = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(panelTitle),
            "<span foreground='#1B2A4A' size='large' weight='bold'>"
            "Citizen Management</span>");
        gtk_misc_set_alignment(GTK_MISC(panelTitle), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(usersPanelBox), panelTitle,
                           FALSE, FALSE, 2);
    }
    gtk_box_pack_start(GTK_BOX(usersPanelBox),
                       admin_panel_entry_row(GTK_STOCK_ORIENTATION_PORTRAIT,
                                             "Username",
                                             &state->adminCitizenName),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(usersPanelBox),
                       admin_panel_entry_row(GTK_STOCK_NETWORK, "Email",
                                             &state->adminCitizenEmail),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(usersPanelBox),
                       admin_panel_entry_row(GTK_STOCK_DIALOG_AUTHENTICATION,
                                             "Password",
                                             &state->adminCitizenPassword),
                       FALSE, FALSE, 2);

    addUser    = make_admin_action_button("Add",    GTK_STOCK_ADD,
                                          "#287A4D", "#1A5C38", "#FFFFFF");
    editUser   = make_admin_action_button("Edit",   GTK_STOCK_EDIT,
                                          "#1F5FBF", "#174AA0", "#FFFFFF");
    deleteUser = make_admin_action_button("Delete", GTK_STOCK_DELETE,
                                          "#B33A3A", "#8B2929", "#FFFFFF");
    gtk_widget_set_tooltip_text(addUser,    "Add new citizen");
    gtk_widget_set_tooltip_text(editUser,   "Edit selected citizen");
    gtk_widget_set_tooltip_text(deleteUser, "Delete selected citizen");

    {
        GtkWidget *actionsRow   = gtk_hbox_new(FALSE, 8);
        GtkWidget *actionsAlign = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
        gtk_box_pack_start(GTK_BOX(actionsRow), addUser,    FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(actionsRow), editUser,   FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(actionsRow), deleteUser, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(actionsAlign), actionsRow);
        gtk_box_pack_start(GTK_BOX(usersPanelBox), actionsAlign,
                           FALSE, FALSE, 6);
    }

    usersPanelCard = make_admin_panel_card(usersPanelBox, 280);
    gtk_box_pack_start(GTK_BOX(usersTab), usersPanelCard, FALSE, FALSE, 0);

    tabLabel = make_stock_tab("Users", GTK_STOCK_ORIENTATION_PORTRAIT);
    gtk_widget_show_all(tabLabel);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), usersTab, tabLabel);

    set_admin_action_role(addUser,    USER_CITIZEN);
    set_admin_action_role(editUser,   USER_CITIZEN);
    set_admin_action_role(deleteUser, USER_CITIZEN);
    g_signal_connect(addUser,    "clicked",
                     G_CALLBACK(admin_add_user_clicked),  state);
    g_signal_connect(editUser,   "clicked",
                     G_CALLBACK(admin_edit_user_clicked), state);
    g_signal_connect(deleteUser, "clicked",
                     G_CALLBACK(delete_user_clicked),     state);
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(
        gtk_bin_get_child(GTK_BIN(gtk_bin_get_child(GTK_BIN(
            state->adminUsers))))));
    g_object_set_data(G_OBJECT(selection), "parky-role",
                      GINT_TO_POINTER(USER_CITIZEN));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(admin_user_row_selected), state);

    /* ================================================================
       4b. AGENTS TAB (with Assigned Parking dropdown)
       ================================================================ */
    gtk_container_set_border_width(GTK_CONTAINER(agentsTab), 8);

    state->adminAgents = make_tree_view("Parking Agents", &store,
                                        agentTitles, 4);
    gtk_widget_set_size_request(state->adminAgents, -1, 400);
    gtk_box_pack_start(GTK_BOX(agentsTab), state->adminAgents,
                       TRUE, TRUE, 0);

    gtk_combo_box_append_text(GTK_COMBO_BOX(parkingCombo), "Unassigned");
    for (i = 0; i < state->data->parkingCount; ++i)
        gtk_combo_box_append_text(GTK_COMBO_BOX(parkingCombo),
                                  state->data->parkings[i].name);
    gtk_combo_box_set_active(GTK_COMBO_BOX(parkingCombo), 0);
    state->adminAgentParking = parkingCombo;

    agentsPanelBox = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(agentsPanelBox), 12);
    {
        GtkWidget *panelTitle = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(panelTitle),
            "<span foreground='#1B2A4A' size='large' weight='bold'>"
            "Agent Management</span>");
        gtk_misc_set_alignment(GTK_MISC(panelTitle), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(agentsPanelBox), panelTitle,
                           FALSE, FALSE, 2);
    }
    gtk_box_pack_start(GTK_BOX(agentsPanelBox),
                       admin_panel_entry_row(GTK_STOCK_ORIENTATION_PORTRAIT,
                                             "Username",
                                             &state->adminAgentName),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(agentsPanelBox),
                       admin_panel_entry_row(GTK_STOCK_NETWORK, "Email",
                                             &state->adminAgentEmail),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(agentsPanelBox),
                       admin_panel_entry_row(GTK_STOCK_DIALOG_AUTHENTICATION,
                                             "Password",
                                             &state->adminAgentPassword),
                       FALSE, FALSE, 2);
    {
        GtkWidget *assignLbl = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(assignLbl),
            "<span foreground='#475569' size='small'"
            " weight='bold'>Assigned Parking</span>");
        gtk_misc_set_alignment(GTK_MISC(assignLbl), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(agentsPanelBox), assignLbl,
                           FALSE, FALSE, 2);
    }
    gtk_box_pack_start(GTK_BOX(agentsPanelBox), parkingCombo,
                       FALSE, FALSE, 2);

    addAgent    = make_admin_action_button("Add",    GTK_STOCK_ADD,
                                           "#287A4D", "#1A5C38", "#FFFFFF");
    editAgent   = make_admin_action_button("Edit",   GTK_STOCK_EDIT,
                                           "#1F5FBF", "#174AA0", "#FFFFFF");
    deleteAgent = make_admin_action_button("Delete", GTK_STOCK_DELETE,
                                           "#B33A3A", "#8B2929", "#FFFFFF");
    assignAgent = make_admin_action_button("Assign", GTK_STOCK_APPLY,
                                           "#F2A900", "#D97706", "#1B2A4A");
    gtk_widget_set_tooltip_text(addAgent,    "Add new parking agent");
    gtk_widget_set_tooltip_text(editAgent,   "Edit selected agent");
    gtk_widget_set_tooltip_text(deleteAgent, "Delete selected agent");
    gtk_widget_set_tooltip_text(assignAgent,
                                "Assign selected agent to parking");

    /* Clean 2x2 grid: the 4th button (Assign) can never be clipped
       by the right edge of the 280px card. */
    actionsTable = gtk_table_new(2, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(actionsTable), 6);
    gtk_table_set_col_spacings(GTK_TABLE(actionsTable), 6);
    gtk_widget_set_size_request(addAgent,    116, 34);
    gtk_widget_set_size_request(editAgent,   116, 34);
    gtk_widget_set_size_request(deleteAgent, 116, 34);
    gtk_widget_set_size_request(assignAgent, 116, 34);
    gtk_table_attach(GTK_TABLE(actionsTable), addAgent,    0, 1, 0, 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(actionsTable), editAgent,   1, 2, 0, 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(actionsTable), deleteAgent, 0, 1, 1, 2,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(actionsTable), assignAgent, 1, 2, 1, 2,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_box_pack_start(GTK_BOX(agentsPanelBox), actionsTable,
                       FALSE, FALSE, 6);

    agentsPanelCard = make_admin_panel_card(agentsPanelBox, 280);
    gtk_box_pack_start(GTK_BOX(agentsTab), agentsPanelCard, FALSE, FALSE, 0);

    tabLabel = make_stock_tab("Agents", GTK_STOCK_EXECUTE);
    gtk_widget_show_all(tabLabel);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), agentsTab, tabLabel);

    set_admin_action_role(addAgent,    USER_AGENT);
    set_admin_action_role(editAgent,   USER_AGENT);
    set_admin_action_role(deleteAgent, USER_AGENT);
    set_admin_action_role(assignAgent, USER_AGENT);
    g_signal_connect(addAgent,    "clicked",
                     G_CALLBACK(admin_add_user_clicked),  state);
    g_signal_connect(editAgent,   "clicked",
                     G_CALLBACK(admin_edit_user_clicked), state);
    g_signal_connect(deleteAgent, "clicked",
                     G_CALLBACK(delete_user_clicked),     state);
    g_signal_connect(assignAgent, "clicked",
                     G_CALLBACK(admin_assign_clicked),    state);
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(
        gtk_bin_get_child(GTK_BIN(gtk_bin_get_child(GTK_BIN(
            state->adminAgents))))));
    g_object_set_data(G_OBJECT(selection), "parky-role",
                      GINT_TO_POINTER(USER_AGENT));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(admin_user_row_selected), state);

    /* ================================================================
       4c. PARKINGS TAB
       ================================================================ */
    gtk_container_set_border_width(GTK_CONTAINER(parkingsTab), 8);

    state->adminParkings = make_tree_view("Parkings", &store,
                                          parkingTitles, 7);
    gtk_widget_set_size_request(state->adminParkings, -1, 400);
    gtk_box_pack_start(GTK_BOX(parkingsTab), state->adminParkings,
                       TRUE, TRUE, 0);

    parkingPanelBox = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(parkingPanelBox), 12);
    {
        GtkWidget *panelTitle = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(panelTitle),
            "<span foreground='#1B2A4A' size='large' weight='bold'>"
            "Parking Management</span>");
        gtk_misc_set_alignment(GTK_MISC(panelTitle), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(parkingPanelBox), panelTitle,
                           FALSE, FALSE, 2);
    }
    gtk_box_pack_start(GTK_BOX(parkingPanelBox),
                       admin_panel_entry_row(GTK_STOCK_DND, "Name",
                                             &state->adminParkingName),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(parkingPanelBox),
                       admin_panel_entry_row(GTK_STOCK_JUMP_TO,
                                             "Address/Zone",
                                             &state->adminParkingAddress),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(parkingPanelBox),
                       admin_panel_entry_row(GTK_STOCK_INDEX, "Total Spots",
                                             &state->adminParkingTotal),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(parkingPanelBox),
                       admin_panel_entry_row(GTK_STOCK_APPLY, "Available",
                                             &state->adminParkingAvailable),
                       FALSE, FALSE, 2);

    addParking    = make_admin_action_button("Add",    GTK_STOCK_ADD,
                                             "#287A4D", "#1A5C38", "#FFFFFF");
    updateParking = make_admin_action_button("Edit",   GTK_STOCK_EDIT,
                                             "#1F5FBF", "#174AA0", "#FFFFFF");
    deleteParking = make_admin_action_button("Delete", GTK_STOCK_DELETE,
                                             "#B33A3A", "#8B2929", "#FFFFFF");
    gtk_widget_set_tooltip_text(addParking,    "Add new parking");
    gtk_widget_set_tooltip_text(updateParking, "Update selected parking");
    gtk_widget_set_tooltip_text(deleteParking, "Delete selected parking");

    {
        GtkWidget *actionsRow   = gtk_hbox_new(FALSE, 8);
        GtkWidget *actionsAlign = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
        gtk_box_pack_start(GTK_BOX(actionsRow), addParking,    FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(actionsRow), updateParking, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(actionsRow), deleteParking, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(actionsAlign), actionsRow);
        gtk_box_pack_start(GTK_BOX(parkingPanelBox), actionsAlign,
                           FALSE, FALSE, 6);
    }

    parkingPanelCard = make_admin_panel_card(parkingPanelBox, 280);
    gtk_box_pack_start(GTK_BOX(parkingsTab), parkingPanelCard,
                       FALSE, FALSE, 0);

    tabLabel = make_stock_tab("Parkings", GTK_STOCK_DIRECTORY);
    gtk_widget_show_all(tabLabel);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), parkingsTab, tabLabel);

    g_signal_connect(addParking,    "clicked",
                     G_CALLBACK(admin_add_parking_clicked),    state);
    g_signal_connect(updateParking, "clicked",
                     G_CALLBACK(admin_update_parking_clicked), state);
    g_signal_connect(deleteParking, "clicked",
                     G_CALLBACK(delete_parking_clicked),       state);
    tree = gtk_bin_get_child(GTK_BIN(
               gtk_bin_get_child(GTK_BIN(state->adminParkings))));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(admin_parking_row_selected), state);

    /* ================================================================
       4d. COMPLAINTS TAB
       ================================================================ */
    gtk_container_set_border_width(GTK_CONTAINER(complaintsTab), 8);

    state->adminComplaints = make_tree_view("Complaints", &store,
                                            complaintTitles, 4);
    gtk_widget_set_size_request(state->adminComplaints, -1, 400);
    gtk_box_pack_start(GTK_BOX(complaintsTab), state->adminComplaints,
                       TRUE, TRUE, 0);

    complaintPanelBox = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(complaintPanelBox), 12);
    {
        GtkWidget *panelTitle = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(panelTitle),
            "<span foreground='#1B2A4A' size='large' weight='bold'>"
            "Complaint Details</span>");
        gtk_misc_set_alignment(GTK_MISC(panelTitle), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(complaintPanelBox), panelTitle,
                           FALSE, FALSE, 2);
    }
    detailScroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(detailScroller),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    state->adminComplaintDetail = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(
        GTK_TEXT_VIEW(state->adminComplaintDetail), GTK_WRAP_WORD);
    gtk_text_view_set_editable(
        GTK_TEXT_VIEW(state->adminComplaintDetail), FALSE);
    gtk_widget_modify_base(state->adminComplaintDetail,
                           GTK_STATE_NORMAL, &white);
    gtk_widget_set_size_request(detailScroller, -1, 240);
    gtk_container_add(GTK_CONTAINER(detailScroller),
                      state->adminComplaintDetail);
    gtk_box_pack_start(GTK_BOX(complaintPanelBox), detailScroller,
                       TRUE, TRUE, 4);

    resolveComplaint = make_admin_action_button(
        "Resolve", GTK_STOCK_APPLY, "#287A4D", "#1A5C38", "#FFFFFF");
    deleteComplaint  = make_admin_action_button(
        "Delete",  GTK_STOCK_DELETE, "#B33A3A", "#8B2929", "#FFFFFF");
    gtk_widget_set_tooltip_text(resolveComplaint,
                                "Resolve selected complaint");
    gtk_widget_set_tooltip_text(deleteComplaint,
                                "Delete selected complaint");

    {
        GtkWidget *actionsRow   = gtk_hbox_new(FALSE, 8);
        GtkWidget *actionsAlign = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
        gtk_box_pack_start(GTK_BOX(actionsRow), resolveComplaint,
                           FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(actionsRow), deleteComplaint,
                           FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(actionsAlign), actionsRow);
        gtk_box_pack_start(GTK_BOX(complaintPanelBox), actionsAlign,
                           FALSE, FALSE, 6);
    }

    complaintPanelCard = make_admin_panel_card(complaintPanelBox, 280);
    gtk_box_pack_start(GTK_BOX(complaintsTab), complaintPanelCard,
                       FALSE, FALSE, 0);

    tabLabel = make_stock_tab("Complaints", GTK_STOCK_DIALOG_WARNING);
    gtk_widget_show_all(tabLabel);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), complaintsTab, tabLabel);

    g_signal_connect(resolveComplaint, "clicked",
                     G_CALLBACK(resolve_complaint_clicked), state);
    g_signal_connect(deleteComplaint,  "clicked",
                     G_CALLBACK(delete_complaint_clicked),  state);
    tree = gtk_bin_get_child(GTK_BIN(
               gtk_bin_get_child(GTK_BIN(state->adminComplaints))));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(admin_complaint_row_selected), state);

    /* ================================================================
       5. Pack notebook and populate tables
       ================================================================ */
    gtk_box_pack_start(GTK_BOX(root), tabs, TRUE, TRUE, 0);
    refresh_admin_tables(state);
    return root;
}

static GtkWidget *agent_view(UiState *state)
{
    GtkWidget *root = gtk_vbox_new(FALSE, 10);
    GtkWidget *header = make_dashboard_header(state, "AGENT");
    GtkWidget *heroInner = gtk_hbox_new(FALSE, 16);
    GtkWidget *heroCard;
    GtkWidget *heroInfo = gtk_vbox_new(FALSE, 4);
    GtkWidget *heroCounters = gtk_hbox_new(TRUE, 14);
    GtkWidget *heroButtons = gtk_hbox_new(FALSE, 10);
    GtkWidget *tabs = gtk_notebook_new();

    GtkWidget *reservationsTab = gtk_hbox_new(FALSE, 10);
    GtkWidget *resTabLabel;
    GtkWidget *resPanelBox = gtk_vbox_new(FALSE, 12);
    GtkWidget *resPanelCard;
    GtkWidget *resPanelTitle = gtk_label_new(NULL);
    GtkWidget *resPanelHint = gtk_label_new(NULL);
    GtkWidget *approveBtn;
    GtkWidget *rejectBtn;

    GtkWidget *servicesTab = gtk_hbox_new(FALSE, 10);
    GtkWidget *svcTabLabel;
    GtkWidget *svcPanelBox = gtk_vbox_new(FALSE, 10);
    GtkWidget *svcPanelCard;
    GtkWidget *svcPanelTitle = gtk_label_new(NULL);
    GtkWidget *svcActions = gtk_hbox_new(FALSE, 8);
    GtkWidget *svcActionsAlign = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    GtkWidget *addSvcBtn;
    GtkWidget *editSvcBtn;
    GtkWidget *toggleSvcBtn;
    GtkWidget *deleteSvcBtn;

    GtkWidget *issueTab = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    GtkWidget *issueTabLabel;
    GtkWidget *issueCardBox = gtk_vbox_new(FALSE, 12);
    GtkWidget *issueCard;
    GtkWidget *issueTitle = gtk_label_new(NULL);
    GtkWidget *issueSubtitle = gtk_label_new(NULL);
    GtkWidget *issueDescLabel = gtk_label_new(NULL);
    GtkWidget *issueDescFrame = gtk_frame_new(NULL);
    GtkWidget *issueScroller = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *sendReportBtn;
    GtkWidget *sendReportAlign = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);

    GtkListStore *store;
    GtkWidget *tree;
    GtkTreeSelection *selection;
    GdkColor white;
    const char *reservationTitles[] = {
        "ID", "Citizen", "Date", "Time", "Services", "Total (TND)", "Status"
    };
    const char *serviceTitles[] = {
        "ID", "Service Name", "Description", "Price (TND)", "Status"
    };

    gdk_color_parse("#FFFFFF", &white);
    gtk_container_set_border_width(GTK_CONTAINER(root), 8);
    gtk_box_pack_start(GTK_BOX(root), header, FALSE, FALSE, 0);

    /* Assigned Parking Hero Card */
    state->agentParkingHeroTitle = gtk_label_new(NULL);
    state->agentParkingRate = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(state->agentParkingHeroTitle), 0.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(state->agentParkingRate), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(heroInfo), state->agentParkingHeroTitle,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(heroInfo), state->agentParkingRate,
                       FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(heroCounters),
                       make_spot_counter_tile("Available Spots",
                                              &state->agentSpotsAvailable),
                       TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(heroCounters),
                       make_spot_counter_tile("Occupied Spots",
                                              &state->agentSpotsOccupied),
                       TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(heroCounters),
                       make_spot_counter_tile("Total Capacity",
                                              &state->agentSpotsTotal),
                       TRUE, TRUE, 0);

    state->agentSpotPlusBtn = make_agent_button(
        "+1 Free Spot", GTK_STOCK_ADD,
        "#10B981", "#059669", "#FFFFFF", 160, 48);
    state->agentSpotMinusBtn = make_agent_button(
        "-1 Spot Occupied", GTK_STOCK_REMOVE,
        "#EF4444", "#DC2626", "#FFFFFF", 178, 48);
    gtk_box_pack_start(GTK_BOX(heroButtons), state->agentSpotPlusBtn,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(heroButtons), state->agentSpotMinusBtn,
                       FALSE, FALSE, 0);

    gtk_container_set_border_width(GTK_CONTAINER(heroInner), 14);
    gtk_box_pack_start(GTK_BOX(heroInner), heroInfo, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(heroInner), heroCounters, FALSE, FALSE, 12);
    gtk_box_pack_start(GTK_BOX(heroInner), heroButtons, FALSE, FALSE, 0);
    heroCard = make_white_card(-1, heroInner);
    gtk_box_pack_start(GTK_BOX(root), heroCard, FALSE, FALSE, 0);

    /* 3 Simple Visual Tabs */
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(tabs), TRUE);

    /* Tab 1: Reservations */
    gtk_container_set_border_width(GTK_CONTAINER(reservationsTab), 8);
    state->agentReservations = make_tree_view_markup(
        "Assigned Parking Reservations", &store, reservationTitles, 7, 6);
    gtk_box_pack_start(GTK_BOX(reservationsTab), state->agentReservations,
                       TRUE, TRUE, 0);

    gtk_container_set_border_width(GTK_CONTAINER(resPanelBox), 14);
    gtk_label_set_markup(GTK_LABEL(resPanelTitle),
        "<span foreground='#1B2A4A' size='large' weight='bold'>"
        "Reservation Actions</span>");
    gtk_misc_set_alignment(GTK_MISC(resPanelTitle), 0.0, 0.5);
    gtk_label_set_markup(GTK_LABEL(resPanelHint),
        "<span foreground='#475569'>Select a reservation in the list, "
        "then tap Approve or Reject.</span>");
    gtk_label_set_line_wrap(GTK_LABEL(resPanelHint), TRUE);
    gtk_misc_set_alignment(GTK_MISC(resPanelHint), 0.0, 0.5);
    gtk_widget_set_size_request(resPanelHint, 224, -1);

    approveBtn = make_agent_button(
        "Approve", GTK_STOCK_YES, "#10B981", "#059669", "#FFFFFF", -1, 48);
    rejectBtn = make_agent_button(
        "Reject", GTK_STOCK_NO, "#EF4444", "#DC2626", "#FFFFFF", -1, 48);

    gtk_box_pack_start(GTK_BOX(resPanelBox), resPanelTitle, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(resPanelBox), resPanelHint, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(resPanelBox), approveBtn, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(resPanelBox), rejectBtn, FALSE, FALSE, 4);
    resPanelCard = make_white_card(260, resPanelBox);
    gtk_box_pack_start(GTK_BOX(reservationsTab), resPanelCard, FALSE, FALSE, 0);
    resTabLabel = make_stock_tab("Reservations", GTK_STOCK_INDEX);
    gtk_widget_show_all(resTabLabel);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), reservationsTab, resTabLabel);

    /* Tab 2: Services */
    gtk_container_set_border_width(GTK_CONTAINER(servicesTab), 8);
    state->agentServices = make_tree_view_markup(
        "Parking Services", &store, serviceTitles, 5, 4);
    gtk_box_pack_start(GTK_BOX(servicesTab), state->agentServices,
                       TRUE, TRUE, 0);

    gtk_container_set_border_width(GTK_CONTAINER(svcPanelBox), 14);
    gtk_label_set_markup(GTK_LABEL(svcPanelTitle),
        "<span foreground='#1B2A4A' size='large' weight='bold'>"
        "Service Details</span>");
    gtk_misc_set_alignment(GTK_MISC(svcPanelTitle), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(svcPanelBox), svcPanelTitle, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(svcPanelBox),
                       agent_entry_row(GTK_STOCK_EDIT, "Service Name",
                                       &state->agentServiceName),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(svcPanelBox),
                       agent_entry_row(GTK_STOCK_PROPERTIES, "Description",
                                       &state->agentServiceDescription),
                       FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(svcPanelBox),
                       agent_entry_row(GTK_STOCK_APPLY, "Price (TND)",
                                       &state->agentServicePrice),
                       FALSE, FALSE, 2);

    addSvcBtn = make_icon_button(GTK_STOCK_ADD, "Add Service");
    editSvcBtn = make_icon_button(GTK_STOCK_EDIT, "Edit Selected Service");
    toggleSvcBtn = make_icon_button(GTK_STOCK_REFRESH,
                                    "Toggle Active / Inactive");
    deleteSvcBtn = make_icon_button(GTK_STOCK_DELETE,
                                    "Delete Selected Service");
    style_action_button(addSvcBtn, "#10B981", "#059669", "#FFFFFF");
    style_action_button(editSvcBtn, "#1F5FBF", "#1D4ED8", "#FFFFFF");
    style_action_button(toggleSvcBtn, "#F2A900", "#D97706", "#1B2A4A");
    style_action_button(deleteSvcBtn, "#EF4444", "#DC2626", "#FFFFFF");
    gtk_widget_set_size_request(addSvcBtn, 46, 38);
    gtk_widget_set_size_request(editSvcBtn, 46, 38);
    gtk_widget_set_size_request(toggleSvcBtn, 46, 38);
    gtk_widget_set_size_request(deleteSvcBtn, 46, 38);
    gtk_box_pack_start(GTK_BOX(svcActions), addSvcBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(svcActions), editSvcBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(svcActions), toggleSvcBtn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(svcActions), deleteSvcBtn, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(svcActionsAlign), svcActions);
    gtk_box_pack_start(GTK_BOX(svcPanelBox), svcActionsAlign,
                       FALSE, FALSE, 8);

    svcPanelCard = make_white_card(260, svcPanelBox);
    gtk_box_pack_start(GTK_BOX(servicesTab), svcPanelCard, FALSE, FALSE, 0);
    svcTabLabel = make_stock_tab("Services", GTK_STOCK_PREFERENCES);
    gtk_widget_show_all(svcTabLabel);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), servicesTab, svcTabLabel);

    /* Tab 3: Report Issue */
    gtk_container_set_border_width(GTK_CONTAINER(issueCardBox), 20);
    gtk_label_set_markup(GTK_LABEL(issueTitle),
        "<span foreground='#1B2A4A' size='x-large' weight='bold'>"
        "Report an Issue</span>");
    gtk_misc_set_alignment(GTK_MISC(issueTitle), 0.5, 0.5);
    gtk_label_set_markup(GTK_LABEL(issueSubtitle),
        "<span foreground='#475569'>"
        "Notify the administrator about a spot, gate, or customer issue."
        "</span>");
    gtk_misc_set_alignment(GTK_MISC(issueSubtitle), 0.5, 0.5);
    gtk_label_set_line_wrap(GTK_LABEL(issueSubtitle), TRUE);
    gtk_label_set_markup(GTK_LABEL(issueDescLabel),
        "<span foreground='#1B2A4A' weight='bold'>Issue Description</span>");
    gtk_misc_set_alignment(GTK_MISC(issueDescLabel), 0.0, 0.5);

    state->agentComplaintText = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(state->agentComplaintText),
                                GTK_WRAP_WORD);
    gtk_widget_modify_base(state->agentComplaintText, GTK_STATE_NORMAL, &white);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(issueScroller),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(issueScroller, -1, 160);
    gtk_container_add(GTK_CONTAINER(issueScroller), state->agentComplaintText);
    gtk_frame_set_shadow_type(GTK_FRAME(issueDescFrame), GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(issueDescFrame), issueScroller);

    sendReportBtn = make_agent_button(
        "Send Report", GTK_STOCK_APPLY,
        "#1F5FBF", "#1D4ED8", "#FFFFFF", 220, 48);
    gtk_container_add(GTK_CONTAINER(sendReportAlign), sendReportBtn);

    gtk_box_pack_start(GTK_BOX(issueCardBox), issueTitle, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(issueCardBox), issueSubtitle, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(issueCardBox),
                       agent_entry_row(GTK_STOCK_DIALOG_WARNING,
                                       "Issue Subject",
                                       &state->agentComplaintSubject),
                       FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(issueCardBox), issueDescLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(issueCardBox), issueDescFrame, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(issueCardBox), sendReportAlign, FALSE, FALSE, 6);

    issueCard = make_white_card(460, issueCardBox);
    gtk_container_add(GTK_CONTAINER(issueTab), issueCard);
    issueTabLabel = make_stock_tab("Report Issue", GTK_STOCK_DIALOG_WARNING);
    gtk_widget_show_all(issueTabLabel);
    gtk_notebook_append_page(GTK_NOTEBOOK(tabs), issueTab, issueTabLabel);

    gtk_box_pack_start(GTK_BOX(root), tabs, TRUE, TRUE, 0);

    g_signal_connect(state->agentSpotPlusBtn, "clicked",
                     G_CALLBACK(mark_available_clicked), state);
    g_signal_connect(state->agentSpotMinusBtn, "clicked",
                     G_CALLBACK(mark_occupied_clicked), state);
    g_signal_connect(approveBtn, "clicked",
                     G_CALLBACK(approve_reservation_clicked), state);
    g_signal_connect(rejectBtn, "clicked",
                     G_CALLBACK(reject_reservation_clicked), state);
    g_signal_connect(addSvcBtn, "clicked",
                     G_CALLBACK(add_service_clicked), state);
    g_signal_connect(editSvcBtn, "clicked",
                     G_CALLBACK(edit_service_clicked), state);
    g_signal_connect(toggleSvcBtn, "clicked",
                     G_CALLBACK(toggle_service_clicked), state);
    g_signal_connect(deleteSvcBtn, "clicked",
                     G_CALLBACK(delete_service_clicked), state);
    g_signal_connect(sendReportBtn, "clicked",
                     G_CALLBACK(send_agent_complaint_clicked), state);

    tree = gtk_bin_get_child(GTK_BIN(
        gtk_bin_get_child(GTK_BIN(state->agentServices))));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(agent_service_row_selected), state);

    refresh_agent_tables(state);
    return root;
}

/* ---- Citizen real-time helpers ---- */

static gboolean on_citizen_realtime_tick(gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int i;
    int isCitizen = 0;

    if (state == NULL || state->activeUserId <= 0 ||
        state->notebook == NULL) {
        if (state != NULL)
            state->citizenRefreshTimerId = 0;
        return FALSE;
    }
    for (i = 0; i < state->data->userCount; ++i) {
        if (state->data->users[i].id == state->activeUserId) {
            isCitizen = (state->data->users[i].role == USER_CITIZEN);
            break;
        }
    }
    if (!isCitizen) {
        state->citizenRefreshTimerId = 0;
        return FALSE;
    }
    /* Only reload while the Citizen dashboard is active */
    if (gtk_notebook_get_current_page(
            GTK_NOTEBOOK(state->notebook)) == 3 &&
        storage_load(state->data))
        refresh_citizen_tables(state);
    return TRUE;
}

static void start_citizen_realtime_timer(UiState *state)
{
    stop_citizen_realtime_timer(state);
    if (state != NULL)
        state->citizenRefreshTimerId =
            g_timeout_add(2000, on_citizen_realtime_tick, state);
}

static void stop_citizen_realtime_timer(UiState *state)
{
    if (state != NULL && state->citizenRefreshTimerId != 0) {
        g_source_remove(state->citizenRefreshTimerId);
        state->citizenRefreshTimerId = 0;
    }
}

/* Opens a modal calendar dialog and writes the chosen date as YYYY-MM-DD
   into the date display label (stored under "parky-date" GObject data). */
static void pick_date_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    GtkWidget *dialog;
    GtkWidget *cal;
    GtkWidget *content;
    guint year = 0, month = 0, day = 0;
    char dateBuf[16];
    gint response;
    (void)widget;

    dialog = gtk_dialog_new_with_buttons(
        "Pick a Date",
        GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_STOCK_OK, GTK_RESPONSE_OK,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        NULL);
    gtk_window_set_position(GTK_WINDOW(dialog),
                            GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

    cal = gtk_calendar_new();
    gtk_calendar_set_display_options(GTK_CALENDAR(cal),
        GTK_CALENDAR_SHOW_HEADING |
        GTK_CALENDAR_SHOW_DAY_NAMES);

    content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);
    gtk_box_pack_start(GTK_BOX(content), cal, TRUE, TRUE, 0);
    gtk_widget_show_all(dialog);

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_OK) {
        gtk_calendar_get_date(GTK_CALENDAR(cal), &year, &month, &day);
        snprintf(dateBuf, sizeof(dateBuf),
                 "%04u-%02u-%02u", year, month + 1, day);

        /* Store the string on the label widget for reserve_clicked to read */
        g_object_set_data_full(G_OBJECT(state->citizenDate),
                               "parky-date",
                               g_strdup(dateBuf), g_free);

        /* Update the visible label text */
        {
            char markup[64];
            snprintf(markup, sizeof(markup),
                     "<span foreground='#1B2A4A' weight='bold'>%s</span>",
                     dateBuf);
            gtk_label_set_markup(GTK_LABEL(state->citizenDate), markup);
        }
    }
    gtk_widget_destroy(dialog);
}

/* Called when the user clicks a row in the Available Parkings table.
   Sets citizenParking (hidden entry) and updates the booking card label. */
static void citizen_parking_row_selected(GtkTreeSelection *selection,
                                         gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *idText  = NULL;
    gchar *nameText = NULL;
    int parkingId;
    int i;
    char labelMarkup[256];

    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;
    gtk_tree_model_get(model, &iter, 0, &idText, 1, &nameText, -1);
    parkingId = idText != NULL ? atoi(idText) : 0;
    g_free(idText);

    /* Store the ID in the hidden entry so reserve_clicked can read it */
    if (state->citizenParking != NULL) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", parkingId);
        gtk_entry_set_text(GTK_ENTRY(state->citizenParking), buf);
    }

    /* Update the "Selected:" label in the booking card */
    if (state->citizenSelectedLabel != NULL) {
        if (nameText != NULL && nameText[0] != '\0') {
            gchar *escaped = g_markup_escape_text(nameText, -1);
            snprintf(labelMarkup, sizeof(labelMarkup),
                     "<span foreground='#1B2A4A' weight='bold'>%s</span>",
                     escaped);
            g_free(escaped);
        } else {
            snprintf(labelMarkup, sizeof(labelMarkup),
                     "<span foreground='#8A94A6'>No parking selected</span>");
        }
        gtk_label_set_markup(GTK_LABEL(state->citizenSelectedLabel),
                             labelMarkup);
    }
    g_free(nameText);

    /* Refresh the Extra Service combo for the newly selected parking */
    if (state->citizenService != NULL) {
        int active = gtk_combo_box_get_active(
            GTK_COMBO_BOX(state->citizenService));
        gtk_list_store_clear(GTK_LIST_STORE(
            gtk_combo_box_get_model(
                GTK_COMBO_BOX(state->citizenService))));
        gtk_combo_box_append_text(GTK_COMBO_BOX(state->citizenService),
                                  "No extra service");
        for (i = 0; i < state->data->serviceCount; ++i) {
            if (state->data->services[i].status == 1) {
                char svcLabel[PARKY_NAME_LENGTH + 20];
                char displayName[PARKY_NAME_LENGTH];
                /* Translate storage underscores to spaces */
                int k;
                for (k = 0; state->data->services[i].name[k] != '\0' &&
                            k < PARKY_NAME_LENGTH - 1; ++k) {
                    displayName[k] =
                        state->data->services[i].name[k] == '_' ?
                        ' ' : state->data->services[i].name[k];
                }
                displayName[k] = '\0';
                snprintf(svcLabel, sizeof(svcLabel), "%s (%.2f TND)",
                         displayName,
                         (double)state->data->services[i].price);
                gtk_combo_box_append_text(
                    GTK_COMBO_BOX(state->citizenService), svcLabel);
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(state->citizenService),
                                 active > 0 ? active : 0);
    }
    update_tariff(state);
}

/* Cancel Reservation button handler */
static void cancel_reservation_clicked(GtkWidget *widget,
                                        gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    GtkWidget *resTree;
    GtkTreeSelection *sel;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *txt = NULL;
    int resId = 0;
    int i, index = -1;
    (void)widget;

    resTree = gtk_bin_get_child(GTK_BIN(
        gtk_bin_get_child(GTK_BIN(state->citizenReservations))));
    sel   = gtk_tree_view_get_selection(GTK_TREE_VIEW(resTree));
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(resTree));
    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) {
        show_message(state, GTK_MESSAGE_WARNING, "No reservation selected",
                     "Select a reservation from the list first.");
        return;
    }
    gtk_tree_model_get(model, &iter, 0, &txt, -1);
    resId = txt != NULL ? atoi(txt) : 0;
    g_free(txt);
    if (resId <= 0)
        return;

    /* Only allow cancellation of Pending reservations */
    for (i = 0; i < state->data->reservationCount; ++i) {
        if (state->data->reservations[i].id == resId) {
            if (state->data->reservations[i].status != RESERVATION_PENDING) {
                show_message(state, GTK_MESSAGE_INFO,
                             "Cannot cancel",
                             "Only pending reservations can be cancelled.");
                return;
            }
            index = i;
            break;
        }
    }
    if (index < 0)
        return;

    if (show_dialog(NULL, GTK_MESSAGE_WARNING,
                    "Confirm cancel",
                    "Cancel this reservation? This action cannot be undone.")
            != GTK_RESPONSE_YES)
        return;

    /* Remove the reservation */
    for (i = index; i < state->data->reservationCount - 1; ++i)
        state->data->reservations[i] = state->data->reservations[i + 1];
    --state->data->reservationCount;

    if (storage_save(state->data)) {
        show_message(state, GTK_MESSAGE_INFO, "Reservation cancelled",
                     "Your reservation was cancelled.");
        refresh_citizen_tables(state);
        refresh_admin_tables(state);
        refresh_agent_tables(state);
    }
}

static GtkWidget *citizen_view(UiState *state)
{
    /* ---- outer shell ---- */
    GtkWidget *box   = gtk_vbox_new(FALSE, 0);
    GtkWidget *inner = gtk_notebook_new();
    GtkWidget *tabLabel;

    /* ---- Tab 1: Find & Book ---- */
    GtkWidget *findTab    = gtk_vbox_new(FALSE, 6);
    GtkWidget *filterBar  = gtk_hbox_new(FALSE, 8);
    GtkWidget *splitPane  = gtk_hpaned_new();
    GtkWidget *leftSide   = gtk_vbox_new(FALSE, 4);
    GtkWidget *bookCard;        /* 290px right panel */
    GtkWidget *bookCardBox = gtk_vbox_new(FALSE, 8);

    /* Booking card widgets */
    GtkWidget *bookTitle     = gtk_label_new(NULL);
    GtkWidget *selectedLabel = gtk_label_new(NULL);
    GtkWidget *confirmBtn;

    /* ---- Tab 2: My Reservations ---- */
    GtkWidget *resTab  = gtk_vbox_new(FALSE, 6);
    GtkWidget *resActBar;
    GtkWidget *cancelBtn;

    /* ---- Tab 3: Reviews & Complaints ---- */
    GtkWidget *feedTab      = gtk_hbox_new(TRUE, 14);
    GtkWidget *reviewCard;
    GtkWidget *reviewCardBox = gtk_vbox_new(FALSE, 8);
    GtkWidget *reviewTitle   = gtk_label_new(NULL);
    GtkWidget *reviewBtn;
    GtkWidget *complaintCard;
    GtkWidget *complaintCardBox = gtk_vbox_new(FALSE, 8);
    GtkWidget *complaintTitle   = gtk_label_new(NULL);
    GtkWidget *complaintBtn;
    GtkWidget *communityReviews;

    GtkListStore *store;
    GtkWidget    *tree;
    GtkTreeSelection *selection;
    GtkAdjustment *ratingAdj;
    GdkColor white, slate;
    GtkWidget *scroller;

    /* Column title arrays */
    const char *parkingTitles[] = {
        "ID", "Name", "Address / Zone", "Available", "Hourly Rate"
    };
    const char *resTitles[] = {
        "ID", "Parking", "Spot", "Date", "Time", "Total (TND)", "Status"
    };
    const char *reviewTitles[] = {"Rating", "Comment"};

    gdk_color_parse("#FFFFFF", &white);
    gdk_color_parse("#C5D3E6", &slate);

    gtk_container_set_border_width(GTK_CONTAINER(box), 0);

    /* ================================================================
       HEADER
       ================================================================ */
    gtk_box_pack_start(GTK_BOX(box),
                       make_dashboard_header(state, "CITIZEN"),
                       FALSE, FALSE, 0);

    /* ================================================================
       TAB 1 — FIND & BOOK
       ================================================================ */
    gtk_container_set_border_width(GTK_CONTAINER(findTab), 8);

    /* Filter bar */
    gtk_box_pack_start(GTK_BOX(filterBar),
                       make_labeled_entry("Parking name",
                                          &state->citizenName),
                       TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(filterBar),
                       make_labeled_entry("Address",
                                          &state->citizenAddress),
                       TRUE, TRUE, 2);
    state->citizenAvailable =
        gtk_check_button_new_with_label("Only available");
    gtk_box_pack_start(GTK_BOX(filterBar), state->citizenAvailable,
                       FALSE, FALSE, 2);
    {
        GtkWidget *clearBtn = make_icon_button(GTK_STOCK_CLEAR,
                                               "Clear filters");
        style_action_button(clearBtn, "#E2E8F0", "#CBD5E1", "#1B2A4A");
        g_signal_connect(clearBtn, "clicked",
                         G_CALLBACK(clear_filters_clicked), state);
        gtk_box_pack_start(GTK_BOX(filterBar), clearBtn,
                           FALSE, FALSE, 2);
    }
    gtk_box_pack_start(GTK_BOX(findTab), filterBar, FALSE, FALSE, 2);

    /* Live filter: re-run search on every keystroke or toggle */
    g_signal_connect(state->citizenName,    "changed",
                     G_CALLBACK(search_clicked), state);
    g_signal_connect(state->citizenAddress, "changed",
                     G_CALLBACK(search_clicked), state);
    g_signal_connect(state->citizenAvailable, "toggled",
                     G_CALLBACK(search_clicked), state);

    /* Left: header row (title left, matched-badge right) + parkings table */
    {
        GtkWidget *tableHeader = gtk_hbox_new(FALSE, 6);
        GtkWidget *tableTitle  = gtk_label_new(NULL);
        GtkWidget *badgeLbl    = gtk_label_new(NULL);
        GtkWidget *spacer      = gtk_label_new("");

        gtk_label_set_markup(GTK_LABEL(tableTitle),
            "<span foreground='#1B2A4A' weight='bold'>Available Parkings</span>");
        gtk_misc_set_alignment(GTK_MISC(tableTitle), 0.0, 0.5);

        gtk_label_set_markup(GTK_LABEL(badgeLbl),
            "<span background='#E2E8F0' foreground='#475569'"
            " size='small'> — </span>");
        gtk_misc_set_alignment(GTK_MISC(badgeLbl), 1.0, 0.5);
        state->citizenSearchStatus = badgeLbl;

        gtk_box_pack_start(GTK_BOX(tableHeader), tableTitle,
                           FALSE, FALSE, 2);
        gtk_box_pack_start(GTK_BOX(tableHeader), spacer,
                           TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(tableHeader), badgeLbl,
                           FALSE, FALSE, 2);
        gtk_box_pack_start(GTK_BOX(leftSide), tableHeader,
                           FALSE, FALSE, 2);
    }

    /* Available Parkings table (ID hidden col 0) */
    state->citizenParkings =
        make_tree_view("Available Parkings", &store, parkingTitles, 5);
    gtk_widget_set_size_request(state->citizenParkings, -1, 300);
    gtk_box_pack_start(GTK_BOX(leftSide), state->citizenParkings,
                       TRUE, TRUE, 0);

    /* Right: booking card — 290 px */
    gtk_container_set_border_width(GTK_CONTAINER(bookCardBox), 12);

    gtk_label_set_markup(GTK_LABEL(bookTitle),
        "<span foreground='#1B2A4A' size='large' weight='bold'>"
        "Book a Spot</span>");
    gtk_misc_set_alignment(GTK_MISC(bookTitle), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(bookCardBox), bookTitle, FALSE, FALSE, 2);

    /* Selected parking display */
    gtk_label_set_markup(GTK_LABEL(selectedLabel),
        "<span foreground='#8A94A6'>No parking selected</span>");
    gtk_misc_set_alignment(GTK_MISC(selectedLabel), 0.0, 0.5);
    gtk_label_set_line_wrap(GTK_LABEL(selectedLabel), TRUE);
    gtk_widget_set_size_request(selectedLabel, 250, -1);
    gtk_box_pack_start(GTK_BOX(bookCardBox), selectedLabel,
                       FALSE, FALSE, 2);
    state->citizenSelectedLabel = selectedLabel;

    /* Hidden GtkEntry for parking ID — never shown, just carries the value */
    state->citizenParking = gtk_entry_new();
    gtk_widget_set_no_show_all(state->citizenParking, TRUE);

    /* Hidden GtkEntry for hours — always "1", drives tariff calc */
    state->citizenHours = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(state->citizenHours), "1");
    gtk_widget_set_no_show_all(state->citizenHours, TRUE);

    /* Spot Number */
    gtk_box_pack_start(GTK_BOX(bookCardBox),
                       admin_panel_entry_row(GTK_STOCK_INDEX,
                                             "Spot Number (optional)",
                                             &state->citizenSpot),
                       FALSE, FALSE, 2);

    /* Date — display label + calendar picker button */
    {
        GtkWidget *dateRow  = gtk_hbox_new(FALSE, 6);
        GtkWidget *dateIcon = gtk_image_new_from_stock(
            GTK_STOCK_INDEX, GTK_ICON_SIZE_BUTTON);
        GtkWidget *dateLabel = gtk_label_new(NULL);
        GtkWidget *calBtn;
        char todayBuf[16];
        char todayMarkup[64];
        time_t now = time(NULL);
        struct tm *lt = localtime(&now);

        snprintf(todayBuf, sizeof(todayBuf), "%04d-%02d-%02d",
                 lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);
        snprintf(todayMarkup, sizeof(todayMarkup),
                 "<span foreground='#1B2A4A' weight='bold'>%s</span>",
                 todayBuf);
        gtk_label_set_markup(GTK_LABEL(dateLabel), todayMarkup);
        gtk_misc_set_alignment(GTK_MISC(dateLabel), 0.0, 0.5);

        /* Store today as the initial "parky-date" value */
        g_object_set_data_full(G_OBJECT(dateLabel), "parky-date",
                               g_strdup(todayBuf), g_free);
        state->citizenDate = dateLabel;

        calBtn = gtk_button_new();
        gtk_container_add(GTK_CONTAINER(calBtn),
            gtk_image_new_from_stock(GTK_STOCK_INDEX,
                                     GTK_ICON_SIZE_BUTTON));
        gtk_widget_set_size_request(calBtn, 32, 32);
        gtk_widget_set_tooltip_text(calBtn, "Pick Date");
        g_signal_connect(calBtn, "clicked",
                         G_CALLBACK(pick_date_clicked), state);

        gtk_box_pack_start(GTK_BOX(dateRow), dateIcon,
                           FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(dateRow), dateLabel,
                           TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(dateRow), calBtn,
                           FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(bookCardBox), dateRow,
                           FALSE, FALSE, 2);
    }

    /* Time — clock icon + hour spin + ":" + minute spin */
    {
        GtkWidget *timeRow   = gtk_hbox_new(FALSE, 4);
        GtkWidget *timeIcon  = gtk_image_new_from_stock(
            GTK_STOCK_EXECUTE, GTK_ICON_SIZE_BUTTON);
        GtkAdjustment *hourAdj = GTK_ADJUSTMENT(
            gtk_adjustment_new(8.0, 0.0, 23.0, 1.0, 1.0, 0.0));
        GtkAdjustment *minAdj  = GTK_ADJUSTMENT(
            gtk_adjustment_new(0.0, 0.0, 55.0, 5.0, 5.0, 0.0));
        GtkWidget *hourSpin  = gtk_spin_button_new(hourAdj, 1.0, 0);
        GtkWidget *colonLbl  = gtk_label_new(":");
        GtkWidget *minSpin   = gtk_spin_button_new(minAdj, 1.0, 0);

        gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(hourSpin), TRUE);
        gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(minSpin),  TRUE);
        gtk_entry_set_width_chars(GTK_ENTRY(hourSpin), 3);
        gtk_entry_set_width_chars(GTK_ENTRY(minSpin),  3);
        gtk_widget_set_size_request(hourSpin, 52, 32);
        gtk_widget_set_size_request(minSpin,  52, 32);

        state->citizenHourSpin   = hourSpin;
        state->citizenMinuteSpin = minSpin;
        /* citizenTime not used for reading; set NULL for safety */
        state->citizenTime = NULL;

        gtk_box_pack_start(GTK_BOX(timeRow), timeIcon,
                           FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(timeRow), hourSpin,
                           FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(timeRow), colonLbl,
                           FALSE, FALSE, 2);
        gtk_box_pack_start(GTK_BOX(timeRow), minSpin,
                           FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(bookCardBox), timeRow,
                           FALSE, FALSE, 2);
    }

    /* Extra service combo — populated dynamically on row-select */
    {
        GtkWidget *svcLbl = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(svcLbl),
            "<span foreground='#475569' size='small'"
            " weight='bold'>Extra Service</span>");
        gtk_misc_set_alignment(GTK_MISC(svcLbl), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(bookCardBox), svcLbl, FALSE, FALSE, 2);
    }
    state->citizenService = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(state->citizenService),
                              "No extra service");
    gtk_combo_box_set_active(GTK_COMBO_BOX(state->citizenService), 0);
    gtk_widget_set_size_request(state->citizenService, -1, 32);
    gtk_box_pack_start(GTK_BOX(bookCardBox), state->citizenService,
                       FALSE, FALSE, 2);

    /* Live tariff label */
    state->tariff = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(state->tariff),
        "<span foreground='#287A4D' weight='bold'>"
        "Estimated Tariff: 2.00 TND</span>");
    gtk_misc_set_alignment(GTK_MISC(state->tariff), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(bookCardBox), state->tariff,
                       FALSE, FALSE, 4);

    /* Confirm Booking button */
    confirmBtn = make_admin_action_button(
        "Confirm Booking", GTK_STOCK_APPLY,
        "#287A4D", "#1A5C38", "#FFFFFF");
    gtk_widget_set_size_request(confirmBtn, -1, 40);
    gtk_box_pack_start(GTK_BOX(bookCardBox), confirmBtn,
                       FALSE, FALSE, 6);

    bookCard = make_admin_panel_card(bookCardBox, 290);
    gtk_paned_pack1(GTK_PANED(splitPane), leftSide,  TRUE,  FALSE);
    gtk_paned_pack2(GTK_PANED(splitPane), bookCard,  FALSE, FALSE);
    gtk_paned_set_position(GTK_PANED(splitPane), 840);
    gtk_box_pack_start(GTK_BOX(findTab), splitPane, TRUE, TRUE, 4);

    /* Connect Available Parkings row-selection */
    tree = gtk_bin_get_child(GTK_BIN(
        gtk_bin_get_child(GTK_BIN(state->citizenParkings))));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(citizen_parking_row_selected), state);

    /* ================================================================
       TAB 2 — MY RESERVATIONS
       ================================================================ */
    gtk_container_set_border_width(GTK_CONTAINER(resTab), 8);

    state->citizenReservations =
        make_tree_view_markup("My Reservations", &store,
                              resTitles, 7, 6);
    gtk_widget_set_size_request(state->citizenReservations, -1, 340);
    gtk_box_pack_start(GTK_BOX(resTab), state->citizenReservations,
                       TRUE, TRUE, 0);

    /* Cancel action bar */
    resActBar  = gtk_hbox_new(FALSE, 8);
    cancelBtn  = make_admin_action_button(
        "Cancel Reservation", GTK_STOCK_CANCEL,
        "#B33A3A", "#8B2929", "#FFFFFF");
    gtk_widget_set_size_request(cancelBtn, 200, 36);
    state->citizenCancelBtn = cancelBtn;
    {
        GtkWidget *resActAlign = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
        gtk_container_add(GTK_CONTAINER(resActAlign), cancelBtn);
        gtk_box_pack_start(GTK_BOX(resActBar), resActAlign,
                           FALSE, FALSE, 0);
        {
            GtkWidget *hint = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(hint),
                "<span foreground='#475569' size='small'>"
                "Only pending reservations can be cancelled."
                "</span>");
            gtk_box_pack_start(GTK_BOX(resActBar), hint,
                               FALSE, FALSE, 8);
        }
    }
    gtk_box_pack_start(GTK_BOX(resTab), resActBar, FALSE, FALSE, 4);

    /* ================================================================
       TAB 3 — REVIEWS & COMPLAINTS
       ================================================================ */
    gtk_container_set_border_width(GTK_CONTAINER(feedTab), 8);

    /* -- Submit Review card -- */
    gtk_container_set_border_width(GTK_CONTAINER(reviewCardBox), 12);
    gtk_label_set_markup(GTK_LABEL(reviewTitle),
        "<span foreground='#1B2A4A' size='large' weight='bold'>"
        "Submit a Review</span>");
    gtk_misc_set_alignment(GTK_MISC(reviewTitle), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(reviewCardBox), reviewTitle,
                       FALSE, FALSE, 2);

    /* Parking dropdown replacing raw ID */
    {
        GtkWidget *pickLbl = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(pickLbl),
            "<span foreground='#475569' size='small'"
            " weight='bold'>Select Parking</span>");
        gtk_misc_set_alignment(GTK_MISC(pickLbl), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(reviewCardBox), pickLbl,
                           FALSE, FALSE, 2);
    }
    state->reviewParkingCombo = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(state->reviewParkingCombo),
                              "Select a parking...");
    {
        int pi;
        for (pi = 0; pi < state->data->parkingCount; ++pi)
            gtk_combo_box_append_text(
                GTK_COMBO_BOX(state->reviewParkingCombo),
                state->data->parkings[pi].name);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(state->reviewParkingCombo), 0);
    gtk_widget_set_size_request(state->reviewParkingCombo, -1, 32);
    gtk_box_pack_start(GTK_BOX(reviewCardBox), state->reviewParkingCombo,
                       FALSE, FALSE, 2);

    /* Rating spin */
    {
        GtkWidget *ratLbl = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(ratLbl),
            "<span foreground='#475569' size='small'"
            " weight='bold'>Rating (1–5)</span>");
        gtk_misc_set_alignment(GTK_MISC(ratLbl), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(reviewCardBox), ratLbl,
                           FALSE, FALSE, 2);
    }
    ratingAdj = GTK_ADJUSTMENT(gtk_adjustment_new(5, 1, 5, 1, 1, 0));
    state->reviewRating = gtk_spin_button_new(ratingAdj, 1, 0);
    gtk_widget_set_size_request(state->reviewRating, -1, 32);
    gtk_box_pack_start(GTK_BOX(reviewCardBox), state->reviewRating,
                       FALSE, FALSE, 2);

    /* Comment text area */
    {
        GtkWidget *comLbl = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(comLbl),
            "<span foreground='#475569' size='small'"
            " weight='bold'>Comment</span>");
        gtk_misc_set_alignment(GTK_MISC(comLbl), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(reviewCardBox), comLbl,
                           FALSE, FALSE, 2);
    }
    state->reviewComment = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(state->reviewComment),
                                GTK_WRAP_WORD);
    gtk_widget_modify_base(state->reviewComment, GTK_STATE_NORMAL, &white);
    scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroller, -1, 120);
    gtk_container_add(GTK_CONTAINER(scroller), state->reviewComment);
    gtk_frame_set_shadow_type(GTK_FRAME(
        gtk_frame_new(NULL)), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(reviewCardBox), scroller,
                       TRUE, TRUE, 2);

    reviewBtn = make_admin_action_button(
        "Submit Review", GTK_STOCK_EDIT,
        "#1F5FBF", "#174AA0", "#FFFFFF");
    gtk_widget_set_size_request(reviewBtn, -1, 36);
    {
        GtkWidget *revAlign = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
        gtk_container_add(GTK_CONTAINER(revAlign), reviewBtn);
        gtk_box_pack_start(GTK_BOX(reviewCardBox), revAlign,
                           FALSE, FALSE, 6);
    }
    reviewCard = make_admin_panel_card(reviewCardBox, -1);
    gtk_box_pack_start(GTK_BOX(feedTab), reviewCard, TRUE, TRUE, 0);

    /* -- Submit Complaint card -- */
    gtk_container_set_border_width(GTK_CONTAINER(complaintCardBox), 12);
    gtk_label_set_markup(GTK_LABEL(complaintTitle),
        "<span foreground='#1B2A4A' size='large' weight='bold'>"
        "Report a Complaint</span>");
    gtk_misc_set_alignment(GTK_MISC(complaintTitle), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(complaintCardBox), complaintTitle,
                       FALSE, FALSE, 2);

    gtk_box_pack_start(GTK_BOX(complaintCardBox),
                       admin_panel_entry_row(GTK_STOCK_DIALOG_WARNING,
                                             "Subject",
                                             &state->complaintSubject),
                       FALSE, FALSE, 2);

    {
        GtkWidget *descLbl = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(descLbl),
            "<span foreground='#475569' size='small'"
            " weight='bold'>Description</span>");
        gtk_misc_set_alignment(GTK_MISC(descLbl), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(complaintCardBox), descLbl,
                           FALSE, FALSE, 2);
    }
    state->complaintText = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(state->complaintText),
                                GTK_WRAP_WORD);
    gtk_widget_modify_base(state->complaintText, GTK_STATE_NORMAL, &white);
    {
        GtkWidget *cmpScroller = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(
            GTK_SCROLLED_WINDOW(cmpScroller),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request(cmpScroller, -1, 120);
        gtk_container_add(GTK_CONTAINER(cmpScroller),
                          state->complaintText);
        gtk_box_pack_start(GTK_BOX(complaintCardBox), cmpScroller,
                           TRUE, TRUE, 2);
    }

    complaintBtn = make_admin_action_button(
        "Submit Complaint", GTK_STOCK_DIALOG_WARNING,
        "#B33A3A", "#8B2929", "#FFFFFF");
    gtk_widget_set_size_request(complaintBtn, -1, 36);
    {
        GtkWidget *cmpAlign = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
        gtk_container_add(GTK_CONTAINER(cmpAlign), complaintBtn);
        gtk_box_pack_start(GTK_BOX(complaintCardBox), cmpAlign,
                           FALSE, FALSE, 6);
    }
    complaintCard = make_admin_panel_card(complaintCardBox, -1);
    gtk_box_pack_start(GTK_BOX(feedTab), complaintCard, TRUE, TRUE, 0);

    /* -- Community Reviews table -- */
    communityReviews =
        make_tree_view("Community Reviews", &store, reviewTitles, 2);
    state->citizenReviews = communityReviews;
    gtk_box_pack_start(GTK_BOX(feedTab), communityReviews,
                       TRUE, TRUE, 0);

    /* ================================================================
       Assemble notebook — 3 tabs, all with gtk_widget_show_all labels
       ================================================================ */
    tabLabel = make_stock_tab("Find & Book", GTK_STOCK_FIND);
    gtk_widget_show_all(tabLabel);
    gtk_notebook_append_page(GTK_NOTEBOOK(inner), findTab, tabLabel);

    tabLabel = make_stock_tab("My Reservations", GTK_STOCK_APPLY);
    gtk_widget_show_all(tabLabel);
    gtk_notebook_append_page(GTK_NOTEBOOK(inner), resTab, tabLabel);

    tabLabel = make_stock_tab("Reviews & Complaints", GTK_STOCK_EDIT);
    gtk_widget_show_all(tabLabel);
    gtk_notebook_append_page(GTK_NOTEBOOK(inner), feedTab, tabLabel);

    gtk_notebook_set_scrollable(GTK_NOTEBOOK(inner), TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(inner), 4);
    gtk_box_pack_start(GTK_BOX(box), inner, TRUE, TRUE, 0);

    /* ================================================================
       Signal connections
       ================================================================ */
    g_signal_connect(confirmBtn,  "clicked",
                     G_CALLBACK(reserve_clicked),    state);
    g_signal_connect(cancelBtn,   "clicked",
                     G_CALLBACK(cancel_reservation_clicked), state);
    g_signal_connect(reviewBtn,   "clicked",
                     G_CALLBACK(review_clicked),     state);
    g_signal_connect(complaintBtn,"clicked",
                     G_CALLBACK(complaint_clicked),  state);
    g_signal_connect(state->citizenService, "changed",
                     G_CALLBACK(tariff_changed),     state);

    /* ================================================================
       Initial populate
       ================================================================ */
    update_tariff(state);
    refresh_citizen_tables(state);
    return box;
}

static void on_window_destroy(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    stop_agent_realtime_timer((UiState *)user_data);
    stop_admin_realtime_timer((UiState *)user_data);
    stop_citizen_realtime_timer((UiState *)user_data);
    gtk_main_quit();
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
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), &state);

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

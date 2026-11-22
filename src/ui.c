#include "ui.h"

#include <gtk/gtk.h>
#include <ctype.h>
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
    GtkWidget *agentReservations;
    GtkWidget *agentServices;
    GtkWidget *citizenParkings;
    GtkWidget *citizenReservations;
    GtkWidget *citizenReviews;
    GtkWidget *loginEmail;
    GtkWidget *loginPassword;
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
} UiState;

static void set_status(UiState *state, const char *message);
static void set_page(UiState *state, int page);
static void refresh_dashboard_identity(UiState *state);

static void show_message(UiState *state, GtkMessageType type,
                         const char *title, const char *message)
{
    GtkWidget *dialog;
    (void)state;
    dialog = gtk_message_dialog_new(
        NULL,
        GTK_DIALOG_MODAL,
        type,
        GTK_BUTTONS_OK,
        "%s",
        message);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
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
    gtk_box_pack_start(GTK_BOX(box), gtk_label_new(label),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), text, TRUE, TRUE, 0);
    *entry = text;
    return box;
}

static GtkWidget *make_stock_button(const char *label, const char *stock)
{
    GtkWidget *button = gtk_button_new_from_stock(stock);
    GtkWidget *labelWidget = gtk_bin_get_child(GTK_BIN(button));
    (void)label;
    if (GTK_IS_HBOX(labelWidget)) {
        GList *children = gtk_container_get_children(GTK_CONTAINER(labelWidget));
        GList *child;
        for (child = children; child != NULL; child = child->next) {
            if (GTK_IS_LABEL(child->data)) {
                gtk_label_set_text(GTK_LABEL(child->data), label);
                break;
            }
        }
        g_list_free(children);
    }
    return button;
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

static GtkWidget *make_dashboard_header(UiState *state, const char *role)
{
    GtkWidget *header = gtk_hbox_new(FALSE, 8);
    GtkWidget *profile = gtk_hbox_new(FALSE, 6);
    GtkWidget *avatar = gtk_image_new_from_stock(
        GTK_STOCK_DIALOG_AUTHENTICATION, GTK_ICON_SIZE_LARGE_TOOLBAR);
    GtkWidget *identity = gtk_label_new("User");
    GtkWidget *badge = gtk_label_new(role);
    GtkWidget *signOut = make_stock_button("Sign Out", GTK_STOCK_QUIT);
    const char *name = "Guest";
    char identityText[128];
    char badgeMarkup[128];
    char identityMarkup[128];
    const char *badgeColor;
    int i;

    for (i = 0; i < state->data->userCount; ++i) {
        if (state->data->users[i].id == state->activeUserId) {
            name = state->data->users[i].name;
            break;
        }
    }
    snprintf(identityText, sizeof(identityText),
             "Signed in: %s", name);
    snprintf(identityMarkup, sizeof(identityMarkup),
             "<span weight='bold'>%s</span>", identityText);
    gtk_label_set_markup(GTK_LABEL(identity), identityMarkup);
    badgeColor = strcmp(role, "ADMIN") == 0 ? "#1F5FBF" :
                 strcmp(role, "AGENT") == 0 ? "#287A4D" : "#8A5A00";
    snprintf(badgeMarkup, sizeof(badgeMarkup),
             "<span foreground='%s' weight='bold'>  %s  </span>",
             badgeColor, role);
    gtk_label_set_markup(GTK_LABEL(badge), badgeMarkup);
    if (strcmp(role, "ADMIN") == 0) state->adminIdentity = identity;
    else if (strcmp(role, "AGENT") == 0) state->agentIdentity = identity;
    else state->citizenIdentity = identity;
    gtk_box_pack_start(GTK_BOX(profile), avatar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(profile), identity, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(profile), badge, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(header), profile, FALSE, FALSE, 8);
    gtk_box_pack_end(GTK_BOX(header), signOut, FALSE, FALSE, 8);
    g_signal_connect(signOut, "clicked",
                     G_CALLBACK(sign_out_clicked), state);
    return header;
}

static int confirm_destructive_action(UiState *state,
                                      const char *action,
                                      const char *details)
{
    GtkWidget *dialog;
    gint response;
    char message[256];
    (void)state;
    snprintf(message, sizeof(message), "%s\n%s", action, details);
    dialog = gtk_message_dialog_new(
        NULL,
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_YES_NO,
        "%s",
        message);
    gtk_window_set_title(GTK_WINDOW(dialog), "Confirm destructive action");
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
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
             "<span weight='bold'>%s</span>", identityText);
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
    GType types[6];
    int i;
    if (columnCount > 6) {
        columnCount = 6;
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

    store = tree_store(state, state->adminUsers);
    gtk_list_store_clear(store);
    for (i = 0; i < state->data->userCount; ++i) {
        const char *role = state->data->users[i].role == USER_ADMIN ? "Admin" :
                           state->data->users[i].role == USER_AGENT ? "Agent" :
                           "Citizen";
        gtk_list_store_append(store, &iter);
        snprintf(value, sizeof(value), "%d", state->data->users[i].id);
        gtk_list_store_set(store, &iter, 0, value, 1,
                           state->data->users[i].name, 2,
                           state->data->users[i].email, 3, role, -1);
    }

    store = tree_store(state, state->adminParkings);
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

    store = tree_store(state, state->adminComplaints);
    gtk_list_store_clear(store);
    for (i = 0; i < state->data->complaintCount; ++i) {
        gtk_list_store_append(store, &iter);
        snprintf(value, sizeof(value), "%d", state->data->complaints[i].id);
        gtk_list_store_set(store, &iter, 0, value, 1,
                           state->data->complaints[i].subject, 2,
                           state->data->complaints[i].description, 3,
                           state->data->complaints[i].status == 2 ?
                           "Resolved" : "Open", -1);
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

static void login_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    const char *email = gtk_entry_get_text(GTK_ENTRY(state->loginEmail));
    const char *password = gtk_entry_get_text(GTK_ENTRY(state->loginPassword));
    int i;
    (void)widget;

    if (!valid_email(email) || !valid_auth_field(password)) {
        show_message(state, GTK_MESSAGE_ERROR, "Login validation",
                     "Email and password must contain at least 3 characters "
                     "and cannot contain whitespace or storage delimiters.");
        set_status(state, "Login validation failed");
        return;
    }

    for (i = 0; i < state->data->userCount; ++i) {
        User *user = &state->data->users[i];
        if (strcmp(user->email, email) == 0 &&
            strcmp(user->password, password) == 0) {
            state->activeUserId = user->id;
            refresh_dashboard_identity(state);
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
    show_message(state, GTK_MESSAGE_ERROR, "Login failed",
                 "Invalid email or password.");
    set_status(state, "Invalid email or password");
}

static void register_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    User user;
    const char *email = gtk_entry_get_text(GTK_ENTRY(state->loginEmail));
    const char *password = gtk_entry_get_text(GTK_ENTRY(state->loginPassword));
    const char *at;
    char username[PARKY_NAME_LENGTH];
    int i;
    (void)widget;

    if (!valid_email(email) || !valid_auth_field(password)) {
        show_message(state, GTK_MESSAGE_ERROR, "Registration validation",
                     "Email and password must contain at least 3 characters "
                     "and cannot contain whitespace or storage delimiters.");
        set_status(state, "Registration validation failed");
        return;
    }

    at = strchr(email, '@');
    if (at == email) {
        show_message(state, GTK_MESSAGE_ERROR, "Registration validation",
                     "Email must include a valid username.");
        return;
    }
    snprintf(username, sizeof(username), "%.*s",
             (int)(at - email), email);
    if (!valid_auth_field(username)) {
        show_message(state, GTK_MESSAGE_ERROR, "Registration validation",
                     "Username must contain at least 3 characters.");
        return;
    }

    for (i = 0; i < state->data->userCount; ++i) {
        if (strcmp(state->data->users[i].email, email) == 0 ||
            strcmp(state->data->users[i].name, username) == 0) {
            show_message(state, GTK_MESSAGE_ERROR,
                         "Duplicate account",
                         "That username or email is already registered.");
            set_status(state, "Duplicate username or email");
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
        set_status(state, "Citizen account registered");
        gtk_entry_set_text(GTK_ENTRY(state->loginPassword), "");
        show_message(state, GTK_MESSAGE_INFO,
                     "Registration successful",
                     "Citizen account registered successfully.");
    } else {
        set_status(state, "Registration failed");
        show_message(state, GTK_MESSAGE_ERROR,
                     "Registration failed",
                     "Unable to register the citizen account.");
    }
}

static void occupancy_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    char message[128];
    double rate = state->data->parkingCount > 0 ?
        admin_parking_occupancy_rate(state->data,
                                    state->data->parkings[0].id) : 0.0;
    (void)widget;
    snprintf(message, sizeof(message),
             "Central parking occupancy: %.1f%%", rate);
    set_status(state, message);
}

static void resolve_complaint_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    (void)widget;
    if (state->data->complaintCount > 0 &&
        admin_resolve_complaint(state->data,
                                state->data->complaints[0].id))
        set_status(state, "Oldest complaint resolved");
    else
        set_status(state, "No complaint to resolve");
    refresh_admin_tables(state);
}

static void generate_demo_data_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    (void)widget;
    if (confirm_destructive_action(state, "Generate demo data?",
                                   "This replaces all existing text data.") &&
        generate_demo_data(state->data)) {
        show_message(state, GTK_MESSAGE_INFO, "Demo data generated",
                     "Tunisian parkings, users, reservations, services, "
                     "reviews and complaints are ready.");
        refresh_admin_tables(state);
        refresh_agent_tables(state);
        refresh_citizen_tables(state);
        set_status(state, "Demo data generated and saved");
    }
}

static void delete_user_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    (void)widget;
    if (state->data->userCount > 0 &&
        confirm_destructive_action(state, "Delete the first user?",
                                   "This action cannot be undone.") &&
        admin_delete_user(state->data, state->data->users[0].id)) {
        show_message(state, GTK_MESSAGE_INFO, "User deleted",
                     "The selected user was deleted.");
        refresh_admin_tables(state);
    }
}

static void delete_parking_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    (void)widget;
    if (state->data->parkingCount > 0 &&
        confirm_destructive_action(state, "Delete the first parking?",
                                   "This action cannot be undone.") &&
        admin_delete_parking(state->data, state->data->parkings[0].id)) {
        show_message(state, GTK_MESSAGE_INFO, "Parking deleted",
                     "The selected parking was deleted.");
        refresh_admin_tables(state);
    }
}

static void delete_agent_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int i;
    int agentId = 0;
    (void)widget;
    for (i = 0; i < state->data->userCount; ++i) {
        if (state->data->users[i].role == USER_AGENT) {
            agentId = state->data->users[i].id;
            break;
        }
    }
    if (agentId > 0 &&
        confirm_destructive_action(state, "Delete the first agent?",
                                   "This action cannot be undone.") &&
        admin_delete_agent(state->data, agentId)) {
        show_message(state, GTK_MESSAGE_INFO, "Agent deleted",
                     "The selected agent was deleted.");
        refresh_admin_tables(state);
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
            if (state->data->parkings[i].availableSpots <
                state->data->parkings[i].totalSpots)
                ++state->data->parkings[i].availableSpots;
            if (storage_save(state->data))
                show_message(state, GTK_MESSAGE_INFO, "Spot available",
                             "One parking spot was marked available.");
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
            if (state->data->parkings[i].availableSpots > 0)
                --state->data->parkings[i].availableSpots;
            if (storage_save(state->data))
                show_message(state, GTK_MESSAGE_INFO, "Spot occupied",
                             "One parking spot was marked occupied.");
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
    else
        set_status(state, "Select a reservation to approve");
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
        set_status(state, "No reservation to reject");
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
    (void)widget;
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

static GtkWidget *login_view(UiState *state)
{
    GtkWidget *box = gtk_vbox_new(FALSE, 10);
    GtkWidget *title = gtk_label_new("Parky - Sign in");
    GtkWidget *registerRow;
    GtkWidget *loginBtn = make_stock_button(
        "Sign in", GTK_STOCK_DIALOG_AUTHENTICATION);
    GtkWidget *registerBtn = make_stock_button(
        "Register as citizen", GTK_STOCK_ADD);
    GtkWidget *hint = gtk_label_new(
        "Demo accounts:\n"
        "Admin: admin@parky.local / admin\n"
        "Agent: agent@parky.local / agent\n"
        "Citizen: citizen@parky.local / citizen");

    gtk_container_set_border_width(GTK_CONTAINER(box), 16);
    gtk_misc_set_alignment(GTK_MISC(title), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(box),
                       make_labeled_entry("Email", &state->loginEmail),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box),
                       make_labeled_entry("Password", &state->loginPassword),
                       FALSE, FALSE, 0);
    registerRow = gtk_hbox_new(TRUE, 8);
    gtk_box_pack_start(GTK_BOX(registerRow), loginBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(registerRow), registerBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), registerRow, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), hint, FALSE, FALSE, 10);
    gtk_entry_set_visibility(GTK_ENTRY(state->loginPassword), FALSE);
    g_signal_connect(loginBtn, "clicked", G_CALLBACK(login_clicked), state);
    g_signal_connect(registerBtn, "clicked",
                     G_CALLBACK(register_clicked), state);
    return box;
}

static GtkWidget *admin_view(UiState *state)
{
    GtkWidget *box = gtk_vbox_new(FALSE, 8);
    GtkWidget *left = gtk_vbox_new(FALSE, 8);
    GtkWidget *right = gtk_vbox_new(FALSE, 8);
    GtkWidget *split = gtk_hpaned_new();
    GtkWidget *occupancy = make_stock_button(
        "View occupancy statistics", GTK_STOCK_REFRESH);
    GtkWidget *resolve = make_stock_button(
        "Resolve oldest complaint", GTK_STOCK_APPLY);
    GtkWidget *deleteUser = make_stock_button(
        "Delete first user", GTK_STOCK_DELETE);
    GtkWidget *deleteParking = make_stock_button(
        "Delete first parking", GTK_STOCK_DELETE);
    GtkWidget *deleteAgent = make_stock_button(
        "Delete first agent", GTK_STOCK_DELETE);
    GtkWidget *demoData = make_stock_button(
        "Generate Demo Data", GTK_STOCK_REFRESH);
    GtkWidget *actions = gtk_hbox_new(TRUE, 6);
    GtkListStore *store;
    const char *userTitles[] = {"ID", "Name", "Email", "Role"};
    const char *parkingTitles[] = {"ID", "Name", "Address", "Available"};
    const char *complaintTitles[] = {"ID", "Subject", "Description", "Status"};
    char summary[160];

    gtk_container_set_border_width(GTK_CONTAINER(box), 12);
    gtk_container_set_border_width(GTK_CONTAINER(left), 8);
    gtk_container_set_border_width(GTK_CONTAINER(right), 8);
    snprintf(summary, sizeof(summary),
             "Users: %d | Parkings: %d | Agents: %d | Complaints: %d",
             state->data->userCount, state->data->parkingCount,
             state->data->complaintCount, state->data->complaintCount);
    gtk_box_pack_start(GTK_BOX(left),
                       make_dashboard_header(state, "ADMIN"),
                       FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(left), gtk_label_new(summary),
                       FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(left), occupancy, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(left), resolve, FALSE, FALSE, 4);
    state->adminUsers = make_tree_view("Users", &store, userTitles, 4);
    state->adminParkings = make_tree_view("Parkings", &store,
                                           parkingTitles, 4);
    state->adminComplaints = make_tree_view("Complaints", &store,
                                             complaintTitles, 4);
    gtk_box_pack_start(GTK_BOX(right), state->adminUsers, TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(right), state->adminParkings, TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(right), state->adminComplaints, TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(actions), deleteUser, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(actions), deleteParking, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(actions), deleteAgent, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(actions), demoData, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(left), actions, FALSE, FALSE, 4);
    g_signal_connect(occupancy, "clicked",
                     G_CALLBACK(occupancy_clicked), state);
    g_signal_connect(resolve, "clicked",
                     G_CALLBACK(resolve_complaint_clicked), state);
    g_signal_connect(deleteUser, "clicked",
                     G_CALLBACK(delete_user_clicked), state);
    g_signal_connect(deleteParking, "clicked",
                     G_CALLBACK(delete_parking_clicked), state);
    g_signal_connect(deleteAgent, "clicked",
                     G_CALLBACK(delete_agent_clicked), state);
    g_signal_connect(demoData, "clicked",
                     G_CALLBACK(generate_demo_data_clicked), state);
    refresh_admin_tables(state);
    gtk_paned_pack1(GTK_PANED(split), left, FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(split), right, TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(split), 330);
    gtk_box_pack_start(GTK_BOX(box), split, TRUE, TRUE, 0);
    return box;
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

    gtk_notebook_append_page(GTK_NOTEBOOK(inner), findTab,
                             make_stock_tab("Find & Book Parking",
                                            GTK_STOCK_FIND));
    gtk_notebook_append_page(GTK_NOTEBOOK(inner), reservationsTab,
                             make_stock_tab("My Reservations & Bills",
                                            GTK_STOCK_APPLY));
    gtk_notebook_append_page(GTK_NOTEBOOK(inner), feedbackTab,
                             make_stock_tab("Reviews & Complaints",
                                            GTK_STOCK_EDIT));
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

    state.status = gtk_statusbar_new();
    {
        GtkWidget *root = gtk_vbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(root), state.notebook, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(root), state.status, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(window), root);
    }

    gtk_widget_show_all(window);
    set_status(&state, "Parky ready");
    gtk_main();
    return 0;
}

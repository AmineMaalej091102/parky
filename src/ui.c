#include "ui.h"

#include <gtk/gtk.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "admin.h"
#include "agent.h"
#include "citoyen.h"

typedef struct {
    ParkyData *data;
    int activeUserId;
    GtkWidget *notebook;
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
} UiState;

static void set_status(UiState *state, const char *message);
static void set_page(UiState *state, int page);

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

static void set_status(UiState *state, const char *message)
{
    gtk_statusbar_push(GTK_STATUSBAR(state->status), 1, message);
}

static void set_page(UiState *state, int page)
{
    gtk_notebook_set_current_page(GTK_NOTEBOOK(state->notebook), page);
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
            if (user->role == USER_ADMIN) set_page(state, 1);
            else if (user->role == USER_AGENT) set_page(state, 2);
            else set_page(state, 3);
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
}

static void set_spot_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    (void)widget;
    if (state->data->parkingCount > 0 &&
        agent_set_spot_status(state->data,
                              state->data->parkings[0].id,
                              PARKING_RESERVED))
        set_status(state, "Parking spot status changed to reserved");
    else
        set_status(state, "No parking available");
}

static void approve_reservation_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    (void)widget;
    if (state->data->reservationCount > 0 &&
        agent_resolve_reservation(state->data,
                                  state->data->reservations[0].id, 1))
        set_status(state, "First reservation approved");
    else
        set_status(state, "No reservation to approve");
}

static void reject_reservation_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    (void)widget;
    if (state->data->reservationCount > 0 &&
        agent_resolve_reservation(state->data,
                                  state->data->reservations[0].id, 0))
        set_status(state, "First reservation rejected");
    else
        set_status(state, "No reservation to reject");
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
}

static void reserve_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int parkingId = atoi(gtk_entry_get_text(GTK_ENTRY(state->citizenParking)));
    int spot = atoi(gtk_entry_get_text(GTK_ENTRY(state->citizenSpot)));
    (void)widget;
    if (citoyen_reserve_spot(state->data, 3, parkingId,
                             gtk_entry_get_text(GTK_ENTRY(state->citizenDate)),
                             gtk_entry_get_text(GTK_ENTRY(state->citizenTime)),
                             spot))
        set_status(state, "Spot reservation submitted");
    else
        set_status(state, "Unable to reserve spot");
}

static void book_service_clicked(GtkWidget *widget, gpointer user_data)
{
    UiState *state = (UiState *)user_data;
    int parkingId = atoi(gtk_entry_get_text(GTK_ENTRY(state->citizenParking)));
    int serviceId = gtk_combo_box_get_active(GTK_COMBO_BOX(state->citizenService));
    (void)widget;
    if (serviceId >= 0) ++serviceId;
    else serviceId = 0;
    if (citoyen_book_service(state->data, 3, parkingId, serviceId))
        set_status(state, "Service booking submitted");
    else
        set_status(state, "Unable to book service");
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
    if (citoyen_submit_review(state->data, 3, parkingId, rating, comment))
        set_status(state, "Review submitted");
    else
        set_status(state, "Unable to submit review");
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
    if (citoyen_submit_complaint(state->data, 3, subject, text))
        set_status(state, "Complaint submitted");
    else
        set_status(state, "Unable to submit complaint");
    g_free(text);
}

static GtkWidget *login_view(UiState *state)
{
    GtkWidget *box = gtk_vbox_new(FALSE, 10);
    GtkWidget *title = gtk_label_new("Parky - Sign in");
    GtkWidget *registerRow;
    GtkWidget *loginBtn = gtk_button_new_with_label("Sign in");
    GtkWidget *registerBtn = gtk_button_new_with_label("Register as citizen");
    GtkWidget *hint = gtk_label_new(
        "Demo accounts:\n"
        "Admin: admin@parky.local / admin\n"
        "Agent: agent@parky.local / agent\n"
        "Citizen: citizen@parky.local / citizen");

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
    GtkWidget *title = gtk_label_new("Administrator Dashboard");
    GtkWidget *occupancy = gtk_button_new_with_label("View occupancy statistics");
    GtkWidget *resolve = gtk_button_new_with_label("Resolve oldest complaint");
    GtkWidget *signOut = gtk_button_new_with_label("Sign Out");
    char summary[160];

    snprintf(summary, sizeof(summary),
             "Users: %d | Parkings: %d | Agents: %d | Complaints: %d",
             state->data->userCount, state->data->parkingCount,
             state->data->complaintCount, state->data->complaintCount);
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(box), gtk_label_new(summary),
                       FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), occupancy, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), resolve, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), signOut, FALSE, FALSE, 4);
    g_signal_connect(occupancy, "clicked",
                     G_CALLBACK(occupancy_clicked), state);
    g_signal_connect(resolve, "clicked",
                     G_CALLBACK(resolve_complaint_clicked), state);
    g_signal_connect(signOut, "clicked",
                     G_CALLBACK(sign_out_clicked), state);
    return box;
}

static GtkWidget *agent_view(UiState *state)
{
    GtkWidget *box = gtk_vbox_new(FALSE, 8);
    GtkWidget *title = gtk_label_new("Parking Agent Dashboard");
    GtkWidget *spot = gtk_button_new_with_label("Mark spot reserved");
    GtkWidget *approve = gtk_button_new_with_label("Approve reservation");
    GtkWidget *reject = gtk_button_new_with_label("Reject reservation");
    GtkWidget *signOut = gtk_button_new_with_label("Sign Out");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(box),
                       gtk_label_new("Spot status | Parking services"),
                       FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), spot, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), approve, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), reject, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), signOut, FALSE, FALSE, 4);
    g_signal_connect(spot, "clicked", G_CALLBACK(set_spot_clicked), state);
    g_signal_connect(approve, "clicked",
                     G_CALLBACK(approve_reservation_clicked), state);
    g_signal_connect(reject, "clicked",
                     G_CALLBACK(reject_reservation_clicked), state);
    g_signal_connect(signOut, "clicked",
                     G_CALLBACK(sign_out_clicked), state);
    return box;
}

static GtkWidget *citizen_view(UiState *state)
{
    GtkWidget *box = gtk_vbox_new(FALSE, 6);
    GtkWidget *title = gtk_label_new("Citizen Dashboard");
    GtkWidget *signOut = gtk_button_new_with_label("Sign Out");
    GtkWidget *search = gtk_button_new_with_label("Search");
    GtkWidget *reserve = gtk_button_new_with_label("Reserve spot");
    GtkWidget *book = gtk_button_new_with_label("Book service");
    GtkWidget *review = gtk_button_new_with_label("Submit review");
    GtkWidget *complaint = gtk_button_new_with_label("Submit complaint");
    GtkWidget *reviewBox = gtk_hbox_new(FALSE, 6);
    GtkWidget *complaintBox = gtk_vbox_new(FALSE, 6);
    GtkAdjustment *adjustment;
    GtkWidget *scroller;

    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(box), signOut, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box),
                       make_labeled_entry("Parking name",
                                          &state->citizenName),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box),
                       make_labeled_entry("Address",
                                          &state->citizenAddress),
                       FALSE, FALSE, 0);
    state->citizenAvailable = gtk_check_button_new_with_label(
        "Only show available parkings");
    gtk_box_pack_start(GTK_BOX(box), state->citizenAvailable,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), search, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box),
                       make_labeled_entry("Parking ID",
                                          &state->citizenParking),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box),
                       make_labeled_entry("Spot number",
                                          &state->citizenSpot),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box),
                       make_labeled_entry("Date",
                                          &state->citizenDate),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box),
                       make_labeled_entry("Time",
                                          &state->citizenTime),
                       FALSE, FALSE, 0);

    state->citizenService = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(state->citizenService),
                              "No extra service");
    gtk_combo_box_append_text(GTK_COMBO_BOX(state->citizenService),
                              "Car Wash");
    gtk_combo_box_set_active(GTK_COMBO_BOX(state->citizenService), 0);
    gtk_box_pack_start(GTK_BOX(box),
                       make_labeled_entry("Service",
                                          &state->citizenHours),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), state->citizenService,
                       FALSE, FALSE, 0);
    gtk_entry_set_text(GTK_ENTRY(state->citizenHours), "1");

    state->tariff = gtk_label_new("Estimated tariff: 2.00 TND");
    gtk_box_pack_start(GTK_BOX(box), state->tariff, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), reserve, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), book, FALSE, FALSE, 4);

    adjustment = GTK_ADJUSTMENT(
        gtk_adjustment_new(5, 1, 5, 1, 1, 0));
    state->reviewRating = gtk_spin_button_new(
        GTK_ADJUSTMENT(adjustment), 1, 0);
    state->reviewComment = gtk_text_view_new();
    gtk_box_pack_start(GTK_BOX(reviewBox),
                       gtk_label_new("Rating"), FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(reviewBox),
                       state->reviewRating, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(reviewBox),
                       state->reviewComment, TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(box), reviewBox, TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(box), review, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(complaintBox),
                       make_labeled_entry("Complaint subject",
                                          &state->complaintSubject),
                       FALSE, FALSE, 0);
    state->complaintText = gtk_text_view_new();
    scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroller), state->complaintText);
    gtk_box_pack_start(GTK_BOX(complaintBox), scroller, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(complaintBox), complaint, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), complaintBox, TRUE, TRUE, 4);

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
    g_signal_connect(signOut, "clicked",
                     G_CALLBACK(sign_out_clicked), state);
    update_tariff(state);
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
    gtk_window_set_default_size(GTK_WINDOW(window), 760, 620);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    state.notebook = gtk_notebook_new();
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(state.notebook), FALSE);
    gtk_notebook_append_page(GTK_NOTEBOOK(state.notebook),
                             login_view(&state), NULL);
    gtk_notebook_append_page(GTK_NOTEBOOK(state.notebook),
                             admin_view(&state), NULL);
    gtk_notebook_append_page(GTK_NOTEBOOK(state.notebook),
                             agent_view(&state), NULL);
    gtk_notebook_append_page(GTK_NOTEBOOK(state.notebook),
                             citizen_view(&state), NULL);

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

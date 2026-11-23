#ifndef MODELS_H
#define MODELS_H

#define PARKY_NAME_LENGTH 64
#define PARKY_TEXT_LENGTH 128
#define PARKY_MAX_USERS 100
#define PARKY_MAX_PARKINGS 50
#define PARKY_MAX_RESERVATIONS 200
#define PARKY_MAX_SERVICES 100
#define PARKY_MAX_REVIEWS 200
#define PARKY_MAX_COMPLAINTS 200

typedef enum {
    USER_ADMIN = 1,
    USER_AGENT = 2,
    USER_CITIZEN = 3
} UserRole;

typedef struct {
    int id;
    char name[PARKY_NAME_LENGTH];
    char password[PARKY_NAME_LENGTH];
    char email[PARKY_NAME_LENGTH];
    char avatarPath[PARKY_TEXT_LENGTH];
    UserRole role;
    int assignedParkingId;
} User;

typedef struct {
    int id;
    char name[PARKY_NAME_LENGTH];
    char address[PARKY_TEXT_LENGTH];
    int totalSpots;
    int availableSpots;
    int status;
} Parking;

typedef struct {
    int id;
    int citizenId;
    int parkingId;
    char date[PARKY_NAME_LENGTH];
    char time[PARKY_NAME_LENGTH];
    int spotNumber;
    int status;
} Reservation;

typedef struct {
    int id;
    char name[PARKY_NAME_LENGTH];
    char description[PARKY_TEXT_LENGTH];
    float price;
    int status;
} Service;

typedef struct {
    int id;
    int citizenId;
    int parkingId;
    int rating;
    char comment[PARKY_TEXT_LENGTH];
} Review;

typedef struct {
    int id;
    int citizenId;
    char subject[PARKY_NAME_LENGTH];
    char description[PARKY_TEXT_LENGTH];
    int status;
} Complaint;

typedef struct {
    User users[PARKY_MAX_USERS];
    int userCount;
    Parking parkings[PARKY_MAX_PARKINGS];
    int parkingCount;
    Reservation reservations[PARKY_MAX_RESERVATIONS];
    int reservationCount;
    Service services[PARKY_MAX_SERVICES];
    int serviceCount;
    Review reviews[PARKY_MAX_REVIEWS];
    int reviewCount;
    Complaint complaints[PARKY_MAX_COMPLAINTS];
    int complaintCount;
} ParkyData;

#endif

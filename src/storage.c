#include "storage.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

#define PARKY_DATA_DIR "data"

static int ensure_data_dir(void)
{
    struct stat info;

    if (mkdir(PARKY_DATA_DIR, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    if (stat(PARKY_DATA_DIR, &info) != 0 || !S_ISDIR(info.st_mode)) {
        return -1;
    }
    if (mkdir("data/avatars", 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    if (stat("data/avatars", &info) != 0 || !S_ISDIR(info.st_mode)) {
        return -1;
    }
    return 0;
}

static int file_exists(const char *path)
{
    FILE *file;

    if (path == NULL || ensure_data_dir() != 0) {
        return 0;
    }
    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}

void storage_seed(ParkyData *data)
{
    if (data == NULL) {
        return;
    }
    memset(data, 0, sizeof(*data));

    data->users[0].id = 1;
    strcpy(data->users[0].name, "admin");
    strcpy(data->users[0].password, "admin");
    strcpy(data->users[0].email, "admin@parky.local");
    data->users[0].role = USER_ADMIN;
    data->users[0].assignedParkingId = 0;

    data->users[1].id = 2;
    strcpy(data->users[1].name, "agent");
    strcpy(data->users[1].password, "agent");
    strcpy(data->users[1].email, "agent@parky.local");
    data->users[1].role = USER_AGENT;
    data->users[1].assignedParkingId = 1;

    data->users[2].id = 3;
    strcpy(data->users[2].name, "citizen");
    strcpy(data->users[2].password, "citizen");
    strcpy(data->users[2].email, "citizen@parky.local");
    data->users[2].role = USER_CITIZEN;
    data->users[2].assignedParkingId = 0;
    data->userCount = 3;

    data->parkings[0].id = 1;
    strcpy(data->parkings[0].name, "Central_Parking");
    strcpy(data->parkings[0].address, "City_Center");
    data->parkings[0].totalSpots = 50;
    data->parkings[0].availableSpots = 50;
    data->parkings[0].status = 1;
    data->parkingCount = 1;

    data->services[0].id = 1;
    strcpy(data->services[0].name, "Car_Wash");
    strcpy(data->services[0].description, "Basic_exterior_wash");
    data->services[0].price = 10.0f;
    data->services[0].status = 1;
    data->serviceCount = 1;
}

int generate_demo_data(ParkyData *data)
{
    if (data == NULL) {
        return 0;
    }
    static const char *parkingNames[] = {
        "Tunis_Centre", "Lac_1", "Ariana", "Menzah",
        "La_Marsa", "Centre_Urbain_Nord", "Ennasr", "Gammarth",
        "La_Goulette", "Le_Bardo", "Sidi_Bou_Said", "Manar_1"
    };
    static const char *parkingAddresses[] = {
        "Tunis_Centre_Ville", "Berges_du_Lac_1", "Ariana_City",
        "El_Menzah_4", "La_Marsa_Centre", "Les_Berges_du_Lac_2",
        "Ennasr_2", "Gammarth_Centre", "La_Goulette_Port",
        "Le_Bardo_City", "Sidi_Bou_Said_Village", "El_Menar"
    };
    static const char *agentNames[] = {
        "agent_sami", "agent_nadia", "agent_amine",
        "agent_leila", "agent_youssef"
    };
    static const char *agentEmails[] = {
        "sami@parky.tn", "nadia@parky.tn", "amine@parky.tn",
        "leila@parky.tn", "youssef@parky.tn"
    };
    static const char *citizenNames[] = {
        "yasmine", "karim", "ines", "mehdi", "salma",
        "rami", "amira", "nour"
    };
    static const char *citizenEmails[] = {
        "yasmine@demo.tn", "karim@demo.tn", "ines@demo.tn",
        "mehdi@demo.tn", "salma@demo.tn", "rami@demo.tn",
        "amira@demo.tn", "nour@demo.tn"
    };
    int i;

    memset(data, 0, sizeof(*data));

    data->users[0].id = 1;
    strcpy(data->users[0].name, "admin");
    strcpy(data->users[0].password, "admin");
    strcpy(data->users[0].email, "admin@parky.local");
    data->users[0].role = USER_ADMIN;

    data->userCount = 1;
    for (i = 0; i < 5; ++i) {
        int id = data->userCount + 1;
        data->users[data->userCount].id = id;
        strcpy(data->users[data->userCount].name, agentNames[i]);
        strcpy(data->users[data->userCount].password, "agent123");
        strcpy(data->users[data->userCount].email, agentEmails[i]);
        data->users[data->userCount].role = USER_AGENT;
        data->users[data->userCount].assignedParkingId = i + 1;
        ++data->userCount;
    }
    for (i = 0; i < 8; ++i) {
        int id = data->userCount + 1;
        data->users[data->userCount].id = id;
        strcpy(data->users[data->userCount].name, citizenNames[i]);
        strcpy(data->users[data->userCount].password, "citizen123");
        strcpy(data->users[data->userCount].email, citizenEmails[i]);
        data->users[data->userCount].role = USER_CITIZEN;
        ++data->userCount;
    }

    for (i = 0; i < 12; ++i) {
        int spots = 60 + i * 20;
        int occupied = 12 + i * 4;
        data->parkings[i].id = i + 1;
        strcpy(data->parkings[i].name, parkingNames[i]);
        strcpy(data->parkings[i].address, parkingAddresses[i]);
        data->parkings[i].totalSpots = spots;
        data->parkings[i].availableSpots = spots - occupied;
        data->parkings[i].status = 1;
    }
    data->parkingCount = 12;

    {
        static const char *serviceNames[] = {
            "Car_Wash", "EV_Charging", "Covered_VIP",
            "Wheel_Service", "Airport_Shuttle", "Reserved_Vehicle_Care"
        };
        static const char *serviceDescriptions[] = {
            "Exterior_and_interior_wash",
            "Fast_Electric_Vehicle_Charging",
            "Covered_reserved_parking",
            "Tire_and_wheel_check",
            "Short_shuttle_to_terminal",
            "Indoor_vehicle_cleaning"
        };
        static const float prices[] = {
            12.0f, 18.0f, 25.0f, 16.0f, 20.0f, 30.0f
        };
        for (i = 0; i < 6; ++i) {
            data->services[i].id = i + 1;
            strcpy(data->services[i].name, serviceNames[i]);
            strcpy(data->services[i].description, serviceDescriptions[i]);
            data->services[i].price = prices[i];
            data->services[i].status = 1;
        }
        data->serviceCount = 6;
    }

    for (i = 0; i < 18; ++i) {
        data->reservations[i].id = i + 1;
        data->reservations[i].citizenId = 7 + (i % 8);
        data->reservations[i].parkingId = 1 + (i % 12);
        strcpy(data->reservations[i].date,
               i % 3 == 0 ? "2026-12-01" :
               i % 3 == 1 ? "2026-12-02" : "2026-12-03");
        strcpy(data->reservations[i].time,
               i % 3 == 0 ? "09:00" : i % 3 == 1 ? "14:30" : "18:00");
        data->reservations[i].spotNumber = 10 + i;
        data->reservations[i].status =
            i < 6 ? 0 : i < 13 ? 1 : i < 16 ? 3 : 2;
    }
    data->reservationCount = 18;

    for (i = 0; i < 10; ++i) {
        data->reviews[i].id = i + 1;
        data->reviews[i].citizenId = 7 + (i % 8);
        data->reviews[i].parkingId = 1 + (i % 12);
        data->reviews[i].rating = 3 + (i % 3);
        snprintf(data->reviews[i].comment,
                 sizeof(data->reviews[i].comment),
                 i % 2 == 0 ? "Clean_and_secure_parking" :
                              "Easy_access_and_helpful_staff");
    }
    data->reviewCount = 10;

    for (i = 0; i < 8; ++i) {
        data->complaints[i].id = i + 1;
        data->complaints[i].citizenId = 7 + (i % 8);
        snprintf(data->complaints[i].subject,
                 sizeof(data->complaints[i].subject),
                 i % 2 == 0 ? "Spot_availability" : "Payment_issue");
        snprintf(data->complaints[i].description,
                 sizeof(data->complaints[i].description),
                 i % 3 == 0 ? "Reserved_spot_was_occupied" :
                 i % 3 == 1 ? "Incorrect_tariff_displayed" :
                              "Parking_signage_needs_update");
        data->complaints[i].status = i == 3 || i == 7 ? 2 : 0;
    }
    data->complaintCount = 8;

    return storage_save(data);
}

static int legacy_storage_load(ParkyData *data)
{
    FILE *file;
    int count;

    if (data == NULL || ensure_data_dir() != 0) {
        return 0;
    }
    memset(data, 0, sizeof(*data));

    file = fopen("data/users.txt", "r");
    if (file == NULL) {
        return 0;
    }
    count = 0;
    while (count < PARKY_MAX_USERS) {
        char line[512];
        int parsed;
        int roleValue;
        if (fgets(line, sizeof(line), file) == NULL)
            break;
        parsed = sscanf(line, "%d %63s %63s %63s %d %d %127s",
                        &data->users[count].id,
                        data->users[count].name,
                        data->users[count].password,
                        data->users[count].email,
                        &roleValue,
                        &data->users[count].assignedParkingId,
                        data->users[count].avatarPath);
        if (parsed == 7) {
            data->users[count].role = (UserRole)roleValue;
            if (strcmp(data->users[count].avatarPath, "none") == 0)
                data->users[count].avatarPath[0] = '\0';
        } else {
            parsed = sscanf(line, "%d %63s %63s %63s %d %d",
                            &data->users[count].id,
                            data->users[count].name,
                            data->users[count].password,
                            data->users[count].email,
                            &roleValue,
                            &data->users[count].assignedParkingId);
            if (parsed != 6)
                continue;
            data->users[count].role = (UserRole)roleValue;
            data->users[count].avatarPath[0] = '\0';
        }
        ++count;
    }
    data->userCount = count;
    fclose(file);

    file = fopen("data/parkings.txt", "r");
    if (file != NULL) {
        count = 0;
        while (count < PARKY_MAX_PARKINGS &&
               fscanf(file, "%d %63s %127s %d %d %d",
                      &data->parkings[count].id,
                      data->parkings[count].name,
                      data->parkings[count].address,
                      &data->parkings[count].totalSpots,
                      &data->parkings[count].availableSpots,
                      &data->parkings[count].status) == 6) {
            ++count;
        }
        data->parkingCount = count;
        fclose(file);
    }

    file = fopen("data/reservations.txt", "r");
    if (file != NULL) {
        count = 0;
        while (count < PARKY_MAX_RESERVATIONS &&
               fscanf(file, "%d %d %d %63s %63s %d %d",
                      &data->reservations[count].id,
                      &data->reservations[count].citizenId,
                      &data->reservations[count].parkingId,
                      data->reservations[count].date,
                      data->reservations[count].time,
                      &data->reservations[count].spotNumber,
                      &data->reservations[count].status) == 7) {
            ++count;
        }
        data->reservationCount = count;
        fclose(file);
    }

    file = fopen("data/services.txt", "r");
    if (file != NULL) {
        count = 0;
        while (count < PARKY_MAX_SERVICES &&
               fscanf(file, "%d %63s %127s %f %d",
                      &data->services[count].id,
                      data->services[count].name,
                      data->services[count].description,
                      &data->services[count].price,
                      &data->services[count].status) == 5) {
            ++count;
        }
        data->serviceCount = count;
        fclose(file);
    }

    file = fopen("data/reviews.txt", "r");
    if (file != NULL) {
        count = 0;
        while (count < PARKY_MAX_REVIEWS &&
               fscanf(file, "%d %d %d %d %127s",
                      &data->reviews[count].id,
                      &data->reviews[count].citizenId,
                      &data->reviews[count].parkingId,
                      &data->reviews[count].rating,
                      data->reviews[count].comment) == 5) {
            ++count;
        }
        data->reviewCount = count;
        fclose(file);
    }

    file = fopen("data/complaints.txt", "r");
    if (file != NULL) {
        count = 0;
        while (count < PARKY_MAX_COMPLAINTS &&
               fscanf(file, "%d %d %63s %127s %d",
                      &data->complaints[count].id,
                      &data->complaints[count].citizenId,
                      data->complaints[count].subject,
                      data->complaints[count].description,
                      &data->complaints[count].status) == 5) {
            ++count;
        }
        data->complaintCount = count;
        fclose(file);
    }

    return 1;
}

typedef struct {
    char magic[4];
    uint32_t version;
    uint32_t count;
} ParkyFileHeader;

static int data_file_exists(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

static int load_records(const char *path, void *records, size_t recordSize,
                        uint32_t capacity, int *count)
{
    FILE *file;
    ParkyFileHeader header;
    size_t got;

    *count = 0;
    file = fopen(path, "rb");
    if (file == NULL) {
        return errno == ENOENT ? 1 : 0;
    }
    if (fread(&header, sizeof(header), 1, file) != 1 ||
        memcmp(header.magic, "PKY2", 4) != 0 || header.version != 2 ||
        header.count > capacity) {
        fclose(file);
        return 0;
    }
    got = fread(records, recordSize, header.count, file);
    if (got != header.count || fgetc(file) != EOF || ferror(file)) {
        fclose(file);
        return 0;
    }
    *count = (int)header.count;
    fclose(file);
    return 1;
}

static int save_records(const char *path, const void *records,
                        size_t recordSize, uint32_t count, uint32_t capacity)
{
    FILE *file;
    ParkyFileHeader header;
    int ok;

    if (count > capacity) {
        return 0;
    }
    memcpy(header.magic, "PKY2", 4);
    header.version = 2;
    header.count = count;
    file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }
    ok = fwrite(&header, sizeof(header), 1, file) == 1 &&
         fwrite(records, recordSize, count, file) == count;
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok;
}

int storage_load(ParkyData *data)
{
    if (data == NULL || ensure_data_dir() != 0) {
        return 0;
    }
    memset(data, 0, sizeof(*data));
    return load_records("data/users.dat", data->users, sizeof(User),
                        PARKY_MAX_USERS, &data->userCount) &&
           load_records("data/parkings.dat", data->parkings, sizeof(Parking),
                        PARKY_MAX_PARKINGS, &data->parkingCount) &&
           load_records("data/reservations.dat", data->reservations,
                        sizeof(Reservation), PARKY_MAX_RESERVATIONS,
                        &data->reservationCount) &&
           load_records("data/services.dat", data->services, sizeof(Service),
                        PARKY_MAX_SERVICES, &data->serviceCount) &&
           load_records("data/reviews.dat", data->reviews, sizeof(Review),
                        PARKY_MAX_REVIEWS, &data->reviewCount) &&
           load_records("data/complaints.dat", data->complaints,
                        sizeof(Complaint), PARKY_MAX_COMPLAINTS,
                        &data->complaintCount);
}

int storage_save(const ParkyData *data)
{
    if (data == NULL || ensure_data_dir() != 0) {
        return 0;
    }
    return save_records("data/users.dat", data->users, sizeof(User),
                        (uint32_t)data->userCount, PARKY_MAX_USERS) &&
           save_records("data/parkings.dat", data->parkings, sizeof(Parking),
                        (uint32_t)data->parkingCount, PARKY_MAX_PARKINGS) &&
           save_records("data/reservations.dat", data->reservations,
                        sizeof(Reservation), (uint32_t)data->reservationCount,
                        PARKY_MAX_RESERVATIONS) &&
           save_records("data/services.dat", data->services, sizeof(Service),
                        (uint32_t)data->serviceCount, PARKY_MAX_SERVICES) &&
           save_records("data/reviews.dat", data->reviews, sizeof(Review),
                        (uint32_t)data->reviewCount, PARKY_MAX_REVIEWS) &&
           save_records("data/complaints.dat", data->complaints,
                        sizeof(Complaint), (uint32_t)data->complaintCount,
                        PARKY_MAX_COMPLAINTS);
}

int storage_init(ParkyData *data)
{
    static const char *binaryFiles[] = {
        "data/users.dat", "data/parkings.dat", "data/reservations.dat",
        "data/services.dat", "data/reviews.dat", "data/complaints.dat"
    };
    static const char *legacyFiles[] = {
        "data/users.txt", "data/parkings.txt", "data/reservations.txt",
        "data/services.txt", "data/reviews.txt", "data/complaints.txt"
    };
    size_t i;
    int anyBinary = 0;
    int anyLegacy = 0;

    if (data == NULL || ensure_data_dir() != 0) {
        return -1;
    }
    for (i = 0; i < sizeof(binaryFiles) / sizeof(binaryFiles[0]); ++i) {
        anyBinary |= data_file_exists(binaryFiles[i]);
        anyLegacy |= file_exists(legacyFiles[i]);
    }
    if (!anyBinary && anyLegacy) {
        if (legacy_storage_load(data) &&
            (data->userCount + data->parkingCount + data->reservationCount +
             data->serviceCount + data->reviewCount + data->complaintCount > 0) &&
            storage_save(data)) {
            return 0;
        }
        /* A bad or empty legacy store falls through to normal initialization. */
    }
    if (storage_load(data) && data->parkingCount >= 2) {
        return 0;
    }
    storage_seed(data);
    return generate_demo_data(data) ? 0 : -1;
}

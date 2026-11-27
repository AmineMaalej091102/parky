#define _POSIX_C_SOURCE 200809L

#include "admin.h"
#include "agent.h"
#include "citoyen.h"
#include "storage.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_DIR "tests/tmp_data"
#define RECORDS 501

typedef struct {
    char magic[4];
    uint32_t version;
    uint32_t count;
} TestHeader;

static int failures;
static int checks;

static void check(int condition, const char *name)
{
    ++checks;
    if (condition) {
        printf("[PASS] %s\n", name);
    } else {
        printf("[FAIL] %s\n", name);
        ++failures;
    }
}

static void remove_runtime_files(void)
{
    static const char *files[] = {
        "data/users.dat", "data/parkings.dat", "data/reservations.dat",
        "data/services.dat", "data/reviews.dat", "data/complaints.dat",
        "data/users.txt", "data/parkings.txt", "data/reservations.txt",
        "data/services.txt", "data/reviews.txt", "data/complaints.txt"
    };
    size_t i;

    for (i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
        (void)unlink(files[i]);
    }
    (void)rmdir("data/avatars");
    (void)rmdir("data");
}

static int write_fixture(const char *path, const void *records,
                         size_t recordSize, uint32_t count)
{
    FILE *file = fopen(path, "wb");
    TestHeader header;
    int ok;

    if (file == NULL) {
        return 0;
    }
    memcpy(header.magic, "PKY2", 4);
    header.version = 2;
    header.count = count;
    ok = fwrite(&header, sizeof(header), 1, file) == 1 &&
         fwrite(records, recordSize, count, file) == count;
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok;
}

static int test_heap_growth_and_binary_round_trip(void)
{
    ParkyData data;
    Parking *parkings;
    Reservation *reservations;
    FILE *file;
    TestHeader header;
    int ok = 1;
    int i;

    memset(&data, 0, sizeof(data));
    parkings = (Parking *)calloc(RECORDS, sizeof(*parkings));
    reservations = (Reservation *)calloc(RECORDS, sizeof(*reservations));
    if (parkings == NULL || reservations == NULL) {
        free(parkings);
        free(reservations);
        check(0, "allocate large binary fixture");
        return 0;
    }
    for (i = 0; i < RECORDS; ++i) {
        parkings[i].id = i + 1;
        snprintf(parkings[i].name, sizeof(parkings[i].name), "Parking_%04d", i);
        snprintf(parkings[i].address, sizeof(parkings[i].address), "Address_%04d", i);
        parkings[i].totalSpots = 100 + i;
        parkings[i].availableSpots = 50 + i;
        parkings[i].status = i % 3;
        reservations[i].id = i + 1;
        reservations[i].citizenId = 1000 + i;
        reservations[i].parkingId = i + 1;
        snprintf(reservations[i].date, sizeof(reservations[i].date), "2026-12-%02d", i % 28 + 1);
        snprintf(reservations[i].time, sizeof(reservations[i].time), "%02d:30", i % 24);
        reservations[i].spotNumber = i + 10;
        reservations[i].status = i % 3;
    }
    ok = write_fixture("data/parkings.dat", parkings, sizeof(*parkings), RECORDS) &&
         write_fixture("data/reservations.dat", reservations,
                       sizeof(*reservations), RECORDS);
    check(ok, "write 501-record binary fixtures");
    free(parkings);
    free(reservations);
    if (!ok) {
        return 0;
    }

    check(storage_load(&data), "load binary fixtures");
    check(data.parkingCount == RECORDS && data.parkingCapacity >= RECORDS &&
          data.reservationCount == RECORDS &&
          data.reservationCapacity >= RECORDS,
          "heap collections grow beyond legacy capacities");
    check(data.parkings[RECORDS - 1].id == RECORDS &&
          data.reservations[RECORDS - 1].citizenId == 1000 + RECORDS - 1,
          "large record collections remain intact");

    file = fopen("data/parkings.dat", "rb");
    ok = file != NULL && fread(&header, sizeof(header), 1, file) == 1;
    if (file != NULL) {
        fclose(file);
    }
    check(ok && memcmp(header.magic, "PKY2", 4) == 0 &&
          header.version == 2 && header.count == RECORDS,
          "binary header contains PKY2, version 2, and record count");
    check(storage_save(&data), "save large binary collections");
    storage_cleanup();
    check(data.users == NULL && data.parkings == NULL &&
          data.reservations == NULL && data.services == NULL &&
          data.reviews == NULL && data.complaints == NULL &&
          data.parkingCapacity == 0 && data.reservationCapacity == 0,
          "storage_cleanup frees every collection buffer");
    check(storage_load(&data) && data.parkingCount == RECORDS &&
          data.reservationCount == RECORDS &&
          data.parkings[RECORDS - 1].totalSpots == 100 + RECORDS - 1 &&
          data.reservations[RECORDS - 1].spotNumber == RECORDS - 1 + 10,
          "binary round trip preserves 501 parking and reservation records");
    storage_cleanup();
    return 1;
}

static int test_roles_and_business_logic(void)
{
    ParkyData data;
    User user;
    User result;
    Parking *parking;
    Parking newParking;
    int ok;

    memset(&data, 0, sizeof(data));
    check(storage_init(&data) == 0, "initialize isolated demo dataset");
    if (data.parkings == NULL || data.users == NULL) {
        storage_cleanup();
        return 0;
    }

    memset(&user, 0, sizeof(user));
    user.id = 9001;
    strcpy(user.name, "test_admin_user");
    strcpy(user.password, "Initial1");
    strcpy(user.email, "initial@example.test");
    user.role = USER_AGENT;
    check(admin_add_user(&data, &user), "admin creates a user");
    strcpy(user.name, "updated_test_user");
    check(admin_update_user(&data, &user) &&
          admin_get_user(&data, user.id, &result) &&
          strcmp(result.name, "updated_test_user") == 0,
          "admin updates and retrieves a user");
    check(admin_delete_user(&data, user.id) &&
          !admin_get_user(&data, user.id, &result),
          "admin deletes a user");
    memset(&newParking, 0, sizeof(newParking));
    newParking.id = 9001;
    strcpy(newParking.name, "CRUD_Parking");
    strcpy(newParking.address, "Test_Street");
    newParking.totalSpots = 20;
    newParking.availableSpots = 12;
    newParking.status = 1;
    check(admin_add_parking(&data, &newParking), "admin creates a parking");
    newParking.availableSpots = 8;
    check(admin_update_parking(&data, &newParking) &&
          data.parkings[data.parkingCount - 1].availableSpots == 8,
          "admin updates a parking");
    check(admin_delete_parking(&data, newParking.id),
          "admin deletes a parking");
    check(admin_parking_occupancy_rate(&data, 1) >= 0.0 &&
          admin_parking_occupancy_rate(&data, 1) <= 100.0,
          "admin occupancy rate stays within 0 to 100 percent");

    check(agent_set_spot_status(&data, 1, PARKING_OCCUPIED) &&
          data.parkings[0].availableSpots == 0,
          "agent marks a parking occupied");
    check(agent_resolve_reservation(&data, data.reservations[0].id, 1) &&
          data.reservations[0].status == RESERVATION_APPROVED,
          "agent approves a reservation");

    memset(&user, 0, sizeof(user));
    user.id = 9002;
    strcpy(user.name, "test_citizen");
    strcpy(user.email, "citizen@example.test");
    strcpy(user.password, "weakpass");
    check(!citoyen_register(&data, &user),
          "citizen registration rejects a weak password");
    strcpy(user.password, "StrongPass1");
    check(citoyen_register(&data, &user) &&
          citoyen_login(&data, user.email, user.password) == user.id,
          "citizen registration accepts a strong password");
    check(citoyen_compute_billing(&data, 1, 1, 3) == 18.0,
          "citizen booking tariff totals 18.00 TND");
    parking = data.parkings;
    ok = citoyen_book_service(&data, user.id, parking[0].id,
                              data.services[0].id);
    check(ok && data.reservations[data.reservationCount - 1].citizenId == user.id,
          "citizen books a service");
    storage_cleanup();
    return 1;
}

int main(void)
{
    int result;

    if (mkdir(TEST_DIR, 0755) != 0 && access(TEST_DIR, F_OK) != 0) {
        perror("create tests/tmp_data");
        return 1;
    }
    if (chdir(TEST_DIR) != 0) {
        perror("enter tests/tmp_data");
        return 1;
    }
    remove_runtime_files();
    (void)mkdir("data", 0755);
    result = test_heap_growth_and_binary_round_trip();
    storage_cleanup();
    remove_runtime_files();
    (void)mkdir("data", 0755);
    if (!test_roles_and_business_logic()) {
        ++failures;
    }
    storage_cleanup();
    remove_runtime_files();
    if (chdir("../..") != 0) {
        perror("return from tests/tmp_data");
        return 1;
    }
    (void)rmdir(TEST_DIR);
    printf("%d checks, %d failures\n", checks, failures);
    return result && failures == 0 ? 0 : 1;
}

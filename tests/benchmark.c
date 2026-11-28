#define _POSIX_C_SOURCE 200809L

#include "models.h"
#include "storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define BENCH_DIR "tests/bench_tmp_data"
#define RECORDS 5000

static double elapsed_seconds(const struct timespec *start,
                              const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
}

static long file_size(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 ? (long)info.st_size : -1L;
}

static int write_ascii(const ParkyData *data)
{
    FILE *file;
    int i;

    file = fopen("data/parkings.txt", "w");
    if (file == NULL) {
        return 0;
    }
    for (i = 0; i < data->parkingCount; ++i) {
        const Parking *parking = &data->parkings[i];
        if (fprintf(file, "%d %s %s %d %d %d\n", parking->id,
                    parking->name, parking->address, parking->totalSpots,
                    parking->availableSpots, parking->status) < 0) {
            fclose(file);
            return 0;
        }
    }
    if (fclose(file) != 0) {
        return 0;
    }

    file = fopen("data/reservations.txt", "w");
    if (file == NULL) {
        return 0;
    }
    for (i = 0; i < data->reservationCount; ++i) {
        const Reservation *reservation = &data->reservations[i];
        if (fprintf(file, "%d %d %d %s %s %d %d\n", reservation->id,
                    reservation->citizenId, reservation->parkingId,
                    reservation->date, reservation->time,
                    reservation->spotNumber, reservation->status) < 0) {
            fclose(file);
            return 0;
        }
    }
    return fclose(file) == 0;
}

static void cleanup_files(void)
{
    static const char *files[] = {
        "data/users.dat", "data/parkings.dat", "data/reservations.dat",
        "data/services.dat", "data/reviews.dat", "data/complaints.dat",
        "data/parkings.txt", "data/reservations.txt"
    };
    size_t i;

    for (i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
        (void)unlink(files[i]);
    }
    (void)rmdir("data/avatars");
    (void)rmdir("data");
}

int main(void)
{
    ParkyData data;
    struct timespec start;
    struct timespec end;
    double asciiTime;
    double binaryTime;
    long asciiBytes;
    long binaryBytes;
    long firstSize;
    long secondSize;
    int i;
    int ok = 1;

    if (mkdir(BENCH_DIR, 0755) != 0 && access(BENCH_DIR, F_OK) != 0) {
        perror("create tests/bench_tmp_data");
        return 1;
    }
    if (chdir(BENCH_DIR) != 0) {
        perror("enter tests/bench_tmp_data");
        return 1;
    }
    cleanup_files();
    memset(&data, 0, sizeof(data));
    (void)mkdir("data", 0755);
    data.parkings = (Parking *)calloc(RECORDS, sizeof(*data.parkings));
    data.reservations = (Reservation *)calloc(RECORDS,
                                               sizeof(*data.reservations));
    if (data.parkings == NULL || data.reservations == NULL) {
        fprintf(stderr, "Unable to allocate benchmark records.\n");
        free(data.parkings);
        free(data.reservations);
        cleanup_files();
        (void)chdir("../..");
        (void)rmdir(BENCH_DIR);
        return 1;
    }
    data.parkingCount = data.parkingCapacity = RECORDS;
    data.reservationCount = data.reservationCapacity = RECORDS;
    for (i = 0; i < RECORDS; ++i) {
        Parking *parking = &data.parkings[i];
        Reservation *reservation = &data.reservations[i];
        parking->id = i + 1;
        snprintf(parking->name, sizeof(parking->name), "Parking_%05d", i + 1);
        snprintf(parking->address, sizeof(parking->address),
                 "Street_%05d_Tunis_Center", i + 1);
        parking->totalSpots = 100 + i % 500;
        parking->availableSpots = 10 + i % 100;
        parking->status = i % 3;
        reservation->id = i + 1;
        reservation->citizenId = 10000 + i;
        reservation->parkingId = i + 1;
        snprintf(reservation->date, sizeof(reservation->date),
                 "2026-12-%02d", i % 28 + 1);
        snprintf(reservation->time, sizeof(reservation->time), "%02d:%02d",
                 i % 24, i % 60);
        reservation->spotNumber = i + 1;
        reservation->status = i % 3;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0 || !write_ascii(&data) ||
        clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        fprintf(stderr, "ASCII benchmark serialization failed.\n");
        ok = 0;
    }
    asciiTime = ok ? elapsed_seconds(&start, &end) : 0.0;
    firstSize = file_size("data/parkings.txt");
    secondSize = file_size("data/reservations.txt");
    asciiBytes = firstSize < 0 || secondSize < 0 ? -1L : firstSize + secondSize;

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0 || !storage_save(&data) ||
        clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        fprintf(stderr, "Binary benchmark serialization failed.\n");
        ok = 0;
    }
    binaryTime = ok ? elapsed_seconds(&start, &end) : 0.0;
    firstSize = file_size("data/parkings.dat");
    secondSize = file_size("data/reservations.dat");
    binaryBytes = firstSize < 0 || secondSize < 0 ?
                  -1L : firstSize + secondSize;

    if (asciiBytes < 0 || binaryBytes < 0) {
        fprintf(stderr, "Unable to read benchmark file sizes.\n");
        ok = 0;
    } else {
        printf("Records per entity: %d\n", RECORDS);
        printf("ASCII .txt:  %.6f seconds, %ld bytes (parking + reservation)\n",
               asciiTime, asciiBytes);
        printf("Binary .dat: %.6f seconds, %ld bytes (parking + reservation)\n",
               binaryTime, binaryBytes);
    }

    free(data.parkings);
    free(data.reservations);
    cleanup_files();
    (void)chdir("../..");
    (void)rmdir(BENCH_DIR);
    return ok ? 0 : 1;
}

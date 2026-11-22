#include <stdio.h>
#include "models.h"
#include "storage.h"

int main(void)
{
    ParkyData data;
    ParkyData loaded;

    if (storage_init(&data) != 0) {
        fprintf(stderr, "Storage initialization failed.\n");
        return 1;
    }

    if (!storage_save(&data)) {
        fprintf(stderr, "Storage save failed.\n");
        return 1;
    }

    if (!storage_load(&loaded)) {
        fprintf(stderr, "Storage load failed.\n");
        return 1;
    }

    printf("Storage verified: %d users, %d parkings, %d reservations, "
           "%d services, %d reviews, %d complaints.\n",
           loaded.userCount,
           loaded.parkingCount,
           loaded.reservationCount,
           loaded.serviceCount,
           loaded.reviewCount,
           loaded.complaintCount);
    return 0;
}

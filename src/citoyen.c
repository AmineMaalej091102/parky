#include "citoyen.h"
#include "agent.h"
#include "storage.h"

#include <stdio.h>
#include <string.h>

static int next_id(const void *items, int count, size_t size)
{
    const unsigned char *array = (const unsigned char *)items;
    int maxId = 0;
    int i;
    for (i = 0; i < count; ++i) {
        const int *id = (const int *)(array + (size_t)i * size);
        if (*id > maxId) {
            maxId = *id;
        }
    }
    return maxId + 1;
}

int citoyen_register(ParkyData *data, const User *user)
{
    int i;
    if (data->userCount >= PARKY_MAX_USERS) {
        return 0;
    }
    for (i = 0; i < data->userCount; ++i) {
        if (strcmp(data->users[i].email, user->email) == 0) {
            return 0;
        }
    }
    data->users[data->userCount++] = *user;
    data->users[data->userCount - 1].role = USER_CITIZEN;
    return storage_save(data);
}

int citoyen_login(const ParkyData *data, const char *email,
                  const char *password)
{
    int i;
    for (i = 0; i < data->userCount; ++i) {
        if (data->users[i].role == USER_CITIZEN &&
            strcmp(data->users[i].email, email) == 0 &&
            strcmp(data->users[i].password, password) == 0) {
            return data->users[i].id;
        }
    }
    return 0;
}

int citoyen_filter_parkings(const ParkyData *data, const char *name,
                            const char *address, int onlyAvailable,
                            Parking *output, int maxCount)
{
    int count = 0;
    int i;
    for (i = 0; i < data->parkingCount && count < maxCount; ++i) {
        const Parking *parking = &data->parkings[i];
        if (name != NULL && name[0] != '\0' &&
            strstr(parking->name, name) == NULL) {
            continue;
        }
        if (address != NULL && address[0] != '\0' &&
            strstr(parking->address, address) == NULL) {
            continue;
        }
        if (onlyAvailable && parking->availableSpots <= 0) {
            continue;
        }
        output[count++] = *parking;
    }
    return count;
}

int citoyen_reserve_spot(ParkyData *data, int citizenId, int parkingId,
                         const char *date, const char *time, int spotNumber)
{
    Reservation reservation;
    int i;

    if (data->reservationCount >= PARKY_MAX_RESERVATIONS) {
        return 0;
    }
    for (i = 0; i < data->parkingCount; ++i) {
        if (data->parkings[i].id == parkingId &&
            data->parkings[i].availableSpots > 0) {
            memset(&reservation, 0, sizeof(reservation));
            reservation.id = next_id(data->reservations,
                                     data->reservationCount,
                                     sizeof(Reservation));
            reservation.citizenId = citizenId;
            reservation.parkingId = parkingId;
            snprintf(reservation.date, sizeof(reservation.date), "%s", date);
            snprintf(reservation.time, sizeof(reservation.time), "%s", time);
            reservation.spotNumber = spotNumber;
            reservation.status = RESERVATION_PENDING;
            data->reservations[data->reservationCount++] = reservation;
            --data->parkings[i].availableSpots;
            return storage_save(data);
        }
    }
    return 0;
}

double citoyen_compute_billing(const ParkyData *data, int parkingId,
                               int serviceId, int hours)
{
    double total = 0.0;
    int i;
    if (hours > 0) {
        total = (double)hours * 2.0;
    }
    (void)parkingId;
    if (serviceId > 0) {
        for (i = 0; i < data->serviceCount; ++i) {
            if (data->services[i].id == serviceId &&
                data->services[i].status == 1) {
                total += (double)data->services[i].price;
                break;
            }
        }
    }
    return total;
}

int citoyen_book_service(ParkyData *data, int citizenId, int parkingId,
                         int serviceId)
{
    int i;
    for (i = 0; i < data->serviceCount; ++i) {
        if (data->services[i].id == serviceId &&
            data->services[i].status == 1) {
            Reservation reservation;
            if (data->reservationCount >= PARKY_MAX_RESERVATIONS) {
                return 0;
            }
            memset(&reservation, 0, sizeof(reservation));
            reservation.id = next_id(data->reservations,
                                     data->reservationCount,
                                     sizeof(Reservation));
            reservation.citizenId = citizenId;
            reservation.parkingId = parkingId;
            snprintf(reservation.date, sizeof(reservation.date), "service");
            snprintf(reservation.time, sizeof(reservation.time),
                     "service-%d", serviceId);
            reservation.spotNumber = 0;
            reservation.status = RESERVATION_PENDING;
            data->reservations[data->reservationCount++] = reservation;
            return storage_save(data);
        }
    }
    return 0;
}

int citoyen_submit_review(ParkyData *data, int citizenId, int parkingId,
                          int rating, const char *comment)
{
    Review review;
    if (data->reviewCount >= PARKY_MAX_REVIEWS ||
        rating < 1 || rating > 5) {
        return 0;
    }
    memset(&review, 0, sizeof(review));
    review.id = next_id(data->reviews, data->reviewCount, sizeof(Review));
    review.citizenId = citizenId;
    review.parkingId = parkingId;
    review.rating = rating;
    snprintf(review.comment, sizeof(review.comment), "%s", comment);
    data->reviews[data->reviewCount++] = review;
    return storage_save(data);
}

int citoyen_submit_complaint(ParkyData *data, int citizenId,
                             const char *subject, const char *description)
{
    Complaint complaint;
    if (data->complaintCount >= PARKY_MAX_COMPLAINTS) {
        return 0;
    }
    memset(&complaint, 0, sizeof(complaint));
    complaint.id = next_id(data->complaints, data->complaintCount,
                           sizeof(Complaint));
    complaint.citizenId = citizenId;
    snprintf(complaint.subject, sizeof(complaint.subject), "%s", subject);
    snprintf(complaint.description, sizeof(complaint.description), "%s",
             description);
    complaint.status = 0;
    data->complaints[data->complaintCount++] = complaint;
    return storage_save(data);
}

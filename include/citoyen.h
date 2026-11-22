#ifndef CITOYEN_H
#define CITOYEN_H

#include "models.h"

int citoyen_register(ParkyData *data, const User *user);
int citoyen_login(const ParkyData *data, const char *email,
                  const char *password);

int citoyen_filter_parkings(const ParkyData *data, const char *name,
                            const char *address, int onlyAvailable,
                            Parking *output, int maxCount);

int citoyen_reserve_spot(ParkyData *data, int citizenId, int parkingId,
                         const char *date, const char *time, int spotNumber);
double citoyen_compute_billing(const ParkyData *data, int parkingId,
                               int serviceId, int hours);
int citoyen_book_service(ParkyData *data, int citizenId, int parkingId,
                         int serviceId);

int citoyen_submit_review(ParkyData *data, int citizenId, int parkingId,
                          int rating, const char *comment);
int citoyen_submit_complaint(ParkyData *data, int citizenId,
                             const char *subject, const char *description);

#endif

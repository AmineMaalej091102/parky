#ifndef AGENT_H
#define AGENT_H

#include "models.h"

#define PARKING_AVAILABLE 0
#define PARKING_OCCUPIED 1
#define PARKING_RESERVED 2

#define RESERVATION_PENDING 0
#define RESERVATION_APPROVED 1
#define RESERVATION_REJECTED 2

int agent_set_spot_status(ParkyData *data, int parkingId, int status);
int agent_add_service(ParkyData *data, const Service *service);
int agent_update_service(ParkyData *data, const Service *service);
int agent_delete_service(ParkyData *data, int serviceId);
int agent_resolve_reservation(ParkyData *data, int reservationId, int approved);
int agent_submit_complaint(ParkyData *data, int agentId,
                           const char *subject, const char *description);

#endif
